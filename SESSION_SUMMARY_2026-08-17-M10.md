# KeybChord Pico — Session 2026-08-17 (M10) Summary

## Goal: M10 — Chord Engine upgrades & Walking Bass (+ strum & drum upgrades)

Complete the roadmap M10 scope plus the Upgrade-Plan strum and rhythm-drum
items: chord roll, minimum note count, minimum-interval spread, manual
inversions, arpeggio modes, held extensions, a dedicated walking-bass engine,
strum mode selection (follow-chord/scale/piano) with duration-as-minimum,
per-drum-piece code + velocity editing, and the keymap consolidation.

## Result: SUCCESS

- **Native tests:** `pio test -e native` → **287/287 passing** (was 261).
- **Pico build:** `pio run -e pico` → SUCCESS (RAM 12.7%, Flash 12.3%).

## Chord engine upgrades

- `ChordParams` gained `chord_roll_ms` (−400..+400, step 5), `min_notes`
  (2..6, default 3), `min_interval` (0..12, 0 = off), `inversion`
  (root/1st/2nd/3rd), and `arp_mode` (up/down/up_down/alternating/random).
  `add9/11/13` removed from `ChordParams`; `PlayMode::Rhythm` removed (4 modes).
- New `voiceChord()` pipeline in `lib/core/voicing.*`: base voicing (root/smart)
  → manual inversion rotation → min-interval spread (permutation search for the
  tightest ascending configuration) → min-note octave padding. All integer.
- `chord_engine.*` rewritten: held `Left`/`Down`/`Right` latch add9/11/13 onto the
  sounding chord (re-triggers, including a latched Held chord); chord-roll
  staggered note-ons (positive asc / negative desc, note-offs together); five arp
  modes with a seeded LCG for Random; `Rhythm` branches removed.

## Walking bass (new `BassEngine`)

- New `lib/core/bass.*`: 11-type interval blueprint (spec §6.8) + meter-adaptive
  `bassOffsetForBeat`/`bassNote` (3/4 uses root-3rd-5th).
- New `lib/engines/bass_engine.*`: fires on rhythm beat edges, monophonic
  percussive note (own octave/duration/velocity/channel), silent when rhythm is
  stopped or in Silent chord mode. Wired in `main.cpp`; included in panic/cancel.
- `BassParams` + Bass Edit menu (`F12`) + preset `bass` block.

## Strum upgrades

- `StrumMode` follow-chord / scale / piano with selectable `root_pc` + `scale_type`
  (12 scales/modes incl. pentatonic + blues). `buildScalePool`/`buildPianoPool`
  added to `strum.*`.
- Duration is now a **minimum**: held strum keys sustain past `note_duration_ms`
  (release tracked per key usage); taps still sound the full minimum.

## Rhythm drum edits

- `DrumMap` gained per-piece velocity (`kick_vel`/`snare_vel`/`hihat_vel`/
  `open_hat_vel`, 0 = follow pattern); `mapDrumVelocity()` applied in
  `rhythm_engine::fireStep`.
- Drum Edit sub-menu (`F11` → `F8`) with per-piece note code + velocity and a
  one-shot audition (wired through a new `EditEngine` drum-audition callback).

## Keymap consolidation

- Menus: `F9`(Chord) `F10`(Strum) `F11`(Rhythm) `F12`(Bass). Hotkeys:
  `F1` play mode, `F2` voicing, `F3` bass, `F4` beat LED, `F5` rhythm on/off,
  `F6` clock, `F7` pattern, `F8` mute. `PrtSc`/`ScLk`/`Pause` = 1st/2nd/3rd
  inversion. `Left`/`Down`/`Right` = held extensions. `Menu`/`Ctrl`/`Alt` no
  longer map to anything at runtime. Boot key moved to `Ctrl` (modifier byte).

## Serialization & backward compatibility

- `presets.cpp` round-trips the new chord/strum/bass/drum fields; the old
  `extensions` object is ignored on load; legacy `play_mode` int remapped
  (3 Rhythm → Arpeggio, 4 Silent → Silent). `defaults.cpp` picks up new fields
  via `PresetSlot::defaults()`.

## Modified / new files

- New: `lib/core/bass.*`, `lib/engines/bass_engine.*`,
  `test/test_arp_modes.cpp`, `test/test_bass.cpp`, `test/test_bass_engine.cpp`,
  `docs/hardware-test-plan.md`.
- Modified: `params.*`, `presets.*`, `param_edit.*`, `keymap.*`, `voicing.*`,
  `strum.*`, `rhythm.*`, `state.*`, `naming.cpp`, `chord_engine.*`,
  `strum_engine.*`, `edit_engine.*`, `preset_engine.cpp`, `rhythm_engine.cpp`,
  `main.cpp`, and the corresponding `test/*` files.
- Docs: `KeybChord_Pico_Spec.md` (§4.2/§4.3/§5.5/§5.6/§8.2/§9) and
  `KeybChord_Pico_Roadmap.md` (M10 completion note) updated.

## Verification approach

- Native: `pio test -e native` green (287).
- On-device: `pio run -e pico` → `firmware.uf2` (BOOTSEL) → monitor 115200.
  Manual checks in `docs/hardware-test-plan.md`.

## Next steps

1. Run the `docs/hardware-test-plan.md` checklist on assembled hardware.
2. Confirm the strum scale/piano mode + drum audition feel (draft items from
   Upgrade-Plan), and tune the LCD line-2 layout if needed.
