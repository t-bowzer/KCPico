---
name: keybchord-json
description: Use when reading, editing, validating, or explaining KeybChord on-device JSON - config.json, presets, keymap.json, rhythm/bass/chordpattern files, sequences, macros, or examples. Contains schemas, limits, enum spellings, and parser locations.
---

# KeybChord on-device JSON

The full reference is `docs/custom-configuration-guide.md`. Read only the
matching section (`rg -n '^#' docs/custom-configuration-guide.md` locates them).
The spec (`docs/KeybChord_Pico_Spec.md` sections 7-9) adds design detail.

## On-disk layout

| Path | Contents |
|---|---|
| `/config.json` | One global settings object. |
| `/keymap.json` | `{"bindings": [...]}`. A bad keymap halts the firmware. |
| `/presets/bank1.json` … `bank5.json` | Array of exactly 8 preset objects. |
| `/sequences/bNpM.json` | All 10 sequence slots for one preset. |
| `/rhythms/*.json` | 12 built-ins plus user files (`userN.json` on-device). |
| `/bass/*.json` | 9 built-ins plus user files (`userN.json` on-device). |
| `/chordpatterns/*.json` | 9 built-ins plus user files (`userN.json` on-device). |

Firmware parsers: `config.cpp`, `presets.cpp`, `keymap_config.cpp`,
`sequence.cpp`, `rhythm.cpp`, `bass.cpp`, `chordpattern.cpp`, `macros.cpp`.
Configurator mirrors: `models/config.py`, `preset.py`, `keymap.py`,
`sequence.py`, `rhythm.py`, `bass.py`, `chordpattern.py`, `macro.py`.

## `config.json`

```json
{
  "midi":    { "din_enabled": true, "usb_enabled": true, "clock_enabled": false },
  "chord":   { "base_root_midi": 60, "note_range": [48, 84] },
  "display": {
    "revert_timeout_ms": 1500, "prompt_timeout_ms": 5000,
    "cursor_timeout_ms": 5000, "menu_timeout_ms": 10000
  },
  "led": { "bpm_indicator": true, "led": "num_lock", "flash_ms": 40 },
  "startup_preset": "B1:P1",
  "logging": { "debug_log": true, "midi_monitor": true }
}
```

Ranges: note values and note range 0-127; revert 250-5000 ms; prompt
1000-30000 ms; cursor/menu 500-30000 ms; flash 5-500 ms. `led` is
`num_lock`, `caps_lock`, `scroll_lock`, or `all`. Inverted `note_range` is
auto-swapped.

## Preset object (`/presets/bankN.json`)

Top-level keys: `name`, `seq_settings` (`"recorded"`/`"live"`),
`number_keys` (`"strum"`/`"macro"`), `macros`, `chord`, `strum`, `bass`,
`rhythm`.

| Section | Keys |
|---|---|
| `chord` | `play_mode`, `octave`, `note_duration_ms`, `velocity`, `pan`, `voicing_mode`, `chord_roll_ms`, `min_notes`, `min_interval`, `inversion`, `arp_mode`, `arp_rate`, `channel`, `pattern` |
| `strum` | `octave`, `note_duration_ms`, `velocity`, `limited_keys`, `mode`, `root_pc`, `scale_type`, `channel` |
| `bass` | `enabled`, `octave`, `note_duration_ms`, `velocity`, `channel`, `pattern` |
| `rhythm` | `enabled`, `pattern`, `tempo`, `swing`, `muted`, `channel`, `drums` |

Preset enum spellings: `held`, `press_to_play`, `arpeggio`, `arp_hold`,
`silent`, `pattern`, `pattern_hold`; voicing `root_position`, `smart`, `down`,
`up`; inversion `root`, `first`, `second`, `third`; arp mode `up`, `down`,
`up_down`, `alternating`, `random`; arp rate `whole`, `half`, `quarter`,
`eighth`, `sixteenth`, `thirty_second`; strum mode `follow_chord`, `scale`,
`piano`; scales include `ionian`, `dorian`, `phrygian`, `lydian`, `mixolydian`,
`aeolian`, `locrian`, `harmonic_minor`, `melodic_minor`, `major_pentatonic`,
`minor_pentatonic`, `blues`.

`rhythm.drums` has 13 pieces: `kick`, `snare`, `hihat`, `open_hat`, `rimshot`,
`clap`, `crash`, `ride`, `bongo`, `conga_lo`, `conga_hi`, `clave`, `shaker`,
each with a GM note (0-127) and `<piece>_vel` (0 = auto, 1-127 fixed,
128 = off). Defaults are 36, 38, 42, 46, 37, 39, 49, 51, 61, 62, 63, 75, 82.

## `keymap.json`

```json
{ "bindings": [
  { "keys": ["F1"], "action": { "type": "cycle", "param": "chord_mode" } },
  { "keys": ["Ctrl", "="], "action": { "type": "inc", "param": "chord_velocity" } },
  { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
  { "keys": ["Home"], "action": { "type": "preset_prev" } }
] }
```

