# KeybChord Pico — Session 2026-08-06 Summary

## Completed: M1 — Scaffolding

### Git & GitHub Setup
- Git repo initialized at `/home/tyler/repos/KCPico`
- Connected to GitHub: `github.com/t-bowzer/KCPico`
- Reason for Linux path: WSL2 NTFS/9p mount doesn't support `chmod`, causing `git init` to fail on `/mnt/c/...`

### Toolchain Installed
| Tool | Version | Purpose |
|------|---------|---------|
| `g++` | 15.x | C++17 compiler (native builds) |
| `cmake` | 4.x | Build system (PlatformIO dependency) |
| `python3-pip` / `python3-venv` | 3.14 | PlatformIO host |
| `pipx` / `platformio` | latest | Build orchestrator (pico + native envs) |

### PlatformIO Environments Verified

| Environment | Status | Details |
|-------------|--------|---------|
| `pio test -e native` | **Passing** | GoogleTest skeleton runs on desktop |
| `pio run -e pico` | **Passing** | Firmware compiles to UF2 for RP2040 |

### Project Structure Created

```
KCPico/
├── .gitignore
├── KeybChord_Pico_Spec.md
├── KeybChord_Pico_Roadmap.md
├── docs/
│   ├── wiring.md / wiring.png
└── keybchord/
    ├── platformio.ini              # [env:pico] + [env:native]
    ├── src/main.cpp                # setup/loop + setup1/loop1 stubs
    ├── lib/
    │   ├── hw/base.h               # InputAdapter, MidiOutAdapter, LcdAdapter, StorageAdapter
    │   └── hw/adapters_null.h      # Null/stub implementations for native testing
    ├── data/config.json            # Default global config (seed for LittleFS)
    ├── scripts/
    └── test/test_main.cpp          # GoogleTest scaffolding
```

### Decisions Made
- **USB-CDC (Serial) deferred to M2.** Stubs-only main.cpp avoids TinyUSB CDC link errors. Debug logging comes in properly during M2.
- **Udev rules deferred.** Not needed until hardware flashing (M2+). BOOTSEL drag-and-drop always available as fallback.
- **WSL2 USB forwarding** will need `usbipd-win` on Windows for Pico flashing from Linux.

---

## Next Step: M2 — Core Framework

**Spec reference:** §12.2 / Roadmap §6 M2

### Deliverables
1. **USB-host HID keyboard Input Manager** — TinyUSB + Pico-PIO-USB, parse HID reports, track keys/modifiers, debounce
2. **State Manager** — pending vs active params scaffolding
3. **MIDI Router** — DIN via `Serial2`/UART1, per-function channel routing
4. **Config/Preset store** — LittleFS + ArduinoJson, load/save/validate
5. **USB-CDC debug log + MIDI monitor** — the on-device feedback instrument for M2-M5
6. **Hardcoded note on DIN** — proof of MIDI output path
7. **Null adapter wiring** — all adapters constructable via factory (real on pico, null on native)

### Verify (exit criteria)
- Raw key events captured on-device and observable via USB-CDC debug log
- A hardcoded note plays on DIN and appears in the MIDI monitor
- Null-logs on native (GoogleTest)
- No LCD required

### Acceptance Criteria Covered
- **AC-9:** DIN output, graceful when unconnected

### Hardware Needed (first time)
- Pico wired per docs/wiring.md
- USB keyboard
- DIN MIDI out circuit (10Ω + 33Ω resistors, DIN-5 socket)
- 100 µF bulk cap on 5V rail
- USB-CDC cable for serial monitor

### Key Tech Involved
- `Pico-PIO-USB` library (sekigon-gonnoc) — PIO-driven USB host on GP0/GP1
- TinyUSB host stack (bundled with arduino-pico core)
- `Serial2` (UART1) on GP8 @ 31250 baud
- LittleFS mount + ArduinoJson for config/preset I/O
- HID report parsing (boot + report protocol)

---

## Troubleshooting Notes

1. **`NotPlatformIOProjectError`** — ensure you're in `/home/tyler/repos/KCPico/keybchord`, not the Windows path
2. **`pip` refused on externally-managed env** — use `pipx install platformio` (done)
3. **Udev rules URL 404** — use `develop` branch path or skip until hardware flashing
4. **Pico link errors** — minimal stubs avoid TinyUSB dependency until M2 properly wires it
