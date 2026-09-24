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
  arpeggio), and Silent, with 5 arpeggio patterns (up / down / up-down /
  alternating / random) and chord roll.
- **Strum plate** — the number row + numpad, with follow-chord / scale / piano
  note pools.
- **Rhythm engine** — 12 built-in drum patterns (Rock, Waltz, Swing, Bossa Nova,
  …) plus user-loadable patterns, tempo, swing, mute, MIDI clock out, and a
  keyboard-LED beat indicator.
- **Walking bass** — beat-synced patterns (walking, whole, half, quarter, hold,
  …) following the active chord, all editable as JSON.
- **Presets** — 10 banks × 8 slots (80 presets) with on-device naming.
- **DIN-5 MIDI OUT** — 3.3 V loop, standards-compliant per the MIDI 1.0 spec.
- **LCD1602** status/parameter display over I2C.
- **Fully configurable** — `config.json`, `keymap.json`, presets, rhythms, and
  bass patterns are all editable JSON on a USB Mass Storage drive.

## Hardware

| Part | Notes |
|------|-------|
| Raspberry Pi Pico (RP2040) | USB host + MIDI + I2C |
| USB-A receptacle (keyboard host) | `GP0`=D+, `GP1`=D- (22 Ω series) |
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

## Flashing

1. Hold the Pico's **BOOTSEL** button while plugging it into USB — it mounts as
   a mass-storage drive.
2. Drag `firmware.uf2` onto the drive; the Pico reboots and runs it.

No BOOTSEL button accessible (enclosed build)? Power off, hold **Esc** on the
connected keyboard, and power on — the firmware reboots straight into the
BOOTSEL drive, so you can drag `firmware.uf2` on without opening the enclosure.

On first boot the firmware self-provisions its config, presets, and rhythms.

## Using it

- Plug a standard USB keyboard into the host port. The three keyboard rows play
  chords (see the chord-grid mapping in the spec); the number row / numpad is
  the strum plate.
- `F1`–`F8` are quick settings; `F9`–`F12` open the Chord/Strum/Rhythm/Bass
  menus. `Home`/`End` browse presets, `Insert`→`Enter` saves, `Space` is tap
  tempo, `Super`+`Esc` is panic.
- To edit config/presets/rhythms, power off, hold **Ctrl**, power on — the
  "KeybChord" drive mounts on your PC.

## Customization

Everything is JSON. See **[docs/custom-configuration-guide.md](docs/custom-configuration-guide.md)**
for the complete reference (config, presets, keymap, and how to add your own
rhythm and bass patterns). Ready-to-copy examples are in
[examples/test_rhythm.json](examples/test_rhythm.json) and
[examples/test_bass.json](examples/test_bass.json).

## Documentation

- [Product spec](docs/KeybChord_Pico_Spec.md) — full behavior and data models.
- [Wiring](docs/wiring.md) — connection diagram.
- [Custom configuration guide](docs/custom-configuration-guide.md) — JSON reference.

## Development

Native unit tests (no hardware needed) via GoogleTest:

```bash
cd keybchord
pio test -e native
```

## License

[MIT](LICENSE)
