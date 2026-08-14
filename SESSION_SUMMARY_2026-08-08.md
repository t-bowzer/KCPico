# KeybChord Pico — Session 2026-08-08 Summary

## Completed: M2 — Core Framework (code)

### Build Status

| Environment | Result | Details |
|-------------|--------|---------|
| `pio test -e native` | **PASSED** | 41 GoogleTest cases, all green |
| `pio run -e pico` | **SUCCESS** | RAM 12.1% (31672/262144), Flash 7.7% (120812/1568768) |

### New Files Created (26 files)

```
keybchord/
├── platformio.ini                  # [UPDATED] Added ArduinoJson, lib/engines, +<../lib/engines/midi_router.*>
├── src/
│   ├── main.cpp                    # [UPDATED] Full wiring: factory, config load, USB-CDC log, event poll, test note
│   └── pins.h                      # [MOVED from root] now at lib/hw/pins.h
├── lib/
│   ├── core/                       # PURE LOGIC (no Arduino/hw includes)
│   │   ├── midimsg.h              # MIDI status bytes + message constructors (NoteOn/Off, CC, System)
│   │   ├── params.h/.cpp          # PlayMode/VoicingMode enums, ChordParams/StrumParams/RhythmParams/GlobalParams, defaults + bounds
│   │   ├── config.h/.cpp          # Load/validate/save config.json via ArduinoJson, first-boot defaults, per-field fallback
│   │   ├── presets.h/.cpp         # 10 banks × 8 slots, load/save/round-trip JSON, defaults
│   │   └── state.h/.cpp           # StateManager: pending/active params, active notes, snapshot, dirty flag
│   ├── hw/                         # HARDWARE ADAPTERS (real Pico + null fallbacks)
│   │   ├── input_usbhost.h/.cpp   # TinyUSB + Pico-PIO-USB HID keyboard host, boot protocol, LED SET_REPORT
│   │   ├── midi_out_uart.h/.cpp   # Serial2 (UART1) on GP8 @ 31250 8N1
│   │   ├── storage_littlefs.h/.cpp # LittleFS mount/format/read/write/mkdir
│   │   ├── storage_stub.h         # In-memory std::unordered_map for native tests
│   │   ├── factory.h/.cpp         # Real-vs-null adapter selection, graceful degradation on probe failure
│   │   ├── debug_log.h            # USB-CDC key event + MIDI output logging macros (compiled out on native)
│   │   └── pins.h                 # GP8 (DIN TX), GP4/GP5 (I2C0 SDA/SCL)
│   └── engines/                    # BRIDGES core + adapters
│       └── midi_router.h/.cpp     # Per-function channel routing, sendTestNote() proof of MIDI path
└── test/                           # [UPDATED + NEW]
    ├── test_main.cpp               # Factory null-adapter smoke, config load from stub, null input/led/lcd no-crash
    ├── test_midimsg.cpp            # Channel status, NoteOn/Off, CC, ProgramChange, System, constants
    ├── test_config.cpp             # Load defaults, missing, empty, invalid JSON, clamp out-of-range, round-trip
    ├── test_presets.cpp            # Default equality, save/load round-trip, invalid bank/slot, loadPresetOrDefault
    ├── test_state.cpp              # SnapshotActive, noteOn/Off, allNotesOff, loadDefaults reset
    └── test_midi_router.cpp        # NoteOn tracks state, NoteOff removes, CC noop, sendTestNote, multiple notes
```

### Deliverables Completed (M1 → M2 transition)

| # | Deliverable | Status |
|---|-------------|--------|
| D1 | USB-host HID keyboard Input Manager (TinyUSB + Pico-PIO-USB) | Done |
| D2 | State Manager (pending vs active params scaffolding) | Done |
| D3 | MIDI Router (DIN via Serial2/UART1) | Done |
| D4 | Config/Preset store (LittleFS + ArduinoJson) | Done |
| D5 | USB-CDC debug log + MIDI monitor | Done |
| D6 | Hardcoded note on DIN + null-factory wiring | Done |
| D7 | Null adapter wiring (all adapters constructable via factory) | Done |

### Acceptance Criteria Covered
- **AC-9 (logic path):** DIN output with graceful behavior when unconnected. Factory falls back to NullMidiOutAdapter on probe failure; MidiOutUart::send() always writes to Serial2 regardless of cable state.

