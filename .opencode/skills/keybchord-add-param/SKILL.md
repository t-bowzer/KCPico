---
name: keybchord-add-param
description: Use when adding or changing an editable parameter, ParamId, preset field, keymap/macro parameter, config setting, or edit-menu entry in KeybChord. Provides the exact firmware, configurator, docs, and test checklist.
---

# Adding a KeybChord parameter

A parameter is intentionally spread across firmware, configurator, docs, and
tests. Work through the layers below instead of exploring them.

Critical detail: in `param_edit.cpp`, `kShortName[]` and `kFullName[]` are
positional arrays indexed by `ParamId`. Inserting a `ParamId` in the middle
requires inserting matching entries at the same indices. Appending immediately
before `ParamId::COUNT` is usually safer.

## 1. Firmware data model

- `keybchord/lib/core/params.h`
  - Add the `ParamId` before `COUNT`.
  - Add the field to `ChordParams`, `StrumParams`, `RhythmParams`,
    `BassParams`, `GlobalParams`, or `DrumMap`.
  - Add bounds to `namespace param_bounds` when the value is numeric.
  - Add a new enum and `COUNT` when the parameter is enumerated.
- `keybchord/lib/core/params.cpp`
  - Set the default in the matching `*Params::defaults()` function.
- `keybchord/lib/core/presets.h` / `presets.cpp`
  - If it is stored per preset: update `sameParams()`, `loadPreset()`,
    `savePreset()`, and `makePreset()` as needed.
  - Preserve JSON back-compat by treating missing fields as defaults.

## 2. Firmware editing/menu plumbing

`keybchord/lib/core/param_edit.cpp`:

- Add the id to `kChordParams`, `kStrumParams`, `kRhythmParams`, `kBassParams`,
  `kDrumParams`, or `kSequenceParams` if it appears in an edit menu.
- Update `kShortName[]` and `kFullName[]` at the same positional index.
- Update each applicable switch:
  - `paramValueString()`
  - `paramStep()`
  - `paramCycle()` when a bool/enum cycle differs from `+1`
  - `isAutoRepeatable()` for held-key repeat
  - `paramValueInt()`
  - `paramSet()`
- `paramToggle()` is generic; only touch it for special behavior.

`keybchord/lib/core/keymap_config.cpp`:

- Add the keymap/macro name to `kParamNames[]`.
- Add the id to `isBoolParam()` if boolean.
- Extend `parseValue()` and the `ValueName` table for enum spellings.
- If a default binding should ship, update `KeymapConfig::defaults()` and
  `keymapDefaultJson()`.
- For a new `open_menu` target, add the `EditMenu` value, the
  `lookupMenuName()` entry, defaults/validation, and the configurator
  `EDIT_MENU_JSON` / keymap editor / validator mirror.
- `macros.cpp` reuses the keymap parameter names; it only needs a separate
  change for drum pieces or special value handling.

## 3. Firmware runtime/wiring

- `keybchord/lib/core/state.h` / `state.cpp`
  - Add `pending*`/`active*` fields only if runtime state is needed.
  - Update `snapshotActive()`, `snapshotChord()`, or `loadDefaults()`.
- Update the engine that consumes the value.
- Update `src/main.cpp` only if the value needs new wiring or callbacks.
- `keybchord/lib/core/defaults.cpp`
  - Update embedded default preset/config/pattern JSON if shipped defaults
    should include the field.
- If adding a new source file to an engine/hw area, update the native env's
  explicit `build_src_filter` in `keybchord/platformio.ini`.

## 4. Configurator mirror

Do not stop at the firmware; the configurator intentionally mirrors it.

- `configurator/src/keybchord_configurator/models/bounds.py`
  - Add numeric bounds and a `PARAM_SPECS` entry. Drum-only params also live
    here even though they are not keymap-bindable.
- `models/enums.py`
  - Add enum values plus preset-JSON and keymap-short spelling tables.
- `models/keymap.py`
  - Update `KEYMAP_PARAM_NAMES`, `BOOL_PARAMS`, `PARAM_VALUE_TABLE`, and
    `default_keymap()` when applicable.
- `models/preset.py` or `models/config.py`
  - Update defaults, `from_dict()`, and `to_dict()`.
- `validation/validator.py`
  - Add strict range/enum/required-field rules and warnings.
- `ui/presets_editor.py`, `ui/config_editor.py`, or the relevant editor
  - Add widgets to `_build*`, then update `load()` and `store()`.
- `models/defaults.py`
  - Update built-in rhythm/bass/chord-pattern data if the parameter affects it.
- Tests: `configurator/tests/test_models.py`,
  `configurator/tests/test_validator.py`, and any focused UI/diagram test.

## 5. Docs and verification

- `docs/custom-configuration-guide.md`: preset/config schema, keymap parameter
  and value tables, and the bounds table.
- `docs/KeybChord_Pico_Spec.md`: data model and parameter reference sections.
- `README.md`: only when the feature is user-facing.
- Run:

```bash
cd keybchord && pio test -e native
cd configurator && pytest
```

Remember: preset JSON and keymap/macro JSON use different enum spellings.
Preset example: `press_to_play`, `root_position`, `follow_chord`. Keymap
example: `press`, `root`, `chord`.
