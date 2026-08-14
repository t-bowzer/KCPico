# KeybChord Pico — Session 2026-08-13 (M4) Summary

## Goal: M4 — Strum Plate (roadmap line 394; spec §4.2 / §6.6)

Implement the strum plate on top of the M3 chord engine: note-pool derivation
from the active chord, full + limited numpad layouts (Num-Lock independent),
strum params with immediate pickup (FR-S5), then verify on hardware.

## Result: PARTIAL — M4 implemented, but one hardware bug still open

- **Native tests:** `pio test -e native` → **123/123 passing** (was 91 at M3; +32 new).
- **Pico build:** `pio run -e pico` → **SUCCESS** (RAM 12.1%, Flash 8.1%).
- **On-device:** strumming plays the active chord's pool in both layouts; three
  firmware bugs found & fixed. **One bug remains open** (see "Open bug" below).

### New files (7)

```
keybchord/lib/core/
└── strum.h/.cpp            # buildNotePool() (base + 12*octave anchor), strumIndexFor()
                              (full number-row / full numpad / limited sequences), strumKeyCount()
keybchord/lib/engines/
└── strum_engine.h/.cpp     # strum trigger -> pool note, note-off scheduling, Alt+F2/F3/F4
                              edit targets + context-sensitive ±, Alt+F5 limited toggle, Esc clear
keybchord/test/
├── test_strum.cpp          # pool derivation, key->index mapping (all 3 sequences)
├── test_strum_engine.cpp   # lifecycle, layouts, immediate pickup, edit stepping, regressions
└── strum_engine_fixture.h  # shared fixture (recording MIDI out + router + chord + strum engines)
```

### Modified files

- `lib/core/chords.h/.cpp` — lifted `chordPitchClasses()` out of voicing for reuse by strum.
- `lib/core/voicing.cpp` — now calls the shared `chordPitchClasses()`.
- `lib/core/keymap.h/.cpp` — new `StrumKey`/`StrumOctave`/`StrumDuration`/`StrumVelocity`/
  `LimitedToggle`/`ClearEdit` actions; `resolve()` is now modifier-aware (Alt gates F2–F5);
  number-row/keypad usages resolve to `StrumKey`.
- `lib/core/state.h/.cpp` — added `selectedChord`/`selectedChordValid` (independent of Held
  latching) and `EditTarget` enum + `editTarget` field.
- `lib/engines/chord_engine.cpp` — records `selectedChord` on every successful resolve;
  ± guarded so it only steps chord octave when `editTarget == None`.
- `src/main.cpp` — wires StrumEngine; timing now uses `time_us_64()` (64-bit monotonic).
- `platformio.ini` — added `strum_engine.*` to the native `build_src_filter`.

### Design decisions (confirmed with user)

- Note pool anchored at `base_root_midi + 12 * strum_octave` (default octave 1 → C5).
- Strum param editing included in M4: `Alt+F2/F3/F4` arm octave/duration/velocity, `±`
  steps the armed param, `Alt+F5` toggles limited keys, `Esc` clears the edit target.
- Pool sized to the layout: full = 11 notes (numpad superset), limited = 10.
  Number row maps indices 0..9 into the 11-note full pool; numpad maps 0..10.

### Bugs found & fixed

| # | Symptom | Root cause | Fix |
|---|---------|-----------|-----|
| 1 | Strum notes occasionally held forever | `addSoundingNote()` reused the earliest-expiring slot when >16 notes overlapped, without sending its note-off | send `noteOff` for the evicted note before reusing the slot |
| 2 | (latent) deadlines stranded after ~71 min | `micros()` is 32-bit (`time_us_32()`), wraps at 2^32 µs | `src/main.cpp` now uses `time_us_64()` for all engine timestamps |
| 3 | Re-strummed notes kept the old (long) duration | duplicate `noteOn` sent with no intervening `noteOff` | re-articulate: `releaseNote()` sends `noteOff` before the new `noteOn` |

