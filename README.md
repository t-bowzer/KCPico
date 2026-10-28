# KeybChord Pico

An Omnichord-inspired MIDI chord instrument for the **Raspberry Pi Pico
(RP2040)**. It hosts a standard USB keyboard as the chord grid + strum plate,
synthesizes chords, arpeggios, rhythm/drums and a walking bass, and sends MIDI
out a DIN-5 connector — no computer required.

## Features

- **Chord engine** — Omnichord-style 3×12 chord grid (major / minor / seventh)
  with combinations, extensions (add9/add11/add13 via arrow keys), inversions,
  and voicing modes (root / smart / down / up).
- **Play modes** — Held, Press-to-play, Arpeggio, **Arp-hold** (latched
  arpeggio), **Pattern**/**Pattern-hold** (user chord patterns, below), and
  Silent, with 5 arpeggio patterns (up / down / up-down / alternating / random)
  and chord roll.
- **Strum plate** — top-row number keys + numpad, with follow-chord / scale /
  piano note pools. Per preset, the top row can instead become **macro keys**.
- **Macros** — each preset can map the top-row keys `1`–`0` to named, ordered
  bundles of parameter changes (set/toggle/cycle/inc/dec and per-drum mute) and
  arbitrary MIDI messages, including commands from JSON libraries.
- **Chord patterns** — 9 built-in polyphonic patterns (Block, Alberti, Pulse,
  …) played in lockstep with the rhythm, all editable as JSON (`/chordpatterns/`).
- **Rhythm engine** — 12 built-in drum patterns (Rock, Waltz, Swing, Bossa Nova,
  …) plus user-loadable patterns, tempo, swing, mute, MIDI clock out, and a
  keyboard-LED beat indicator.
- **Walking bass** — beat-synced patterns (walking, whole, half, quarter, hold,
  …) following the active chord, all editable as JSON.
- **Presets** — 10 banks × 8 slots (80 presets) with on-device naming.
- **DIN-5 MIDI OUT** — 3.3 V loop, standards-compliant per the MIDI 1.0 spec.
- **USB MIDI OUT** — composite USB MIDI device on the native port (alongside the
  debug serial), toggleable via `Ctrl+F8` / `midi.usb_enabled`.
- **LCD1602** status/parameter display over I2C.
- **Fully configurable** — `config.json`, `keymap.json`, presets, rhythms, and
  bass patterns are all editable JSON on a USB Mass Storage drive.

## Hardware

| Part | Notes |
|------|-------|
| Raspberry Pi Pico (RP2040) | MIDI + I2C + native USB device/debug |
| CH559L USB-host coprocessor | Hosts the keyboard; UART bridge on `GP0`/`GP1` |
| USB-A receptacle (keyboard host) | Wired to CH559 HP/HM via 22 Ω series |
| DIN-5 MIDI OUT | `GP8` (TX) via 10 Ω / 33 Ω from 3V3 |
| LCD1602 + PCF8574 (I2C) | `GP4`=SDA, `GP5`=SCL, 3V3 |

See **[docs/wiring.md](docs/wiring.md)** for the full connection diagram and
component notes.

## Building

Requirements: [PlatformIO](https://platformio.org/) (with the Raspberry Pi RP2040
platform).

```bash
cd keybchord
pio run -e pico          # builds keybchord/.pio/build/pico/firmware.uf2
```

The keyboard is hosted by the CH559 coprocessor, which runs its own firmware.
Build it with SDCC:

```bash
cd firmware/ch559
make                     # produces ch559.hex + ch559.bin
```

## Flashing

1. Hold the Pico's **BOOTSEL** button while plugging it into USB — it mounts as
   a mass-storage drive.
2. Drag `firmware.uf2` onto the drive; the Pico reboots and runs it.
3. Flash the CH559 firmware (`firmware/ch559/ch559.bin`) over its device USB
   port per [docs/wiring.md](docs/wiring.md#flashing-the-ch559).

No BOOTSEL button accessible (enclosed build)? Power off, hold **Esc** on the
connected keyboard, and power on — the firmware reboots straight into the
BOOTSEL drive, so you can drag `firmware.uf2` on without opening the enclosure.

On first boot the firmware self-provisions its config, presets, and rhythms.

## Using it

- Plug a standard USB keyboard into the host port. The three keyboard rows play
  chords (see the chord-grid mapping in the spec); the numpad is the strum
  plate. By default the top-row number keys also strum; a preset can switch them
  to macro keys instead.
- `F1`–`F8` are quick settings; `F9`–`F12` open the Chord/Strum/Rhythm/Bass
  menus. `Home`/`End` browse presets, `Insert`→`Enter` saves, `Space` is tap
  tempo, `Super`+`Esc` is panic.
- `Ctrl`+`Esc` shows the firmware version and current git commit (any key
  dismisses the debug screen).
- The keyboard's **Volume +/−** keys are fixed controls: on the main screen they
  change tempo; inside an edit menu they scroll parameters, then edit the
  selected value after `Enter`/`Mute`. **Mute** is also fixed and behaves like
  `Enter` (select/commit). Other media keys can be bound in `keymap.json`.
- In an edit menu, arrows/volume scroll; `Enter`/`Mute` selects a parameter.
  `Enter`/`Mute` again keeps the edited value, while `Backspace`/`Esc` restores
  the value from selection and returns to scrolling.
- To edit config/presets/rhythms, power off, hold **Ctrl**, power on — the
  "KeybChord" drive mounts on your PC. Ejecting the drive reboots KeybChord
  automatically and reloads the edited files.

## Customization

Everything is JSON. See **[docs/custom-configuration-guide.md](docs/custom-configuration-guide.md)**
for the complete reference (config, presets, keymap, and how to add your own
rhythm and bass patterns). Ready-to-copy examples are in
[examples/test_rhythm.json](examples/test_rhythm.json) and
[examples/test_bass.json](examples/test_bass.json).

For a point-and-click way to edit all of the above — presets, rhythm/bass
patterns, the keymap, and global settings, plus backup/restore and writing
straight to the device — use the cross-platform
**[KeybChord Configurator](configurator/README.md)** desktop app.

## Documentation

- [Product spec](docs/KeybChord_Pico_Spec.md) — full behavior and data models.
- [Wiring](docs/wiring.md) — connection diagram.
- [Custom configuration guide](docs/custom-configuration-guide.md) — JSON reference.
- [v0.1.0 roadmap](docs/v0.1.0-roadmap.md) — milestones and acceptance coverage.
- [v0.2.0 roadmap](docs/v0.2.0-roadmap.md) — current development plan.

## Development

Native unit tests (no hardware needed) via GoogleTest:

```bash
cd keybchord
pio test -e native
```

## License

[MIT](LICENSE)
