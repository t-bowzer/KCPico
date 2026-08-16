# KeybChord (Pico Edition) — Requirements & Specification

**Project:** KeybChord — Raspberry Pi Pico (RP2040) Edition
**Description:** Turn any USB keyboard into an Omnichord-style MIDI controller using a Raspberry Pi Pico (RP2040), written in C++ on the Arduino-Pico core.
**Document status:** v1 (Pico port) — build-ready. Derived from the Raspberry Pi Zero 2 W spec (`KeybChord_Spec.md` v3). This edition retargets the project to a bare-metal RP2040 microcontroller: the firmware is C++ (not Python), keyboard input is via a USB **host** stack (TinyUSB + Pico-PIO-USB, not Linux `evdev`), MIDI output is **DIN/UART only** (the USB port hosts the keyboard), persistence is **JSON on LittleFS** (not SD/JSON files), and there is **no OS/systemd** (the board runs on power-up). Two minor items remain open (LCD line-2 layout and final rhythm list, Section 14) and are flagged inline with **[DRAFT — REVIEW]**; both are deferred to post-hardware testing and do not block implementation.
**Target audience:** Coding agent implementing the firmware.

---

## 1. Overview & Goals

### 1.1 Summary
KeybChord is a standalone MIDI controller. A standard USB keyboard is plugged into a Raspberry Pi Pico (RP2040), which acts as a **USB host** to enumerate the keyboard, interprets key presses as Omnichord-style musical actions (chords, strum plate, rhythm accompaniment), and emits MIDI over a 5-pin DIN connector (UART). An LCD1602 provides status and parameter feedback. The keyboard's Scroll Lock LED is used as a BPM indicator.

### 1.2 Goals
- Faithfully recreate the Omnichord playing paradigm (chord buttons + strum plate + rhythm) on a commodity USB keyboard.
- Emit standards-compliant MIDI via DIN (UART @ 31250 baud).
- Provide expressive, real-time control with low latency (<10 ms key-to-MIDI).
- Persist configuration via 80 recallable presets (10 banks × 8) on the Pico's onboard flash (LittleFS).

### 1.3 Non-Goals (v1)
- No onboard audio synthesis. KeybChord is a **MIDI controller only**; sound is produced by an external synth/DAW.
- No graphical UI beyond the LCD1602.
- **No USB MIDI device output.** The single USB port is dedicated to hosting the keyboard; MIDI leaves the device over DIN only. (USB MIDI could be revisited later with a second USB interface or a different board; out of scope for v1.)
- No network/Bluetooth MIDI.

### 1.4 Implementation Constraints
- **Language:** C++17.
- **Platform / core:** Arduino-Pico core (earlephilhower) targeting RP2040 (Raspberry Pi Pico / Pico W).
- **Build system:** PlatformIO, with two environments — an on-device `pico` environment and a desktop `native` environment for unit-testing the pure-logic core.
- **USB keyboard input:** USB **host** via TinyUSB + Pico-PIO-USB. The firmware parses HID keyboard reports directly (boot and/or report protocol), tracking modifiers and N-key rollover from the report data. There is no OS input layer; the keyboard is owned exclusively by the firmware.
- **MIDI output:** DIN via hardware UART (`Serial2` = UART1) at 31250 baud, 8N1. DIN is the only MIDI path (see 1.3).
- **Persistence:** JSON on **LittleFS** in the Pico's onboard flash, parsed/serialized with ArduinoJson. Human-readable and user-editable via the flash filesystem.
- **No FPU:** RP2040 (Cortex-M0+) has **no hardware floating-point**. All timing-sensitive and per-event math (chord/voicing/strum/rhythm/clock) MUST be integer or fixed-point. In particular, **swing is stored and computed as a fixed-point integer** (see Sections 7.3 and 9). Floats are permitted only in cold paths (e.g. one-time config parsing), never in the real-time loops.
- **Portable core / hardware adapters:** All pure logic (chord/voicing/strum/rhythm computation, config/preset handling, keymap resolution, state) MUST have zero hardware/Arduino includes and be unit-testable on a desktop (`native` env) via GoogleTest. All hardware I/O (USB-host input, LCD, DIN/UART, keyboard LED, LittleFS) sits behind adapter interfaces with safe null/no-op fallbacks so the logic compiles and runs on the dev host without Pico hardware present.
- **No OS / autostart:** There is no operating system or systemd. The firmware runs from flash on power-up; "boot-to-ready" is simply the setup path (Section 10, NFR-4).
- **Project layout & VCS:** Self-contained PlatformIO project under a dedicated `keybchord/` directory, version-controlled with git. Default JSON templates ship in the repo (`data/`, flashed to LittleFS); generated/runtime files live on LittleFS on the device and are not committed.

### 1.5 Recommended Libraries (pinned defaults)
These are the recommended, commonly-used libraries for this hardware/stack. The agent may substitute with justification, but should keep the stack internally consistent.
| Purpose | Library | Notes |
|---------|---------|-------|
| RP2040 Arduino core | `arduino-pico` (earlephilhower) | Provides Arduino API, dual-core (`setup1/loop1`), TinyUSB, LittleFS, hardware UARTs (`Serial1`/`Serial2`) |
| USB host stack | TinyUSB (bundled with core) + `Pico-PIO-USB` (sekigon-gonnoc) | HID keyboard host on a PIO-driven USB port; FS/LS host + hub supported |
| DIN MIDI (UART) | Arduino `Serial2` (UART1, raw bytes) | 31250 baud 8N1; optional `Arduino MIDI Library` for framing |
| LCD1602 over PCF8574 I2C | `hd44780` (by Bill Perry) or `LiquidCrystal_I2C` | PCF8574 I/O-expander mode over `Wire` |
| JSON parse/serialize | `ArduinoJson` (bblanchon) | Config/preset/pattern read/write on LittleFS |
| Filesystem | `LittleFS` (bundled with core) | Onboard-flash persistence |
| Unit testing (desktop) | `GoogleTest` | `native` PlatformIO env; pure-core tests (Section 11) |

### 1.6 Deliverables & Build Sequence
The agent will produce the entire firmware project. Work in the following ordered steps; after each step, stop and present the result for the user to test and give feedback before proceeding.