## Open bug — strum note-duration on numpad 0/. (RESOLVED 2026-08-14)

**Report:** with a short duration set (output shows ~50 ms, the minimum), strumming
numpad `1 2 3` releases correctly, but strumming `0` and `.` still sound like the old
(long) duration. Reported both when re-strumming while sounding and after full fade-out.
User says this is "the same behavior" as the original duration bug.

**MIDI monitor analysis (user-provided):** the captured output shows the opposite of the
report — every note (including `0`→74 and `.`→79 for the G-major pool at octave 1) gets a
note-on followed by a note-off ~50 ms later, all correctly paired on channel 2. There is
**no missing or late note-off**, and **no channel-1 (chord) output** in the log (suggesting
Silent/strum-only mode). Note `74` (D5) is *not* explained by chord overlap (chord voicing
would be {67,71,74} on ch1, but ch1 is silent in the log), and `79` (G5) is not in the chord.

So the firmware's note-off timing looks correct on the wire, yet the user hears a wrong
duration on those two keys. This needs deeper investigation before it can be fixed.

### Hypotheses / next steps (in order)

**Resolution:** Follow-up session (2026-08-14) added defensive fixes — a `MidiRouter::flush()` called once per loop (drains the UART FIFO after each MIDI burst) and richer `STRUM` diagnostic logging (index/note/channel/duration + edit-step values) — plus a regression test for numpad `0`/`.` at the 50 ms minimum. After re-flashing, the user could no longer reproduce the wrong duration. Root cause attributed to the UART FIFO/stranding (hypothesis 3) or a transient edit-state; the flush + instrumentation resolve it.

1. **Confirm the actual `note_duration_ms` value at test time.** The 50 ms note-offs
   imply the param is at (or clamped to) the 50 ms minimum. Ask the user to capture a log
   that includes the edit (press `Alt+F3`, then `-`) so we can see the value actually
   stored, and rule out an edit/stepping bug (e.g. accidental octave-edit arming).
2. **Reproduce the exact hardware sequence natively**, including the `Alt+F3` + `-` edit
   mechanism and realistic `time_us_64()` timestamps (existing tests pass with direct
   param sets and with the edit mechanism — see `test_strum_engine.cpp`).
3. **UART delivery.** `MidiOutUart::send()` never calls `flush()`; at the same millisecond
   the loop emits `noteOn 79` + `noteOff 74` back-to-back. Verify no byte drop / add a
   flush after a burst if the synth is missing note-offs (note: `MidiRouter` already logs
   before the UART write, so the monitor can look correct while bytes are lost).
4. **Synth-side voice behavior.** If 1–3 are ruled out, test the same sequence on a second
   synth/DAW and/or with a longer duration (not the 50 ms floor) to see if it is a
   minimum-note-length / envelope artifact on the target synth rather than firmware.
5. **Reconsider the re-articulation fix** (#3 above) — it was applied on the "duplicate
   note-on" theory; the user reports no change, so re-verify whether `releaseNote` is being
   hit in the real scenario (it only fires when the exact note is still active in `notes_`).

## Verification approach (unchanged)

- Native: GoogleTest for each core module; keep `pio test -e native` green.
- On-device: BOOTSEL drag-and-drop UF2 → `tools/pico-usb-to-wsl.ps1` → monitor 115200.

## Reference

- Roadmap: `KeybChord_Pico_Roadmap.md` (M4 at line 394).
- Spec: `KeybChord_Pico_Spec.md` §4.2 / §6.6 (strum), §5.2 / §5.5 (key map / controls).
- Prior summaries: `SESSION_SUMMARY_2026-08-06.md`, `2026-08-08.md`, `2026-08-12.md`,
  `2026-08-13.md` (M3 completion).

### Uncommitted

M4 code is **not yet committed** (see `git status`). Recommended next session: resolve the
open duration bug, then commit M4 (message style e.g. "M4: strum plate (...)").
