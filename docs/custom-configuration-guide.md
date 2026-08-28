# KeybChord Pico — Custom Configuration & JSON Guide

This guide documents every JSON file the firmware reads from (and writes to) its
onboard flash filesystem, so you can customize the device without recompiling.

All JSON is parsed with **ArduinoJson**. Integers are used throughout (the
RP2040 has no floating-point unit). Values that are missing, malformed, or out
of range are filled from the defaults listed below; a single bad field never
crashes the device.

---

## 1. How to edit files on the device

The filesystem is presented as a USB Mass Storage drive named **"KeybChord"**:

1. Power the unit **off**.
2. Hold **`Right Ctrl`** and power it **on** (keep holding until the drive appears).
3. On your PC the **"KeybChord"** drive mounts. Edit/add files there.
4. Eject the drive and power-cycle **without** holding `Ctrl`.

> The keyboard stays on the separate PIO-USB host port; the drive is on the
> native USB port. While the drive is mounted the firmware must not write to the
> filesystem, so preset Save/Clear is suppressed until you eject and reboot.

**First boot:** when the flash is empty the firmware self-provisions the whole
layout — `config.json`, `/keymap.json`, `/presets/`, and `/rhythms/`. You do not
need to prepare anything.

### On-disk layout

| Path | Contents |
|------|----------|
| `/config.json` | Global settings (single JSON object). |
| `/keymap.json` | Configurable key bindings (`bindings` array). |
| `/presets/bank1.json` … `/presets/bank10.json` | 10 banks × 8 preset slots (each file is a JSON **array of 8** objects). |
| `/rhythms/*.json` | Rhythm patterns — the 12 built-ins plus any user files you add. |
| `/bass/*.json` | Bass patterns — the 9 built-ins plus any user files you add. |
| `/keymap_error.log` | Written only when `/keymap.json` fails validation (see §4). |

---

## 2. `config.json` — global settings

A single object. Any omitted key uses its default.

```json
{
  "midi":    { "din_enabled": true, "clock_enabled": false },
  "chord":   { "base_root_midi": 60, "note_range": [48, 84] },
  "display": {
    "revert_timeout_ms": 1500,
    "prompt_timeout_ms": 5000,
    "cursor_timeout_ms": 5000,
    "menu_timeout_ms": 10000
  },
  "led":     { "bpm_indicator": true, "led": "num_lock", "flash_ms": 40 },
  "startup_preset": "B1:P1",
  "logging": { "debug_log": true, "midi_monitor": true }
}
```

| Key | Type | Range | Default | Notes |
|-----|------|-------|---------|-------|
| `midi.din_enabled` | bool | — | `true` | Reserved; parsed but not currently used (DIN MIDI out is always active). |
| `midi.clock_enabled` | bool | — | `false` | Transmit MIDI clock (`0xF8`) on DIN. Toggle = `F8`. |
| `chord.base_root_midi` | int | 0–127 | `60` | Anchor MIDI note for chord root position (60 = C4). |
| `chord.note_range` | [int,int] | 0–127 each | `[48, 84]` | Voicing note window. An inverted pair is auto-swapped. |
| `display.revert_timeout_ms` | int | 250–5000 | `1500` | How long a parameter value stays on the LCD before reverting. |
| `display.prompt_timeout_ms` | int | 1000–30000 | `5000` | Save/Clear prompt auto-cancel. |
| `display.cursor_timeout_ms` | int | 500–30000 | `5000` | Preset-cursor auto-reset. |
| `display.menu_timeout_ms` | int | 500–30000 | `10000` | Edit-menu idle auto-exit. |
| `led.bpm_indicator` | bool | — | `true` | Flash a keyboard LED on each beat. |
| `led.led` | enum | — | `num_lock` | Which LED(s): `num_lock`, `caps_lock`, `scroll_lock`, or `all`. |
| `led.flash_ms` | int | 5–500 | `40` | Beat-flash duration (ms). |
| `startup_preset` | string | `"B#:P#"` | `"B1:P1"` | Preset loaded at boot (bank 1–10, slot 1–8). |
| `logging.debug_log` | bool | — | `true` | Key/strum/LED debug log over USB-CDC. |
| `logging.midi_monitor` | bool | — | `true` | Decode outgoing MIDI over USB-CDC. |

