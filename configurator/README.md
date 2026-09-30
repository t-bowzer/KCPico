# KeybChord Configurator

A cross-platform (Windows, Linux, macOS) GUI for configuring the
[KeybChord Pico](../README.md) — an Omnichord-inspired MIDI chord instrument.
Edit presets, rhythm and bass patterns, the keymap, and global settings; back up
and restore your whole configuration; and write the result straight to the
device's mass-storage drive.

## Features

- **Presets** — browse and edit all 10 banks × 8 slots (chord / strum / bass /
  rhythm / drum sections).
- **Rhythm editor** — step-grid editor for drum patterns (tracks × 16th-note
  steps) with velocity editing.
- **Bass editor** — step-grid editor for bass patterns (chord-degree + sustain).
- **Keymap editor** — map non-reserved keys and combos to parameters, with live
  validation and coverage checking.
- **JSON validator** — validate every file against the same rules the firmware
  enforces (bounds, enums, reserved keys, required keymap coverage).
- **Backup / restore** — save the whole configuration to a `.zip`, or restore
  from one.
- **Save to device or folder** — write directly to a mounted **"KeybChord"**
  drive, or any user-chosen directory, and optionally eject the drive after.

## Installing

Requires Python 3.9+.

```bash
pip install -e .            # core app (PySide6)
pip install -e ".[preview]" # optional synthesized rhythm/bass preview
pip install -e ".[dev]"     # tests + pyinstaller
```

Run it (after installing):

```bash
keybchord-configurator       # installed command (note the hyphen)
python -m keybchord_configurator
```

### Running without installing (Windows and others)

If you don't want to install the package, run it straight from a checkout:

```bash
pip install PySide6          # only the GUI dependency is needed
python run.py
```

On Windows you can just double-click **`run.bat`** — it installs PySide6 if
missing and launches the app.

> The package uses a `src/` layout, so `python -m keybchord_configurator`
> (and the installed commands) only work after `pip install -e .`, or when run
> via `run.py`/`run.bat`.

## Packaging standalone binaries

```bash
pip install pyinstaller
pyinstaller packaging/keybchord-configurator.spec
```

Binaries land in `dist/`.

## Development

```bash
pip install -e ".[dev]"
pytest
```

## License

[MIT](../LICENSE). Third-party dependencies (PySide6 LGPL, numpy, sounddevice)
are documented in [THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md).