- `keys`: exactly one non-modifier key, optionally one of `Ctrl`, `Alt`, or
  `Super`. Shift is a chord key, not a modifier. Media keys take no modifier.
- Action types: `open_menu`, `cycle`, `set`, `inc`, `dec`, `toggle`, `ext`,
  `drum_mute`, `preset_prev`, `preset_next`, `preset_bank_prev`,
  `preset_bank_next`, `preset_load`, `preset_save`, `preset_clear`,
  `tap_tempo`, `sequence_record`, `sequence_play`, `sequence_clear`.
- `open_menu` menu names: `chord`, `rhythm`, `strum`, `bass`, `drum`,
  `sequence`, plus the pattern editors `chord_pattern`, `rhythm_pattern`,
  `bass_pattern`. Defaults: `F9`=chord, `F10`=rhythm, `F11`=strum, `F12`=bass,
  `Super+F11`=drum, `Ctrl+F12`=sequence, `Super+F9/F10/F12`=pattern editors.
- Required coverage: `open_menu` for chord/strum/rhythm/bass/drum, plus
  `preset_prev`, `preset_next`, and `preset_save`.
- Keymap uses short enum names: `press`, `root`, `chord`, `arp_hold`, etc.
- Bool params accept `on`/`off`, `true`/`false`, `1`/`0`.
- Pattern params accept a loaded pattern name or an integer index.

## Pattern files

Rhythm:

```json
{ "name": "Rock 1", "short_name": "Rk", "steps_per_bar": 16, "swing": 0,
  "tracks": [{ "note": 36, "name": "kick",
               "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] }] }
```

Bass: `name`, `steps_per_bar`, `steps[]` with `-1` rest / `0` root / `1` 3rd /
`2` 5th / `3` 6th-or-7th, optional `sustain_steps[]`, optional `hold`.

Chord pattern: `name`, `steps_per_bar`, `steps[]` as bitmask `1` root,
`2` 3rd, `4` 5th, `8` 7th (combine bits), optional `sustain_steps[]`.

Counts and caps: rhythm 12 built-ins, bass 9, chord patterns 9; each total is
capped at 32. Built-ins keep low indices; user files are appended in
alphabetical filename order. A pattern with no tracks/steps is skipped.
On-device Pattern Edit writes compact `userN.json` with the same schema (no new
fields; `bars` is inferred from `steps.length / steps_per_bar`). Pattern files share a
combined 72 KB budget (48 KB user share), with per-file caps of 8 KB rhythm,
4 KB bass, and 4 KB chord. Built-ins may be overwritten; user patterns may be
renamed/deleted on device.

## Sequences (`/sequences/bNpM.json`)

```json
{ "version": 1, "sequences": [
  null,
  { "bars": 4, "steps_per_bar": 16,
    "recorded": [0,500,100,64,0,0,3,0,0,0,0,0,4],
    "live": 0,
    "events": [[0,0], [16,640]] }
] }
```

- 10 slots; unused slots are `null`.
- `bars` is 1/2/4/8; `steps_per_bar` is 4-64; max 64 events.
- `recorded` order: octave, note_duration_ms, velocity, pan, voicing (0-3),
  chord_roll_ms, min_notes, min_interval, inversion (0-3), arp_mode (0-4),
  pattern index, play_mode (0-6), arp_rate (0-5).
- `live` bit i set means use the live value for `recorded[i]`.
- `chord_id = root_pc * 128 + chord_type * 8 + extension_mask`; extension bits
  add9=1, add11=2, add13=4. Chord types: 0 major, 1 minor, 2 dom7, 3 maj7,
  4 min7, 5 dim, 6 dim7, 7 aug, 8 sus4, 9 sus2, 10 min7b5.
- Sequences and pattern libraries are compact JSON on purpose for the flash budget; do not pretty-print them. The configurator save path also writes patterns compactly.

## Macros (inside a preset)

`number_keys: "macro"` makes top-row `1`-`9`,`0` trigger macros; the numpad is
still strum. Macro keys are labelled `"1"`…`"9"`, `"0"`. Each macro has an
optional `name` (≤16 chars) and ordered `commands`.

Command types: `set` (`param`, `value`), `toggle` (`param`, `a`, `b`), `cycle`
(`param`), `inc`/`dec` (`param`), `drum_mute` (`piece`, `value`), `midi`
(`bytes`, optional `delay_ms`). Limits: 16 commands per key, 48 per preset,
256 MIDI bytes, 10000 ms delay.

## Verification

- `cd configurator && pytest` covers model round-trips, limits, and the strict
  validator; `tests/test_validator.py` is the best map of validation rules.
- Examples: `examples/test_rhythm.json`, `examples/test_bass.json`.
- Do not read the bundled `roland-jd-xi-*.json` libraries unless the task is
  specifically about JD-Xi MIDI data.
