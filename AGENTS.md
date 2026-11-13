# KeybChord Pico — agent notes

Omnichord-style MIDI instrument: RP2040 firmware (`keybchord/`), PySide6
configurator (`configurator/`), and a CH559 USB-host coprocessor
(`firmware/ch559/`).

## Commands

- Firmware build: `cd keybchord && pio run -e pico`
- Firmware tests: `cd keybchord && pio test -e native`
- CH559 build: `cd firmware/ch559 && make` (requires SDCC)
- Configurator run: `cd configurator && python run.py`
- Configurator tests: `cd configurator && pytest`
- USB-MIDI hardware harness: see `tools/midi_test/README.md`

## Map

- `keybchord/lib/core/` — portable C++ data/logic; no Arduino headers.
- `keybchord/lib/engines/` — stateful engines (chord, strum, rhythm, bass,
  sequence, edit, preset, display, pattern editor).
- `keybchord/lib/hw/` — hardware adapters behind `base.h`, selected in
  `factory.cpp`.
- `keybchord/src/main.cpp` — RP2040 wiring and the Core 0/Core 1 loop.
- `keybchord/test/` — GoogleTest native tests using null/stub adapters.
- `configurator/src/keybchord_configurator/` — `models/` mirror the firmware,
  `validation/` enforces JSON rules, `ui/` is PySide6.
- `firmware/ch559/` — 8051 SDCC firmware (GPL-3.0). `.asm`, `.lst`, `.rel`,
  `.rst`, `.sym`, `.map`, `.mem`, `.hex`, `.bin`, `.ihx`, `.lk` are build outputs.
- `docs/` — spec, wiring, and the authoritative device JSON guide
  (`docs/custom-configuration-guide.md`).

## Invariants

- Keep `lib/core` portable: no Arduino/pico headers and no exceptions.
- Native tests must pass without hardware; add tests next to the code you change.
- Configurator models and validators mirror firmware behavior; update both sides.
- Preset JSON and keymap/macro JSON use different enum spellings.
- Pattern editors share the per-kind 32-pattern cap and the combined 72 KB
  rhythm/bass/chord budget; keep firmware, configurator validation, and docs in
  sync. The default keymap is `F9`=Chord, `F10`=Rhythm, `F11`=Strum,
  `F12`=Bass, `Super+F9/F10/F12`=pattern editors, `Super+F11`=Drum, and
  `Ctrl+F12`=Seq Edit. In the bass/rhythm editors numpad pad keys are live;
  chord keys and direct parameter keys stay active (F2/F3/F4 are not editor
  shortcuts).
- Do not edit generated CH559 artifacts or bundled `roland-jd-xi-*.json` files
  unless the task is specifically about them.
- `testhook` is a gated debug build; keep test-hook code out of the `pico` build.

## Token discipline

- Prefer `rg -n '<symbol>'` and targeted line ranges over reading whole files.
- Do not read the full spec or configuration guide; load the matching skill and
  read only the referenced section.
- Never read `.pio/`, `configurator/build/`, `configurator/dist/`, `.venv/`,
  caches, or generated CH559 artifacts.

## Skills

- `keybchord-firmware` — build/test commands, layering, runtime invariants.
- `keybchord-add-param` — checklist for a new ParamId/preset/keymap setting.
- `keybchord-json` — on-device JSON schemas, limits, and enum spellings.
- `keybchord-configurator` — configurator layout and firmware-mirror contract.
- `keybchord-ch559` — CH559 source vs generated files, protocol, SDCC build.
