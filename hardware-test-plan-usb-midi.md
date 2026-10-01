# Hardware Test Plan — USB MIDI Out (M11)

Detailed, step-by-step instructions to fully verify that the v0.2 **USB MIDI
out** feature works. Follow in order; record results in the checklist at the
end.

> Summary of what "working" means: the Pico's native USB port enumerates as a
> composite CDC (debug serial) + MIDI device; chord/strum/bass/rhythm/clock MIDI
> is sent over USB **and** over DIN simultaneously (byte-identical); toggling
> USB MIDI off (`Ctrl+F8`) silences USB only; DIN keeps working throughout.

---

## 1. Prerequisites

Hardware:
- KeybChord Pico (assembled, v0.1 hardware: Pico + keyboard host port + LCD +
  DIN out).
- A USB **data** cable for the Pico's native micro-USB port.
- A Windows PC.
- Your **DIN MIDI → USB interface** + a DIN MIDI cable.

Software (Windows):
- Python 3.9–3.12 (64-bit). `python-rtmidi` has no prebuilt wheels for
  Python 3.13+; newer interpreters fail to build it from source.
- PlatformIO (to build the firmware), or a pre-built `testhook` `.uf2`.

---

## 2. Install the test harness dependencies

```powershell
python -m pip install -r tools/midi_test/requirements.txt
```

This installs `mido`, `python-rtmidi`, and `pyserial`.

---

## 3. Build and flash the `testhook` firmware

```powershell
cd keybchord
pio run -e testhook
```

Output firmware: `keybchord/.pio/build/testhook/firmware.uf2`.

Flash it:
1. Hold **BOOTSEL** while plugging the Pico into the PC, then drag
   `firmware.uf2` onto the `RPI-RP2` drive.
2. Or (enclosed build) power off, hold **Esc** on the keyboard, power on, and
   drag onto the drive.

> The `testhook` build adds a CDC command parser on top of normal firmware; it
> does **not** change playing behavior.

---

## 4. Physical connections

1. Connect the Pico's **native micro-USB** to the Windows PC (this powers the
   device *and* carries CDC + USB MIDI).
2. Connect the **keyboard** to the PIO-USB host port.
3. Connect the **DIN MIDI OUT** to your **DIN MIDI → USB interface**; plug that
   interface into the PC.
4. Let it enumerate for ~10 s.

---

## 5. Verify enumeration

### 5.1 CDC serial port

PowerShell:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

Note the COM port for the device (often the highest-numbered one; cross-check in
Device Manager → "Ports (COM & LPT)" → "USB Serial Device (COMx)").

### 5.2 MIDI ports

```powershell
python -c "import mido; mido.set_backend('mido.backends.rtmidi'); print(mido.get_input_names())"
```

You should see **two** MIDI inputs:
1. The device's native USB MIDI (the name depends on the USB descriptor — may be
   "KeybChord", "Raspberry Pi Pico", "Pico", or similar).
2. Your DIN MIDI → USB interface.

If you only see **one** MIDI input, the composite MIDI interface did not
enumerate (see Troubleshooting §8.1).

Note both names — you'll pass the device's name to `--usb` (or a unique
substring of it) and the DIN interface to `--din` (optional; the harness
auto-picks the non-USB input if omitted).

---

## 6. Run the automated harness

```powershell
python tools/midi_test/harness.py --serial COMx --usb "KeybChord"
```

- Replace `COMx` with the port from §5.1.
- Replace `"KeybChord"` with the device's actual MIDI name from §5.2 (any unique
  substring works). Omit `--din` to auto-detect, or pass `--din "YourInterface"`.

Expected behavior: the script prints `PASS`/`FAIL` per check and ends with
either `ALL CHECKS PASSED` (exit 0) or a failure list (exit 1).

It verifies, in order:
1. CDC handshake (`PING`/`PONG`).
2. Note-on / CC / note-off arrive on **both** USB and DIN, byte-identical.
3. `USB off` silences USB while DIN keeps transmitting; `USB on` resumes USB.
4. `CLOCK on` produces the 24-PPQN `0xF8` stream on both.
5. `RHYTHM on` produces GM drum notes on channel 10.
6. `PANIC` produces CC120/CC123 on all 16 channels, identical on both outputs.

> If `--usb` matches nothing or multiple ports, the harness prints the available
> ports and exits — adjust `--usb`/`--din` accordingly.

---

## 7. Manual keyboard smoke test

The one step the harness cannot automate is physical key presses. Do this by
hand with a MIDI monitor open on the device's USB MIDI input (e.g. MIDI-OX on
Windows):

1. Press a chord (e.g. `T` = C major) — hear/see the chord notes over USB.
2. Strum a number-key (e.g. `1`) — strum notes over USB.
3. Enable rhythm (`F5`) and bass (`F7`) — drums (ch 10) and bass (ch 3) over USB.
4. Toggle USB MIDI off with **Ctrl+F8** — USB goes silent; DIN still transmits
   (verify on the DIN→USB port or an external synth). Toggle back on with
   **Ctrl+F8**.

---

## 8. Troubleshooting

### 8.1 Host sees CDC but no MIDI port
The MIDI interface was registered before host enumeration in theory, but if the
host only enumerates CDC, the device needs a re-enumeration after MIDI is
registered. This is the one open item from implementation:
- Report it back; the fix is to add `tud_disconnect(); delay(20);
  tud_connect();` after the USB MIDI interface is registered (mirroring the MSC
  boot-key path).

### 8.2 Harness says "No MIDI input matching ..."
Run the §5.2 listing command and pass the correct substring. Multiple devices
on the bus can cause ambiguity — use a more specific substring.

### 8.3 No `PONG` / handshake fails
- Wrong COM port (`--serial`).
- The device was flashed with the **production** `pico` build, not `testhook`
  (rebuild with `pio run -e testhook`).
- CDC is still initializing — retry once after a couple of seconds.

### 8.4 USB MIDI "off" still sends
Confirm the config default (`midi.usb_enabled` defaults **on**) and that the
toggle command took effect. The interface always enumerates; the toggle only
gates bytes.

### 8.5 DIN side empty
- Check the DIN cable and that the DIN→USB interface appears in the §5.2 list.
- If DIN is confirmed dead but USB works, re-check the DIN wiring (GP8 → 10 Ω →
  DIN pin 5, 3V3 → 33 Ω → pin 4, GND → pin 2).

---

## 9. Results checklist

| # | Check | Pass / Fail |
|---|-------|-------------|
| 1 | Device enumerates CDC + MIDI (two MIDI inputs total) | |
| 2 | Harness: CDC handshake (`PING`/`PONG`) | |
| 3 | Harness: note/CC on both USB and DIN, identical | |
| 4 | Harness: `USB off` silences USB, DIN unaffected | |
| 5 | Harness: `USB on` resumes USB | |
| 6 | Harness: MIDI clock stream on both | |
| 7 | Harness: ch10 drum notes | |
| 8 | Harness: panic CC120/CC123 ×16, both outputs | |
| 9 | Manual: chord + strum audible/visible over USB | |
| 10 | Manual: Ctrl+F8 toggles USB (DIN keeps working) | |

---

## 10. Notes

- USB MIDI compares are on message **sequence/content**, not timestamps (the two
  MIDI drivers can differ by a few ms).
- The debug CDC stream also logs outgoing MIDI when `midi.midi_monitor` is on;
  the harness ignores non-`[TH]` lines but you can watch them in a serial
  terminal at 115200 baud.
