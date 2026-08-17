# KeybChord Pico — Session 2026-08-16 (M8) Summary

## Goal: M8 — Polish (roadmap §6 line 417; spec §13 + NFRs)

Final milestone: panic, latency/jitter instrumentation, hot-plug robustness +
watchdog, config-validation hardening, logging/MIDI-monitor finalization,
cross-core shared-state hardening, and the full test suite.

## Result: SUCCESS

- **Native tests:** `pio test -e native` → **259/259 passing** (was 243 at M7).
- **Pico build:** `pio run -e pico -t buildfs` + `-t buildunified` → SUCCESS
  (`firmware_with_fs.uf2`, RAM 12.5%, Flash 10.7%).

## Panic (FR-C11 / AC-17)

- `ActionType::Panic` maps `Super+Esc` (0x29 under the Super modifier) in the
  keymap resolver; Esc alone remains `ClearEdit`.
- `MidiRouter::panic()` sends All-Sound-Off (CC120) + All-Notes-Off (CC123) on
  **all 16 channels**, then clears active-note state.
- `src/main.cpp` handles `Panic` *before* the preset/edit/chord/strum dispatch,
  so it works from any state (prompt, cursor, edit menu, mid-strum). A shared
  `clearAllSound()` releases the chord/strum/rhythm engines first, cancels any
  transient UI state, and forces the beat LED off; then the CC flood is sent.
- Tests: `SuperEscIsPanic`, `PanicSendsCcOnAllChannelsAndClearsState` (asserts
  16×CC120 + 16×CC123 and empty active-note state).

## Latency / jitter instrumentation (NFR-1/NFR-2, AC-10)

- New pure `lib/core/perf.h` `PerfStats` (int-µs count/min/max/avg).
- Core 0 records loop-processing time (an upper bound on key-to-MIDI latency);
  `RhythmEngine` records step-lateness jitter (actual − deadline) on Core 1.
- Every 5 s both are summarized over USB-CDC via `logPerf(...)`.

## Robustness / hot-plug (NFR-5) + watchdog

- `InputAdapter::connected()`; `InputUsbHost` reports `mounted_`.
- On a keyboard disconnect the firmware releases all sounding notes
  (`clearAllSound()`), drops the pending beat-LED SET_REPORT retry (no spin while
  unmounted), and logs the event; reconnect is logged too. The beat-LED apply is
  now gated on `connected()`.
- **Watchdog:** `rp2040.wdt_begin(1000)` at boot, fed from both `loop()` and
  `loop1()` to recover from a wedged USB stack.

## Config validation/fallback hardening (NFR-9 / AC-22)

- `AppConfig::load` now rejects a non-object top-level document and requires
  `is<int>()` on each `note_range` element; an inverted `note_range` is swapped
  back to sane order. Per-field fallback remains never-throw.

## Logging / MIDI-monitor finalization (NFR-8)

- Compile gate `KEYBCHORD_LOG` (added to the `pico` env; release builds omit it
  for zero log overhead); run-time toggles `debug_log_enabled` /
  `midi_monitor_enabled` in `config.json` (default on), applied at boot.
- Richer MIDI decode (`lib/core/naming.*`): channel, message-type name, flat
  note name (e.g. `F#4`), CC name (e.g. `Volume`, `AllSoundOff`).

## Cross-core shared-state hardening (deferred M7)

- `LedIndicator::flash` and all `RhythmClock` fields are now `std::atomic`.
- `pendingRhythm` Core-1 reads are documented as best-effort ≤32-bit field reads
  (individually atomic on RP2040); no mutex on the Core 1 deadline path (NFR-2).

## New files

```
lib/core/perf.h                 # PerfStats (latency/jitter accumulator)
test/test_perf.cpp
```

## Modified files

- `lib/core/keymap.h/.cpp` — `ActionType::Panic`, `Super+Esc` mapping.
- `lib/engines/midi_router.h/.cpp` — `panic()`.
- `lib/engines/rhythm_engine.h/.cpp` — step-lateness jitter + `jitterStats()`.
- `lib/core/state.h/.cpp` — `std::atomic` clock/LED fields; field-wise reset.
- `lib/core/config.h/.cpp` — `debug_log_enabled`/`midi_monitor_enabled`, non-object
  guard, `note_range` type check + inversion swap.
