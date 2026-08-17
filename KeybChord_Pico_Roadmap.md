# KeybChord (Pico Edition) — Development Roadmap & Architecture Addendum

**Companion to:** `KeybChord_Pico_Spec.md` (v1 Pico port, build-ready)
**Document status:** v1 — detailed implementation addendum for the RP2040 / C++ build
**Purpose:** Provide a concrete development plan, architecture design, project layout, dependency/hardware configuration, milestone breakdown, and comprehensive test plan that expands the Pico spec into an actionable build guide. Where the spec is authoritative (parameter bounds, key map, acceptance criteria) this addendum defers to it and references section numbers.

---

## Table of Contents

1. [Overview & Guiding Principles](#1-overview--guiding-principles)
2. [Architecture Design](#2-architecture-design)
3. [Project Layout](#3-project-layout)
4. [Requirements & Dependencies](#4-requirements--dependencies)
5. [Hardware Layout / Configuration](#5-hardware-layout--configuration)
6. [Feature Buildout — Milestones](#6-feature-buildout--milestones)
7. [Comprehensive Test Plan](#7-comprehensive-test-plan)
8. [Risks, Decisions & Next Steps](#8-risks-decisions--next-steps)

---

## 1. Overview & Guiding Principles

KeybChord (Pico Edition) turns a commodity USB keyboard into an Omnichord-style MIDI controller running on a Raspberry Pi Pico (RP2040), written in C++ on the Arduino-Pico core. It hosts the keyboard over a PIO-driven USB port (TinyUSB + Pico-PIO-USB), emits MIDI over DIN (UART), drives an LCD1602 for feedback, and flashes a keyboard LED (Num Lock by default) as a BPM indicator. It is a MIDI controller only — no onboard synthesis (spec §1.3).

The following principles derive from spec §1.4 and drive every design decision in this document:

- **Portable core, hardware behind adapters.** All pure logic (chord/voicing/strum/rhythm computation, config/preset handling, keymap resolution, state) has **zero Arduino/hardware includes** and is unit-testable on a desktop via GoogleTest (`native` env). All hardware I/O (USB-host input, LCD, UART/DIN, keyboard LED, LittleFS) sits behind adapter interfaces with safe null/no-op fallbacks so the logic compiles and runs without Pico hardware.
- **Integer/fixed-point only in real-time paths.** The RP2040 (Cortex-M0+) has **no FPU**; all per-event and scheduler math is integer or fixed-point. Swing is an integer 0–75 (spec §7.3/§9). Floats only in cold paths (config parse).
- **Test-first for core logic.** GoogleTest unit tests (spec §11.1) are written alongside/before each core module. Hardware paths are verified later on-device (spec §11.2/§11.3).
- **Data-driven behavior.** Chord type formulas, combination matrix, root order, keymap, rhythm patterns, and parameter bounds live in editable JSON on LittleFS (spec §8.3, NFR-7).
- **DIN is the only MIDI path.** No USB MIDI device in v1; the single USB port hosts the keyboard. This eliminates the Pi Zero 2 W host-vs-gadget conflict entirely (spec §2.3).
- **Dual-core real-time model.** Core 0 = input/keymap/chord/strum/MIDI; Core 1 = rhythm scheduler/clock/LED (spec §3.2). Meets NFR-1 (<10 ms) and NFR-2 (<2 ms jitter).
- **Milestone-by-milestone with a review stop after each** (spec §1.6).
- **Never crash on bad input.** Corrupt/missing config or presets regenerate or fall back to defaults (spec NFR-9). Missing/unavailable hardware degrades gracefully (spec NFR-5).

### 1.1 Confirmed build decisions

These were confirmed with the user and govern the plan:

| Decision | Choice |
|----------|--------|
| Board / MCU | Raspberry Pi Pico (RP2040). Pico 2 not required; RP2040 has ample compute for this MIDI-controller workload (no synthesis). |
| Language | C++17. |
| Core / framework | Arduino-Pico core (earlephilhower). |
| Build & test tooling | PlatformIO with `pico` (device) + `native` (desktop test) environments; GoogleTest for the pure core. |
| USB keyboard hosting | TinyUSB + Pico-PIO-USB (PIO-driven USB host port). |
| MIDI output | DIN/UART only (`Serial2` = UART1). No USB MIDI device in v1. |
| Persistence | JSON on LittleFS via ArduinoJson (human-readable, editable). |
| Swing representation | Integer 0–75 (fixed-point), not float (no FPU). |
| Build cadence | Milestone-by-milestone (M1–M8) with a review stop after each (spec §1.6). |
| Draft items | Implement the spec's draft LCD line-2 layout and the 12 named rhythms as-is, kept data-driven for painless post-hardware tuning. |

---

## 2. Architecture Design

The system (spec §3) is a set of cooperating modules coordinated by a central State Manager, split across the RP2040's two cores. This addendum organizes those modules into three layers: a **pure portable core**, a set of **hardware adapters**, and a thin **engine/orchestration** layer that bridges them.

### 2.1 Layered view

```
+------------------------------------------------------------+
| ENTRY POINT  src/main.cpp                                  |
|  - setup(): mount LittleFS, load/validate config           |
|    (generate defaults on first boot), build adapters via   |
|    factory (real on Pico / null on native), wire modules   |
|  - loop():  Core 0 real-time input->MIDI path              |
|  - setup1()/loop1(): Core 1 rhythm scheduler + clock + LED |
+------------------------------------------------------------+
| APPLICATION / ORCHESTRATION  (thin wiring only)            |
|   App, Core0Loop, Core1Scheduler                           |
+-------------------------------+----------------------------+
| PORTABLE CORE (pure, no HW)   | HARDWARE ADAPTERS (I/O)     |
|  keymap resolver              |  input  (TinyUSB/PIO-USB)   |
|  chord engine + voicing       |  midi out (Serial2 UART1)   |
|  strum engine                 |  lcd    (hd44780/PCF8574)   |
|  rhythm math (clock/steps)    |  led    (HID SET_REPORT)    |
|  state manager                |  storage (LittleFS)         |
|  config / preset store logic  |  factory (real-vs-null)     |
|  parameter model + validation |  + Null* for each          |
|  chord naming, midi msg model |                             |
+-------------------------------+----------------------------+
```

**Rule:** nothing in the portable core may include `Arduino.h`, TinyUSB, `Pico-PIO-USB`, `Wire`, `LittleFS`, `Serial2`, etc. Those includes live only in the adapter modules under `lib/hw/`. This is what makes spec §11.1 GoogleTest unit tests buildable in the `native` environment.

### 2.2 Module responsibilities (spec §3.1 mapping)

| Spec module | Addendum home | Responsibility |
|-------------|---------------|----------------|
| Input Manager | `lib/hw/input_usbhost.*` + Core 0 loop | TinyUSB + Pico-PIO-USB host; parse HID reports; track keys/modifiers; debounce; emit semantic events. |
| Keymap Resolver | `lib/core/keymap.*` | HID usage codes + modifier byte → logical actions. Pure. |
| Chord Engine | `lib/core/chords.*`, `lib/core/voicing.*` + `lib/engines/chord_engine.*` | Resolve root/quality + combinations; compute voicings; play modes + latching. |
| Strum Engine | `lib/core/strum.*` + `lib/engines/strum_engine.*` | Derive note pool; map strum keys; apply strum params. |
| Rhythm Engine | `lib/core/rhythm.*` + `lib/engines/rhythm_engine.*` | Core 1 scheduler/clock; GM drums ch10; arp/rhythm sync; integer tempo/swing; MIDI clock; LED beat callback. |
| MIDI Router | `lib/engines/midi_router.*` | Emit to DIN/UART; per-function channel routing; graceful behavior when unconnected. |
| Display Manager | `lib/engines/display_manager.*` | Idle screen + transient param-edit screens + prompts on LCD1602. |
| LED Indicator | `lib/hw/input_usbhost.*` (driven by rhythm engine) | Flash a configurable keyboard LED (Num/Caps/Scroll, default Num Lock) via HID SET_REPORT in sync with the master clock (FR-R8); best-effort. |
| Preset/Config Store | `lib/core/config.*`, `lib/core/presets.*`, `lib/core/params.*` (+ `lib/hw/storage_littlefs.*`) | Load/save/validate config + 80 presets; parameter bounds & clamping. Storage behind an adapter. |
| State Manager | `lib/core/state.*` | Own global runtime state; mediate; `pending_params` vs `active_params`. |

### 2.3 Adapter contract

Each adapter is an abstract base class in `lib/hw/base.h` with a real implementation and a `Null*` implementation:

- `InputAdapter` — `poll()` producing HID key events; `setLed(usage, on)` for the keyboard LED. Null yields nothing.
- `MidiOutAdapter` — `send(msg)`, `flush()`, `available()`. Real: `Serial2` (UART1) @ 31250 8N1. Null: no-op, always "available" so logic runs.
- `LcdAdapter` — `write(line1, line2)`, `clear()`. Null: no-op.
- `StorageAdapter` — `readFile/writeFile/exists/mkdir` over LittleFS. Native test build uses a `std::filesystem`/in-memory stub.

A `lib/hw/factory.*` selects real vs null at construction (real on `pico`, null/stub on `native`). On any probe failure it logs a warning and returns the null implementation. This enables graceful degradation for AC-9, AC-22, AC-23 and NFR-5, and lets the pure core build and be tested on the desktop.

### 2.4 Cores & timing model (spec §3.2, §7.3, NFR-1/2)

- **Core 0 — real-time input path:** USB-host poll (TinyUSB task) → keymap resolver → engines mutate state → MIDI router dispatch over UART. Non-blocking writes. Target key-to-MIDI latency < 10 ms (NFR-1), bounded mainly by the ~1 ms USB poll interval.
- **Core 1 — rhythm/clock:** a single **master MIDI clock** (24 PPQN) runs continuously in the background from boot; its tick counter and phase never reset, so nothing (rhythm or Clock-Out toggles) can make it drift or fall out of sync. The drum-step scheduler, the arp/rhythm chord sync, and the keyboard-LED beat flash all **slave to this master clock** (`time_us_64()`/`micros()`). Target step jitter < 2 ms (NFR-2). Only the `0xF8` clock byte is streamed (no Start/Stop/Continue). LED HID writes are queued here and applied on Core 0, off the Core 0 critical path.
- **Cross-core state:** the State Manager mediates shared state; Core 1 reads a consistent snapshot of chord/arp params. Use SDK spin-lock/`mutex`/inter-core FIFO or double-buffered snapshots. Held-mode latching is two structures on the State Manager: `pending_params` (edited live on Core 0) and `active_params` (snapshot on chord trigger), per FR-C9 / VR-5.
- **No dynamic allocation** in the real-time paths; use static/fixed buffers (NFR-3).

### 2.5 Data flow (spec §3.3)

```
USB host (TinyUSB/PIO-USB) -> Input Manager -> Keymap Resolver
      -> (Chord | Strum | Rhythm | Nav | Param) action
      -> State Manager -> engines produce MIDI events
      -> MIDI Router -> DIN/UART
Display Manager and LED Indicator observe State Manager / scheduler for updates.
```

### 2.6 Key mechanisms locked in

| Concern | Mechanism |
|---------|-----------|
| Held-mode latching (FR-C9/VR-5) | Snapshot sounding-relevant params on trigger into `active_params`; edits & preset-loads mutate only `pending_params`; applied on next trigger. |
| Num-Lock independence (FR-S6) | Interpret raw keypad HID usages directly; never assert or depend on Num Lock. |
| Shift/Caps/Tab as chord roots | Firmware owns the keyboard; reads modifier byte + key array from HID reports; functions use Ctrl/Alt/Super only. |
| DIN-only MIDI | Single `MidiOutAdapter` over `Serial2` (UART1); no USB MIDI; no host/gadget conflict. |
| No FPU | Integer/fixed-point everywhere in real-time paths; swing = integer 0–75. |
| Panic (FR-C11) | `Super+Esc` → router sends CC120 + CC123 on all channels to DIN; clears active-note state. |
| Draft items | LCD line-2 layout and 12-rhythm list implemented as spec shows, sourced from JSON for later tuning. |

---

## 3. Project Layout

Self-contained PlatformIO project under a dedicated `keybchord/` directory, version-controlled with git (spec §1.4). Default JSON templates ship in `data/` and are flashed to LittleFS; generated runtime files live on-device and are **not committed**.

```
keybchord/                      # git repo root (PlatformIO project)
├── platformio.ini              # envs: [env:pico] (device) + [env:native] (GoogleTest)
├── src/
│   └── main.cpp                # setup/loop (Core 0) + setup1/loop1 (Core 1); wiring
│
├── lib/
│   ├── core/                   # PURE LOGIC — no Arduino/HW includes (native test target)
│   │   ├── state.h/.cpp        # StateManager: pending vs active params, active notes
│   │   ├── keymap.h/.cpp       # KeymapResolver: HID usage + modifiers -> actions
│   │   ├── chords.h/.cpp       # interval formulas + combination matrix resolution
│   │   ├── voicing.h/.cpp      # root_position / smart voice-leading, octave clamp (int)
│   │   ├── strum.h/.cpp        # note-pool derivation + key->note mapping
│   │   ├── rhythm.h/.cpp       # step-sequence -> event math; integer tempo/swing/clock
│   │   ├── naming.h/.cpp       # chord name (Omnichord flat spelling) for LCD
│   │   ├── params.h/.cpp       # Parameter Reference model, ranges, clamp/step
│   │   ├── presets.h/.cpp      # preset/bank model, dirty tracking, defaults
│   │   ├── config.h/.cpp       # load / validate / generate-default global config
│   │   └── midimsg.h/.cpp      # MIDI message value objects (note/cc/clock)
│   │
│   ├── hw/                     # ADAPTERS — hardware includes isolated here
│   │   ├── base.h              # abstract adapter interfaces
│   │   ├── input_usbhost.*     # TinyUSB + Pico-PIO-USB HID keyboard host + LED report
│   │   ├── input_null.*        # no-op input (native)
│   │   ├── midi_out_uart.*     # DIN via Serial2 (UART1)
│   │   ├── midi_out_null.*     # no-op MIDI out
│   │   ├── lcd_hd44780.*       # LCD1602 over PCF8574 I2C (Wire)
│   │   ├── lcd_null.*          # no-op LCD
│   │   ├── led_hid.*           # Scroll Lock LED via HID SET_REPORT (on input device)
│   │   ├── led_null.*          # no-op LED
│   │   ├── storage_littlefs.*  # LittleFS-backed StorageAdapter
│   │   ├── storage_stub.*      # std::filesystem/in-memory stub for native tests
│   │   └── factory.*           # real-vs-null adapter selection + probing
│   │
│   └── engines/                # bridge core + adapters + scheduler
│       ├── chord_engine.*      # play modes, note lifecycle, latching
│       ├── strum_engine.*      # strum trigger -> pool notes -> router
│       ├── rhythm_engine.*     # Core 1 scheduler, clock, LED beat callback
│       ├── midi_router.*       # DIN fan-out, per-function channel routing
│       └── display_manager.*   # idle screen, param-edit feedback, prompts
│
├── data/                       # shipped default JSON, flashed to LittleFS (read-only seed)
│   ├── config.json             # default global config (spec §8.1)
│   ├── chord_types.json        # interval formulas (spec §6.2)
│   ├── combinations.json       # same-column + left-adjacent matrix (spec §6.3)
│   ├── root_order.json         # circle-of-fifths order (spec §5.1)
│   ├── keymap.json             # HID usage -> grid position + function bindings
│   ├── param_defaults.json     # Parameter Reference defaults (spec §9)
│   ├── presets/                # default banks (10 x 8)
│   │   ├── bank1.json ... bank10.json
│   └── rhythms/                # one JSON per rhythm (spec §7.1/§7.2)
│       ├── rock1.json ... (12 shipped + user-defined)
│
├── test/                       # GoogleTest — mirrors core/ (spec §11.1), native env
│   ├── test_chords.cpp
│   ├── test_combinations.cpp
│   ├── test_voicing.cpp
│   ├── test_extensions.cpp
│   ├── test_strum.cpp
│   ├── test_rhythm.cpp
│   ├── test_latching.cpp
│   ├── test_keymap.cpp
│   ├── test_naming.cpp
│   ├── test_params.cpp
│   ├── test_presets.cpp
│   └── test_config.cpp
│
├── scripts/
│   └── install.md              # provisioning notes: pin map, LittleFS upload, flashing UF2
├── README.md                   # build/run/test instructions
└── .gitignore                  # excludes .pio/, build artifacts
```

### 3.1 Runtime data on LittleFS (not committed)

At runtime the device reads/writes on its LittleFS partition:

```
LittleFS (onboard flash)
├── config.json                 # generated on first boot from data/ seed (spec §8.1)
├── presets/
│   ├── bank1.json ... bank10.json   # 10 banks x 8 = 80 slots (spec §8.2)
└── rhythms/                    # pattern JSON (spec §7.2)
```

On first boot the config loader mounts LittleFS (formatting if needed), and if files are missing it generates defaults from the shipped `data/` seed (spec NFR-9, FR-P9). The shipped `data/` image can be uploaded with PlatformIO's filesystem-image build/upload target.

### 3.2 Separation guarantee

- `lib/core/` and the `data/` JSON schemas are the only things needed by `test/`. The `native` env compiles `lib/core/` + a storage stub; it never links `lib/hw/` real adapters or `lib/engines/` code that touches hardware.
- `lib/engines/` may include `lib/core/` and `lib/hw/base.h` (interfaces) but constructs concrete adapters only via `lib/hw/factory`.
- `src/main.cpp` is the only wiring point that knows the concrete runtime environment (and the pin map).

---

## 4. Requirements & Dependencies

### 4.1 Toolchain & environments

- **Language:** C++17.
- **Framework:** Arduino-Pico core (earlephilhower), which bundles the Pico SDK, TinyUSB, LittleFS, dual-core (`setup1/loop1`), and hardware UARTs (`Serial1`/`Serial2`).
- **Build system:** PlatformIO with two environments:
  - `[env:pico]` — `platform = raspberrypi`, `board = pico` (or `pico_w`), `framework = arduino`, on-device build/upload (UF2) + filesystem-image upload.
  - `[env:native]` — desktop compile of `lib/core/` + storage stub + GoogleTest; no Arduino includes.

### 4.2 `platformio.ini` sketch

```ini
[platformio]
default_envs = pico

[env:pico]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = pico
framework = arduino
board_build.core = earlephilhower
board_build.filesystem_size = 0.5m         ; LittleFS partition for config/presets
build_flags =
  -std=gnu++17
  -DUSE_TINYUSB
  -DPIO_USB_DP_PIN_DEFAULT=0                ; PIO-USB D+ base pin (D- = +1); finalize in M1/M2
  ; DIN MIDI out: Serial2 (UART1) TX on GP8 — set in firmware pin map, no collision with PIO-USB GP0/GP1
lib_deps =
  sekigon-gonnoc/Pico PIO USB
  bblanchon/ArduinoJson
  duinowitchery/hd44780                     ; or marcoschwartz/LiquidCrystal_I2C
monitor_speed = 115200

[env:native]
platform = native
build_flags = -std=gnu++17 -DKEYBCHORD_NATIVE
lib_deps =
  google/googletest
lib_compat_mode = off                       ; allow core-only libs on native
test_framework = googletest
```

> Versions to be pinned exactly during M1 once the working combination is verified on hardware. `board_build.filesystem_size` reserves LittleFS space; tune during M1 based on preset/rhythm footprint.

### 4.3 Library roles (spec §1.5)

| Purpose | Library | Include guard (isolated to) |
|---------|---------|------------------------------|
| RP2040 Arduino core + dual-core + TinyUSB + LittleFS + UARTs (Serial1/Serial2) | arduino-pico | `src/main.cpp`, `lib/hw/*` only |
| USB host HID keyboard (+ LED output report) | Pico-PIO-USB + TinyUSB host | `lib/hw/input_usbhost.*` |
| DIN/UART MIDI bytes | Arduino `Serial2` (UART1) | `lib/hw/midi_out_uart.*` |
| LCD1602 over PCF8574 I2C | `hd44780` (or `LiquidCrystal_I2C`) + `Wire` | `lib/hw/lcd_hd44780.*` |
| JSON parse/serialize | ArduinoJson | `lib/core/config.*`, `lib/core/presets.*`, `lib/hw/storage_littlefs.*` |
| Filesystem | LittleFS | `lib/hw/storage_littlefs.*` |
| Unit testing (desktop) | GoogleTest | `test/` (native env) only |

### 4.4 Native buildability

- `lib/core/*` MUST compile with a plain host C++17 compiler (no Arduino). This is enforced by §3.2 layering and verified by the `native` PlatformIO test env / a standalone CMake target.
- The storage stub (`lib/hw/storage_stub.*`) implements `StorageAdapter` over `std::filesystem` (or an in-memory map) so `test_config.cpp`/`test_presets.cpp` exercise the real load/validate/generate logic without a device.
- ArduinoJson is header-capable and compiles on the host, so config/preset parsing logic can be tested natively.

### 4.5 Configuration files (data-driven, spec §8 / NFR-7)

- **`config.json`** (spec §8.1): I2C address, DIN/clock enable flags, modifier bindings, chord base root + note range, `chord_root_order`, display timeouts, `led` block, startup preset. Missing values fill from Parameter Reference defaults (spec §9). No `usb_enabled`/`keyboard_device` fields.
- **`presets/bank<N>.json`** (spec §8.2): array of 8 preset objects (chord/strum/rhythm blocks + channels); `rhythm.swing` integer 0–75.
- **Editable tables** (spec §8.3): `chord_types.json`, `combinations.json`, `root_order.json`, `keymap.json`, `param_defaults.json`, `rhythms/*.json`.
- **Validation & first-boot generation** (NFR-9): mount LittleFS (format if needed), validate against schema/Parameter Reference, generate defaults when missing, fall back per-field on corrupt/out-of-range values with a warning; never crash.

---

## 5. Hardware Layout / Configuration

Reproduced and expanded from spec §2 for build reference.

### 5.1 Bill of materials (spec §2.1)

Raspberry Pi Pico (RP2040); USB keyboard; USB-A receptacle/breakout (host port; pin1=VBUS, pin2=D-, pin3=D+, pin4=GND); 2× 22Ω resistors (PIO-USB D+/D- series); LCD1602 + PCF8574 I2C backpack (run at **3.3V**); 5-pin DIN-5 socket; 10Ω + 33Ω resistors (**3.3V** MIDI OUT current loop, per MIDI 1.0 2014 update — no transistor needed); 1× 100 µF bulk capacitor (5V-rail decoupling near the receptacle); 5V power source **≥ 1 A** (native micro-USB → `VBUS`). All resistors **1/4 W (0.25 W)** (dissipation is milliwatts). No OTG adapter, no USB MIDI cable, no hub.

### 5.2 Wiring (spec §2.2) — canonical default pin map

This is the authoritative pin/power map (spec §2.2 references it). GPIO are shown with physical Pico header pin numbers. Only two override points exist: the `PIO_USB_DP_PIN_DEFAULT` build flag (USB host D+) and `pins.h` (DIN TX + I2C); wiring and firmware must always match.

| Interface | Pico pins (GPIO / header) | Connection |
|-----------|---------------------------|------------|
| USB host (keyboard) | D+ `GP0` (pin 1), D- `GP1` (pin 2); set via `PIO_USB_DP_PIN_DEFAULT=0` (D- = D+ +1) | USB-A receptacle D+/D- via **22Ω** series each (only series element; device supplies pull-ups); FS/LS 3.3V signaling. Receptacle: pin1 VBUS(5V), pin2 D-, pin3 D+, pin4 GND. |
| DIN MIDI out (UART, 3.3V loop) | `Serial2` (UART1) TX on `GP8` (pin 11) — avoids GP0/GP1; call `Serial2.setTX(8)` in firmware | Direct 3.3V drive (MIDI 1.0 2014 update): `GP8` → 10Ω → DIN pin 5; `3V3(OUT)` (pin 36) → 33Ω → DIN pin 4; DIN pin 2 → GND. 31250 baud 8N1. No transistor/level-shifter needed. |
| LCD1602 (I2C0) | SDA `GP4` (pin 6), SCL `GP5` (pin 7) | PCF8574 backpack **at 3.3V** (`3V3(OUT)`, pin 36) + GND — keeps I2C 3.3V-safe. Pull-ups from backpack or 4.7 kΩ to 3.3V. Boot auto-probe addr `0x27`→`0x3F`→null adapter. |
| Keyboard LED | (over USB host) | Scroll Lock via HID SET_REPORT; no extra wiring. |
| Power | native micro-USB → `VBUS` (pin 40) + GND | 5V supply **≥ 1 A** for Pico + keyboard. 100 µF bulk cap on 5V rail near receptacle. **Do not also inject `VSYS` without its own series Schottky diode** (§5.3). |
| Debug/log (optional) | native USB (USB-CDC) | serial console for logs (NFR-8). |

> **Pin-collision rule (spec §2.2):** PIO-USB D+/D-, `Serial2`/UART1 TX, and I2C0 SDA/SCL do not overlap in the canonical map (`GP0`/`GP1`, `GP8`, `GP4`/`GP5`). Pins are set only via the `PIO_USB_DP_PIN_DEFAULT` build flag (USB) and `pins.h` (DIN/I2C). Confirm on hardware during M1/M2 bring-up; keep this table and spec §2.2 in sync.

> See [`docs/wiring.md`](docs/wiring.md) for a rendered connection diagram of this pin map.

### 5.3 USB topology (spec §2.3)

- One native USB port on the Pico (used for power and/or USB-CDC debug), plus a **software-defined PIO-USB host port** on GPIO for the keyboard.
- **No host-vs-gadget conflict** (DIN-only MIDI). This is the deliberate simplification vs. the Pi Zero 2 W design.
- **DIN (UART) is the sole, always-available MIDI path.**
- **Power (canonical):** native micro-USB powers the Pico; the keyboard draws 5V from `VBUS`. Use a **≥ 1 A** supply (the supply, cable, and connector carry combined Pico + keyboard current) and a **100 µF bulk cap** on the 5V rail near the receptacle (hot-plug inrush / brownout protection, NFR-5). The keyboard taps `VBUS`, upstream of the Pico's onboard `VBUS`→`VSYS` Schottky (**D1**), so keyboard current bypasses D1. **Back-feed rule:** never inject a regulated 5V into `VSYS` while also powered from native USB unless that external `VSYS` feed has its own series Schottky diode — choose one power path. The LCD runs from `3V3(OUT)` (≈ 300 mA rail shared with the RP2040); keep backlit-LCD load within budget or power its backlight separately.

### 5.4 Provisioning / flashing checklist (captured in `scripts/install.md`)

1. Install PlatformIO; select the `pico` env (arduino-pico core).
2. Wire per §5.2: 22Ω on D+/D-; the 3.3V MIDI OUT loop (10Ω on `GP8` TX, 33Ω on `3V3(OUT)`); LCD backpack at 3.3V; 100 µF bulk cap on the 5V rail near the receptacle. All resistors 1/4 W.
3. Confirm the power rule: power via native micro-USB (→`VBUS`); do **not** also inject `VSYS` without its own series Schottky diode. Use a ≥ 1 A supply.
4. Build + upload firmware (UF2 via BOOTSEL or picotool).
5. Build + upload the **filesystem image** (`data/` → LittleFS) so default config/presets/rhythms are present; the firmware will otherwise generate defaults on first boot (NFR-9).
6. Connect the keyboard to the USB-A host receptacle; verify enumeration via the debug log.
7. Connect DIN to an external synth/DAW (or DIN monitor); verify notes.
8. (Optional) attach USB-CDC serial for the MIDI monitor/log mode (NFR-8, §7.2).

### 5.5 Reference designs / precedent

The power topology (native USB → `VBUS`, keyboard on the same 5V rail) and the 3.3V DIN MIDI OUT circuit follow established, widely-used designs on this exact chip/stack:

- **rppicomidi/midi2usbhost** — RP2040 USB-host → old-school DIN MIDI IN/OUT; drives MIDI OUT from the Pico's UART1 TX at 3.3V and powers the board from `VBUS` (pin 40)/GND. Includes a **Pico-PIO-USB variant (`midi2piousbhost`)** — the closest match to our topology: <https://github.com/rppicomidi/midi2usbhost>
- **Adafruit MIDI FeatherWing** — commercial 3.3V DIN MIDI OUT (+ opto-isolated IN) explicitly for 3.3V-logic microcontrollers: <https://learn.adafruit.com/adafruit-midi-featherwing>
- **PJRC / Teensy MIDI library hardware page** — canonical 3.3V MIDI OUT schematic (2-resistor loop, no transistor) and the `setTX()`/`begin()` ordering note: <https://www.pjrc.com/teensy/td_libs_MIDI.html>
- **MIDI.org — 5-Pin DIN Electrical Specs (CA-033, 2014 update)** — standardizes 3.3V MIDI signaling and optional ferrite-bead RF filtering: <https://midi.org/5-pin-din-electrical-specs>
- **Pico-PIO-USB** — USB-host library; documents the 22Ω D+/D- series resistors, host power, and resource usage: <https://github.com/sekigon-gonnoc/Pico-PIO-USB>
- **Raspberry Pi Pico datasheet, "Powering Pico"** — `VBUS`/`VSYS`/onboard Schottky **D1** topology and external `VSYS`-diode guidance: <https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf>

> Note (per rppicomidi): the RP2040 UART TX is push-pull rather than open-drain, a benign deviation from the letter of the MIDI spec that works with standard opto-isolated inputs. These reference builds "would likely fail formal EMC/RF compliance" (author's disclaimer) but are proven for hobby/prototype use; add ferrite beads on the DIN signal pins to reduce RFI (2014 update, optional).

---

## 6. Feature Buildout — Milestones

Aligned 1:1 with spec §1.6 / §12. Each milestone ends with a **review stop** for the user to test and give feedback before proceeding. Every milestone must keep the full GoogleTest `native` suite green before its stop. "Verify" rows are the spec's exit criteria; "AC" lists the Acceptance Criteria (spec §13) that become testable.

> **On display ordering:** the Display Manager (M6) intentionally follows the input/chord/strum/rhythm engines because it only renders state those engines already produce — state that is already verifiable via native tests and the M2 USB-CDC debug log / MIDI monitor (see §7.0). No earlier milestone depends on the LCD. The display could be pulled earlier purely as a human-facing demo choice, but it is **not** a testing dependency.

### M1 — Scaffolding
**Deliverables:** PlatformIO project + git init; `.gitignore` (excludes `.pio/`); `platformio.ini` with `pico` + `native` envs and pinned lib deps; `src/main.cpp` entry (`setup/loop` + `setup1/loop1` stubs); LittleFS mount + config loader that generates default `config.json` + 10 banks × 8 default presets from `data/` seed on first boot; all adapter interfaces (`lib/hw/base.h`) with null/stub implementations and the factory; GoogleTest skeleton building on `native`.
**Verify:** firmware builds/uploads for `pico`; `native` tests build and run; config loads/creates without hardware.
**AC groundwork:** AC-22 (regeneration/fallback), NFR-9.

### M2 — Core framework
**Deliverables:** USB-host HID keyboard Input Manager (TinyUSB + Pico-PIO-USB); State Manager (pending vs active params scaffolding); MIDI Router (DIN via `Serial2`/UART1, per-function channel routing); Config/Preset store (LittleFS + ArduinoJson); **a minimal USB-CDC debug log + MIDI monitor** (logs raw key events in and every outgoing MIDI message) — this is the on-device feedback instrument for M2–M5, standing in for the LCD, which does not arrive until M6.
**Verify:** raw key events captured on-device **and observed via the USB-CDC debug log**; a hardcoded note plays on DIN **and appears in the MIDI monitor** (null-logs on `native`). No LCD required.
**AC:** AC-9 (DIN output, graceful when unconnected).

### M3 — Chord engine
**Deliverables:** Keymap Resolver (grid + modifier convention, HID usages); root/quality resolution; same-column + left-adjacent + leftmost-`` ` `` combination matrix; root-position & smart voicing (integer); independent add9/add11/add13; all five play modes with correct note lifecycle; Held-mode latching (snapshot vs pending).
**Verify:** chords/combinations correct; play modes behave; latching holds.
**AC:** AC-1, AC-2, AC-3, AC-11, AC-12, AC-13, AC-14, AC-15.

### M4 — Strum plate
**Deliverables:** note-pool derivation from active chord voicing (incl. Silent); full + limited numpad layouts, Num-Lock-independent via raw keypad usages; strum params (octave, note duration, velocity); immediate edit pickup (no latching).
**Verify:** strumming plays active chord's notes in both layouts; edits picked up immediately.
**AC:** AC-4, AC-18.

### M5 — Rhythm engine
**Deliverables:** Core 1 monotonic scheduler/clock; JSON pattern playback on ch10; integer tempo / fixed-point swing; mute (suppress drums, keep sync); MIDI clock 24 PPQN toggle (+ Start/Stop/Continue); arp/rhythm chord sync; Scroll Lock BPM LED indicator (FR-R8) via HID SET_REPORT with accented downbeat.
**Verify:** patterns play; tempo/swing/mute/clock behave; Scroll Lock LED flashes on the beat when rhythm enabled.
**AC:** AC-5, AC-6, AC-16, AC-23.

### M6 — Display manager
**Deliverables:** LCD1602 idle screen with dirty `*`; parameter-edit feedback with revert timeout; save/clear prompts. Line-2 layout per spec draft (tempo, rhythm short-code, play mode), data-sourced for post-hardware tuning.
**Verify:** screen updates correctly for all events.
**AC:** AC-8.

### M7 — Presets & banks
**Deliverables:** JSON load/save on LittleFS; 80-slot **cursor navigation** (Home/End move the cursor across slots, Super+Home/End move banks, Enter loads the cursor, Super+1..8 loads directly); save/clear via `Insert`/`Delete` with confirm + 5 s auto-cancel + play-key cancel; per-function channels (chord 1 / strum 2 / rhythm 10); defaults for uninitialized/cleared slots; dirty-state tracking (`*` marker); cursor auto-reset (5 s) and edit-menu idle auto-exit (10 s).
**Verify:** presets save/recall/clear correctly; navigation + prompts behave.
**AC:** AC-7, AC-19, AC-20, AC-21.

> **Note:** M7 also folded in several feedback-driven refinements: F6 = Clock Out hotkey, F10 = beat-LED hotkey, F12 = Chord-Edit fallback (was F10); arrow-key parameter navigation and key-hold auto-repeat in edit menus; a configurable beat-LED target (default Num Lock) with a consistent short pulse and a hardened (non-sticking) LED state machine; and the clock-as-master timing redesign (see §2.4 / spec §7.3).

### M8 — Polish
**Deliverables:** panic control (`Super+Esc` → CC120/CC123 all channels + clear state); latency/jitter measurement; robustness/hot-plug (keyboard re-enumeration, LCD, LED, DIN); config validation/fallback hardening; **harden/finalize** the logging/debug mode and MIDI monitor/log mode first introduced in M2 (richer message decoding, runtime toggle, compiled-out release option); full test suite; optional watchdog.
**Verify:** acceptance criteria (spec §13) pass; NFRs met.
**AC:** AC-10, AC-17, AC-22 + all NFRs.

> **M8 completed (2026-08-16).** Decisions: (1) hardware watchdog **enabled**
> (`rp2040.wdt_begin(1000)`, fed on both cores); (2) debug-log/MIDI-monitor
> runtime toggle is **config-only** (`logging.debug_log` / `logging.midi_monitor`,
> no hotkey); (3) panic does a **full engine reset** — releases chord/strum/rhythm
> state and cancels transient UI *before* the CC120/CC123 flood. Also delivered:
> a `PerfStats` accumulator + 5 s USB-CDC PERF summaries (Core 0 loop time, Core 1
> step jitter), `std::atomic` cross-core `RhythmClock`/`LedIndicator`, keyboard
> hot-plug note-release, and `KEYBCHORD_LOG` as the compile-out release gate.
> Native suite: 259/259 green.

### M9 (future) — USB Mass Storage & dev serial build
**Deliverables (planned, not implemented):** mount the onboard LittleFS as a **USB Mass Storage** drive on the native USB port so `config.json`/presets/rhythms can be drag-drop edited from a PC (replacing `uploadfs`). A companion **dev build** keeps the native USB port as the USB-CDC serial debug log (the current paradigm) for troubleshooting when MSC is active. The keyboard remains on the PIO-USB host port (GP0/GP1), so it never conflicts.
**Constraint:** MSC and the serial log both want the single native USB port — this is an either/or build-time choice (a `-DKEYBCHORD_MSC` flag + factory/env selection).

### 6.1 Milestone → Acceptance Criteria coverage matrix

| AC | Covered by |
|----|------------|
| AC-1 Chord notes on DIN | M2 (output) + M3 (chords) |
| AC-2 Quality combinations | M3 |
| AC-3 Five play modes / lifecycle | M3 |
| AC-4 Strum order + layouts + params | M4 |
| AC-5 Rhythm drums / tempo / swing / mute | M5 |
| AC-6 Arp/rhythm follow clock | M5 |
| AC-7 80-slot presets + nav + confirm | M7 |
| AC-8 LCD idle + edits + prompts | M6 |
| AC-9 DIN output, graceful when unconnected | M2 |
| AC-10 Key-to-MIDI latency (NFR-1) | M8 |
| AC-11 Modifier keys as chord roots | M3 |
| AC-12 Voicing mode toggle + persist | M3 (+ M7 persist) |
| AC-13 Independent extensions + persist | M3 (+ M7 persist) |
| AC-14 Left-adjacent + leftmost combos | M3 |
| AC-15 Held-mode latching | M3 |
| AC-16 MIDI clock toggle | M5 (clock-as-master in M7) |
| AC-17 Panic | M8 |
| AC-18 Num-Lock independence | M4 |
| AC-19 Prompt auto-cancel | M7 |
| AC-20 Dirty marker | M6 (display) + M7 (logic) |
| AC-21 Uninitialized defaults + channels | M1/M7 |
| AC-22 Corrupt/missing config fallback | M1/M8 |
| AC-23 BPM LED (configurable) | M5 (refined M7) |

---

## 7. Comprehensive Test Plan

Three layers per spec §11: off-device unit tests (pure logic, GoogleTest on `native`), on-device instrumented verification, and a manual hardware checklist mapped to Acceptance Criteria.

### 7.0 Verification instruments & the feedback dependency

A common misconception is that testing keyboard input requires the LCD for feedback, implying the screen must be built before input/chords. **It does not.** The architecture deliberately decouples input verification from the display, so the milestone order (input/chords early, LCD at M6) is correct. Each layer has a defined feedback instrument, in priority order:

1. **Native GoogleTest (`native` env) — primary.** All pure logic is verified here with **no hardware**: keymap resolution (HID usage + modifiers → action), chords, combinations, voicing, extensions, strum pool, rhythm/swing math, latching, params, presets, config. A physical keyboard, LCD, or MIDI link is **not** needed to prove correctness of this logic.
2. **USB-CDC debug log + MIDI monitor — the on-device instrument, available from M2.** Raw HID key events (in) and every outgoing MIDI message (out) are logged over USB-CDC. This is what verifies on-device input capture and MIDI output for **M2–M5**, standing in for the LCD. It is a deliverable of M2 (not M8); M8 only hardens it.
3. **LCD — a feature under test at M6, not a test prerequisite.** The screen renders already-derived state that is already verifiable via (1) and (2). It is therefore built and tested in M6; nothing earlier depends on it.

**Dependency rule:** on-device milestones M2–M5 depend on the USB-CDC debug log / MIDI monitor for feedback, **never** on the LCD. This is why the display does not need to precede the keyboard or chord engine.

### 7.1 Off-device unit tests (GoogleTest, spec §11.1) — test-first

Written before/alongside each core module. No Arduino/hardware includes (enforced by §3.2). Runnable in the `native` PlatformIO env / standalone CMake.

| Test file | Cases | Spec ref | AC |
|-----------|-------|----------|-----|
| `test_chords.cpp` | Every type in §6.2 → exact MIDI note sets; all 12 roots at base octave; flat spelling of pitch classes. | §6.2 | AC-1 |
| `test_combinations.cpp` | Same-column (maj-only, min-only, 7th-only, maj+7=maj7, min+7=min7, maj+min=dim, maj+min+7=aug); left-adjacent (maj+left-7th=sus4, maj+left-min=add9); leftmost col via `` ` `` (Tab+`` ` ``=sus4 Db, Caps+`` ` ``=add9 Db). | §6.3 | AC-2, AC-14 |
| `test_voicing.cpp` | Root-position within note range; smart voice-leading picks nearest inversion (integer distance); octave shift ±12 clamped to MIDI 0–127 (VR-3). | §6.4 | AC-12 |
| `test_extensions.cpp` | add9 (+14), add11 (+17), add13 (+21) each independent; all 8 flag combinations; left-adjacent add9 sets the flag. | §6.2 | AC-13 |
| `test_strum.cpp` | Note-pool derivation over strum octave range; key→note ordering for full (`1..0`, keypad `0 . 1..9`) and limited (`0 . 2 3 5 6 8 9 / *`) layouts; Silent-mode pool still derived; keypad +/- excluded. | §6.6, FR-S2 | AC-4, AC-18 |
| `test_rhythm.cpp` | Pattern → scheduled step events; integer BPM→interval math; **fixed-point swing** delay on off-beat steps (0–75); mute suppresses note-ons but keeps timeline; MIDI clock 24 PPQN interval math. | §7.2, §7.3 | AC-5, AC-6, AC-16 |
| `test_latching.cpp` | Held-mode: snapshot on trigger; editing any chord param or loading a preset does NOT mutate active notes; applies on next trigger; other modes apply per timing (FR-C9/VR-5). | FR-C9 | AC-15 |
| `test_keymap.cpp` | HID usage + modifier byte → action mapping; Shift/Caps/Tab resolve as chord roots; keypad usages Num-Lock-independent; Ctrl/Alt/Super combos; `+`/`-` context-sensitivity. | §5 | AC-11, AC-18 |
| `test_naming.cpp` | `<Root><quality>` flat spelling across all qualities/extensions and 12 roots (`Eb`, `Ebm`, `Eb7`, `Ebmaj7`, `Ebm7`, `Ebdim`, `Ebaug`, `Ebsus4`, `Ebadd9`). | §6.5 | AC-8 |
| `test_params.cpp` | Clamp to [Min,Max] in Step for every Parameter Reference row; enum cycling; out-of-range → Default; swing integer 0–75 step 5. | §9 | AC-21 |
| `test_presets.cpp` | 80-slot save/recall of chord/strum/rhythm + channels (via storage stub); dirty flag set/cleared; default channels 1/2/10; uninitialized/cleared slots load factory defaults. | §4.4 | AC-7, AC-20, AC-21 |
| `test_config.cpp` | Schema validation; first-boot generation of config + 10×8 presets (storage stub); corrupt/missing regenerate; out-of-range per-field fallback with warning; never throws. | NFR-9 | AC-22 |

**Additional cross-cutting unit tests:**
- Null-adapter smoke: the App object constructs and processes a scripted event sequence with all null adapters (proves off-device build / NFR-5 path).
- Router fan-out/degradation: given the DIN "unavailable", `send()` is a safe no-op and the app keeps running (AC-9 logic-level).
- Panic: `Super+Esc` action produces CC120+CC123 on all 16 channels and clears active-note state (AC-17 logic-level).

### 7.2 On-device verification (spec §11.2)

- **MIDI monitor/log mode:** a build/runtime flag that logs every outgoing MIDI message (channel, note/CC, velocity, timestamp) over USB-CDC/UART.
- **Latency measurement:** instrumented timing from HID report receipt to MIDI dispatch; report typical/worst-case for NFR-1 (<10 ms).
- **Timing/jitter measurement:** log Core 1 scheduler step timestamps for NFR-2 (<2 ms).
- **Boot-to-ready timing:** measure power-up to playable for NFR-4 (<3 s).

### 7.3 Manual hardware checklist (spec §11.3)

Executed on assembled hardware with an external synth/DAW and optional DIN monitor. 1:1 with AC-1…AC-23. Hardware-only highlights:

- **AC-9 / NFR-5:** unplug/replug keyboard (hot-plug re-enumeration); remove LCD; disconnect DIN — app keeps running; DIN resumes when reconnected.
- **AC-11:** confirm Tab/Caps/Shift×2/`[`/`'` act as chord roots (HID report parsing).
- **AC-17:** stuck-note recovery via panic.
- **AC-18:** numpad strum identical with Num Lock on and off.
- **AC-23:** Scroll Lock LED flashes in time (accented downbeat); `Super+F8` toggles it; removing the keyboard/LED does not affect timing or MIDI.
- **NFR-6:** simultaneous chord + strum key presses (document keyboard rollover limits; try a full-NKRO keyboard vs a boot-protocol one).

### 7.4 Test gates

- **Per-milestone gate:** full GoogleTest `native` suite green before each review stop.
- **On-device feedback instrument (M2–M5):** the USB-CDC debug log / MIDI monitor (delivered in M2, §6) is the instrument for on-device input/MIDI checks at these milestones; the LCD is not required until its own gate at M6.
- **Pre-hardware gate (end of M4/M5 logic):** all §7.1 unit tests pass; null-adapter app build verified.
- **Acceptance gate (M8):** every AC in spec §13 mapped to an automated test (§7.1) or a checklist item (§7.3) with a recorded result.

---

## 8. Risks, Decisions & Next Steps

### 8.1 Risks & mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Keyboard N-key rollover limits (NFR-6) | Chord + strum combos may drop keys on boot-protocol (6KRO) keyboards | This is a keyboard trait, not a chip limit. Prefer report-protocol/full-NKRO keyboards; request report protocol via TinyUSB where possible; document per-keyboard limits; test simultaneous chord+strum on the hardware checklist. **Highest-priority risk to validate early with the actual keyboard.** |
| USB-host-keyboard bring-up (PIO-USB pin/timing) | Keyboard fails to enumerate | Follow Pico-PIO-USB examples (proven, incl. a QMK keyboard-host firmware); canonical D+/D- = GP0/GP1 (set via `PIO_USB_DP_PIN_DEFAULT`), verified on hardware in M1/M2; 22Ω series resistors; verify on the debug log before building higher layers. |
| No FPU on RP2040 | Slow/incorrect float math in real-time paths | Integer/fixed-point everywhere in Core 0/Core 1; swing is integer 0–75 (spec §7.3/§9); floats only in cold config-parse paths. Enforced by code review + `test_rhythm.cpp` fixed-point cases. |
| Pin collisions (PIO-USB / Serial2 / I2C) | Silent malfunction | Single pin map in `src/main.cpp`/`pins.h`; documented in §5.2 (PIO-USB GP0/GP1, DIN `Serial2`/UART1 TX GP8, I2C0 GP4/GP5); validated during bring-up (spec §2.2 rule). |
| LittleFS flash wear from frequent writes | Flash longevity | Only write on explicit save (FR-P6); avoid autosave; keep files small; reserve a modest LittleFS partition. |
| Scheduler jitter > 2 ms (NFR-2) | Audible timing wobble | Dedicated Core 1 monotonic scheduler; non-blocking UART writes; LED HID writes off the critical path; measure + tune in M8. |
| Key-to-MIDI latency > 10 ms (NFR-1) | Sluggish feel | Minimal per-event work on Core 0; precomputed voicings; ~1 ms USB poll; measure + tune in M8. |
| Corrupt/missing config or presets | Crash on boot | NFR-9 validation + first-boot generation + per-field fallback; covered by `test_config.cpp` with a storage stub. |
| Keyboard rejects LED output report | No BPM indicator | Best-effort per FR-R8; degrade gracefully; never affects timing/MIDI. |
| Keyboard hot-plug inrush / brownout | Pico resets or USB stack wedges | 100 µF bulk cap on the 5V rail near the receptacle; ≥ 1 A supply; watchdog recovery (NFR-5); documented in §5.2/§5.3. |
| 3.3V MIDI OUT compatibility | Rare/vintage MIDI inputs may expect a 5V loop | Use the standards-compliant 3.3V loop (10Ω on TX + 33Ω on `3V3(OUT)`) per the MIDI 1.0 2014 update; proven by rppicomidi / Adafruit MIDI FeatherWing / Teensy (see §5.5). If a specific vintage input misbehaves, a 5V buffer stage can be added, but it is not needed for standard opto-isolated inputs. |
| 5V PCF8574 backpack on 3.3V I2C | I2C lines pulled toward 5V; out of spec / possible damage | Run the LCD backpack at 3.3V (canonical); pull-ups to 3.3V; boot auto-probe with null-adapter fallback (spec §2.2). |
| Power back-feed (native USB + VSYS) | Damage / undefined behavior | Single power path; external `VSYS` injection only via its own series Schottky diode (the onboard D1 is `VBUS`→`VSYS` only); back-feed rule stated in §5.3 / spec §2.3. |
| RP2350-E9 not applicable | n/a | Chosen board is RP2040 (Pico), which does not carry the RP2350 E9 GPIO-input latch erratum; recorded here as part of the board-choice rationale. |
| Arduino-pico / Pico-PIO-USB version drift | Build breakage | Pin exact library + platform versions in `platformio.ini` after the first verified build (M1). |

### 8.2 Open items carried from spec §14 (deferred to post-hardware testing)

1. **Final GPIO pin map** (PIO-USB D+/D-, `Serial2`/UART1 TX, I2C SDA/SCL): defaults in §5.2 (PIO-USB GP0/GP1, DIN TX GP8, I2C0 GP4/GP5); finalize during M1/M2 and record here.
2. **LCD line-2 layout (FR-D1):** implement the draft (`q=120  Rk  >Held`) as-is; revisit after hardware testing. (Corresponds to Spec §14 item 2 / FR-D1.)
3. **Rhythm list (§7.1):** ship the 12 named patterns (Rock 1, Rock 2, Waltz, Swing, Slow Rock, Bossa Nova, Rhumba, Tango, March, Samba, Disco, Foxtrot) as JSON; confirm/expand post-hardware. (Corresponds to Spec §14 item 3 / §7.1.)
4. **Persistence detail:** default is JSON on LittleFS via ArduinoJson for all tables; optionally compile large static tables into flash with JSON overrides if RAM/flash pressure warrants (spec §8.3).

Items 2–4 are isolated to data/display files and do not block implementation.

### 8.3 Assumptions

- Arduino-pico core + TinyUSB + Pico-PIO-USB provide working FS/LS HID keyboard hosting on RP2040 (confirmed by the libraries' support tables and example firmware).
- The target keyboard has no `Fn` key; the Windows/Super key serves as `global_fn` (spec §5.3).
- DIN is the sole MIDI output; USB MIDI is out of scope for v1.
- Runtime data lives on LittleFS and is disposable/regenerable.
- A desktop C++17 compiler is available for the `native` GoogleTest env.

### 8.4 Definition of done (per milestone)

1. Deliverables implemented per the milestone list (§6).
2. Associated GoogleTest unit tests written and green on `native` (§7.1).
3. Spec "Verify" criteria demonstrably met (null adapters/stubs off-device where hardware is absent).
4. Mapped Acceptance Criteria identified and, where automatable, covered.
5. Review stop: results presented to the user for testing/feedback before the next milestone (spec §1.6).

### 8.5 Next steps

1. User reviews this addendum against the Pico spec; confirm architecture, layout, and milestone plan.
2. On approval, begin **M1 — Scaffolding** (PlatformIO project, git init, `platformio.ini` with pinned deps, `src/main.cpp` dual-core entry, LittleFS config loader with first-boot defaults, adapter interfaces + null/stub fallbacks, GoogleTest skeleton), then stop for review.
3. **Validate the USB-host keyboard path and N-key rollover early** (during/right after M2) with the actual target keyboard — this is the highest-risk item.
4. Proceed through M3–M8, stopping after each for user testing/feedback.

---

*End of KeybChord (Pico Edition) Development Roadmap & Architecture Addendum.*

