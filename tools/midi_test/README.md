# KeybChord USB-MIDI hardware test

Automated end-to-end test for the v0.2 **USB MIDI out** feature (M11). It drives
the device's gated CDC test-command hook and verifies that the USB-MIDI device
output matches the DIN output (fed back through a DIN MIDI→USB interface).

## How it works

- The device is flashed with the **`testhook`** build (`pio run -e testhook`),
  which adds a CDC command parser on top of the normal firmware. It accepts
  newline commands (`PING`, `NOTEON`, `CC`, `KEY`, `USB on/off`, `CLOCK on/off`,
  `RHYTHM on/off`, `PANIC`) and answers with `[TH] ...` lines.
- The host sees two MIDI inputs: the native-USB **KeybChord MIDI** device and the
  **DIN MIDI→USB** interface. The firmware fans identical messages to USB MIDI
  and DIN, so the harness asserts **USB ≡ DIN** for every stimulus.

## Setup

Requires Python 3.9–3.12 (64-bit): `python-rtmidi` has no prebuilt wheels for
3.13+, and building it from source on newer interpreters fails.

```bash
python -m pip install -r requirements.txt
```

Flash the test hook build:

```bash
cd keybchord
pio run -e testhook          # -> .pio/build/testhook/firmware.uf2
```

Drag `firmware.uf2` onto the Pico, then connect the Pico's native USB to the
test PC (this powers the device and carries CDC + USB MIDI). Plug the DIN MIDI
out into your DIN MIDI→USB interface.

## Running

```bash
python tools/midi_test/harness.py --serial COM5 --usb KeybChord
```

- `--serial` — the CDC COM/serial port (Windows: `COMx`; `mode` / Device Manager
  shows it).
- `--usb` — substring matching the native USB MIDI input name (default
  `KeybChord`).
- `--din` — optional substring for the DIN MIDI→USB input; if omitted the
  harness picks the non-USB MIDI input automatically.

## What it checks

1. CDC handshake (`PING`/`PONG`).
2. Note-on/CC/note-off arrive on **both** USB and DIN, byte-identical.
3. `USB off` silences USB while DIN keeps transmitting; `USB on` resumes it.
4. `CLOCK on` produces the 24-PPQN `0xF8` stream on both.
5. `RHYTHM on` produces GM drum notes on channel 10.
6. `PANIC` produces CC120/CC123 on all 16 channels, identical on both outputs.

Exit code 0 = all checks passed; 1 = at least one failed (with a failure list).

## Notes

- The harness reads only the `[TH] `-prefixed lines from the CDC stream; the
  normal debug log is ignored.
- Timing between the two MIDI drivers can differ slightly, so comparisons are
  on message *sequence and content*, not timestamps.
- The physical-keyboard smoke test (press a chord/strum by hand) is the one
  manual step not covered by this harness.
