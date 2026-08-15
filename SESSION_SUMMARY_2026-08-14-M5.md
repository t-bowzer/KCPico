# KeybChord Pico — Session 2026-08-14 (M5) Summary

## Goal: M5 — Rhythm Engine (roadmap line 399; spec §4.3 / §7)

Core 1 monotonic scheduler/clock, JSON pattern playback on ch10, integer tempo /
fixed-point swing, mute, MIDI clock 24 PPQN, arp/rhythm chord sync, and the
Scroll Lock BPM LED (FR-R8).

## Result: SUCCESS — M5 complete (plus two hardware-driven follow-ups)

- **Native tests:** `pio test -e native` → **164/164 passing** (was 124 at M4).
- **Pico build:** `pio run -e pico -t buildunified` → SUCCESS
  (`firmware_with_fs.uf2`, RAM 12.4%, Flash 8.7%).
- **On-device:** drums play (velocity fixed), tempo/swing/mute/clock all
  confirmed; arp/rhythm follow the clock; clock is a pure tempo master (no
  Start/Stop); negative swing works. Scroll Lock LED report is sent and ACKed
  by the keyboard (`len=1`) but the specific test keyboard does not light it
  (keyboard-side limitation, FR-R8 best-effort).

### New files

```
keybchord/lib/core/rhythm.h/.cpp      # RhythmPattern model, JSON parse, stepUs /
                                        clockTickUs, signed fixed-point stepOffsetUs
                                        swing, stepEvents(), mapDrumNote(), 12-name
                                        rhythm list
keybchord/lib/engines/rhythm_engine.h/.cpp  # Core 1 deadline scheduler, key handling,
                                        MIDI clock, LED beat callback, RhythmClock
                                        snapshot, loadRhythmPatterns() + builtin fallback
keybchord/lib/engines/midi_event_queue.h    # lock-free SPSC MidiMessage ring (Core 1 -> Core 0)
keybchord/data/rhythms/*.json         # 12 GM drum patterns (Rock 1..Foxtrot)
keybchord/test/test_rhythm.cpp        # pattern/timing/swing/clock/drum-map unit tests
keybchord/test/test_rhythm_engine.cpp # scheduler lifecycle + key handling + loader tests
```

### Modified files

- `lib/core/params.h/.cpp` — `RhythmParams.pattern`, signed `swing` (-75..+75),
  `DrumMap drums`; `SWING_MIN/MAX`, `RHYTHM_PATTERN_*` bounds.
- `lib/core/presets.cpp` — `rhythm.pattern` (name<->index) + `rhythm.drums`
  (kick/snare/hihat/open_hat) load/save + equality.
- `lib/core/state.h/.cpp` — `RhythmClock` snapshot + `LedIndicator` shared state.
- `lib/core/keymap.h/.cpp` — `F7/F8/F9`, `PageUp/Down` (tempo), `Ctrl+PageUp/Down`
  (swing), `Super+F7/F8` (clock/LED); `isCtrl`/`isSuper`.
- `lib/engines/midi_router.h/.cpp` — `sendRaw()` (logs all but the 0xF8 clock).
- `lib/engines/chord_engine.cpp` — Arpeggio/Rhythm step at `stepUs(tempo)` and
  phase-lock to the rhythm clock when enabled (AC-6).
- `lib/hw/input_usbhost.cpp` — `tuh_hid_set_report_complete_cb` logs the real
  LED transfer result (len=0 fail / len=1 success).
- `lib/hw/debug_log.h` — `logLed` / `logLedComplete` / `logStrum*`.
- `src/main.cpp` — RhythmEngine wiring, SPSC queue drain, LED apply on Core 0,
  `loop1()` scheduler.
- `platformio.ini` — `rhythm.*` / `rhythm_engine.*` in native filter.

### Design decisions

- **Cross-core MIDI:** Core 1 schedules and enqueues `MidiMessage`s into a
  lock-free SPSC ring; Core 0 drains into `MidiRouter::sendRaw` so the UART
  stays single-threaded (no cross-core `Serial2` writes).
- **LED:** Core 1 computes flash state; Core 0 applies via `input->setLed`
  (single-threaded TinyUSB). Clock streams whenever the `Super+F7` toggle is on.
- **Clock:** `0xF8` only (24 PPQN), no Start/Stop/Continue — KeybChord is a
  pure tempo master. Streams independent of drums running.
- **Swing:** signed `int8_t` -75..+75 (step 5); delays the **off-beat 8th**
  (`stepInBeat == 2`) by `(base*swing)/100`. Positive = laid-back, negative = rushed.
- **Drum map:** per-preset `DrumMap` (kick 36 / snare 38 / hihat 42 / open_hat 46
  defaults) remapped at fire time via `mapDrumNote()` (FR-R3).
- **Arp/rhythm tempo:** Arpeggio/Rhythm always advance at `stepUs(stored tempo)`
  (not `note_duration_ms`); rhythm disabled no longer falls back to Held.

### Bugs found & fixed during hardware testing

| # | Symptom | Root cause | Fix |
|---|---------|-----------|-----|
| 1 | Drums inaudible | pattern velocity `1` sent literally | `1` -> `RHYTHM_DEFAULT_VELOCITY = 100` |
| 2 | Snare sounded like a kick | test synth maps 38 oddly (code was correct) | per-preset `DrumMap` for kick/snare/hats |
| 3 | Clock sent Start to the synth | transport messages unwanted | removed Start/Stop/Continue; 0xF8 only |
| 4 | Arp/rhythm reverted to default tempo with rhythm+clock off | arp used `note_duration_ms` | arp interval = `stepUs(tempo)` |
| 5 | Drums did not swing (only arp did) | swing delayed the 16th "e"/"a" steps, but drums sit on the 8th grid | swing the off-beat 8th (`stepInBeat == 2`) |
| 6 | LED report "ok" but not lit | `ok` = queued, not delivered | completion callback; shows `len=1` (delivered) |

## Open items (deferred)

- **Scroll Lock LED:** host sends and the keyboard ACKs the report, but the test
  keyboard does not light it. Not a firmware fault (FR-R8 best-effort). A
  configurable LED target (`scroll_lock`/`caps_lock`/`num_lock`, per spec §9)
  is a candidate follow-up.
- **Cross-core shared state:** `pendingRhythm` (Core 0 write / Core 1 read) and
  `RhythmClock` (Core 1 write / Core 0 read) use plain volatile fields; individual
  aligned accesses are atomic on RP2040. A mutex/double-buffer is an M8 hardening item.
- **Core 0 drain latency:** the loop `delay(10)` means rhythm events can lag up
  to ~10 ms on the wire (audible at 260 BPM). Tighten during M8 / NFR-2.

## Verification approach (unchanged)

- Native: GoogleTest; `pio test -e native` green.
- On-device: `pio run -e pico -t buildunified` -> drag `firmware_with_fs.uf2`
  (BOOTSEL) -> monitor 115200.

## Reference

- Roadmap `KeybChord_Pico_Roadmap.md` (M5 at line 399).
- Spec `KeybChord_Pico_Spec.md` §4.3 / §5.6 / §7 (rhythm), §9 (params).
- Prior summaries: `SESSION_SUMMARY_2026-08-13-M4.md` (M4).