---

## 3. Presets — `/presets/bankN.json`

Each bank file is a JSON **array of exactly 8** objects (slot 0 … slot 7),
covering 10 banks × 8 slots = 80 presets. Editing one object changes that slot.

### 3.1 Example slot

```json
{
  "name": "Ballad Pad",
  "chord": {
    "play_mode": "held",
    "octave": 0,
    "note_duration_ms": 500,
    "velocity": 100,
    "pan": 64,
    "voicing_mode": "root_position",
    "chord_roll_ms": 0,
    "min_notes": 3,
    "min_interval": 0,
    "inversion": "root",
    "arp_mode": "up",
    "channel": 1
  },
  "strum": {
    "octave": 1,
    "note_duration_ms": 300,
    "velocity": 90,
    "limited_keys": false,
    "mode": "follow_chord",
    "root_pc": 0,
    "scale_type": "ionian",
    "channel": 2
  },
  "bass": {
    "enabled": false,
    "octave": -1,
    "note_duration_ms": 150,
    "velocity": 90,
    "channel": 3,
    "pattern": "walking"
  },
  "rhythm": {
    "enabled": false,
    "pattern": "Rock 1",
    "tempo": 120,
    "swing": 0,
    "muted": false,
    "channel": 10,
    "drums": {
      "kick": 36, "kick_vel": 0,
      "snare": 38, "snare_vel": 0,
      "hihat": 42, "hihat_vel": 0,
      "open_hat": 46, "open_hat_vel": 0,
      "rimshot": 37, "rimshot_vel": 0,
      "clap": 39, "clap_vel": 0,
      "crash": 49, "crash_vel": 0,
      "ride": 51, "ride_vel": 0,
      "bongo": 61, "bongo_vel": 0,
      "conga_lo": 62, "conga_lo_vel": 0,
      "conga_hi": 63, "conga_hi_vel": 0,
      "clave": 75, "clave_vel": 0,
      "shaker": 82, "shaker_vel": 0
    }
  }
}
```

### 3.2 `chord` parameters

| Key | Type | Range | Default | Notes |
|-----|------|-------|---------|-------|
| `play_mode` | enum | — | `held` | `held` / `press_to_play` / `arpeggio` / `arp_hold` / `silent` |
| `octave` | int | −3 … +3 | `0` | Whole-octave transpose. |
| `note_duration_ms` | int | 50–4000 | `500` | Press-to-play/arp note length (step 50). |
| `velocity` | int | 1–127 | `100` | Note-on velocity. |
| `pan` | int | 0–127 | `64` | CC10; 64 = center. |
| `voicing_mode` | enum | — | `root_position` | `root_position` / `smart` / `down` / `up` |
| `chord_roll_ms` | int | −2000 … +2000 | `0` | Stagger note-ons (step 10); + up, − down; Held/Press only. |
| `min_notes` | int | 2–6 | `3` | Pad the voicing with octave notes to at least this count. |
| `min_interval` | int | 0–12 | `0` | Min semitones between adjacent notes; 0 = off. |
| `inversion` | enum | — | `root` | `root` / `first` / `second` / `third` |
| `arp_mode` | enum | — | `up` | `up` / `down` / `up_down` / `alternating` / `random` |
| `channel` | int | 1–16 | `1` | MIDI channel. |

> `arp_hold` is an arpeggio that **latches**: it keeps stepping until the next
> chord is pressed or `Esc` is pressed (a combination of `arpeggio` and `held`).

### 3.3 `strum` parameters

