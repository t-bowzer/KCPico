---
name: keybchord-firmware
description: Use when changing KeybChord RP2040 firmware under keybchord/ (lib/core, lib/engines, lib/hw, src/main.cpp), building with PlatformIO, or running native GoogleTest tests. Covers placement, commands, layering, and runtime invariants.
---

# KeybChord firmware

## Commands

```bash
cd keybchord && pio run -e pico      # production firmware
cd keybchord && pio test -e native   # hardware-free unit tests
```

Read `AGENTS.md` for the repo map. If the task adds a setting, load
`keybchord-add-param`; if it changes device JSON, load `keybchord-json`; if it
touches the CH559 bridge, load `keybchord-ch559`.

## Layering

- `lib/core/` — portable data and logic. No Arduino/pico headers, no
  exceptions. Files include `params.*`, `presets.*`, `config.*`, `state.*`,
  `keymap*`, `macros.*`, `rhythm.*`, `bass.*`, `chordpattern.*`, `sequence.*`,
  `chords.*`, `voicing.*`, `strum.*`, `naming.*`, `defaults.*`.
- `lib/engines/` — stateful behavior driven by `update(now_us)` and
  `handleKeyEvent()`: `chord_engine`, `strum_engine`, `bass_engine`,
  `rhythm_engine`, `sequencer_engine`, `edit_engine`, `preset_engine`,
  `display_manager`, `midi_router`.
- `lib/hw/` — adapters implementing `base.h` (`InputAdapter`,
  `MidiOutAdapter`, `LcdAdapter`, `StorageAdapter`). `factory.cpp` selects the
  real RP2040 adapters or the native null/stub adapters.
- `src/main.cpp` — hardware wiring and the Core 0/Core 1 loop.
- `test/` — GoogleTest tests and fixtures using null/stub adapters.

## Runtime model

- Core 0 handles input polling, keymap resolution, engine updates, LCD, and MIDI
  dispatch. Core 1 runs the rhythm scheduler and posts work through
  `MidiEventQueue`.
- `StateManager` holds `pending*` parameter edits and `active*` values, plus
  `selectedChord`, preset cursor state, config, keymap, and runtime clocks.
  Cross-core fields are atomics; do not add a mutex to the Core 1 deadline path.
- `MidiRouter` fans messages to DIN and USB MIDI and owns panic/all-notes-off.
  Engines should call the router rather than sending bytes directly.
- The beat LED is Core 0 state; Core 1 only posts a flash request.

## Change workflow

1. Put pure data/parsing/timing helpers in `lib/core` so native tests cover them.
2. Put stateful timing or input behavior in the relevant `lib/engines` class.
3. Wire new hardware or callbacks in `src/main.cpp`.
4. Add or extend a focused test under `test/`.
5. Run `pio test -e native`, then `pio run -e pico` for compile/link issues.

If you add a new engine or hardware source file, update the explicit
`build_src_filter` in `platformio.ini` for the native env; it lists files
individually rather than globbing everything.

## Test conventions

- `test_main.cpp` initializes GoogleTest and has adapter smoke tests.
- Chord/strum engine fixtures (`chord_engine_fixture.h`,
  `strum_engine_fixture.h`) wire `StateManager`, `MidiRouter`, and
  `RecordingMidiOutAdapter`.
- `recording_midi_out.h` exposes emitted messages for assertions.
- `StorageStub` backs config/preset/keymap JSON tests without hardware.
- Keep tests hardware-free; native builds use `*_null.*`/`*_stub.*` adapters.

## Token discipline

- Start with `rg -n '<symbol>'` and read the matching header or a narrow line
  range. Do not read the full spec or configuration guide.
- Prefer headers, tests, and focused source ranges over re-reading README.
- `testhook` is only for the `testhook` env (`KEYBCHORD_TEST_HOOK`); never let
  test-hook code enter the production `pico` build.