### Implementation Notes
- **Namespace:** All new code lives in the global namespace (matching existing base.h/adapters_null.h from M1). The `kc::` namespace wrapper was removed during debugging.
- **TinyUSB integration:** The Pico-PIO-USB library plugs into the arduino-pico core's bundled Adafruit TinyUSB through `#include "pio_usb.h"` + `#include "host/usbh.h"` + `#include "class/hid/hid_host.h"` (include order matters).
- **Native env:** ArduinoJson added to `lib_deps`. `lib/engines` added to `lib_extra_dirs`. `midi_router.*` added to `build_src_filter`.
- **pins.h location:** Moved from project root to `lib/hw/pins.h` so it's in the PlatformIO include path for both `src/` and `lib/` files.

---

## Remaining: T1 + T19 + T20 (User hardware)

### T1 — Hardware Preparation ([COMPLETE?])

The wiring cheatsheet (saved separately) was reviewed and verified against the spec. All connections confirmed correct:
- PIO-USB host (GP0/D+, GP1/D- via 22Ω to USB-A receptacle)
- DIN MIDI OUT (GP8→10Ω→DIN pin 5; 3V3→33Ω→DIN pin 4; GND→DIN pin 2)
- LCD1602 (GP4/SDA, GP5/SCL, powered at 3V3) — not needed for M2 verification
- 100µF bulk cap across VBUS/GND near receptacle
- Pico powered via native micro-USB, ≥1A supply
- PCF8574 backpack has onboard pull-ups (confirmed via multimeter)

### T19 — Flash the UF2 firmware

The compiled firmware is at:

```
/home/tyler/repos/KCPico/keybchord/.pio/build/pico/firmware.uf2
```

**Method 1 — BOOTSEL drag-and-drop (easiest):**
1. Hold the BOOTSEL button on the Pico
2. While holding BOOTSEL, plug the Pico's native micro-USB into your computer
3. Release BOOTSEL — the Pico appears as a USB mass storage drive named `RPI-RP2`
4. Copy `firmware.uf2` to the `RPI-RP2` drive
5. The Pico will automatically reboot and run the firmware