| Key | Type | Range | Default | Notes |
|-----|------|-------|---------|-------|
| `octave` | int | −3 … +3 | `1` | Strum-pool octave placement. |
| `note_duration_ms` | int | 50–4000 | `300` | Strummed note length (step 50). |
| `velocity` | int | 1–127 | `90` | |
| `limited_keys` | bool | — | `false` | Limited (10-key) numpad path instead of full (14-key). |
| `mode` | enum | — | `follow_chord` | `follow_chord` / `scale` / `piano` |
| `root_pc` | int | 0–11 | `0` | Root pitch class for Scale/Piano modes (0 = C … 11 = B). |
| `scale_type` | enum | — | `ionian` | See §6 scale list. |
| `channel` | int | 1–16 | `2` | MIDI channel. |

### 3.4 `bass` parameters

| Key | Type | Range | Default | Notes |
|-----|------|-------|---------|-------|
| `enabled` | bool | — | `false` | Walking-bass on/off. |
| `octave` | int | −3 … +3 | `−1` | Bass octave transpose. |
| `note_duration_ms` | int | 50–4000 | `150` | Bass note length (step 50). |
| `velocity` | int | 1–127 | `90` | |
| `channel` | int | 1–16 | `3` | MIDI channel. |
| `pattern` | enum | — | `walking` | Pattern **name** (built-in name or a user file's `name`): `Walking`, `Whole`, `Half`, `Quarter`, `Half Alt`, `Quarter Alt`, `3/4 Alt`, `No 6th`, `Hold`. |

### 3.5 `rhythm` parameters

| Key | Type | Range | Default | Notes |
|-----|------|-------|---------|-------|
| `enabled` | bool | — | `false` | Rhythm on/off. |
| `pattern` | string | — | `"Rock 1"` | Pattern **name** (built-in name or a user file's `name`). |
| `tempo` | int | 40–260 | `120` | BPM. |
| `swing` | int | −75 … +75 | `0` | Signed off-beat delay % (step 5). |
| `muted` | bool | — | `false` | Suppress drum note-ons (clock keeps running). |
| `channel` | int | 1–16 | `10` | GM percussion channel. |
| `drums` | object | — | (see §3.6) | Per-piece GM note remap + velocity override. |

### 3.6 `rhythm.drums` — per-piece mapping

Each `*_note` remaps a standard GM percussion sound to a different note; each
`*_vel` overrides its velocity.

| Sub-key | Type | Range | Default | Notes |
|---------|------|-------|---------|-------|
| `kick`, `snare`, `hihat`, `open_hat`, `rimshot`, `clap`, `crash`, `ride`, `bongo`, `conga_lo`, `conga_hi`, `clave`, `shaker` | int | 0–127 | 36, 38, 42, 46, 37, 39, 49, 51, 61, 62, 63, 75, 82 | GM percussion note. |
| the matching `*_vel` | int | 0–128 | `0` | `0` = Auto (follow pattern), 1–127 = fixed, **128 = Off** (mute the piece). |

---

## 4. `keymap.json` — configurable key bindings

The file is a single object with a `bindings` array. Each binding maps a
key-combo to an action.

```json
{
  "bindings": [
    { "keys": ["F1"], "action": { "type": "cycle", "param": "chord_mode" } },
    { "keys": ["Ctrl", "="], "action": { "type": "inc", "param": "chord_velocity" } },
    { "keys": ["PrtSc"], "action": { "type": "set", "param": "chord_inversion", "value": "first" } },
    { "keys": ["Ctrl", "F4"], "action": { "type": "toggle", "param": "chord_roll", "a": 0, "b": 50 } },
    { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
    { "keys": ["Alt", "F1"], "action": { "type": "drum_mute", "index": 0 } },
    { "keys": ["Home"], "action": { "type": "preset_prev" } },
    { "keys": ["Space"], "action": { "type": "tap_tempo" } }
  ]
}
```

### 4.1 `keys` — one key + at most one modifier

- Exactly one non-modifier key, optionally preceded by one modifier.
- Modifiers: **`Ctrl`**, **`Alt`**, **`Super`** (only the *left* Ctrl/Alt
  participate; either Gui counts as Super). Shift is **not** a modifier — it is
  a chord key.

**Bindable keys:** `F1`–`F12`, `PrtSc`, `ScLk`, `Pause`, `Insert`, `Home`,
`PgUp`, `Delete`, `End`, `PgDn`, `Space`, `=`, `-`, `KpPlus`, `KpMinus`,
`KpEnter`.

**Reserved keys (cannot be rebound):** the chord grid (`Tab`, `Q`…`[`, `Caps`,
`A`…`'`, `LShift`…`/`, `RShift`), strum keys (number row `1`…`0` and the keypad
`0 . 1…9 NumLock / *`), `` ` `` (backtick), `Esc`, the arrow keys
(`Left`/`Down`/`Right`/`Up`), `Enter`, and `Backspace`. `Super`+`Esc` (panic)
and `Super`+`1`…`8` (preset load) are also hard-coded.

### 4.2 Action types

| `type` | Extra fields | Effect |
|--------|--------------|--------|
| `open_menu` | `menu` | Open an edit menu (`chord`/`strum`/`rhythm`/`bass`/`drum`). |
| `cycle` | `param` | Advance the parameter by one (wraps). |
| `set` | `param`, `value` | Set the parameter to an absolute value. |
| `inc` | `param` | Step the parameter +1 (clamps; auto-repeats where applicable). |
| `dec` | `param` | Step the parameter −1. |
| `toggle` | `param`, `a`, `b` | If value == `a`, set to `b`, else set to `a`. |
| `drum_mute` | `index` (0–11) | Mute/unmute the Nth track of the current pattern. |
| `preset_prev` / `preset_next` | — | Move the preset cursor back/forward. |
| `preset_bank_prev` / `preset_bank_next` | — | Move the preset bank back/forward. |
| `preset_load` | `slot` (0–7) | Load slot N of the current bank. |
| `preset_save` / `preset_clear` | — | Save/Clear prompt for the active slot. |
| `tap_tempo` | — | Tap tempo (sets rhythm tempo). |

### 4.3 Parameter names (`param`)

`chord_octave`, `chord_mode`, `chord_voicing`, `chord_duration`,
`chord_velocity`, `chord_pan`, `chord_roll`, `chord_min_notes`,
`chord_min_interval`, `chord_inversion`, `arp_mode`,
`strum_octave`, `strum_duration`, `strum_velocity`, `strum_layout`,
`strum_mode`, `strum_root`, `strum_scale`,
`tempo`, `swing`, `rhythm_pattern`, `rhythm_mute`, `rhythm_enable`,
`rhythm_clock`, `rhythm_led`,
`bass_enable`, `bass_octave`, `bass_duration`, `bass_velocity`,
`bass_channel`, `bass_pattern`.

(Channel parameters and the per-drum note/velocity parameters are **not**
bindable in the keymap — they are edited in the Drum menu only.)

### 4.4 Value strings for `set` / `toggle` (and `inc`/`dec` params)

Values may be an integer, or a name for enum/bool parameters. The keymap uses
these **short** names (which differ slightly from the preset JSON names):

| `param` | Accepts (keymap.json) |
|---------|----------------------|
| `chord_mode` | `held`, `press`, `arpeggio`, `arp_hold`, `silent` (or 0–4) |
| `chord_voicing` | `root`, `smart`, `down`, `up` (or 0–3) |
| `chord_inversion` | `root`, `first`, `second`, `third` (or 0–3) |
| `arp_mode` | `up`, `down`, `up_down`, `alternating`, `random` (or 0–4) |
| `strum_mode` | `chord`, `scale`, `piano` (or 0–2) |
| `strum_scale` | `ionian`, `dorian`, `phrygian`, `lydian`, `mixolydian`, `aeolian`, `locrian`, `harmonic_minor`, `melodic_minor`, `major_pentatonic`, `minor_pentatonic`, `blues` |
| `bass_pattern` | `Walking`, `Whole`, `Half`, `Quarter`, `Half Alt`, `Quarter Alt`, `3/4 Alt`, `Hold`, `No 6th` (or 0–8, or any user pattern's `name`) |
| `rhythm_pattern` | any built-in or user rhythm `name` (e.g. `Rock 1`, `Test Groove`), or an index 0–N |
| bool params (`strum_layout`, `rhythm_mute`, `rhythm_enable`, `rhythm_clock`, `rhythm_led`, `bass_enable`) | `on`/`off`, `true`/`false`, `1`/`0` |

> `rhythm_pattern` / `bass_pattern` names are resolved against the patterns
> loaded at boot (built-ins + user files), so a name that doesn't match a loaded
> pattern is treated as a keymap error (see §4.5).

### 4.5 Validation & required coverage

On boot the keymap is validated. Any error (bad JSON, unknown key/param/value,
duplicate binding, reserved key, a `keys` list with 0 or >2 entries) makes the
firmware **halt**: the LCD shows `Keymap error` / `Remount to fix`, a
`/keymap_error.log` is written with one line per error, and no input is
processed until the file is fixed (remount via the boot key).

Even a valid keymap must still cover these (else it is rejected as missing a
required binding): an `open_menu` for each of **chord, strum, rhythm, bass,
drum**, plus **`preset_prev`**, **`preset_next`**, and **`preset_save`**.

---

## 5. Patterns — `/rhythms/*.json` and `/bass/*.json`

### 5.1 Schema (rhythm)

```json
{
  "name": "Rock 1",
  "short_name": "Rk",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] }
  ]
}
```

| Field | Type | Notes |
|-------|------|-------|
| `name` | string | Display name (shown on the LCD in the Rhythm menu). |
| `short_name` | string (optional) | 2-character code shown on the idle line of the LCD (truncated to the first two characters if longer). Falls back to the built-in code for built-ins, or the first two letters of `name` for user patterns. |
| `steps_per_bar` | int | Length of each `pattern` array; 4 steps = 1 beat (16th-note grid). |
| `swing` | int | Per-pattern default swing (−75…+75, clamped). |
| `tracks[]` | array | One entry per percussion voice. |
| `tracks[].note` | int | GM percussion note (0–127). |
| `tracks[].name` | string | Voice name (used for drum-mute display). |
| `tracks[].pattern` | int array | Per-step velocity: `0` = rest, `1` = default velocity (100), `2`–`127` = literal velocity. |

### 5.2 The 12 built-in patterns

These ship in firmware and are written to `/rhythms/` on first boot (and
re-provisioned if you delete one):

| Index | File | Name |
|-------|------|------|
| 0 | `rock1.json` | Rock 1 |
| 1 | `rock2.json` | Rock 2 |
| 2 | `waltz.json` | Waltz |
| 3 | `swing.json` | Swing |
| 4 | `slow_rock.json` | Slow Rock |
| 5 | `bossa_nova.json` | Bossa Nova |
| 6 | `rhumba.json` | Rhumba |
| 7 | `tango.json` | Tango |
| 8 | `march.json` | March |
| 9 | `samba.json` | Samba |
| 10 | `disco.json` | Disco |
| 11 | `foxtrot.json` | Foxtrot |

### 5.3 Adding your own rhythms

You **can** add new rhythms. Drop any extra `*.json` file into `/rhythms/`
(see §1 for how to mount the drive). They are:

- **Loaded after** the 12 built-ins, at indices 12, 13, … (built-ins always keep
  indices 0–11, so existing presets and the `F4` cycle order are unchanged).
- **Ordered alphabetically by filename** among themselves.
- **Named by the `name` field** in the file (falls back to the filename).
- **Selected with `F4`** (cycle) or the Rhythm menu; the name shows on the LCD.
- **Referenced by name in presets** (`rhythm.pattern`). If you later rename or
  delete a user file, a preset referencing the old name falls back to the first
  pattern.

Practical rules:

- The filename must end in `.json` and must not match one of the 12 built-in
  filenames above.
- Use a simple, unique name (e.g. `my_groove.json`). Alphabetical order among
  your files determines their slot order.
- Keep at most ~20 user files (the firmware caps the total at **32** patterns).
- A pattern with no `tracks` is skipped.
- You can also edit any built-in `.json` file in place to change its feel;
  deleting a built-in file re-provisions the default.

A ready-to-copy example lives at `examples/test_rhythm.json` (a "Test Groove"
pattern); copy it to `/rhythms/test_rhythm.json` to verify user rhythms load.

### 5.4 Schema (bass)

```json
{
  "name": "Half Alt",
  "steps_per_bar": 16,
  "steps": [0, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, -1, -1],
  "sustain_steps": [8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0]
}
```

| Field | Type | Notes |
|-------|------|-------|
| `name` | string | Display name (shown on the LCD in the Bass menu, used by presets/keymap). |
| `steps_per_bar` | int | Length of the `steps` array; 4 steps = 1 beat (16th-note grid, same as rhythm). |
| `steps[]` | int array | Chord-degree codes: `-1` = rest, `0` = root, `1` = 3rd, `2` = 5th, `3` = 6th/7th. The note is resolved through the chord's interval formula, so it follows the chord type (a minor 3rd for minor chords, etc.). |
| `sustain_steps[]` | int array (optional) | Parallel array giving each note's length in 16th steps. `0`/absent = percussive (uses `bass.note_duration_ms`). |
| `hold` | bool (optional) | `true` marks the special Hold pattern (root sustains while the chord is sounding, not beat-driven). |

### 5.5 The 9 built-in bass patterns

These ship in firmware and are written to `/bass/` on first boot (and
re-provisioned if you delete one):

| Index | File | Name |
|-------|------|------|
| 0 | `walking.json` | Walking |
| 1 | `whole.json` | Whole |
| 2 | `half.json` | Half |
| 3 | `quarter.json` | Quarter |
| 4 | `half_alt.json` | Half Alt |
| 5 | `quarter_alt.json` | Quarter Alt |
| 6 | `three_four_alt.json` | 3/4 Alt |
| 7 | `hold.json` | Hold |
| 8 | `walk_no_6th.json` | No 6th |

### 5.6 Adding your own bass patterns

You **can** add new bass patterns. Drop any extra `*.json` file into `/bass/`
(see §1 for how to mount the drive). They behave like user rhythms:

- **Loaded after** the 9 built-ins, at indices 9, 10, … (built-ins keep indices
  0–8, so existing presets and the `F3` cycle order are unchanged).
- **Ordered alphabetically by filename** among themselves.
- **Named by the `name` field** (falls back to the filename).
- **Selected with `F3`** (cycle) or the Bass menu; the name shows on the LCD.
- **Referenced by name in presets** (`bass.pattern`) and in the keymap
  (`bass_pattern`). A preset/keymap referencing a deleted or renamed file falls
  back to the first pattern (Walking).

Practical rules:

- The filename must end in `.json` and must not match one of the 9 built-in
  filenames above.
- Keep at most ~23 user files (the firmware caps the total at **32** patterns).
- A pattern with an empty `steps` array is skipped.
- You can edit any built-in `.json` in place; deleting one re-provisions the
  default.

A ready-to-copy example lives at `examples/test_bass.json` (a "Test Bass" root/
5th half-note pattern); copy it to `/bass/test_bass.json` to verify user bass
patterns load.

> **Meter note:** the bass follows the rhythm's meter when it is shorter than
> the bass pattern. A 4/4 bass pattern over a 3/4 (waltz) rhythm plays only its
> first 12 steps, then wraps — so a walking bass naturally drops its 6th/7th on
> the fourth beat, and `3/4 Alt` becomes root on beat 1, 5th on beat 3.

---

## 6. Reference tables

### 6.1 GM percussion notes

| Sound | Note | Sound | Note |
|-------|------|-------|------|
| Kick (Bass Drum 1) | 36 | Ride Cymbal 1 | 51 |
| Rimshot (Side Stick) | 37 | Low Bongo | 61 |
| Acoustic Snare | 38 | Mute Hi Conga | 62 |
| Hand Clap | 39 | Open Hi Conga | 63 |
| Closed Hi-Hat | 42 | Claves | 75 |
| Open Hi-Hat | 46 | Shaker | 82 |
| Crash Cymbal 1 | 49 | | |

### 6.2 Scale/mode names (`strum.scale_type`, and `strum_scale`)

`ionian`, `dorian`, `phrygian`, `lydian`, `mixolydian`, `aeolian`, `locrian`,
`harmonic_minor`, `melodic_minor`, `major_pentatonic`, `minor_pentatonic`,
`blues`.

### 6.3 Parameter bounds (default / min / max / step)

| Parameter | Default | Min | Max | Step |
|-----------|---------|-----|-----|------|
| chord.octave | 0 | −3 | +3 | 1 |
| chord.note_duration_ms | 500 | 50 | 4000 | 50 |
| chord.velocity | 100 | 1 | 127 | 1 |
| chord.pan | 64 | 0 | 127 | 1 |
| chord.chord_roll_ms | 0 | −2000 | +2000 | 10 |
| chord.min_notes | 3 | 2 | 6 | 1 |
| chord.min_interval | 0 | 0 | 12 | 1 |
| chord.channel | 1 | 1 | 16 | 1 |
| strum.octave | 1 | −3 | +3 | 1 |
| strum.note_duration_ms | 300 | 50 | 4000 | 50 |
| strum.velocity | 90 | 1 | 127 | 1 |
| strum.channel | 2 | 1 | 16 | 1 |
| rhythm.tempo | 120 | 40 | 260 | 1 |
| rhythm.swing | 0 | −75 | +75 | 5 |
| rhythm.channel | 10 | 1 | 16 | 1 |
| bass.octave | −1 | −3 | +3 | 1 |
| bass.note_duration_ms | 150 | 50 | 4000 | 50 |
| bass.velocity | 90 | 1 | 127 | 1 |
| bass.channel | 3 | 1 | 16 | 1 |
| drums `*_note` | (see §3.6) | 0 | 127 | 1 |
| drums `*_vel` | 0 | 0 | 128 | 1 (128 = Off) |

---

## 7. Worked examples

### 7.1 A custom preset (bank 3, slot 5 → `/presets/bank3.json`)

Edit element index 4 of the array:

```json
{
  "name": "Lush Arp",
  "chord": { "play_mode": "arp_hold", "arp_mode": "up_down", "velocity": 110, "octave": 0 },
  "strum": { "mode": "scale", "root_pc": 7, "scale_type": "dorian" },
  "bass": { "enabled": true, "pattern": "half_alt" },
  "rhythm": { "pattern": "Rock 1", "tempo": 90 }
}
```

(Missing fields fall back to defaults.)

### 7.2 A custom keymap binding

To make `Ctrl`+`Pause` select the `arp_hold` play mode, add:

```json
{ "keys": ["Ctrl", "Pause"], "action": { "type": "set", "param": "chord_mode", "value": "arp_hold" } }
```

### 7.3 A custom rhythm

Copy `examples/test_rhythm.json` to `/rhythms/test_rhythm.json`, reboot, then
`F4` to cycle to "Test Groove" (it appears after the 12 built-ins) and `F5` to
play it.
