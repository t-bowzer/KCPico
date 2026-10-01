# KeybChord USB-MIDI hardware test harness.
#
# Drives the device's gated CDC test-command hook (build [env:testhook]) and
# asserts that the USB-MIDI device output matches the DIN output (fed back
# through a DIN MIDI->USB interface). Run on a host with the device plugged in:
#
#   python harness.py --serial COM5 --usb KeybChord
#
# Requirements: pip install -r requirements.txt
from __future__ import annotations

import argparse
import sys
import time

import mido

try:
    import serial  # pyserial
except ImportError:
    serial = None


TH_PREFIX = "[TH] "


class MidiCollector:
    """Collects normalized MIDI messages from a mido input port via callback."""

    def __init__(self, port_name: str):
        self.name = port_name
        self.messages = []
        self._in = mido.open_input(port_name, callback=self._on_message)

    def _on_message(self, msg):
        self.messages.append(self._normalize(msg))

    @staticmethod
    def _normalize(msg):
        t = msg.type
        if t == "note_on" and msg.velocity == 0:
            t = "note_off"
        if t in ("note_on", "note_off"):
            return (t, msg.channel + 1, msg.note, msg.velocity)
        if t == "control_change":
            return (t, msg.channel + 1, msg.control, msg.value)
        if t == "program_change":
            return (t, msg.channel + 1, msg.program, 0)
        if t in ("clock", "start", "continue", "stop"):
            return (t, 0, 0, 0)
        return (t, 0, 0, 0)

    def clear(self):
        self.messages.clear()

    def close(self):
        self._in.close()


def cmd(ser, line: str, timeout: float = 2.0):
    """Send a command line; return the list of [TH] response lines."""
    ser.reset_input_buffer()
    ser.write((line + "\n").encode())
    responses = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode(errors="replace").strip()
        if text.startswith(TH_PREFIX):
            responses.append(text[len(TH_PREFIX):])
            if responses[-1].startswith(("OK", "PONG", "KEYBCHORD-TESTHOOK")):
                break
        else:
            # Ignore debug-log lines, but honor a bounded read window.
            pass
    return responses


def expect(ser, line, timeout=2.0):
    responses = cmd(ser, line, timeout)
    return responses


def open_serial(port: str, baud: int):
    if serial is None:
        sys.exit("pyserial is not installed (pip install -r requirements.txt)")
    return serial.Serial(port, baud, timeout=0.2)


def find_ports(usb_substr: str, din_substr: str | None):
    mido.set_backend("mido.backends.rtmidi")
    names = mido.get_input_names()
    usb = [n for n in names if usb_substr.lower() in n.lower()]
    if not usb:
        sys.exit(f"No MIDI input matching {usb_substr!r}; available: {names}")
    if len(usb) > 1:
        sys.exit(f"Multiple matches for {usb_substr!r}: {usb}")
    usb_port = usb[0]

    if din_substr:
        din = [n for n in names if din_substr.lower() in n.lower() and n != usb_port]
        if not din:
            sys.exit(f"No MIDI input matching {din_substr!r}; available: {names}")
        if len(din) > 1:
            sys.exit(f"Multiple matches for {din_substr!r}: {din}")
        din_port = din[0]
    else:
        others = [n for n in names if n != usb_port]
        if not others:
            sys.exit("No second MIDI input found for DIN; pass --din <substr>")
        din_port = others[0]
        print(f"DIN input auto-detected: {din_port!r}")
    return usb_port, din_port


def collect(collector: MidiCollector, seconds: float):
    time.sleep(seconds)
    return list(collector.messages)