**Method 2 — WSL2 USB forwarding (for flashing from WSL):**
If you want to use `picotool` or PlatformIO upload from within WSL2:
1. On Windows, install `usbipd-win` from https://github.com/dorssel/usbipd-win
2. Put the Pico in BOOTSEL mode
3. In an admin PowerShell: `usbipd list` (find the Pico's BUSID)
4. `usbipd bind --busid <BUSID>` then `usbipd attach --wsl --busid <BUSID>`
5. In WSL: `pio run -e pico -t upload` from the `keybchord/` directory
6. After flashing: `usbipd detach --busid <BUSID>` in PowerShell

**Rebuilding if needed:**
```bash
cd /home/tyler/repos/KCPico/keybchord
pio run -e pico
# Output: .pio/build/pico/firmware.uf2
```

### T20 — On-Device Hardware Verification

#### 20a. Open USB-CDC Serial Monitor

After flashing, the Pico presents a USB-CDC serial port. Open it at **115200 baud**:

**From Linux/WSL with picocom:**
```bash
# Install if needed: sudo apt install picocom
# Find the port: ls /dev/ttyACM*
picocom -b 115200 /dev/ttyACM0
```

**From Linux/WSL with PlatformIO:**
```bash
cd /home/tyler/repos/KCPico/keybchord
pio device monitor -b 115200
```

**From Windows:**
Use PuTTY, Tera Term, or the Arduino IDE serial monitor. Connect to the Pico's COM port at 115200 baud.

You should see on startup:
```
KeybChord Pico -- M2 Core Framework
[INFO] KeybChord Pico ready
```

#### 20b. Verify USB Keyboard Input

1. Plug a USB keyboard into the USB-A receptacle (NOT the Pico's native micro-USB — the native port is for power/debug)
2. Press any key on the keyboard
3. In the serial monitor, you should see lines like:
   ```
   [1234] KEY DN usage=0x04 mods=0x00
   [INFO] Test note sent (C4 on channel 1)
   [1235] KEY UP usage=0x04 mods=0x00
   ```
   - `DN` = key pressed down
   - `UP` = key released
   - `usage=0x04` = HID usage code for the 'A' key (for example)
   - `mods=0x00` = modifier byte (0x01=LCtrl, 0x02=LShift, 0x04=LAlt, 0x08=LGui, etc.)

**Troubleshooting if nothing appears:**
- Make sure the keyboard is plugged into the USB-A receptacle wired to GP0/GP1, NOT the Pico's native micro-USB port
- Verify the Pico is powered via its native micro-USB with ≥1A supply
- Check that the 22Ω resistors are properly soldered on D+/D- lines
- Try a different keyboard (some keyboards draw too much current or use non-standard descriptors)
- If the Pico resets when plugging in the keyboard, the 100µF bulk cap may be insufficient — try a larger cap (220µF) or a powered USB hub

#### 20c. Verify DIN MIDI Output

1. Connect the DIN-5 socket to a MIDI synthesizer, DAW interface, or MIDI monitor
2. Press any key on the USB keyboard
3. A **C4 (MIDI note 60) Note-On at velocity 100 on channel 1** should be sent via DIN
4. The serial monitor shows:
   ```
   [1234] MIDI OUT st=0x91 d1=60 d2=100
   ```
   - `st=0x91` = Note-On on channel 1 (0x90 | 1)
   - `d1=60` = C4
   - `d2=100` = velocity

**Troubleshooting if no MIDI is heard:**
- Verify DIN wiring: GP8→10Ω→DIN pin 5, 3V3→33Ω→DIN pin 4, GND→DIN pin 2
- Check that the MIDI cable is connected to the synth's MIDI IN (not OUT or THRU)
- Verify the synth is set to receive on channel 1
- Measure voltage at DIN pin 5 with a multimeter — should see ~3.3V pulsing when a key is pressed
- The firmware sends MIDI regardless of whether anything is connected (AC-9: graceful when unconnected)

#### 20d. Test Hot-Plug Robustness

1. **Keyboard hot-plug:** While watching the serial monitor, unplug and replug the USB keyboard
   - The firmware should NOT crash or hang
   - Key events should resume flowing after re-enumeration (may take 1-3 seconds)
2. **DIN disconnect:** Unplug the DIN MIDI cable while pressing keys
   - The firmware should keep running; no crash
   - Reconnect the DIN cable; MIDI output resumes immediately (there's no buffering — new notes appear on DIN)

#### 20e. Acceptance Criteria Checklist

| Check | AC | Expected Behavior |
|-------|----|-------------------|
| Key events in debug log | M2 exit | HID usage codes, pressed/released, modifiers logged per keystroke |
| Test note on DIN | M2 exit | C4 Note-On ch1 vel=100 on key press, logged to monitor |
| MIDI monitor in log | M2 exit | Every outgoing MIDI message logged with status + data bytes |
| DIN unplugged → no crash | AC-9 | Firmware keeps running; MIDI writes to UART are harmless |
| Keyboard unplug/replug → recovery | AC-9 | Re-enumeration within a few seconds; key events resume |
| Null-logs on native | M2 exit | 41 GoogleTest cases pass; log macros are no-ops on native |

### Next Step: M2 Review Stop → M3 Chord Engine

After T20 hardware verification is complete, M2 can be signed off. M3 will add:
- Keymap Resolver (HID usage + modifier → logical action)
- Chord root/quality resolution
- Same-column + left-adjacent combination matrix
- Root-position & smart voicing
- Independent extensions (add9/add11/add13)
- All five play modes with note lifecycle
- Held-mode latching

### Troubleshooting Notes (updated)
1. **`NotPlatformIOProjectError`** — ensure you're in `/home/tyler/repos/KCPico/keybchord`
2. **`pip` refused on externally-managed env** — use `pipx install platformio` (done)
3. **Udev rules URL 404** — use `develop` branch path or skip; BOOTSEL drag-and-drop is the fallback
4. **`pins.h` not found** — now at `lib/hw/pins.h`, included by both `src/main.cpp` and `lib/hw/midi_out_uart.cpp`
5. **TinyUSB include order** — `host/usbh.h` must precede `class/hid/hid_host.h` to resolve `tuh_itf_info_t`
6. **WSL2 flashing** — use BOOTSEL drag-and-drop (copy UF2 to RPI-RP2 drive) or set up `usbipd-win` for command-line upload
