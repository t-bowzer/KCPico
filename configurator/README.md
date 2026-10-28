# KeybChord Configurator

A cross-platform (Windows, Linux, macOS) GUI for configuring the
[KeybChord Pico](../README.md) — an Omnichord-inspired MIDI chord instrument.
Edit presets, rhythm and bass patterns, the keymap, and global settings; back up
and restore your whole configuration; and write the result straight to the
device's mass-storage drive.

## Features

- **Presets** — browse and edit all 10 banks × 8 slots (chord / strum / bass /
  rhythm / drum sections).
- **Macros** — per-preset top-row number-key mode (strum or macro), optional
  macro names (default `M1`…`M9`, `M0`), and an ordered macro-command editor
  (set/toggle/cycle/inc/dec, per-drum mute, and arbitrary MIDI messages
  including SysEx), with the firmware's per-key and per-preset limits enforced.
- **MIDI command libraries** — insert named multi-message commands from JSON
  libraries, including bundled Roland JD-Xi program, tone, and control
  libraries.
- **Rhythm editor** — step-grid editor for drum patterns (tracks × 16th-note
  steps) with velocity editing.
- **Bass editor** — step-grid editor for bass patterns (chord-degree + sustain).
- **Keymap editor** — map non-reserved keys and combos to parameters, with live
  validation and coverage checking, plus a live ANSI 104-key overlay of the
  keymap (fixed chord/strum maps, modifier layers, click-to-select / double-click
  to edit, and PNG/SVG export). A synced preset selector shows the top row as
  strum or per-preset macros, with the macro editor embedded in the same tab.
- **JSON validator** — validate every file against the same rules the firmware
  enforces (bounds, enums, reserved keys, required keymap coverage).
- **Backup / restore** — save the whole configuration to a `.zip`, or restore
  from one.
- **Save to device or folder** — write directly to a mounted **"KeybChord"**
  drive, or any user-chosen directory, and optionally eject the drive after.
- **Startup device detection** — when a KeybChord volume is already mounted,
  the configurator offers to open it when it starts.

## MIDI macros and command libraries

Macro keys can send arbitrary MIDI in addition to changing KeybChord
parameters. Use **Add MIDI...** in the macro editor to build a message
visually, or pick a named command from the **MIDI libraries** panel and press
**Add to Macro** (double-click also works). Use **Edit** to adjust an inserted
MIDI command later.

Before adding a library command, set **Insert channel** in the MIDI libraries
panel. The selected channel is applied to every channel-voice message in that
library command as it is inserted (SysEx and raw byte commands are unaffected).
For example, select **16** before adding a JD-Xi command if the JD-Xi's
`Prog Rx/Tx Ch` is 16. Changing the selector does not modify commands that are
already in the macro.

The MIDI command editor supports note on/off, control change, program change,
pitch bend, channel pressure, poly pressure, SysEx, and raw hex bytes. The
optional **Delay** is cumulative within a macro: it is added to the delay of
the preceding MIDI command, so a sequence can space messages over time. The
firmware sends MIDI commands to both DIN and USB MIDI outputs.

### Library locations

Built-in libraries live in `src/keybchord_configurator/libraries/` and are
included in packaged builds. User libraries are loaded from:

- every directory in the `KEYBCHORD_LIBRARIES` environment variable
  (`os.pathsep` separated);
- Windows: `%APPDATA%\KeybChord\libraries`;
- macOS: `~/Library/Application Support/KeybChord/libraries`;
- Linux: `$XDG_DATA_HOME/keybchord/libraries` (default
  `~/.local/share/keybchord/libraries`);
- `libraries/` and `configurator/libraries/` under the current working
  directory.

Press **Refresh** after adding or changing library files. Libraries with the
same `name` are de-duplicated; the last one loaded wins.

### Bundled Roland JD-Xi libraries

The bundled files are generated from Roland's *JD-Xi MIDI Implementation*
(Version 1.00, May 1, 2015):

| File | Contents |
|------|----------|
| `roland-jd-xi-programs.json` | Preset banks A–D and user banks E–H (64 programs each). |
| `roland-jd-xi-extra-programs.json` | Extra banks S–Z (64 programs each). |
| `roland-jd-xi-tones.json` | SuperNATURAL synth tones 001–256, analog synth tones 001–064, and drum kits 001–033. |
| `roland-jd-xi-controls.json` | Channel-mode messages, performance CCs, effect levels, SuperNATURAL/analog/drum NRPN controls, and common SysEx commands. |

Program and tone commands send Bank Select MSB, Bank Select LSB, then Program
Change, with a **25 ms delay** before each of the latter two messages. The
JD-Xi needs that settling time before it accepts the program change. Commands
default to **MIDI channel 1** (the JD-Xi factory Program Control Channel) and
device ID **17 (`0x10`)**. Set **Insert channel** to your JD-Xi's
`Prog Rx/Tx Ch` before adding the command (for example, **16**). If your JD-Xi
uses a different device ID, edit the inserted SysEx command after adding it.

### Library file format

A library is a single JSON object with one or more groups of commands. Each
command expands into one or more macro commands when inserted.

```json
{
  "schema": 1,
  "name": "My Device",
  "device": "My Synth",
  "description": "Optional notes",
  "source": "Where these values came from",
  "groups": [
    {
      "name": "Programs",
      "commands": [
        {
          "name": "Piano",
          "messages": [
            {"type": "cc", "channel": 1, "controller": 0, "value": 85},
            {"type": "cc", "channel": 1, "controller": 32, "value": 64},
            {"type": "program_change", "channel": 1, "program": 0}
          ]
        }
      ]
    }
  ]
}
```

Message `type` is one of `note_on`, `note_off`, `cc`, `program_change`,
`pitch_bend`, `channel_pressure`, `poly_pressure`, `sysex`, or `raw`.
Channels are `1`–`16`; notes, controllers, and values are `0`–`127` (program
`0` is program 1 on the device). For `sysex` and `raw`, provide `bytes` as an
array of `0`–`255` integers. A message may also set `delay_ms` (0–10000).

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