def main():
    ap = argparse.ArgumentParser(description="KeybChord USB-MIDI hardware test")
    ap.add_argument("--serial", required=True, help="CDC COM/serial port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--usb", default="KeybChord", help="substring of the USB-MIDI input name")
    ap.add_argument("--din", default=None, help="substring of the DIN MIDI->USB input name")
    ap.add_argument("--wait", type=float, default=0.3, help="settle time after each command (s)")
    args = ap.parse_args()

    usb_port, din_port = find_ports(args.usb, args.din)
    print(f"USB MIDI input: {usb_port!r}")
    print(f"DIN MIDI input: {din_port!r}")

    ser = open_serial(args.serial, args.baud)
    usb = MidiCollector(usb_port)
    din = MidiCollector(din_port)

    failures = []

    def check(name, cond, detail=""):
        status = "PASS" if cond else "FAIL"
        print(f"  [{status}] {name}" + (f"  ({detail})" if detail else ""))
        if not cond:
            failures.append(name)

    try:
        # Handshake.
        r = expect(ser, "PING")
        check("CDC handshake (PING/PONG)", any(x == "PONG" for x in r), str(r))

        # 1. USB MIDI enabled by default -> NOTEON/CC/NOTEOFF on both, identical.
        usb.clear(); din.clear()
        expect(ser, "NOTEON 1 60 100")
        expect(ser, "CC 1 10 64")
        expect(ser, "NOTEOFF 1 60")
        collect(usb, args.wait); collect(din, args.wait)
        usb_msgs, din_msgs = list(usb.messages), list(din.messages)
        check("USB carries note/cc", len(usb_msgs) >= 3, f"{len(usb_msgs)} msgs")
        check("DIN carries note/cc", len(din_msgs) >= 3, f"{len(din_msgs)} msgs")
        check("USB == DIN", usb_msgs == din_msgs,
              f"usb={usb_msgs} din={din_msgs}" if usb_msgs != din_msgs else "")

        # 2. Toggle USB off -> USB silent, DIN still active.
        expect(ser, "USB off")
        usb.clear(); din.clear()
        expect(ser, "NOTEON 1 62 100")
        expect(ser, "NOTEOFF 1 62")
        collect(usb, args.wait); collect(din, args.wait)
        check("USB silent when off", len(usb.messages) == 0, f"{len(usb.messages)} msgs")
        check("DIN still active when USB off", len(din.messages) >= 2, f"{len(din.messages)} msgs")

        # 3. Toggle USB back on.
        expect(ser, "USB on")
        usb.clear(); din.clear()
        expect(ser, "NOTEON 1 64 100")
        expect(ser, "NOTEOFF 1 64")
        collect(usb, args.wait); collect(din, args.wait)
        check("USB resumes after re-enable", len(usb.messages) >= 2, f"{len(usb.messages)} msgs")

        # 4. MIDI clock -> 0xF8 stream on both.
        expect(ser, "CLOCK on")
        usb.clear(); din.clear()
        collect(usb, 0.5); collect(din, 0.5)
        check("USB carries clock", any(m[0] == "clock" for m in usb.messages))
        check("DIN carries clock", any(m[0] == "clock" for m in din.messages))
        expect(ser, "CLOCK off")

        # 5. Rhythm -> GM drum notes on channel 10.
        expect(ser, "RHYTHM on")
        usb.clear(); din.clear()
        collect(usb, 0.8); collect(din, 0.8)
        ch10 = [m for m in usb.messages if m[0] == "note_on" and m[1] == 10]
        check("USB receives ch10 drum notes", len(ch10) > 0, f"{len(ch10)} notes")
        expect(ser, "RHYTHM off")

        # 6. Panic -> CC120/CC123 on all 16 channels.
        usb.clear(); din.clear()
        expect(ser, "PANIC")
        collect(usb, args.wait); collect(din, args.wait)
        cc120 = sum(1 for m in usb.messages if m[0] == "control_change" and m[2] == 120)
        cc123 = sum(1 for m in usb.messages if m[0] == "control_change" and m[2] == 123)
        check("USB panic CC120 x16", cc120 == 16, f"{cc120}")
        check("USB panic CC123 x16", cc123 == 16, f"{cc123}")
        check("DIN panic matches USB", usb.messages == din.messages)
    finally:
        usb.close()
        din.close()
        ser.close()

    print()
    if failures:
        print(f"FAILED: {len(failures)} check(s): {failures}")
        sys.exit(1)
    print("ALL CHECKS PASSED")
    sys.exit(0)


if __name__ == "__main__":
    main()
