# KeybChord Pico — Session 2026-08-15 (M7) Summary

## Goal: M7 — Presets & banks (roadmap §6 line 409; spec §4.4, AC-7/19/20/21), plus a round of feedback-driven refinements

Two themes landed this session:

1. **M7 — PresetEngine** (80-slot JSON presets with cursor navigation, save/clear
   prompts, dirty-state tracking, startup-preset load).
2. **Feedback-driven fixes & a timing redesign** requested on-device testing:
   display padding, menu/cursor timeouts, hotkey remaps (F6/F10/F12), arrow
   navigation + auto-repeat, a configurable non-sticking beat LED, and a
   **clock-as-master** redesign so LED, drums, and MIDI clock can never drift.

## Result: SUCCESS

- **Native tests:** `pio test -e native` → **243/243 passing** (was 190 at M6).
- **Pico build:** `pio run -e pico -t buildunified` → SUCCESS
  (`firmware_with_fs.uf2`, RAM 12.5%, Flash 10.5%).
- **On-device (reported by user):** presets/cursor work; the beat LED pulses on
  Num Lock; after the clock redesign, "LED, drums, and the synth's clock LED
  stay locked together" and mashing `F7` no longer desyncs or drops a pulse.

## M7 — Preset model (new)

- **`PresetEngine`** (`lib/engines/preset_engine.*`) owns cursor, load, save,
  clear, prompts, dirty recompute, and startup-preset load.
- **Cursor navigation:** `Home`/`End` move a preset cursor (wraps across all 80
  slots); `Super+Home/End` move banks (slot preserved); `Enter` loads the cursor
  slot; `Super+1..8` loads directly. The cursor auto-resets after
  `cursor_timeout_ms` (5 s). Any hotkey cancels browsing and is handled; chord/
  strum keys stay live.
- **Save/Clear:** `Insert` = Save (confirm), `Delete` = Clear (confirm; resets
  live params + stores defaults). The old `Super+Insert`/`Super+Delete` bindings
  were removed. Prompts auto-cancel after `display_prompt_ms` (5 s) and cancel on
  any chord/strum key (FR-P10).
- **Dirty `*`** tracks pending-vs-stored (FR-P11), recomputed on every edit via
  the `EditEngine` any-edit callback; cleared on save/load.
- **Startup preset** loads `config.startup_preset` (`B1:P1`) at boot.

## Feedback refinements

- **Display bugfix:** `DisplayManager::emit` now pads both LCD lines to 16
  columns, so a shorter location can't leave a stale trailing digit.
- **Timeouts:** `cursor_timeout_ms` 3000→5000; new `menu_timeout_ms` (10 s) exits
  an open edit menu after idleness.
- **Hotkey remap:** `F6` = Clock Out toggle, `F10` = beat-LED toggle, `F12` =
  Chord-Edit fallback (was `F10`).
- **Menu navigation:** `Left`/`Right` step between parameters; `Up`/`Down` change
  the value. Large-range params (tempo/velocity/pan/duration/swing) auto-repeat
  while held (~500 ms delay, ~80 ms interval); small ranges don't repeat.

## Beat LED rework (FR-R8)

- Configurable target `led.led` — `num_lock` (default), `caps_lock`,
  `scroll_lock`, or `all` (RGB keyboards often lack a Scroll Lock LED).
- Removed the downbeat accent and `accent_downbeat`/`accent_flash_ms`/
  `led_only_when_rhythm` config fields — a consistent `led_flash_ms` pulse.
- **Stuck-on fix:** `LedIndicator` is now a single `flash` request flag; Core 0
  owns the on/off state (no cross-core torn deadline) and retries the HID report
  until queued, so a dropped SET_REPORT can't leave the LED on.

## Clock-as-master redesign (spec §7.3)

- A single **master MIDI clock** (24 PPQN) runs continuously from boot in
  `RhythmEngine::advanceClock()`; its tick counter and phase **never reset**
  (not on rhythm toggle, not on Clock-Out toggle). `midi_clock_enabled` only
  gates whether `0xF8` is emitted.
- The **LED** flashes on every 24th tick; the **rhythm** aligns its downbeat to a
  24-tick beat boundary and steps are exactly `clockTickUs × 6`, eliminating the
  `stepUs` integer-division drift. Nothing preempts or resets the clock.

## New files

```
lib/engines/preset_engine.h/.cpp   # cursor nav, save/clear, prompts, dirty, startup
test/test_preset_engine.cpp
```

## Modified files

- `lib/core/presets.*` — `sameParams`, `makePreset`, `parsePresetLocation`.
- `lib/core/keymap.*` — preset/cursor actions, F6/F10 hotkeys, Insert/Delete.
- `lib/core/state.*` — cursor fields; `LedIndicator` reduced to `flash`.
- `lib/core/config.*` — `cursor_timeout_ms`, `menu_timeout_ms`, `LedTarget` enum;
  removed accent fields.
- `lib/core/param_edit.*` — `isAutoRepeatable`.
- `lib/engines/edit_engine.*` — arrow nav, auto-repeat, any-edit callback, menu
  timeout, F12 fallback.
- `lib/engines/rhythm_engine.*` — clock-as-master redesign.
- `lib/engines/display_manager.cpp` — 16-col padding; cursor `>` marker.
- `src/main.cpp` — PresetEngine wiring + Core-0-owned LED state.
- `data/config.json` — `led: num_lock`, timeouts, removed accent fields.
- `KeybChord_Pico_Spec.md` / `KeybChord_Pico_Roadmap.md` — sync'd (see below).

## Verification approach

- Native: GoogleTest; `pio test -e native` green (243).
- On-device: `pio run -e pico -t buildunified` → drag `firmware_with_fs.uf2`
  (BOOTSEL) → monitor 115200. **Note:** `buildunified` reuses a cached
  `littlefs.bin`; run `pio run -e pico -t buildfs` first after any `data/`
  change, then `buildunified`.

## Next steps

1. **M8 — Polish** (roadmap §6): panic (`Super+Esc`), latency/jitter measurement,
   robustness/hot-plug, config-validation hardening, finalize logging/MIDI
   monitor, full test suite, optional watchdog.
2. **M9 (future) — USB Mass Storage + dev serial build** (roadmap §6): mount
   LittleFS as an MSC drive on the native USB port (drag-drop JSON editing), with
   a companion dev build that keeps USB-CDC serial for the debug log — an
   either/or build-time choice, keyboard staying on PIO-USB (GP0/GP1).
3. Cross-core shared-state hardening (`pendingRhythm`/`RhythmClock` plain
   volatiles) remains deferred to M8.
