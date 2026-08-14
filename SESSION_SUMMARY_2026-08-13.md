# KeybChord Pico — Session 2026-08-13 Summary

## Goal: M3 — Chord Engine (roadmap line 389; spec §12.3)

Implement the full chord engine on top of the M2 core framework, verify the pure
logic natively, then iterate on real hardware feedback until the chord-playing
behavior feels correct.

## Result: SUCCESS — M3 complete

- **Native tests:** `pio test -e native` → **91/91 passing** (was 41 at M2; +50 new).
- **Pico build:** `pio run -e pico` → **SUCCESS** (RAM 12.1%, Flash 8.0%).
- **On-device:** user confirmed chords, combinations, play modes, extensions,
  voicing, octave, and latching all behave correctly on the synth.

### New files (19)

```
keybchord/lib/core/
├── keymap.h/.cpp        # KeymapResolver: HID usage + modifier -> action (36-key grid, backtick, F1/F5/arrows/±)
├── chords.h/.cpp        # 11 interval formulas, combination matrix, root order, extensions, flat-spelling names
└── voicing.h/.cpp       # root-position & smart voice-leading (integer math), C1..C7 root clamp
keybchord/lib/engines/
└── chord_engine.h/.cpp  # five play modes, note lifecycle, Held-mode latching, release debounce
keybchord/test/
├── test_keymap.cpp, test_chords.cpp, test_combinations.cpp, test_voicing.cpp,
├── test_extensions.cpp, test_latching.cpp, test_chord_engine.cpp
├── chord_engine_fixture.h   # shared fixture (recording MIDI out + router + engine)
└── recording_midi_out.h     # test spy capturing outgoing MIDI messages
```

### Modified files

- `lib/core/state.h/.cpp` — added `snapshotChord()` (pending→active chord only).
- `lib/core/midimsg.h` — **two MIDI bug fixes** (see below).
- `lib/engines/midi_router.cpp` — wired `logMidiOut()` into `send()` (enabled the
  on-device MIDI monitor; was defined but never called).
- `src/main.cpp` — replaced the M2 `sendTestNote()` scaffold with `ChordEngine`.
- `platformio.ini` — added `chord_engine.*` to the native `build_src_filter`.

### Bugs found & fixed during hardware testing (user-reported)

| # | Symptom | Root cause | Fix |
|---|---------|-----------|-----|
| 1 | Combo release glitch: releasing one key of a combo re-triggered the remaining key | release re-resolved immediately | press-driven re-resolution + 25ms release debounce (`RELEASE_BUFFER_US`) |
| 2 | Octave limited to C3–C5; "hidden" drift when pressing ± | `fitRange` collapsed voicings into [48,84] | full C1–C7 root range via `clampRootOctave`; edge-clamped |
| 3 | Smart voicing always walked downward | anchored to `previous[0]` | anchor to note-range midpoint (center), minimize movement |
| 4 | Arrow keys (add9/11/13) did nothing | `ChordParams` extensions never merged into the resolved chord | merge `chord.add9/11/13 \|= p.add9/11/13` |
| 5 | Held mode: both chords stacked, notes stuck forever after unplugging MIDI | note-off sent as "Note On vel 0" (`0x90`) — ignored by some synths; plus channel off-by-one | true Note Off (`0x80`) + 1-indexed `channelStatus` |
| 6 | Held chord lingered when switching play modes | mode cycle never released the latched chord | on mode cycle out of Held/Rhythm with no key held, release chord |

### Behavior notes (locked in after hardware iteration)

- **Held mode latches:** notes are released only when another chord is played
  (or the mode is switched away with no key held) — never on key-up.
- **Release debounce = 25ms** (was 50ms; user halved it after testing).
- **Momentary modes** (PressToPlay/Arpeggio) still debounce releases; arpeggio
  free-runs on `note_duration_ms` and loops while held. Rhythm mode behaves as
  Held until the M5 clock arrives.
- **Roots span C1–C7** (octave param −3…+3 from base C4), clamped at the edges.

### Acceptance criteria covered

AC-1, AC-2, AC-3, AC-11, AC-12, AC-13, AC-14, AC-15 (per roadmap §6.1).

---

## Next Steps: Resume Plan

### 0. Commit (done)
M3 committed as `70c9f1a` and pushed to `github.com:t-bowzer/KCPico`.

### 1. M4 — Strum Plate (roadmap line 394; spec §4.2 / §6.6)
- **Note-pool derivation** from the active chord voicing (including Silent mode).
- **Full + limited numpad layouts**, Num-Lock-independent via raw keypad usages
  (`Keypad 0–9`, `.`, `/`, `*`; `Keypad +/−` reserved for octave/value).
- **Strum params** (octave, note duration, velocity) and **immediate edit pickup**
  (no latching, per FR-S5).
- Deliverables: `lib/core/strum.*`, `lib/engines/strum_engine.*`, wire number-row
  + numpad keys through `KeymapResolver` (currently unmapped → `ActionType::None`).

### 2. M5 — Rhythm Engine (roadmap line 399; spec §4.3 / §7)
- Core 1 monotonic scheduler/clock; JSON pattern playback on ch10; integer tempo /
  fixed-point swing; mute; MIDI clock 24 PPQN toggle; arp/rhythm chord sync;
  Scroll Lock BPM LED (FR-R8). Revisit Rhythm play mode (currently Held-equivalent).

### 3. M6 — Display Manager, then M7 Presets & Banks, M8 Polish
- Keep the USB-CDC debug log / MIDI monitor as the on-device instrument through M5.

### Verification approach (unchanged)
- Native: GoogleTest for each core module; keep `pio test -e native` green.
- On-device: BOOTSEL drag-and-drop UF2 → `tools/pico-usb-to-wsl.ps1` → monitor 115200.

### Reference
- Roadmap: `KeybChord_Pico_Roadmap.md` (M4 at line 394).
- Spec: `KeybChord_Pico_Spec.md` §4.2 / §6.6 (strum), §4.3 / §7 (rhythm).
- Prior summaries: `SESSION_SUMMARY_2026-08-06.md`, `2026-08-08.md`, `2026-08-12.md`.