- `lib/core/naming.h/.cpp` — `noteName`, `ccName`, `messageTypeName`.
- `lib/hw/base.h` — `InputAdapter::connected()`.
- `lib/hw/input_usbhost.h` — `connected()`.
- `lib/hw/debug_log.h` — `KEYBCHORD_LOG` gate, run-time toggles, richer decode,
  `logPerf`.
- `src/main.cpp` — panic handling, hot-plug edge detection, watchdog, perf
  instrumentation, log-toggle application.
- `platformio.ini` — `-DKEYBCHORD_LOG`.
- `data/config.json` — `logging` block.
- `test/test_keymap.cpp`, `test/test_midi_router.cpp`, `test/test_config.cpp`,
  `test/test_naming.cpp` — new cases.

## Verification approach

- Native: GoogleTest; `pio test -e native` green (259).
- On-device: `pio run -e pico -t buildfs` then `-t buildunified` → drag
  `firmware_with_fs.uf2` (BOOTSEL) → monitor 115200.

## Next steps

1. **Hardware test plan** (this session): verify panic, hot-plug, latency/jitter
   (PERF lines), watchdog recovery, and the logging toggles on real hardware.
2. **M9 (future) — USB Mass Storage + dev serial build** (roadmap §6).

---

# Post-M8 hardware-feedback fixes (2026-08-16)

Testing findings from the field and the fixes applied:

## 1. Panic semantics (Esc vs Super+Esc)

- **Root cause:** `RhythmEngine::allNotesOff()` → `stop()` only cleared
  `running_`, but `update()` restarted the rhythm because
  `pendingRhythm.enabled` stayed true; the beat LED also kept flashing because
  it slaves to the always-running master clock (independent of rhythm).
- **Change:** `Esc` (no super, plain main menu) now cancels chord + strum
  notes only (`cancelChordStrum()`). `Super+Esc` is the true panic:
  `cancelChordStrum()` + `pendingRhythm.enabled = false` (drums stop and stay
  off until `F7`) + CC120/CC123 flood + force the beat LED off for the current
  pulse (it resumes on the next beat — `bpm_indicator` is left unchanged).
- `PresetEngine::promptActive()` lets the loop tell Esc-in-menu/cursor/prompt
  apart from Esc-cancels-sound. Hot-plug disconnect now calls
  `cancelChordStrum()` (rhythm keeps running).

## 2. Jitter metric

- **Root cause:** jitter was recorded only in the rhythm *step* loop, which is
  idle when the rhythm is off — hence `step-jitter-us n=0`.
- **Change:** jitter is now recorded in `advanceClock()` (tick lateness
  `now_us - nextClockUs_`) so it measures the always-running master clock; the
  log label is `clock-jitter-us`. Regression test
  `ClockJitterIsRecordedEvenWithoutRhythm` asserts `count() > 0`.

## 3. Key-to-MIDI latency

- **Root cause:** `core0-loop-us` is a whole-loop metric inflated by blocking
  USB-CDC logging and multi-key bursts (saw 21–25 ms under mash).
- **Change:** `KeyEvent` now carries `received_us` (stamped in
  `InputUsbHost::onReport`). A new `key-to-midi-us` `PerfStats` records the
  per-event HID-receipt → MIDI-dispatch latency (skips null-adapter/test
  events). `core0-loop-us` is retained as a Core-0 load indicator; debug
  logging inflates both, and a release build (omit `-DKEYBCHORD_LOG`) reports
  the true latency.

## Verification

- Native: `pio test -e native` → **260/260 passing** (was 259).
- Pico: `pio run -e pico -t buildfs` + `-t buildunified` → SUCCESS
  (RAM 12.6%, Flash 10.7%).

## Deferred

- USB Mass Storage + dev-serial boot-key toggle remains **M9** (feasibility
  confirmed: keyboard is on PIO-USB GP0/GP1, so the native USB port can be
  chosen at boot by polling a held key; MSC-active must suppress LittleFS
  writes).

(End of file - total 92 lines)
