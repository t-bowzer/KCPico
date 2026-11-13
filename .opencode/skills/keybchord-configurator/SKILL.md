---
name: keybchord-configurator
description: Use when changing the KeybChord PySide6 configurator under configurator/ - models, validation, storage, UI editors, keymap diagram, backup, or pytest. Covers layout, the firmware-mirror contract, and change workflow.
---

# KeybChord configurator

## Commands

```bash
cd configurator && python run.py             # run from a checkout
cd configurator && pip install -e ".[dev]"  # editable install + pytest
cd configurator && pytest                   # model/IO/validator tests
```

Packaging is via `pyinstaller packaging/keybchord-configurator.spec`; do not
read `build/` or `dist/`.

## Layout

- `app.py` — QApplication entry point.
- `main_window.py` — tabs and menu actions; owns the `ConfigTree`.
- `models/` — firmware-mirroring domain models:
  `config.py`, `preset.py`, `keymap.py`, `macro.py`, `sequence.py`,
  `rhythm.py`, `bass.py`, `chordpattern.py`, `bounds.py`, `enums.py`,
  `defaults.py`.
- `validation/validator.py` — strict JSON validation and warnings.
- `io/storage.py` — on-disk layout, `ConfigTree`, load/save/validate helpers.
- `io/device.py` — mounted "KeybChord" drive discovery and eject.
- `io/backup.py` — zip backup/restore; `io/libraries.py` — MIDI libraries.
- `ui/` — PySide6 editors and views. `ui/widgets.py` has the shared
  `int_spin`, `bool_check`, and `enum_combo` helpers.
- `tests/` — models, IO, validator, and keymap-visual tests.

## Firmware-mirror contract

- Model fields, defaults, and JSON spellings must match the firmware loaders.
  `from_dict()` is lenient like the firmware; `validation/validator.py` is
  stricter and reports errors vs warnings.
- `models/bounds.py::PARAM_SPECS` is the shared source for UI widgets and
  validator metadata. Add bounds there when adding a parameter.
- Preset JSON and keymap/macro JSON use different enum spellings; keep both
  tables in sync.
- Pattern counts (12 rhythms, 9 bass, 9 chord patterns, max 32 each) live in
  both firmware headers and `models/bounds.py`.
- Pattern-file limits mirror the firmware: per-kind 32 cap, combined 72 KB
  rhythm/bass/chord budget (48 KB user share), and 8/4/4 KB per-file caps.
  On-device-created `userN.json` files use the same schemas.
- Keymap menu names include `chord_pattern`, `rhythm_pattern`, and `bass_pattern`
  for the pattern editors. The default keymap mirrors the firmware remap:
  `F9`=Chord, `F10`=Rhythm, `F11`=Strum, `F12`=Bass, `Super+F9/F10/F12`=pattern
  editors, `Super+F11`=Drum, `Ctrl+F12`=Seq Edit.
- Update both sides of the mirror whenever either side changes.

## Adding or changing a field

1. Update the model dataclass/default and `from_dict()`/`to_dict()`.
2. Update bounds, enums, and keymap tables as applicable.
3. Update `validation/validator.py`.
4. Update the editor (`ui/presets_editor.py`, `config_editor.py`, or a
   pattern/sequence editor) in `_build*`, `load()`, and `store()`.
5. Add tests in `tests/test_models.py` and `tests/test_validator.py`.
6. Mirror the change in firmware and docs; load `keybchord-add-param` for the
   full cross-repo checklist.

## UI conventions

- Editors emit `changed` when the user edits something.
- `load(model)` populates widgets; `store(model)` writes them back.
- `main_window.py` connects tabs to `ConfigTree` and marks the document dirty.
- Keymap display labels live in the keymap editor/visual modules; when adding
  an action, menu name, or parameter, update its label tables and check
  `tests/test_keymap_visual.py`.
- Pattern budget/count validation belongs with the validator; update its tests
  when the firmware limits change.

## IO and validation

- `io/storage.py` defines the on-disk layout: `config.json`, `keymap.json`,
  `presets/bankN.json`, `sequences/`, `rhythms/`, `bass/`, `chordpatterns/`.
- `load_tree()` is lenient (missing/bad files fall back); `save_tree()` writes
  the device layout.
- `default_tree()` seeds built-ins; `validate_tree()` and
  `validate_directory()` back the Validate tab.
- `tests/test_io.py` covers save/load round-trips, backup/restore, legacy bank
  detection, and bundled JD-Xi library validity.

## Token discipline

- Start with `rg -n` and read the relevant model or editor only.
- Do not read `libraries/roland-jd-xi-*.json` unless the task is about JD-Xi
  data. Do not read `build/`, `dist/`, `__pycache__/`, or caches.
- The firmware side is authoritative for behavior; the configurator mirrors it.
