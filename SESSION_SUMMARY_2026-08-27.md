# KeybChord Pico — Session 2026-08-27 Summary

## Goal: Configurable keymap + menu rework, bug fixes, voicing modes, preset naming

Make all main-menu shortcuts data-driven via a boot-loaded `/keymap.json`,
rework the default F1–F8/menu layout, invert menu navigation, add directional
voicing modes + a voicing reset control, and add preset naming with LCD display.
Plus two follow-up bug fixes (stuck chord on combo release; arp sequence staleness).

## Result: SUCCESS

- **Native tests:** `pio test -e native` → **370/370 passing**.
- **Pico build:** `pio run -e pico` → SUCCESS (RAM 12.7%, Flash 14.2%).

## 1. Configurable keymap (`/keymap.json`)

- Replaced the flat `ActionType` enum with a parameterized `KeyCmd`/`KeyAction`
  (`lib/core/keymap.h`); moved `ParamId` from `param_edit.h` into `params.h`.
- New `lib/core/keymap_config.*`: parses/validates `/keymap.json` (key/param/value
  name tables, `KeymapConfig::defaults()`, `keymapDefaultJson()`). Validation:
  malformed JSON, reserved-key rebinding, duplicate keys, unknown names, missing
  required coverage (5 menus + preset prev/next/save) → error list.
- `KeymapResolver` is table-driven; reserved keys (chord grid, strum, backtick,
  Esc, arrows, Enter/Backspace) stay hardcoded, as do `Super+Esc` (panic) and
  `Super+1..8` (preset load).
- On validation failure: write `/keymap_error.log`, show a persistent LCD error
  (`Keymap error / Remount to fix`), and **halt** (input disabled; MSC boot-key
  still works so the log can be read). `defaults.cpp` provisions `/keymap.json`.

## 2. Default layout + menu rework

- F1–F4 = cycle chord mode / arp mode / bass pattern / rhythm pattern.
- F5–F8 = toggle rhythm / rhythm mute / bass / clock.
- F9–F12 = menus; `Super+F11` = Drum menu.
- Menu navigation **inverted**: Up/Down select the parameter, Left/Right change
  the value.
- Ctrl/Alt combos (user-specified): voicing/inversion/arp/roll/beat-LED, chord
  velocity ±, strum layout/mode/scale/root, bass octave ± (Alt+=/-), and
  Alt+F1–F12 = dynamic drum mute (mutes the Nth track of the current pattern,
  restores the pre-mute velocity; LCD shows `<Drum> Mute On/Off`).

## 3. Bug fixes (found in feedback)

- **Stuck chord / dead chords after a combo:** reserved play keys (chord/strum/
  backtick/extensions) now resolve to their action while Ctrl/Alt is held, so a
  chord/strum key released mid-combo still registers. Only Super keeps them inert.
- **Arp sequence staleness:** added `ChordEngine::onArpModeChanged()` (adopts the
  new mode + rebuilds the step sequence, wired via `setArpModeChangedCallback`)
  and a defensive rebuild in `stepArpeggio` (no divide-by-zero out of Random).

## 4. Voicing modes Down / Up + reset

- `VoicingMode` → `{RootPosition, Smart, Down, Up}`. New directional voice-leading
  (`voiceDown`/`voiceUp`): nearest voicing whose notes are all ≤ (down) / ≥ (up)
  the previous note-wise, walking across octaves; falls back to smart/root.
- **Voicing reset = `Backspace` from the main screen:** `ChordEngine::resetVoicing()`
  clears `prevVoicing_` so the next chord returns to root position at the
  configured octave (held/sounding chord untouched).

## 5. Preset naming

- `PresetSlot.name` (JSON `name`) is now surfaced: `presetDisplayName()` falls
  back to `B#:P#` for `"Default"`/empty.
- Save flow (`Insert` → `Enter`) continues into a **Rename** text-entry: chord-grid
  keys type letters (`keymapCharForUsage`), Backspace deletes, Enter commits,
  Esc cancels. Stored in `/presets/bank*.json`.
- LCD top-left shows the preset name while browsing the cursor and, in the main
  view, after 3 s with no chord (`state_.lastChordUs`).

## Modified / new files

- New: `lib/core/keymap_config.*`, `test/test_keymap_config.cpp`,
  `docs/keymap-config-test-plan.md`.
- Modified: `keymap.*`, `params.h`, `param_edit.*`, `voicing.*`, `naming.cpp`,
  `presets.*`, `state.*`, `defaults.cpp`, `chord_engine.*`, `strum_engine.cpp`,
  `edit_engine.*`, `preset_engine.*`, `rhythm.*`, `rhythm_engine.*`,
  `display_manager.*`, `src/main.cpp`, and most test files.

## Verification approach

- Native: GoogleTest; `pio test -e native` green (370).
- On-device: `pio run -e pico` → `firmware.uf2`; follow
  `docs/keymap-config-test-plan.md` (14 sections covering the new keymap,
  validation/halt, drum mute, voicing modes/reset, and preset naming).

## Next steps

1. Flash `firmware.uf2` and run `docs/keymap-config-test-plan.md` on hardware.
2. Confirm the name-edit text entry and the 3-second idle name display on the
   real LCD (letter case/spacing may want tuning).