1. **Repo structure & scaffolding.** Create the PlatformIO project layout, `platformio.ini` with `pico` + `native` environments and pinned library deps (per 1.5), entry point (`src/main.cpp` with `setup/loop` and `setup1/loop1`), a config loader that generates default `config.json` and default preset banks on LittleFS on first boot (Section 12.x robustness), and hardware adapter interfaces with null fallbacks (input, LCD, MIDI out, keyboard LED, storage) so the pure core builds and runs on the `native` host. Verify: firmware builds for `pico`; native test target builds; app logic initializes and loads/creates config.
2. **Core framework.** Implement Input Manager (USB-host HID keyboard via TinyUSB/Pico-PIO-USB), State Manager, MIDI Router (DIN via `Serial2`/UART1), Config/Preset store (LittleFS + ArduinoJson), and a **minimal USB-CDC debug log + MIDI monitor** (raw key events in, outgoing MIDI messages out) — the on-device feedback instrument for steps 2–5, since the LCD does not arrive until step 6. Verify: raw key events are captured on-device and observed via the USB-CDC debug log; a hardcoded note plays on the DIN output and appears in the MIDI monitor (and null-logs on the native host). No LCD required.
3. **Chord engine.** Root/quality resolution from the key grid, same-column + left-adjacent + leftmost combination matrix, voicing (root-position & smart), extensions, and the five play modes with correct note lifecycle. Verify: chords and combinations produce correct notes; play modes behave per spec.
4. **Strum plate.** Note-pool derivation, full + limited numpad layouts (independent of the keyboard's Num-Lock state), strum params, immediate edit pickup. Verify: strumming plays the active chord's notes correctly in both layouts.
5. **Rhythm engine.** Scheduler/clock (on the second core), JSON pattern playback on ch10, tempo/swing (fixed-point), mute, MIDI clock toggle, arp/rhythm chord sync, and the keyboard-LED BPM indicator (FR-R8). Verify: patterns play; tempo/swing/mute/clock behave; Scroll Lock LED flashes on the beat when rhythm is enabled.
6. **Display manager.** LCD1602 idle screen (with dirty `*` indicator), parameter-edit feedback, and prompts. Verify: screen updates correctly for all events.
7. **Presets & banks.** JSON load/save on LittleFS, 80-slot navigation, save/clear with confirm (auto-cancel), per-function channels, defaults for uninitialized slots. Verify: presets save/recall/clear correctly.
8. **Polish.** Panic control, latency tuning, robustness/hot-plug, config validation/fallback, and **hardening/finalizing** the logging + MIDI monitor (over USB-CDC or UART debug) first introduced in step 2, and the full test suite. Verify: acceptance criteria (Section 13) pass.

---

## 2. Hardware Requirements

### 2.1 Bill of Materials
| Item | Purpose |
|------|---------|
| Raspberry Pi Pico (RP2040) | Main compute unit (Pico W also fine; W's wireless is unused in v1) |
| USB keyboard | Primary input device |
| USB-A receptacle / breakout | Host port for the keyboard, wired to the Pico's PIO-USB D+/D- GPIO pins. Receptacle pinout: pin 1 = VBUS (5V), pin 2 = D-, pin 3 = D+, pin 4 = GND. |
| 2× 22Ω resistors (1/4 W) | **Required** series resistors on the PIO-USB D+/D- lines (per Pico-PIO-USB reference). These are the only series elements on D+/D-; the keyboard supplies its own device-side pull-ups. |
| LCD1602 display + PCF8574 I2C backpack | Status / parameter display. **Operated at 3.3V** so its I2C bus stays 3.3V-safe for the RP2040 (see 2.2). |
| 5-pin DIN socket (DIN-5, 180°) | Classic MIDI output. TRS (Type-A) MIDI is out of scope for v1. |
| 1× 10Ω + 1× 33Ω resistor (1/4 W) | MIDI OUT current-loop resistors for a **3.3V** loop (10Ω in series with the `GP8` TX line, 33Ω from `3V3(OUT)` to DIN pin 4), per the MIDI 1.0 Electrical Specification update (2014), which standardizes 3.3V signaling. See 2.2. |
| 1× 100 µF electrolytic capacitor (bulk) | Bulk decoupling on the 5V rail near the keyboard receptacle; absorbs hot-plug inrush and prevents Pico brownout / USB-stack wedge (see 2.3, NFR-5). |
| 5V power source (≥ 1 A recommended) | Powers the Pico + keyboard. **Canonical topology:** power the Pico via its native micro-USB connector; the keyboard draws 5V from `VBUS`. Size the supply for both (Pico ≤ ~150 mA + keyboard 100–500 mA; more for backlit/RGB keyboards). A regulated 5V into `VSYS` is an alternative, but do **not** source both simultaneously without an external `VSYS` series Schottky diode (see 2.3). |

> **Resistor wattage:** all resistors in this design (2× 22Ω USB series, 10Ω + 33Ω MIDI loop) dissipate only milliwatts in normal operation, so standard **1/4 W (0.25 W)** parts are used throughout with large margin.

> **Note vs. the Pi Zero 2 W BOM:** the micro-USB-to-USB-OTG adapter is **removed** (no longer applicable). A USB MIDI interface/cable is **removed** (no USB MIDI device role — DIN only). USB hub is **not required** and is not part of the design.

### 2.2 Connections

This is the **canonical default pin map**. GPIO are given with their physical Pico header pin numbers. The only supported overrides are the PIO-USB D+ base pin (via the `PIO_USB_DP_PIN_DEFAULT` build flag) and the DIN/I2C pins (via `pins.h`); the wiring below and the firmware defaults must always agree. The authoritative map lives in the roadmap (§5.2).

- **Keyboard (USB host):** USB keyboard → USB-A receptacle → Pico PIO-USB pins. **D+ = `GP0` (header pin 1), D- = `GP1` (header pin 2)**, set by `PIO_USB_DP_PIN_DEFAULT=0` (D- is always D+ +1). Each line carries a **22Ω series resistor** (the only series element; the keyboard provides device-side pull-ups). Signaling is full/low-speed USB at 3.3V logic levels. The keyboard is powered from `VBUS` (5V, header pin 40) + GND through the receptacle (receptacle pin 1 = VBUS, pin 2 = D-, pin 3 = D+, pin 4 = GND). Enumerated and parsed by the firmware's TinyUSB host + Pico-PIO-USB HID stack.
- **LCD1602 (I2C):** SDA→`GP4` (header pin 6), SCL→`GP5` (header pin 7) on I2C0. **VCC→3.3V (`3V3(OUT)`, header pin 36), GND→GND** — the backpack is run at 3.3V so the I2C bus stays within the RP2040's 3.3V range (do not power the backpack from 5V without a level shifter). Pull-ups: use the backpack's onboard pull-ups, or add 4.7 kΩ from SDA/SCL to 3.3V if absent. The firmware auto-probes the PCF8574 address at boot: try `0x27`, then `0x3F`; if neither ACKs, fall back to the null LCD adapter (no crash, per NFR-5). (`GP4`/`GP5` are the fixed default, chosen to avoid the PIO-USB and UART pins.)
- **DIN MIDI out (UART, 3.3V current loop):** DIN uses **`Serial2` (UART1), TX on `GP8` (header pin 11)**, chosen to avoid the PIO-USB `GP0`/`GP1` pins. In firmware, call `Serial2.setTX(8)` before `Serial2.begin(31250)` (UART1's default TX pin is not guaranteed to be `GP8`). MIDI OUT is driven **directly from the 3.3V `GP8` TX pin** — a standards-compliant 3.3V current loop per the MIDI 1.0 Electrical Specification update (2014). Wiring: `GP8` → 10Ω → DIN pin 5; `3V3(OUT)` (header pin 36) → 33Ω → DIN pin 4; GND → DIN pin 2. 31250 baud, 8N1. Notes: the RP2040 UART TX is push-pull, not open-drain — a benign deviation from the letter of the spec that works with standard opto-isolated MIDI inputs (see references, roadmap §5.5); optional ferrite beads on the DIN signal pins reduce RFI (2014 update, optional). No transistor/level-shifter is required at 3.3V.
- **Keyboard LEDs (BPM indicator):** The keyboard's Scroll Lock LED is driven by sending a **HID Output Report (SET_REPORT)** to the keyboard over the USB-host connection (FR-R8). No additional wiring required.

> **Pin-assignment rule:** PIO-USB D+/D- , `Serial2`/UART1 TX (DIN), and I2C0 SDA/SCL must not overlap. Pins are set in exactly two places — the `PIO_USB_DP_PIN_DEFAULT` build flag (USB host) and `pins.h` (DIN TX + I2C) — which are the single source of truth mirrored by the wiring above. The canonical map is `GP0`/`GP1` (USB), `GP8` (DIN TX), `GP4`/`GP5` (I2C0); confirm on hardware during M1/M2 bring-up and keep this section and roadmap §5.2 in sync.

> A rendered connection diagram of this pin map is in [`docs/wiring.md`](docs/wiring.md).

### 2.3 Hardware Notes & USB Topology
- The Pico has **one native USB port**. In this design that port is **not** used for the keyboard: instead, a **second, software-defined USB host port** is created on ordinary GPIO using **Pico-PIO-USB** (PIO + DMA), and the keyboard connects there. The native USB port is left free (may be used for power and/or a USB-CDC debug/log console during development).
- **No host-vs-gadget conflict:** Because MIDI output is DIN-only (Section 1.3) and the keyboard is hosted on the PIO-USB port, there is **no** USB role conflict of the kind that affected the Pi Zero 2 W. This is a deliberate simplification of the original design risk.
- **Guaranteed MIDI path:** DIN (UART) is the sole and always-available MIDI path. All functionality works over DIN.
- **Power (canonical):** Power the Pico via its native micro-USB connector; the keyboard draws 5V from `VBUS`. Use a supply rated **≥ 1 A** (Pico ≤ ~150 mA + keyboard 100–500 mA; size up for backlit/RGB keyboards). The 5V supply, USB cable, and connector must carry the **combined** Pico + keyboard current. (The keyboard load taps `VBUS`, which is *upstream* of the Pico's onboard `VBUS`→`VSYS` Schottky diode **D1**, so the keyboard current does not pass through D1.) Fit a **100 µF bulk capacitor** on the 5V rail near the keyboard receptacle to absorb hot-plug inrush and prevent Pico brownout / USB-stack wedge (supports NFR-5; a watchdog is the secondary safeguard).
  - **Back-feed safety rule:** The Pico already has an onboard Schottky (**D1**) between `VBUS` and `VSYS`. If you power an *alternative* way by injecting a regulated 5V into `VSYS`, do **not** also power via native USB unless that external `VSYS` feed goes through its **own series Schottky diode** (per the Raspberry Pi "Powering Pico" guidance) — otherwise the two sources fight through D1. Pick one power path. The `VSYS`-fed option is for hungry keyboards; the canonical native-USB→`VBUS` path needs no extra diode.
- **LCD 3.3V rail budget:** The LCD is powered from `3V3(OUT)` (header pin 36), which is the RP2040's onboard 3.3V regulator output (≈ 300 mA total, shared with the RP2040 itself). A plain LCD1602 + PCF8574 backpack draws only a few mA, but a **backlit** module can pull 20–40 mA; if using a high-draw/backlit LCD, keep it within the `3V3(OUT)` budget or power its backlight from a separate rail.
- **USB host maturity:** Full-speed/low-speed HID keyboard hosting (incl. hubs) is supported by TinyUSB + Pico-PIO-USB on RP2040. N-key rollover is limited by the *keyboard's* HID report protocol (boot protocol commonly caps at 6 simultaneous keys) — see NFR-6.

---

## 3. System Architecture

### 3.1 Module Overview
The system is a set of cooperating modules coordinated by a central state manager, split across the RP2040's **two cores**.

| Module | Responsibility |
|--------|----------------|
| Input Manager | Drive TinyUSB + Pico-PIO-USB host; parse HID keyboard reports; track pressed keys & modifiers; debounce; emit semantic input events. |
| Keymap Resolver | Translate raw HID usage codes + modifier state into logical actions (chord select, function toggle, strum, navigation). |
| Chord Engine | Maintain active chord state; compute voicings; handle play modes (held/arp/rhythm/silent/press-to-play). |
| Strum Engine | Map strum keys to notes of the current chord; apply strum params. |
| Rhythm Engine | Sequencer/clock; emit GM drum notes on ch10; provide timing to chord/strum arp sync; tempo & swing (fixed-point). |
| MIDI Router | Emit MIDI messages to the DIN/UART output; per-function channel routing. |
| Display Manager | Render idle screen and transient parameter-edit screens on LCD1602. |
| LED Indicator | Drive the keyboard's Scroll Lock LED via HID Output Report; flash the BPM indicator in sync with the Rhythm Engine clock (FR-R8). |
| Preset/Config Store | Load/save global config and 80 presets as JSON on LittleFS. |
| State Manager | Owns global runtime state; mediates between modules; central source of truth (incl. `pending_params` vs `active_params`). |

### 3.2 Cores, Event Loop & Timing
- **Core 0 — real-time input path:** USB-host polling → Input Manager → Keymap Resolver → engines mutate state → MIDI Router dispatch over UART. Target key-to-MIDI latency <10 ms (NFR-1). UART writes are effectively non-blocking at MIDI baud for the small messages involved.
- **Core 1 — rhythm/clock scheduler:** a monotonic-time scheduler (`micros()` / `time_us_64()` / hardware timer) drives tempo/swing, rhythm steps, MIDI-clock ticks (24 PPQN), arp/rhythm chord sync, and the Scroll Lock LED beat callback. Target step jitter <2 ms (NFR-2). LED HID writes happen here, off the Core 0 critical path.
- **Shared state:** the State Manager mediates cross-core access; the scheduler reads a consistent snapshot of chord/arp params. Use lightweight synchronization appropriate to RP2040 dual-core (e.g. the SDK's spin-lock/`mutex`/inter-core FIFO or double-buffered snapshots). Keep shared mutable state minimal.
- **No FPU:** all real-time math (Section 1.4) is integer/fixed-point; swing in particular is fixed-point (Sections 7.3, 9).

### 3.3 Data Flow
`USB host (TinyUSB/PIO-USB)` → Input Manager → Keymap Resolver → (Chord | Strum | Rhythm | Nav | Param) action → State Manager → engines produce MIDI events → MIDI Router → DIN/UART. Display Manager and LED Indicator observe State Manager / scheduler for updates.

### 3.4 MIDI Output Detail
- **DIN/UART:** Open `Serial2` (UART1) at 31250 baud, 8N1; write raw MIDI bytes. This is the only MIDI output.
- Per-function channel/route is applied per message (chord/strum/rhythm channels, Section 4.5).
- The output must degrade gracefully if nothing is connected on the far end (the firmware keeps transmitting; an unconnected DIN is not an error).
- **No USB MIDI:** there is no USB MIDI device endpoint in v1 (Section 1.3). Any reference to a "USB output" from the original spec does not apply here.

---

## 4. Functional Requirements

### 4.1 Chord Function
- **FR-C1** Letter keys act as chord buttons per the Key Map (Section 5). Each letter selects a chord root/type.
- **FR-C2** Chord button combinations follow Omnichord logic (Section 6.3), e.g. major+7th = maj7, minor+7th = min7, major+minor = diminished.
- **FR-C3** Supported play modes (all v1). Note lifecycle is specified in FR-C10:
  - *Held:* chord sounds on trigger and sustains until the next chord is pressed or panic/cancel is used (see FR-C10).
  - *Press-to-play:* chord triggers once on press (note-on), releases on key-up.
  - *Arpeggio:* chord notes played sequentially; when rhythm is enabled, arpeggiation follows rhythm timing.
  - *Rhythm mode:* chord notes triggered in a rhythmic pattern synced to the Rhythm Engine.
  - *Silent (strum-only):* no chord notes sounded directly; chord defines the note pool for the strum plate only.
- **FR-C4** Adjustable chord parameters: note duration, velocity, pan (CC10), via the Chord Edit menu (5.4). Values shown on LCD when edited.
- **FR-C5** Chord octave shift via `+` / `-` keys (Section 5), clamped to a sensible MIDI range.
- **FR-C6** Voicing mode is a live-toggleable parameter (`root_position` ⇄ `smart` voice-leading), saved per-preset (see Section 6.4).
- **FR-C7** Chord extensions (add9, add11, add13) are **independently toggleable** on/off flags. Left arrow = add9, Down arrow = add11, Right arrow = add13 (see 5.4). Saved per-preset.
- **FR-C8** Left-adjacent chord combinations (newer Omnichord behavior): Major + left-adjacent 7th = **sus4**; Major + left-adjacent Minor = **add9**. Leftmost column uses the `` ` `` key as modifier source (see 6.3).
- **FR-C9** **Parameter latching in Held mode:** while chord mode is `held`, changing any chord parameter (note duration, velocity, pan, octave, voicing mode, extensions, channel, chord type/combination) — **including loading a preset** — must NOT alter notes that are currently sounding. The change takes effect only on the **next** chord triggered. The currently-held chord retains the parameter values it was triggered with until it is released and re-triggered. (In other play modes, parameter changes may apply on the next note event per that mode's timing.)
- **FR-C10** **Note lifecycle per play mode:**
  - *Held:* chord sounds on trigger and sustains **indefinitely** until the next chord is pressed or the panic/cancel control is used. Pressing a new chord ends the previous one.
  - *Press-to-play:* note-on on key press; note-off on **key-up**.
  - *Arpeggio:* each arpeggiated note lasts `note_duration_ms` (chord param); sequence timing follows the rhythm clock when rhythm is enabled.
  - *Rhythm mode:* notes triggered per the active rhythm-step timing.
  - *Silent:* no chord notes are sounded (strum pool only).
- **FR-C11** **Panic / all-notes-off:** `Super+Esc` immediately sends All-Sound-Off (CC120) and All-Notes-Off (CC123) on all channels to the DIN output, and clears internal active-note state. Used to recover from stuck notes.

### 4.2 Strum Plate
- **FR-S1** Number keys simulate the strum plate, playing the notes of the currently selected chord spread across octaves.
- **FR-S2** Two layouts:
  - *Full:* number row (top-of-keyboard number keys `1 2 3 4 5 6 7 8 9 0`) and/or full numpad. The numpad strum set is the digit/decimal keys only — `0 . 1 2 3 4 5 6 7 8 9`. The numpad `Keypad +` / `Keypad -` keys are **NOT** part of the strum plate; they adjust the strum octave (see 5.5).
  - *Limited strum keys* (toggle): numpad center path `0 → . → 2 → 3 → 5 → 6 → 8 → 9 → / → *` for a more realistic feel.
- **FR-S3** Strum parameters (via the Strum Edit menu, 5.5): octave, note duration, note velocity, layout. Shown on LCD when edited. (Note length governs how long strummed notes sound; there is no separate sustain parameter.)
- **FR-S4** Strum notes are derived from the active chord voicing (including in Silent mode).
- **FR-S5** Unlike Held-mode chords (FR-C9), the strum plate picks up parameter and chord edits **immediately** — the next strummed note reflects current parameters and the currently-selected chord, with no latching.
- **FR-S6** Numpad strum keys are identified via their raw HID **keypad usage codes** and behave identically **regardless of the keyboard's Num Lock state** (the firmware interprets keypad usages directly; Num Lock is ignored). The firmware never asserts Num Lock as a precondition.

### 4.3 Rhythm Function
- **FR-R1** Selectable rhythm modes providing backing percussion, including original Omnichord rhythms plus additional patterns (Section 7).
- **FR-R2** Percussion output can be muted (rhythm timing continues for sync; drum notes suppressed).
- **FR-R3** Rhythm emits GM percussion notes on **MIDI channel 10**.
- **FR-R4** When rhythm is enabled, Chord Engine arpeggio/rhythm modes follow rhythm timing.
- **FR-R5** Tempo adjust via `Page Up` / `Page Down`; swing (and other rhythm parameters) adjust in the Rhythm Edit menu (`Ctrl`, see 5.6).
- **FR-R6** Rhythm patterns are defined as step sequences in JSON on LittleFS (Section 7.2).
- **FR-R7** MIDI clock transmission (24 PPQN) can be toggled on/off in the Rhythm Edit menu (Clock Out, see 5.6); when on, clock is sent to the DIN output.
- **FR-R8** **Keyboard-LED BPM indicator:** the keyboard's **Scroll Lock** LED can flash in time with the Rhythm Engine clock as a visual tempo indicator. Default behavior: a short quarter-note flash on every beat, with a longer accented flash on beat 1 of the bar. The LED is driven by sending a **HID Output Report (SET_REPORT)** to the hosted keyboard. It blinks only while the Rhythm Engine is enabled (follows the active clock), and is disabled if `led.bpm_indicator` is off. Scroll Lock is used because it is not mapped to any chord/strum/function key; Caps Lock and Num Lock LEDs are left untouched to avoid confusion (Caps is a chord root; numpad state is irrelevant per FR-S6). This is best-effort visual feedback and must degrade gracefully if the LED is unavailable (missing keyboard, keyboard that rejects the output report, USB write failure) without affecting timing or MIDI output.

### 4.4 Presets
- **FR-P1** Presets store chord, rhythm, and strum parameters plus MIDI channel assignments.
- **FR-P2** 80 slots: 10 banks × 8 presets.
- **FR-P3** **Cursor navigation:** `Home` / `End` move a preset **cursor** (wraps across all 80 slots). The cursor auto-resets to the active slot after `display.cursor_timeout_ms` of idleness (default 5 s). `Enter` loads the slot under the cursor.
- **FR-P4** Move the cursor to the previous/next bank: `Super + Home/End` (wraps; the slot position is preserved).
- **FR-P5** Load preset in current bank: `Super + <number 1–8>` (loads directly), or move the cursor and press `Enter`.
- **FR-P6** Save preset: `Insert` (prompt: `Enter` to confirm, `Backspace` to cancel).
- **FR-P7** Clear preset: `Delete` (same confirm/cancel prompt; resets the live parameters and stores factory defaults).
- **FR-P8** All prompts and current bank/slot reflected on the LCD.
- **FR-P9** **Default initialization:** every preset slot that has never been saved (or has been cleared) loads factory-default parameters (see Parameter Reference, Section 9). There are no "empty" slots that fail to load.
- **FR-P10** **Prompt auto-cancel:** save/clear confirmation prompts auto-cancel after **5 seconds** of no key press. Additionally, pressing any chord or strum key immediately cancels the prompt (so the user can keep playing).
- **FR-P11** **Dirty-state indicator:** when the active runtime parameters differ from the stored values of the current preset, the LCD shows a dirty marker (`*`) next to the preset location. Saving clears the marker; loading a preset resets it.
- **FR-P12** **Default channels:** on default initialization, MIDI channels differ per function — chord = 1, strum = 2, rhythm = 10. The user may edit these; overlaps are permitted and are the user's responsibility.

### 4.5 MIDI Output
- **FR-M1** MIDI output via DIN (UART). This is the sole output in v1.
- **FR-M2** Independent MIDI channels selectable per function (chord / rhythm / strum), saved as part of the preset.
- **FR-M3** Rhythm defaults to channel 10 (GM percussion) but channel is configurable per FR-M2.
- **FR-M4** The system continues without error if the DIN cable is disconnected or nothing is listening (it keeps transmitting).

### 4.6 Info Display (LCD1602)
- **FR-D1** Idle screen shows the current chord name and preset location, with a dirty marker (`*`) when the active state differs from the stored preset, e.g.:
  ```
  Ebmaj7    *B1:P3
  q=120  Rk  >Held
  ```
  (Line 1: active chord + optional `*` dirty marker + bank/preset. Line 2: tempo, rhythm short-code, play mode — final layout **[DRAFT — REVIEW]**.)
- **FR-D2** Parameters are edited through three **edit menus** (see 5.4–5.6). Inside a menu the LCD shows the menu title on line 1 and the selected parameter name on line 2; `F1`..`F9` select a parameter and `+` / `-` (or `Page Up`/`Page Down`) change its value, showing the full parameter name and value as a transient screen that reverts to the menu after `display.revert_timeout_ms`. Single-key edits from the main menu show the same transient and revert to idle.
- **FR-D3** Save/clear/confirm prompts are shown on the LCD, including the auto-cancel behavior (FR-P10).

---

## 5. Key Map

The chord grid faithfully reproduces the Omnichord's 3×12 button layout (5.1). Because `Shift` is used as a chord root, function access uses dedicated keys and menus rather than modifier combos (5.3).

### 5.1 Chord Buttons — Authentic Omnichord 3×12 Grid
The Omnichord exposes 36 chord buttons: three quality rows (Major / Minor / 7th) × twelve roots ordered around the **circle of fifths**. KeybChord reproduces this exactly by treating each of the three main keyboard rows as a quality row and using the row-bounding modifier keys as the outermost columns.

**Raw-HID requirement:** The firmware parses HID keyboard reports directly and owns the keyboard exclusively (there is no OS). This lets us treat `Caps Lock`, `Tab`, and both `Shift` keys as ordinary held keys (present in the report = held; absent = released), independent of any lock-LED semantics. Modifier bytes and key-array usages are read from the report. Consequently, **Shift is a chord root and is NOT used as a function modifier** anywhere in the key map — function access uses the dedicated menu keys and `Super` (see 5.3+).

**Row = quality, Column = root (circle of fifths, left→right):**
| Col | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|-----|---|---|---|---|---|---|---|---|----|----|----|----|
| **Root** | Db | Ab | Eb | Bb | F | C | G | D | A | E | B | F# |
| **Major** (top row) | `Tab` | `Q` | `W` | `E` | `R` | `T` | `Y` | `U` | `I` | `O` | `P` | `[` |
| **Minor** (home row) | `Caps` | `A` | `S` | `D` | `F` | `G` | `H` | `J` | `K` | `L` | `;` | `'` |
| **7th** (bottom row) | `LShift` | `Z` | `X` | `C` | `V` | `B` | `N` | `M` | `,` | `.` | `/` | `RShift` |

> The circle-of-fifths root order (Db Ab Eb Bb F C G D A E B F#) mirrors the authentic Omnichord OM-series button layout. This order is stored in config (Section 8.3) so it can be adjusted. Column 1 uses `Tab`/`Caps`/`LShift`; column 12 uses `[`/`'`/`RShift`.

**Same-column combinations** (same root — see matrix 6.3): Major `W` + 7th `X` = Eb7; Major `W` + Minor `S` + 7th `X` = Eb augmented.

**Left-adjacent combinations** (newer Omnichord models): pressing a Major key together with the **7th key one column to its left** yields **sus4**; pressing a Major key together with the **Minor key one column to its left** yields **add9**. See 6.3 for the full rule and the leftmost-column special case.

### 5.2 Strum Plate (number keys / numpad)
The three main letter rows plus their bounding modifiers are fully consumed by the chord grid (5.1). The strum plate therefore uses the **number row** and/or the **numpad**. Numpad keys are identified by their raw keypad HID usages and are **Num-Lock-independent** (FR-S6).
| Keys | Function |
|------|----------|
| Number row `1 2 3 4 5 6 7 8 9 0` | Strum across chord notes (Full layout) |
| Numpad digits + decimal `0 . 1 2 3 4 5 6 7 8 9` | Strum (Full layout). `Keypad +`/`Keypad -` are excluded — they adjust the strum octave (5.5). |
| Limited strum path `0 . 2 3 5 6 8 9 / *` | Strum (Limited layout, when enabled) |

### 5.3 Function Modifier Convention
Because `Shift` (both keys) is a chord root, **Shift is never used as a function modifier**. Parameter editing uses **no modifier-key combos**: instead, three dedicated keys open a parameter-edit menu (see 5.4), where F-keys select a parameter and `+` / `-` change its value.

- **`Ctrl`** — opens the **Rhythm Edit** menu (toggle).
- **`Alt`** — opens the **Strum Edit** menu (toggle).
- **`Menu`** (the Application key, to the left of Right-Ctrl) — opens the **Chord Edit** menu (toggle). `F12` is a fallback that enters Chord Edit from the main menu on keyboards without a `Menu` key.
- **`Super`** (`Windows` key) — global/system modifier, used for preset navigation/save/clear (5.7) and panic (5.8). The target keyboard has no `Fn` key, so the `Windows`/`Super` key is used. (Stored in config as `global_fn: "super"`.)
- **`Esc`** — returns to the main menu from any edit menu (and cancels prompts).

Inside an edit menu, **chord and strum keys remain live** so a parameter change can be heard immediately without leaving the menu.

### 5.4 Function Keys — Chord Controls & the Chord Edit Menu

Main-menu single-key shortcuts (no menu):

| Key | Action |
|-----|--------|
| `F1` | Cycle play mode (Held → Press-to-play → Arpeggio → Rhythm → Silent) |
| `F5` | Toggle voicing mode (Root-position ⇄ Smart voice-leading) |
| `Left` arrow | Toggle **add9** on/off (independent) |
| `Down` arrow | Toggle **add11** on/off (independent) |
| `Right` arrow | Toggle **add13** on/off (independent) |
| `+` / `-` (number row `=` / `-`) | Chord octave up / down |

**Chord Edit menu** — opened by `Menu` (or `F12` from the main menu). F-keys select a parameter; `+` / `-` change its value:

| F-key | Parameter | Range / cycle |
|-------|-----------|----------------|
| `F1` | Octave | −3..+3 (step 1) |
| `F2` | Mode | Held / Press / Arp / Rhythm / Silent |
| `F3` | Voicing | Root / Smart |
| `F4` | Duration | 50..4000 ms (step 50) |
| `F5` | Velocity | 1..127 |
| `F6` | Pan | 0..127 |
| `F7` | Add9 | On / Off |
| `F8` | Add11 | On / Off |
| `F9` | Add13 | On / Off |

### 5.5 Function Keys — Strum Controls & the Strum Edit Menu

Main-menu single-key shortcut:

| Key | Action |
|-----|--------|
| `Keypad +` / `Keypad -` | Strum octave up / down (the numpad math keys are free because they are excluded from the strum plate, FR-S2) |

**Strum Edit menu** — opened by `Alt`. F-keys select a parameter; `+` / `-` change its value:

| F-key | Parameter | Range / cycle |
|-------|-----------|----------------|
| `F1` | Octave | −3..+3 (step 1) |
| `F2` | Duration | 50..4000 ms (step 50) |
| `F3` | Velocity | 1..127 |
| `F4` | Layout | Full / Limited |

### 5.6 Rhythm Controls & the Rhythm Edit Menu

Main-menu single-key shortcuts (no menu):

| Key | Action |
|-----|--------|
| `F6` | Toggle MIDI clock transmit (Clock Out, FR-R7) |
| `F7` | Enable/disable rhythm |
| `F8` | Select rhythm pattern (cycle) |
| `F9` | Mute/unmute percussion |
| `F10` | Toggle the beat LED on/off (FR-R8) |
| `Page Up` / `Page Down` | Tempo up / down |

**Rhythm Edit menu** — opened by `Ctrl`. F-keys select a parameter; `+` / `-` change its value:

| F-key | Parameter | Range / cycle |
|-------|-----------|----------------|
| `F1` | Tempo | 40..260 BPM (step 1) |
| `F2` | Swing | −75..+75 (step 5) |
| `F3` | Pattern | cycle the 12 rhythms |
| `F4` | Mute | On / Off |
| `F5` | Enable | On / Off |
| `F6` | Clock Out | On / Off (MIDI clock transmit, FR-R7) |
| `F7` | Beat LED | On / Off (BPM indicator, FR-R8) |

**Inside any edit menu,** `Left`/`Right` arrows step between parameters (wrapping)
and `Up`/`Down` arrows change the selected value, in addition to `+`/`-`/`Page
Up`/`Page Down`. Large-range parameters (tempo, velocity, pan, durations, swing)
auto-repeat while the step key is held (≈500 ms delay, then ≈80 ms interval);
small-range parameters (octave, enums, toggles) step once per press. A menu
auto-exits to the idle screen after `display.menu_timeout_ms` of idleness
(default 10 s).

### 5.7 Preset & Navigation
| Key | Action |
|-----|--------|
| `Home` / `End` | Move the preset **cursor** previous/next (wraps across all 80 slots) |
| `Super + Home/End` | Move the cursor to the previous/next bank (wraps; slot preserved) |
| `Super + 1..8` | Load preset N in current bank (direct load) |
| `Enter` | Load the slot under the cursor (while browsing) |
| `Esc` | Exit cursor navigation (reset the cursor to the active slot) |
| `Insert` | Save preset (confirm `Enter` / cancel `Backspace`) |
| `Delete` | Clear preset (confirm `Enter` / cancel `Backspace`) |

While browsing (cursor active), the LCD shows the cursor location with a `>`
marker instead of the dirty `*`. Any hotkey (e.g. `F1`, `F7`, arrows) cancels
cursor navigation and is handled normally; chord/strum keys stay live and keep
the cursor open. The cursor auto-resets after 5 s of idleness.

### 5.8 Global / MIDI
| Key | Action |
|-----|--------|
| `F10` | Toggle the beat LED on/off (main-menu single-key, FR-R8) |
| `F12` | Enter the Chord Edit menu (fallback for keyboards without a `Menu` key) |
| `F11` | Reserved |
| `Super + Esc` | **Panic:** All-Sound-Off + All-Notes-Off on all channels/DIN output (FR-C11) |
| `Esc` | Exit the current edit menu / cancel the current prompt |

> **Note on modifiers:** all modifiers are read from the HID report modifier byte (`LCtrl/LShift/LAlt/LGui/RCtrl/RShift/RAlt/RGui`). `Shift` is a chord root. `Ctrl` and `Alt` are menu toggles (not held modifiers); `Super` is the global modifier for presets/panic; the `Menu` key is a normal key-array usage.

---

## 6. Chord Theory & Voicing Rules

Voicings are **rule-based**: the engine computes MIDI notes from a root + chord type. No hardcoded per-chord note tables required. **All computation is integer** (MIDI note numbers and semitone offsets) — no floating point is used anywhere in the chord/voicing/strum path (RP2040 has no FPU, Section 1.4).

### 6.1 Roots
Roots are MIDI note numbers. Base octave is configurable (default C4 = MIDI 60 as chord root reference). Root pitch class is determined by the selected chord button (Section 5.1); the octave is set by the chord octave parameter (FR-C5).

### 6.2 Chord Type Interval Formulas
Intervals are semitone offsets from the root (0).
| Type | Intervals (semitones) |
|------|-----------------------|
| Major (maj)        | 0, 4, 7 |
| Minor (min)        | 0, 3, 7 |
| Dominant 7 (7)     | 0, 4, 7, 10 |
| Major 7 (maj7)     | 0, 4, 7, 11 |
| Minor 7 (min7)     | 0, 3, 7, 10 |
| Diminished (dim)   | 0, 3, 6 |
| Diminished 7 (dim7)| 0, 3, 6, 9 |
| Augmented (aug)    | 0, 4, 8 |
| Suspended 4 (sus4) | 0, 5, 7 |
| Suspended 2 (sus2) | 0, 2, 7 |
| Minor 7 flat 5 (m7b5) | 0, 3, 6, 10 |

Chord **extensions** (FR-C7) are **independently toggleable** upper tensions added on top of the base chord: add9 (+14), add11 (+17), add13 (+21). Each is an independent on/off flag (any combination allowed), toggled via arrow keys (5.4). Note: the left-adjacent `add9` combination (6.3) sets the add9 flag for that chord.

### 6.3 Omnichord Combination Matrix
Two categories of combination exist: **same-column** (same root, combine qualities) and **left-adjacent** (a Major key plus a key in the column immediately to its left).

**Same-column (same root):**
| Held combination | Resulting chord |
|------------------|-----------------|
| Major only | Major triad |
| Minor only | Minor triad |
| 7th only | Dominant 7 |
| Major + 7th | Major 7 (maj7) |
| Minor + 7th | Minor 7 (min7) |
| Major + Minor | Diminished |
| Major + Minor + 7th | Augmented |

**Left-adjacent (newer Omnichord models):** the root/quality is taken from the **Major key pressed**; the left-adjacent key selects the modifier.
| Held combination | Resulting chord |
|------------------|-----------------|
| Major(col N) + 7th(col N−1) | **sus4** (on Major's root) |
| Major(col N) + Minor(col N−1) | **add9** (on Major's root) |

**Leftmost-column special case (col 1, no column to the left):** use the backtick key `` ` `` as the modifier source:
| Held combination | Resulting chord |
|------------------|-----------------|
| Major(col 1 = `Tab`) + `` ` `` | **sus4** (on col-1 root, Db) |
| Minor(col 1 = `Caps`) + `` ` `` | **add9** (on col-1 root, Db) |

> All combination rules — same-column, left-adjacent, and the leftmost-column `` ` `` mapping — are data-driven (lookup tables in config, Section 8.3) so they can be tuned without code changes. Left-adjacency is computed from `chord_root_order` column indices.

### 6.4 Voicing & Inversion Rules
Voicing behavior is controlled by a **chord voicing-mode parameter** (`root_position` | `smart`), toggled live via `F5` (5.4) and saved per-preset.
- **VR-1 Root position mode:** notes placed in root position within a target octave window; keep all notes within a configurable range (default C3–C6 for chord notes).
- **VR-2 Smart voice-leading mode:** when moving between chords, choose the inversion nearest the previous voicing to minimize note movement (integer distance comparison). Both modes are first-class and user-selectable (no single default is "correct").
- **VR-3** Octave shift (FR-C5) transposes the whole voicing by ±12 semitones per step, clamped to MIDI 0–127.
- **VR-4** Pan is applied as CC10 on the chord channel; velocity from the chord velocity param.
- **VR-5 Parameter snapshot on trigger (Held mode):** When a chord is triggered, the engine takes a snapshot of all sounding-relevant chord parameters (voicing mode, extensions, octave, note duration, velocity, pan, channel, and the resolved chord type). The active voice uses this snapshot for its entire lifetime. Subsequent parameter edits (including loading a preset) update only the "pending" parameter state and are applied to the next chord trigger, never mutating an already-sounding held chord (see FR-C9). Implementation: maintain separate `pending_params` (edited live) and `active_params` (snapshot per active chord) structures on the State Manager.

### 6.5 Chord Naming (for LCD)
Display uses `<Root><quality>` shorthand and **Omnichord-style flat spelling** matching the chord-button root table (5.1): roots are spelled `Db Ab Eb Bb F C G D A E B F#`. Examples: `Eb`, `Ebm`, `Eb7`, `Ebmaj7`, `Ebm7`, `Ebdim`, `Ebaug`, `Ebsus4`, `Ebadd9`. (Flats-by-default; this matches the button layout and is the required display convention.)

### 6.6 Strum Note Pool Derivation
- The strum plate plays the active chord's notes spread over multiple octaves.
- Given chord pitch classes, build an ascending note pool spanning the strum octave range (strum octave param), then map strum keys in order to successive notes of that pool.
- In **Silent** chord mode, the chord is not sounded but still defines this pool.

---

## 7. Rhythm Pattern Specification

### 7.1 Rhythm List
Original Omnichord-style rhythms to include (reconstructed), plus room for custom additions:
Rock 1, Rock 2, Waltz, Swing, Slow Rock, Bossa Nova, Rhumba, Tango, March, Samba, Disco, Foxtrot, plus user-defined patterns.
> Final list **[DRAFT — REVIEW]**.

### 7.2 Pattern Data Format (JSON on LittleFS)
Each rhythm is a step sequence stored as a JSON file on LittleFS (one file per pattern, under `/rhythms/`). Steps map to GM percussion notes on channel 10.
```json
{
  "name": "Rock 1",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] }
  ]
}
```
- `pattern` values may be `0` (rest) or `1..127` (velocity; `1` = default velocity).
- GM percussion note numbers: kick 36, snare 38, closed hat 42, open hat 46, etc.
- **`swing` is an integer** here (see 7.3): per-pattern swing is stored as a signed integer percentage −75..+75, not a float. (A pattern authored with `"swing": 0` means none; `"swing": 50` means a 50% off-beat delay.) This differs from the original Pi spec, which used a 0.0–0.75 float.

### 7.3 Timing, Tempo & Swing (integer / fixed-point)
All timing math runs on Core 1 using integer microseconds; the RP2040 has no FPU (Section 1.4), so **no floats are used in the scheduler**.
- **Tempo:** BPM, adjustable via Page Up/Down (FR-R5). Range e.g. 40–260 BPM. Step interval is computed with integer division, e.g. `step_us = 60000000 / (bpm * steps_per_beat)`.
- **Swing:** stored as a **signed integer percentage −75..+75** (0 = straight; positive = laid-back, negative = rushed), adjustable in the Rhythm Edit menu (`Ctrl` → `F2`) in steps of 5. The off-beat delay is applied as fixed-point integer math, e.g. `delay_us = (base_step_us * swing) / 100`, applied to the off-beat 8th-note step only. (Semantically equivalent to the original 0.0–0.75 float, expressed as a signed −0.75..+0.75.)
- **Sync:** The Rhythm Engine clock drives chord Arpeggio/Rhythm modes (FR-R4).
- **MIDI clock:** A single **master clock** (24 PPQN) runs continuously in the background from boot — its tick counter and phase never reset, regardless of whether the rhythm or Clock Out is enabled. Clock Out (`F6`, or `Ctrl` → `F6`) only gates whether the `0xF8` byte is emitted. The beat LED and the rhythm scheduler both **slave to this master clock**: the LED flashes on every 24th tick, and the rhythm downbeat aligns to a 24-tick beat boundary with each 16th-note step = exactly 6 ticks. This keeps LED, drums, and clock mutually phase-locked (nothing preempts or resets the clock). Only the `0xF8` byte is streamed; no Start/Stop/Continue.
- **Mute (FR-R2):** suppresses drum note-ons while the clock/sync continues.
- **LED BPM indicator (FR-R8):** the beat LED flashes with a **consistent short pulse** (`led.flash_ms`, default 40 ms) on every beat of the master clock, independent of rhythm/clock-out state. The target LED is configurable (`led.led` = `num_lock` default, `caps_lock`, `scroll_lock`, or `all`); Core 0 owns the LED on/off state and retries the HID report until it is queued, so a dropped report cannot leave the LED stuck on.

---

## 8. Data Models (JSON on LittleFS)

All config/preset/pattern files live on the Pico's **LittleFS** filesystem in onboard flash, parsed/serialized with **ArduinoJson**. Default templates ship in the repo `data/` directory and are flashed to LittleFS on the image; runtime files are generated/overwritten on the device.

### 8.1 Global Config (`/config.json`)
```json
{
  "i2c_address": "0x27",
  "midi": { "din_enabled": true, "clock_enabled": false },
  "modifiers": { "chord_fn": "ctrl", "strum_fn": "alt", "global_fn": "super" },
  "chord": { "base_root_midi": 60, "note_range": [48, 84] },
  "chord_root_order": ["Db","Ab","Eb","Bb","F","C","G","D","A","E","B","F#"],
  "display": { "revert_timeout_ms": 1500, "prompt_timeout_ms": 5000, "cursor_timeout_ms": 5000, "menu_timeout_ms": 10000 },
  "led": {
    "bpm_indicator": true,
    "led": "num_lock",
    "flash_ms": 40
  },
  "startup_preset": "B1:P1"
}
```
> Notes vs. the Pi spec: there is **no `usb_enabled`** MIDI flag (DIN only); **no `keyboard_device`** field (the firmware owns the single hosted keyboard, no device path). Any value absent from `config.json` or a preset is filled from the defaults in the Parameter Reference (Section 9). See NFR-9 for validation and first-boot generation.

### 8.2 Preset (`/presets/bank<N>.json` → array of 8)
```json
{
  "name": "Ballad Pad",
  "chord": {
    "play_mode": "held",
    "octave": 0,
    "note_duration_ms": 500,
    "velocity": 100,
    "pan": 64,
    "extensions": { "add9": false, "add11": false, "add13": false },
    "voicing_mode": "root_position",
    "channel": 1
  },
  "strum": {
    "octave": 1,
    "note_duration_ms": 300,
    "velocity": 90,
    "limited_keys": false,
    "channel": 2
  },
  "rhythm": {
    "enabled": false,
    "pattern": "Rock 1",
    "tempo": 120,
    "swing": 0,
    "muted": false,
    "channel": 10
  }
}
```
> `rhythm.swing` is a **signed integer −75..+75** (Section 7.3), not a float.

### 8.3 Combination, Chord-Type & Root-Order Tables
The Omnichord combination matrix (6.3), chord interval formulas (6.2), the circle-of-fifths root order (5.1, `chord_root_order`), the keymap, and per-parameter defaults are stored as editable JSON on LittleFS (shipped from repo `data/`) so behavior/layout can be tuned without recompiling. To minimize flash writes and RAM, large static tables may alternatively be compiled into flash as C++ constants with JSON overrides — see the roadmap for the chosen persistence strategy (default: JSON on LittleFS via ArduinoJson).

---

## 9. Parameter Reference (defaults, ranges, steps)

All adjustable parameters, with min/max/default/step. On default initialization (FR-P9) and when a config/preset value is missing, use the **Default** column. Editing clamps to [Min, Max] in increments of **Step**. This table is the single source of truth for parameter bounds. **All values are integers** (RP2040 has no FPU).

| Parameter | Scope | Type | Min | Max | Default | Step | Notes |
|-----------|-------|------|-----|-----|---------|------|-------|
| play_mode | chord | enum | — | — | `held` | cycle | held / press_to_play / arpeggio / rhythm / silent |
| note_duration_ms | chord | int | 50 | 4000 | 500 | 50 | note length for press-to-play/arp lifecycles |
| velocity | chord | int | 1 | 127 | 100 | 1 | MIDI note-on velocity |
| pan | chord | int | 0 | 127 | 64 | 1 | CC10; 64 = center |
| octave | chord | int | −3 | +3 | 0 | 1 | transpose in octaves |
| voicing_mode | chord | enum | — | — | `root_position` | toggle | root_position / smart |
| add9 | chord | bool | off | on | off | toggle | independent extension |
| add11 | chord | bool | off | on | off | toggle | independent extension |
| add13 | chord | bool | off | on | off | toggle | independent extension |
| channel (chord) | chord | int | 1 | 16 | 1 | 1 | MIDI channel |
| octave (strum) | strum | int | −3 | +3 | 1 | 1 | strum-pool octave placement |
| note_duration_ms (strum) | strum | int | 50 | 4000 | 300 | 50 | strummed note length |
| velocity (strum) | strum | int | 1 | 127 | 90 | 1 | |
| limited_keys | strum | bool | off | on | off | toggle | limited numpad path |
| channel (strum) | strum | int | 1 | 16 | 2 | 1 | MIDI channel |
| tempo | rhythm | int | 40 | 260 | 120 | 1 | BPM |
| swing | rhythm | int | −75 | +75 | 0 | 5 | signed off-beat delay percentage (integer/fixed-point; see 7.3) |
| enabled | rhythm | bool | off | on | off | toggle | rhythm on/off |
| muted | rhythm | bool | off | on | off | toggle | suppress drum note-ons |
| pattern | rhythm | enum | — | — | `Rock 1` | cycle | from rhythm list (7.1) |
| channel (rhythm) | rhythm | int | 1 | 16 | 10 | 1 | GM percussion |
| clock_enabled | global | bool | off | on | off | toggle | MIDI clock transmit (FR-R7) |
| bpm_indicator | global | bool | off | on | on | toggle | keyboard-LED BPM indicator (FR-R8) |
| led (indicator) | global | enum | — | — | `num_lock` | — | which keyboard LED(s) to flash: num_lock / caps_lock / scroll_lock / all |
| flash_ms | global | int | 5 | 500 | 40 | 5 | beat flash duration (consistent on every beat) |
| revert_timeout_ms | global | int | 250 | 5000 | 1500 | 250 | LCD param-edit revert |
| prompt_timeout_ms | global | int | — | — | 5000 | — | save/clear auto-cancel (FR-P10) |
| cursor_timeout_ms | global | int | 500 | 30000 | 5000 | — | cursor-navigation auto-reset (FR-P3) |
| menu_timeout_ms | global | int | 500 | 30000 | 10000 | — | edit-menu idle auto-exit |

> **Changed from the Pi spec:** `swing` is now a **signed integer −75..+75 with step 5** (was float 0.0–0.75 step 0.05). Semantics are identical (percentage off-beat delay; positive laid-back, negative rushed); the representation is integer for the no-FPU RP2040.

---

## 10. Non-Functional Requirements

- **NFR-1 Latency:** Key-report to MIDI-output latency < 10 ms (target). Bounded largely by USB-host poll interval (typically 1 ms) plus minimal per-event work on Core 0. Measure and document typical/worst-case.
- **NFR-2 Timing accuracy:** Rhythm/arp step timing jitter < 2 ms. Achieved via the dedicated Core 1 monotonic scheduler with non-blocking MIDI writes; LED HID writes kept off the critical path.
- **NFR-3 Resource use:** Run within RP2040 constraints (264 KB SRAM, 2 MB flash). Pico-PIO-USB uses ~15 KB RAM + 1 PIO block (3 state machines); keep the rest lean. No dynamic allocation in real-time paths; prefer static buffers.
- **NFR-4 Boot-to-ready:** No OS/systemd; firmware runs from flash on power-up. Reach playable state quickly (target < 3 s), dominated by USB enumeration of the keyboard and LittleFS mount.
- **NFR-5 Robustness:** Handle keyboard hot-plug/unplug (re-enumerate), missing/failed LCD, keyboards that reject LED output reports, and an unconnected DIN, without crashing or hanging. A watchdog may be used to recover from a wedged USB stack.
- **NFR-6 N-key rollover:** Support simultaneous chord + strum key presses. Rollover is bounded by the *keyboard's* HID report protocol (boot-protocol keyboards commonly report at most 6 concurrent keys plus modifiers). Prefer report-protocol (full NKRO) when the keyboard offers it; document per-keyboard limitations.
- **NFR-7 Configurability:** All tunable behavior (keymap, chord tables, rhythms, parameter defaults) lives in editable JSON on LittleFS.
- **NFR-8 Logging:** Provide a debug log mode over USB-CDC (native USB port) or a spare UART for troubleshooting input/MIDI, compiled out of release builds if needed.
- **NFR-9 Config validation & first-boot generation:** On startup, mount LittleFS (format if unmounted/first boot) and validate `config.json` and preset banks against the schema/Parameter Reference (Section 9). On missing files/directories, generate defaults (default config + 10 banks × 8 default presets, from shipped `data/` templates). On corrupt or out-of-range values, fall back to defaults for the affected fields and log a warning; never crash on bad config.

---

## 11. Testing Strategy

> **Verification instruments & feedback dependency:** Testing keyboard input does **not** require the LCD. Feedback instruments, in priority order: (1) **native GoogleTest** for all pure logic (keymap, chords, voicing, strum, rhythm, config, latching) with no hardware; (2) the **USB-CDC debug log + MIDI monitor**, available from M2 (Section 12), as the on-device instrument for input capture and MIDI output through M2–M5; (3) the **LCD**, which is a feature under test at M6 and is *not* a prerequisite for verifying earlier milestones. On-device milestones M2–M5 depend on the debug log/MIDI monitor for feedback, never on the screen — which is why the display is built at M6 rather than before the input/chord engines.

### 11.1 Off-device unit tests (GoogleTest, `native` PlatformIO env)
The pure-logic core must be testable on a desktop without the Pico hardware. Structure the code so these have no Arduino/hardware includes:
- **Chord computation:** root + type → correct MIDI note sets (per 6.2).
- **Combination matrix:** same-column, left-adjacent (sus4/add9), and leftmost-column `` ` `` cases (6.3) resolve to the correct chord.
- **Voicing:** root-position vs smart voice-leading produce expected voicings; octave clamping (VR-3).
- **Extensions:** independent add9/add11/add13 flags add the correct tensions (6.2).
- **Strum pool:** note-pool derivation and key→note mapping for full and limited layouts (6.6).
- **Rhythm stepping:** pattern → scheduled events; integer tempo/swing math (7.2/7.3), including the fixed-point swing formula.
- **Held-mode latching:** parameter snapshot vs pending state (FR-C9/VR-5).
- **Config:** validation, default generation, and out-of-range fallback (NFR-9), using an in-memory/std::filesystem stub for the storage adapter.
- **Keymap:** HID usage + modifier byte → action mapping; Shift/Caps/Tab resolve as chord roots; keypad usages Num-Lock-independent; `+`/`-` context-sensitivity.

### 11.2 On-device verification
- **MIDI monitor/log mode:** a build/runtime flag that logs every outgoing MIDI message (channel, note/CC, velocity, timestamp) over USB-CDC/UART for verifying output without external gear.
- **Latency measurement:** instrumented timing from HID report receipt to MIDI dispatch to validate NFR-1.
- **Timing/jitter measurement:** log Core 1 scheduler step timestamps to validate NFR-2.

### 11.3 Manual hardware test checklist
A checklist mapped 1:1 to the Acceptance Criteria (Section 13), executed on assembled hardware with an external synth/DAW and (optionally) a DIN MIDI monitor.

---

## 12. Development Phases / Milestones
These milestones align with the Build Sequence (Section 1.6); stop for user feedback after each. Every milestone keeps the GoogleTest `native` suite green before its stop.

1. **M1 — Scaffolding:** PlatformIO project (`pico` + `native` envs), pinned lib deps, `src/main.cpp` entry (`setup/loop` + `setup1/loop1`), LittleFS layout + config loader with first-boot defaults, adapter interfaces with null implementations, GoogleTest skeleton.
2. **M2 — Core framework:** USB-host HID keyboard input (TinyUSB + Pico-PIO-USB); State Manager (pending vs active params scaffolding); MIDI Router (DIN via `Serial2`/UART1); Config/Preset store (LittleFS + ArduinoJson); hardcoded note on DIN; minimal USB-CDC debug log + MIDI monitor (on-device feedback instrument for M2–M5).
3. **M3 — Chord Engine:** keymap resolver, chord/interval computation, full combination matrix, voicing modes, extensions, all five play modes with correct note lifecycle, Held-mode latching.
4. **M4 — Strum Plate:** note-pool derivation, full + limited (Num-Lock-independent) layouts, strum params, immediate edit pickup.
5. **M5 — Rhythm Engine:** Core 1 scheduler/clock, pattern playback on ch10, integer tempo/fixed-point swing, mute, MIDI clock toggle, chord sync, keyboard-LED BPM indicator (FR-R8).
6. **M6 — Display:** LCD idle screen (+ dirty `*`), parameter-edit feedback, prompts.
7. **M7 — Presets/Config:** JSON load/save on LittleFS, 80-slot navigation, save/clear with auto-cancel, per-function channels, defaults for uninitialized slots.
8. **M8 — Polish:** panic control, latency/jitter tuning, robustness/hot-plug, config validation/fallback, full test suite, and hardening/finalizing the logging + MIDI monitor introduced in M2.

---

## 13. Acceptance Criteria

- **AC-1** Pressing a mapped chord key produces the correct chord notes (verified per 6.2) on the DIN output.
- **AC-2** Chord quality combinations produce chords per the matrix in 6.3 (incl. Major+Minor+7th = Augmented).
- **AC-3** All five play modes behave as specified, including note lifecycle (FR-C3/FR-C10): held sustains until next chord/panic, press-to-play releases on key-up, arpeggio uses note_duration_ms.
- **AC-4** Strum keys play chord notes in order; both full and limited layouts work; strum params take effect; strum picks up edits immediately (FR-S5).
- **AC-5** Rhythm patterns play correct GM drum notes on ch10; tempo and swing adjust live; mute suppresses drums but keeps sync.
- **AC-6** In arp/rhythm chord modes with rhythm enabled, chord timing follows the rhythm clock.
- **AC-7** All 80 preset slots save/recall chord, strum, rhythm params and MIDI channels on LittleFS; navigation keys behave per 4.4; save/clear confirm/cancel works.
- **AC-8** LCD shows the correct idle screen and updates on parameter edits and prompts.
- **AC-9** MIDI is emitted correctly on the DIN output; an unconnected/disconnected DIN does not disrupt operation; the firmware keeps running and transmitting.
- **AC-10** Measured key-to-MIDI latency meets NFR-1.
- **AC-11** Modifier keys (`Tab`/`Caps`/`Shift`×2/`[`/`'`) function as chord roots via raw HID report parsing, independent of any lock-LED state.
- **AC-12** Toggling voicing mode changes output between root-position and smart voice-leading; setting persists in preset.
- **AC-13** add9/add11/add13 extensions toggle independently via Left/Down/Right arrows and persist in preset.
- **AC-14** Left-adjacent combos produce sus4 (Major+left-7th) and add9 (Major+left-minor); leftmost column produces these via the `` ` `` key.
- **AC-15** In Held mode, editing any chord parameter (or loading a preset) while a chord is sounding does not change the currently-held notes; the change applies only when the next chord is triggered (FR-C9 / VR-5).
- **AC-16** MIDI clock transmit toggle (`F6`, or Clock Out in the Rhythm Edit menu) emits/stops the 24 PPQN clock stream (0xF8 only; no Start/Stop) on the DIN output; the master clock timebase keeps running in the background regardless of the toggle (FR-R7, §7.3).
- **AC-17** Panic (`Super+Esc`) silences all notes on all channels/DIN and clears active-note state (FR-C11).
- **AC-18** Numpad strum keys behave identically regardless of the keyboard's Num Lock state (FR-S6).
- **AC-19** Save/clear prompt auto-cancels after 5 s idle, and any chord/strum key cancels it (FR-P10).
- **AC-20** Dirty marker (`*`) appears when active state differs from the stored preset and clears on save/load (FR-P11).
- **AC-21** Uninitialized/cleared preset slots load factory defaults with per-function channels 1/2/10 (FR-P9/FR-P12).
- **AC-22** Corrupt or missing config/presets on LittleFS are regenerated or fall back to defaults without crashing (NFR-9).
- **AC-23** When `bpm_indicator` is on, the configured LED (default Num Lock) flashes once per beat of the master clock, independent of rhythm/clock-out state; `F10` (or Beat LED, `Ctrl` → `F7`) enables/disables it; LED unavailability does not affect timing or MIDI (FR-R8).

> **Removed vs. the Pi spec:** the USB-MIDI-gadget acceptance clauses are gone (no USB MIDI in v1). AC-9 is reworded from "both USB and DIN" to DIN-only with graceful behavior when unconnected.

> **Acceptance-criteria numbering:** the criteria above are final for this Pico edition and numbered **AC-1 … AC-23** with no gaps; the roadmap's coverage matrix (Roadmap §6.1) maps every one of these to a milestone.

---

## 14. Open Items for Review
Resolved in this Pico port (see referenced sections): board/compute choice (RP2040 sufficient; see roadmap), language (C++17 / Arduino-Pico), USB keyboard hosting (TinyUSB + Pico-PIO-USB), MIDI path (DIN-only, USB host/gadget conflict eliminated), persistence (JSON on LittleFS via ArduinoJson), no-FPU integer/fixed-point swing, dual-core threading model, no-OS boot, testing via GoogleTest on a `native` env.

Still open (deferred to post-hardware testing):
1. **Final GPIO pin map** (PIO-USB D+/D-, `Serial2`/UART1 TX, I2C SDA/SCL): defaults proposed in 2.2; finalize during M1/M2 bring-up and record in the roadmap hardware section.
2. **LCD line-2 layout (4.6 FR-D1):** to revisit after hardware testing. (Implementation approach in Roadmap §8.2 item 2 — shipped as the draft layout, data-sourced for tuning.)
3. **Rhythm list (7.1):** confirm/expand the pattern set. (The 12 named patterns to ship as-is are listed in Roadmap §8.2 item 3.)
4. **Persistence detail:** confirm JSON-on-LittleFS for all tables vs. compiling large static tables into flash with JSON overrides (8.3).

