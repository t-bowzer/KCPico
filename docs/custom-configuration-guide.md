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
4. Eject the drive. KeybChord detects the eject, reboots automatically, and
   reloads the edited files. You do **not** need to power-cycle it.

> The keyboard stays on the separate CH559 host coprocessor; the drive is on the
> native USB port. While the drive is mounted the firmware must not write to the
> filesystem, so preset Save/Clear is suppressed until you eject (which reboots).

### Updating the firmware (enclosed build)

If the Pico's BOOTSEL button is not reachable, hold **`Esc`** on the connected
keyboard and power the unit **on** — it reboots into the USB bootloader
(`RPI-RP2` drive) so you can drag a new `firmware.uf2` onto it without opening
the enclosure.

**First boot:** when the flash is empty the firmware self-provisions the whole
layout — `config.json`, `/keymap.json`, `/presets/`, `/rhythms/`, `/bass/`, and
`/chordpatterns/`. You do not need to prepare anything.

### On-disk layout

| Path | Contents |
|------|----------|
| `/config.json` | Global settings (single JSON object). |
| `/keymap.json` | Configurable key bindings (`bindings` array). |
| `/presets/bank1.json` … `/presets/bank5.json` | 5 banks × 8 preset slots (each file is a JSON **array of 8** objects). |
| `/sequences/b1p1.json` … `/sequences/b5p8.json` | Per-preset chord sequences (all 10 slots in one compact file; see §3.8). |
| `/rhythms/*.json` | Rhythm patterns — the 12 built-ins plus any user files you add. |
| `/bass/*.json` | Bass patterns — the 9 built-ins plus any user files you add. |
| `/chordpatterns/*.json` | Chord patterns — the 9 built-ins plus any user files you add. |
| `/keymap_error.log` | Written only when `/keymap.json` fails validation (see §4). |

> Rhythm, bass, and chord patterns can also be created or edited on the device
> with the **Pattern Edit** mode (see §5.10); the files it writes are ordinary
> `userN.json` files in the same directories.

---

## 2. `config.json` — global settings

A single object. Any omitted key uses its default.

```json
{
  "midi":    { "din_enabled": true, "usb_enabled": true, "clock_enabled": false },
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
| `midi.usb_enabled` | bool | — | `true` | USB MIDI device output on/off (v0.2). The interface always enumerates; this only gates whether bytes are sent. Toggle = `Ctrl+F8`. |
| `midi.clock_enabled` | bool | — | `false` | Transmit MIDI clock (`0xF8`) on DIN and USB. Toggle = `F8`. Clock Out is clock-only: enabling waits for the next 24-tick beat boundary so the external beat division starts on the LED boundary; disabling simply stops clocks (no MIDI Stop). **Double-tap `F8`** to send MIDI Start (`0xFA`) at the next beat boundary and re-align a slave after it was stopped or restarted locally. |
| `chord.base_root_midi` | int | 0–127 | `60` | Anchor MIDI note for chord root position (60 = C4). |
| `chord.note_range` | [int,int] | 0–127 each | `[48, 84]` | Voicing note window. An inverted pair is auto-swapped. |
| `display.revert_timeout_ms` | int | 250–5000 | `1500` | How long a parameter value stays on the LCD before reverting. |
| `display.prompt_timeout_ms` | int | 1000–30000 | `5000` | Save/Clear prompt auto-cancel. |
| `display.cursor_timeout_ms` | int | 500–30000 | `5000` | Preset-cursor auto-reset. |
| `display.menu_timeout_ms` | int | 500–30000 | `10000` | Edit-menu idle auto-exit while scrolling; a selected parameter never times out. |
| `led.bpm_indicator` | bool | — | `true` | Flash a keyboard LED on each beat. |
| `led.led` | enum | — | `num_lock` | Which LED(s): `num_lock`, `caps_lock`, `scroll_lock`, or `all`. |
| `led.flash_ms` | int | 5–500 | `40` | Beat-flash duration (ms). |
| `startup_preset` | string | `"B#:P#"` | `"B1:P1"` | Preset loaded at boot (bank 1–5, slot 1–8). Values above B5 fall back to B1:P1. |
| `logging.debug_log` | bool | — | `true` | Key/strum/LED debug log over USB-CDC; lines are dropped rather than blocking playback if the host is not draining the log. |
| `logging.midi_monitor` | bool | — | `true` | Decode outgoing MIDI over USB-CDC; sent after the MIDI bytes and dropped on a full CDC buffer so timing is never blocked. |

---

## 3. Presets — `/presets/bankN.json`

Each bank file is a JSON **array of exactly 8** objects (slot 0 … slot 7),
covering 5 banks × 8 slots = **40 presets**. Editing one object changes that
slot. v0.2 removed the old banks 6–10; back them up before upgrading (the
firmware deletes those files on first boot to reclaim flash).

### 3.1 Example slot

```json
{
  "name": "Ballad Pad",
  "seq_settings": "recorded",
  "number_keys": "strum",
  "macros": {},
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
    "arp_rate": "sixteenth",
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

`seq_settings` (`"recorded"` or `"live"`) is the preset's global Recorded/Live
sequence-settings override, saved with the preset. Omitted values default to
`"recorded"`; boolean and integer spellings are also accepted when loading.

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
| `min_interval` | int | 0–12 | `0` | Min semitones between adjacent notes; follows the voicing mode and inversion; 0 = off. |
| `inversion` | enum | — | `root` | `root` / `first` / `second` / `third` |
| `arp_mode` | enum | — | `up` | `up` / `down` / `up_down` / `alternating` / `random` |
| `arp_rate` | enum | — | `sixteenth` | `whole` / `half` / `quarter` / `eighth` / `sixteenth` / `thirty_second` |
| `channel` | int | 1–16 | `1` | MIDI channel. |

> `arp_hold` is an arpeggio that **latches**: it keeps stepping until the next
> chord is pressed or `Esc` is pressed (a combination of `arpeggio` and `held`).
> Chord-grid keys pressed within ~15 ms are grouped as one chord, so combination
> shapes do not briefly sound the first key's chord when the keys are not
> perfectly simultaneous.

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

### 3.7 `number_keys` and `macros` — per-preset top-row macros

`number_keys` selects what the **top-row** number keys `1 2 3 4 5 6 7 8 9 0`
do in this preset. The numpad is always the strum plate.

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| `number_keys` | string | `"strum"` | `"strum"` = top row strums; `"macro"` = top row triggers macros. |
| `macros` | object | `{}` | Keyed by top-row label `"1"`…`"9"`, `"0"`; each value is a macro object with an optional `name` and an ordered `commands` array. Legacy bare command arrays are still accepted. |

Macro keys trigger on key-down (no modifiers); `Super+1`…`Super+8` still loads
presets. A command is one of:

```json
{
  "number_keys": "macro",
  "macros": {
    "1": {
      "name": "Verse",
      "commands": [
        { "type": "set", "param": "rhythm_mute", "value": "off" },
        { "type": "drum_mute", "piece": "kick", "value": "on" },
        { "type": "midi", "bytes": [176, 0, 85] },
        { "type": "midi", "bytes": [176, 32, 64], "delay_ms": 10 },
        { "type": "midi", "bytes": [192, 0] },
        { "type": "set", "param": "tempo", "value": 88 }
      ]
    },
    "2": {
      "name": "Chorus",
      "commands": [
        { "type": "toggle", "param": "rhythm_enable", "a": 1, "b": 0 }
      ]
    }
  }
}
```

Macro names default to `M1`…`M9`, `M0` (matching the key labels) when `name` is
missing or empty. Names are limited to 16 characters. When a macro key is
pressed, the LCD shows `Macro <name> loaded` (or `Macro <name>` / `loaded` on
two lines for longer names).

| Command `type` | Fields | Notes |
|----------------|--------|-------|
| `set` | `param`, `value` | Absolute value, using the keymap parameter/value names below. |
| `toggle` | `param`, `a`, `b` | If the value equals `a`, set `b`; otherwise set `a`. |
| `cycle` | `param` | Advance one step (same as a keymap `cycle` action). |
| `inc` / `dec` | `param` | Step the value up/down using its normal increment. |
| `drum_mute` | `piece`, `value` | `piece` is one of `kick`, `snare`, `hihat`, `open_hat`, `rimshot`, `clap`, `crash`, `ride`, `bongo`, `conga_lo`, `conga_hi`, `clave`, `shaker`; `value` is `on`/`off`. Mute-off sets the piece to Auto velocity (`0`). |
| `midi` | `bytes`, `delay_ms` (optional) | Arbitrary MIDI bytes, `0`–`255` per byte, up to 256 bytes (for example a note, CC, program change, or complete SysEx message). The optional delay is `0`–`10000` ms and is cumulative within the macro. |

Limits: **16 commands per key** and **48 commands per preset**. The firmware
truncates excess commands; the configurator rejects them. Parameter names and
value strings match the keymap names in §4.3/§4.4, plus the per-drum parameters
`drum_kick`, `drum_kick_vel`, … (note / velocity). The firmware writes macro
values as integers, names as default or custom strings, but `on`/`off`, enum
names, and pattern names are accepted when editing by hand. MIDI commands are
sent to both DIN and USB MIDI outputs; delayed commands are queued relative to
the key press. The configurator can insert prebuilt multi-message sequences
from JSON libraries (including the bundled Roland JD-Xi libraries); see the
[configurator README](../configurator/README.md#midi-macros-and-command-libraries).

> On-device preset Save preserves the preset's macro configuration; Clear
> resets it to `number_keys: "strum"` with no macros.

### 3.8 Chord sequences — `/sequences/bNpM.json`

Each preset owns **10 sequence slots** (1–10, selected with `Ctrl+1`…`Ctrl+0`).
They are stored in one file per preset, named with the preset bank/slot:
`/sequences/b1p1.json` for bank 1 slot 1, through `/sequences/b5p8.json`. The
file holds all ten slots; an unused slot is `null`. Files are compact JSON (no
pretty whitespace) so the 40-preset × 10-slot worst case fits the 512 KB
filesystem.

```json
{
  "version": 1,
  "sequences": [
    null,
    {
      "bars": 4,
      "steps_per_bar": 16,
      "recorded": [0, 500, 100, 64, 0, 0, 3, 0, 0, 0, 0, 0, 4],
      "live": 0,
      "events": [[0, 0], [16, 640]]
    }
  ]
}
```

| Field | Type | Notes |
|-------|------|-------|
| `bars` | int | `1`, `2`, `4`, or `8`. |
| `steps_per_bar` | int | Recorded meter, 4–64 (usually `16`, or `12` for 3/4). |
| `recorded` | int[13] | Settings snapshot, in this order: `octave`, `note_duration_ms`, `velocity`, `pan`, `voicing_mode` (0–3), `chord_roll_ms`, `min_notes`, `min_interval`, `inversion` (0–3), `arp_mode` (0–4), `pattern` index, `play_mode` (0–6), `arp_rate` (0–5). Legacy 11-value arrays load with play mode `held` and arp rate `16th`. |
| `live` | int | Bitmask; bit *i* set = use the **live** value for `recorded[i]` during playback. Channel stays live. |
| `events` | `[step, chord_id]` array | Max **64** events. `step` is a 16th-note index in `0 .. bars*steps_per_bar-1`. The last event at a duplicate step wins. |

**Chord ID encoding:** `chord_id = root_pc * 128 + type * 8 + extension_mask`.
`root_pc` is 0–11 (C = 0). `type` follows the firmware `ChordType` order:
`0` major, `1` minor, `2` dom7, `3` maj7, `4` min7, `5` dim, `6` dim7, `7` aug,
`8` sus4, `9` sus2, `10` min7b5. Extension bits are add9 = `1`, add11 = `2`,
add13 = `4` (combine them). Example: C major = `0`; G7 = `7*128 + 2*8 = 912`;
Fm7 with add9+add13 = `5*128 + 4*8 + 5 = 677`.

**Recording/playback:**
- `Ctrl+F9` arms recording. The normal pattern is muted immediately, but the
  hi-hat guide stays silent until the next bar boundary. It then plays
  **exactly one bar** of quarter-note closed hi-hats; recording starts at the
  following bar boundary and the hi-hat guide continues for the whole take.
  Chord changes are quantized to the nearest 16th; only chord changes are
  captured (strum is never recorded). When the take ends, the guide stops and
  the original pattern resumes. The LCD count-in/recording status is
  persistent (`Rec arm`/`Wait bar` → `Count-in`/`1 bar` → `Recording N/N`), so
  it does not flash back to idle between bars.
- `Ctrl+F10` arms playback; it loops from the next rhythm bar until stopped.
- If rhythm is off, record/play enable it automatically and mute the drums.
- While a live chord is held or latched, sequence events are skipped; the
  sequence resumes at the next event after the live chord ends.
- Recording, bar changes, and clearing save immediately. The preset dirty `*`
  marker does not track sequences.
- `Ctrl+F11` clears the selected sequence (confirm with `Enter`). Clearing a
  preset also removes its sequence file.
- `Ctrl+F12` opens **Seq Edit** (`Slot`, `Bars`, `Settings`). `Settings`
  chooses the global Recorded/Live override and is saved with the preset
  (`seq_settings`: `"recorded"` or `"live"`); per-setting Live bits are edited
  in the configurator's **Sequences** tab.

**Quick start (on the device):**

1. Optionally press `F5` to enable rhythm; record/play enables it automatically
   and mutes the drums when it is off. Press `F6` to mute drums if you want a
   chord-only recording.
2. Press `Ctrl+1` to select sequence slot 1 (use `Ctrl+1`…`Ctrl+9`, `Ctrl+0`
   for slots 1–10).
3. Press `Ctrl+F9`. The LCD shows `Rec arm` / `Wait bar`; the normal pattern is
   muted. Wait for the next bar, then exactly one bar of quarter-note closed
   hi-hats plays (`Count-in` / `1 bar`). Recording then starts at the following
   bar (`Recording 1/N`) and the hi-hat guide continues.
4. Play chord changes on the chord grid. Do not strum — only chord changes are
   recorded. The take saves automatically after the last bar (`Seq saved`), and
   the original rhythm pattern resumes.
5. Press `Ctrl+F10` to loop it (`Play arm` / `Wait bar`, then `Seq N` /
   `play`). Press `Ctrl+F10` again to stop.
6. Press `Ctrl+F12` to change the slot, bar length, or the global
   Recorded/Live setting. Use `Ctrl+F11` to clear the selected sequence.
7. For per-setting Live mixing, open the device in the configurator and use the
   preset's **Sequences** tab.

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
`KpEnter`, `Left`, `Down`, `Right`, `Up`, plus these Consumer/media keys:
`PlayPause`, `Stop`, `PrevTrack`, `NextTrack`, `Rewind`,
`FastForward`, `Eject`, `MediaSelect`, `Mail`, `Calculator`, `BrowserSearch`,
`BrowserHome`, `BrowserBack`, `BrowserForward`, `BrowserStop`,
`BrowserRefresh`, `BrowserFavorites`.

Media keys are bindable **without** `Ctrl`/`Alt`/`Super`; a modifier combo on a
media key is rejected. They require a keyboard that exposes a USB Consumer
Control collection (either a separate interface or the shared QMK keyboard/
consumer report; the CH559 forwards it to the RP2040).

**Reserved keys (cannot be rebound):** the chord grid (`Tab`, `Q`…`[`, `Caps`,
`A`…`'`, `LShift`…`/`, `RShift`), strum keys (number row `1`…`0` and the keypad
`0 . 1…9 NumLock / *`), `` ` `` (backtick), `Esc`, `Enter`, `Backspace`, and the
fixed keys `Mute`, `VolumeUp`, and `VolumeDown`. `Super`+`Esc` (panic) and
`Super`+`1`…`8` (preset load) are also hard-coded.

> **Mute is fixed** to behave exactly like pressing `Enter` (select/commit a
> parameter in a menu, confirm a save/clear prompt, load a preset from cursor
> mode, or commit a preset name). **Volume keys are always fixed:** on the main
> screen `VolumeUp`/`VolumeDown` step the tempo up/down; inside an edit menu they
> scroll parameters while browsing and change the selected value once a parameter
> is selected. Holding either volume key repeats value edits.

> The arrow keys (`Left`/`Down`/`Right`/`Up`) are bindable; by default `Left`,
> `Down`, and `Right` are bound to the held add9/add11/add13 extensions. Inside
> an edit menu the arrows scroll parameter highlights while browsing and change
> the selected value once selected, regardless of any keymap binding.

> **Edit-menu flow:** arrows or volume scroll the parameters; `Enter`/`Mute`
> selects the highlighted parameter. While selected, the same keys edit its
> value. `Enter`/`Mute` keeps the value and deselects; `Backspace`/`Esc` restores
> the value from when it was selected and deselects. A selected parameter stays
> selected indefinitely; only an unselected menu auto-exits after
> `display.menu_timeout_ms`. `F1`–`F12` are inert inside menus (menu-open keys
> such as `F9`–`F12`, `Super+F11`, and `Ctrl+F12` still work).

### 4.2 Action types

| `type` | Extra fields | Effect |
|--------|--------------|--------|
| `open_menu` | `menu` | Open an edit menu (`chord`/`rhythm`/`strum`/`bass`/`drum`/`sequence`) or a pattern editor (`chord_pattern`/`rhythm_pattern`/`bass_pattern`). |
| `cycle` | `param` | Advance the parameter by one (wraps). |
| `set` | `param`, `value` | Set the parameter to an absolute value. |
| `inc` | `param` | Step the parameter +1 (clamps; auto-repeats where applicable). |
| `dec` | `param` | Step the parameter −1. |
| `toggle` | `param`, `a`, `b` | If value == `a`, set to `b`, else set to `a`. |
| `ext` | `value` | Trigger a held chord extension: `9` (add9), `11` (add11), or `13` (add13). |
| `drum_mute` | `index` (0–11) | Mute/unmute the Nth track of the current pattern. |
| `preset_prev` / `preset_next` | — | Move the preset cursor back/forward. |
| `preset_bank_prev` / `preset_bank_next` | — | Move the preset bank back/forward. |
| `preset_load` | `slot` (0–7) | Load slot N of the current bank. |
| `preset_save` / `preset_clear` | — | Save/Clear prompt for the active slot. |
| `tap_tempo` | — | Tap tempo (sets rhythm tempo). |
| `sequence_record` | — | Arm/stop chord-sequence recording. |
| `sequence_play` | — | Arm/stop chord-sequence playback. |
| `sequence_clear` | — | Clear the selected sequence (confirm with `Enter`). |

### 4.3 Parameter names (`param`)

`chord_octave`, `chord_mode`, `chord_voicing`, `chord_duration`,
`chord_velocity`, `chord_pan`, `chord_roll`, `chord_min_notes`,
`chord_min_interval`, `chord_inversion`, `arp_mode`, `arp_rate`,
`strum_octave`, `strum_duration`, `strum_velocity`, `strum_layout`,
`strum_mode`, `strum_root`, `strum_scale`,
`tempo`, `swing`, `rhythm_pattern`, `rhythm_mute`, `rhythm_enable`,
`rhythm_clock`, `rhythm_led`, `usb_midi`,
`bass_enable`, `bass_octave`, `bass_duration`, `bass_velocity`,
`bass_channel`, `bass_pattern`, `seq_slot`, `seq_bars`, `seq_settings`.

(Channel parameters and the per-drum note/velocity parameters are **not**
bindable in the keymap — they are edited in the Drum menu only. `seq_*`
parameters are the on-device Sequencer menu values; `seq_settings` is a bool.)

### 4.4 Value strings for `set` / `toggle` (and `inc`/`dec` params)

Values may be an integer, or a name for enum/bool parameters. The keymap uses
these **short** names (which differ slightly from the preset JSON names):

| `param` | Accepts (keymap.json) |
|---------|----------------------|
| `chord_mode` | `held`, `press`, `arpeggio`, `arp_hold`, `silent` (or 0–4) |
| `chord_voicing` | `root`, `smart`, `down`, `up` (or 0–3) |
| `chord_inversion` | `root`, `first`, `second`, `third` (or 0–3) |
| `arp_mode` | `up`, `down`, `up_down`, `alternating`, `random` (or 0–4) |
| `arp_rate` | `whole`, `half`, `quarter`, `eighth`, `sixteenth`, `thirty_second` (or `1/1`…`1/32`, `16th`, `32nd`, or 0–5) |
| `strum_mode` | `chord`, `scale`, `piano` (or 0–2) |
| `strum_scale` | `ionian`, `dorian`, `phrygian`, `lydian`, `mixolydian`, `aeolian`, `locrian`, `harmonic_minor`, `melodic_minor`, `major_pentatonic`, `minor_pentatonic`, `blues` |
| `bass_pattern` | `Walking`, `Whole`, `Half`, `Quarter`, `Half Alt`, `Quarter Alt`, `3/4 Alt`, `Hold`, `No 6th` (or 0–8, or any user pattern's `name`) |
| `rhythm_pattern` | any built-in or user rhythm `name` (e.g. `Rock 1`, `Test Groove`), or an index 0–N |
| bool params (`strum_layout`, `rhythm_mute`, `rhythm_enable`, `rhythm_clock`, `rhythm_led`, `usb_midi`, `bass_enable`, `seq_settings`) | `on`/`off`, `true`/`false`, `1`/`0` |

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
The `sequence` menu, the three sequence actions, and the three pattern-editor
menus (`chord_pattern`/`rhythm_pattern`/`bass_pattern`) are optional. The default
keymap binds `Ctrl+F12` for Seq Edit and `Ctrl+F9`/`Ctrl+F10`/`Ctrl+F11` for
sequence record/play/clear; it binds `Super+F9`/`F10`/`F12` for the chord, rhythm,
and bass pattern editors. Existing v0.1 keymaps remain valid. Older keymaps that
do not bind the sequence actions still get those four combos as fallbacks.
`Ctrl+1`…`Ctrl+0` sequence selection is hard-coded.

> **v0.2 default keymap change:** the shipped defaults now use
> `F9`/`F10`/`F11`/`F12` for Chord/Rhythm/Strum/Bass menus, `Super+F11` for Drum,
> `Ctrl+F12` for Seq Edit, and `Super+F9`/`F10`/`F12` for the pattern editors.
> The Strum and Rhythm menu keys swapped, and Seq Edit moved from `Super+F12` to
> `Ctrl+F12`. Custom `keymap.json` files are unaffected; add the new `open_menu`
> bindings to use the pattern editors.

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
  A combined rhythm/bass/chord pattern budget of **72 KB** also applies (user
  patterns share a 48 KB portion); the on-device editor rejects an 8 KB+ rhythm
  file.
- A pattern with no `tracks` is skipped.
- You can also edit any built-in `.json` file in place to change its feel;
  deleting a built-in file re-provisions the default.
- The device can create/edit rhythm patterns too (Pattern Edit, §5.10). It writes
  `userN.json` with at most 8 KB per file and enforces the combined pattern
  budget below.

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
| `steps[]` | int array | Chord-degree codes: `-1` = rest, `0` = root, `1` = 3rd, `2` = 5th, `3` = 6th/7th, `4` = root one octave up. The note is resolved through the chord's interval formula, so it follows the chord type (a minor 3rd for minor chords, etc.). |
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
  The combined pattern budget (72 KB; 48 KB user share) also applies, and the
  on-device editor enforces a 4 KB per-file cap.
- A pattern with an empty `steps` array is skipped.
- You can edit any built-in `.json` in place; deleting one re-provisions the
  default.
- The device can create/edit bass patterns too (Pattern Edit, §5.10), including
  captured sustains.

A ready-to-copy example lives at `examples/test_bass.json` (a "Test Bass" root/
5th half-note pattern); copy it to `/bass/test_bass.json` to verify user bass
patterns load.

> **Meter note:** the bass follows the rhythm's meter when it is shorter than
> the bass pattern. A 4/4 bass pattern over a 3/4 (waltz) rhythm plays only its
> first 12 steps, then wraps — so a walking bass naturally drops its 6th/7th on
> the fourth beat, and `3/4 Alt` becomes root on beat 1, 5th on beat 3.

> **No-drums note:** when the rhythm engine is stopped, the bass free-runs on
> the same 16th-note grid at the preset tempo. It stays audible without drums
> and resyncs to the shared step counter when the rhythm starts.

### 5.7 Schema (chord patterns)

```json
{
  "name": "Alberti",
  "steps_per_bar": 16,
  "steps": [1, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0]
}
```

| Field | Type | Notes |
|-------|------|-------|
| `name` | string | Display name (shown in the Chord menu / preset `chord.pattern`). |
| `steps_per_bar` | int | Length of the `steps` array; 4 steps = 1 beat (16th-note grid). |
| `steps[]` | int array | Chord-degree **bitmask**: `0` = rest, bit0 (`1`) = root, bit1 (`2`) = 3rd, bit2 (`4`) = 5th, bit3 (`8`) = 7th. Combine bits for multi-note steps (e.g. `7` = root+3rd+5th, `15` = all four). Degree 3 (bit3) plays the chord's 7th when present, otherwise the root an octave up on triads. When the rhythm is running, chord-pattern playback is fully quantized: step 0 always waits for the next rhythm step boundary. |
| `sustain_steps[]` | int array (optional) | Parallel array giving each step's length in 16th steps. `0`/absent = percussive (uses `chord.note_duration_ms`). |

### 5.8 The 9 built-in chord patterns

These ship in firmware and are written to `/chordpatterns/` on first boot (and
re-provisioned if you delete one):

| Index | File | Name | Steps |
|-------|------|------|-------|
| 0 | `block.json` | Block | full chord held for the bar |
| 1 | `alberti.json` | Alberti | root-5th-3rd-5th on quarter notes |
| 2 | `broken_up.json` | Broken Up | root-3rd-5th-7th on quarter notes |
| 3 | `broken_down.json` | Broken Down | 7th-5th-3rd-root on quarter notes |
| 4 | `pulse.json` | Pulse | full triad on every 8th note |
| 5 | `oom_pah.json` | Oom-Pah | root, then 3rd+5th, alternating beats |
| 6 | `roll.json` | Roll | root-3rd-5th-3rd on 8th notes |
| 7 | `split.json` | Split | root on beat 1, 3rd+5th+7th on beats 2–4 |
| 8 | `arpeggio.json` | Arpeggio | root-3rd-5th-7th-5th-3rd 16th-note cycle |

### 5.9 Adding your own chord patterns

Like bass patterns: drop any extra `*.json` file into `/chordpatterns/`. They
are loaded after the 9 built-ins (indices 9, 10, …), ordered alphabetically by
filename, named by their `name` field, selected via the Chord menu / `chord.pattern`
in a preset (or `chord_pattern` in the keymap). A preset referencing a deleted or
renamed pattern falls back to the first pattern (Block). The firmware caps the
total at **32** patterns, and skips any file with an empty `steps` array. The
combined pattern budget (72 KB; 48 KB user share) also applies, and the on-device
editor enforces a 4 KB per-file cap. The device can create/edit chord patterns
too (Pattern Edit, §5.10), recording degree bitmasks and sustains.

### 5.10 On-device pattern editing (Pattern Edit)

Rhythm, bass, and chord patterns can be created, recorded, grid-edited, and saved
without a PC. Each type has its own editor screen; there is no Kind selector.
Open the editor with the **Edit Pattern** item in the Chord/Rhythm/Bass menu, or
by default with `Super+F9` (chord), `Super+F10` (rhythm), and `Super+F12`
(bass). These are rebindable `open_menu` actions with menu names
`chord_pattern`/`rhythm_pattern`/`bass_pattern`.

The command screen uses the standard menu navigation: Up/Down/Left/Right or
Volume +/− scroll the list; Enter/Mute selects a value item; while selected the
same keys change the pending value; Enter/Mute applies it and returns to the
list, while Esc/Backspace cancels it and returns to the list. The list order is
Pattern, New, Bars, Quantize, Grid, Record, Preview, Rename, Save, Save As,
Delete, Clear; action items execute on Enter/Mute. Creating a pattern prompts for a name, then Bars, then `Record now?` / `Ent/Bk`.
Declining recording leaves the new pattern selected and confirms with
`New Pat N` / `Created` on the LCD.

| Kind | Record pads (numpad) | Grid cell |
|------|----------------------|-----------|
| Rhythm | `1`–`9`, `0`, `.`, `/`, `*` = the 13 standard pieces | `.` rest / `x` hit / `X` accent |
| Bass | `1`–`5` = root/3rd/5th/6th-or-7th/root+8va; `0` = rest | `.`/`R`/`3`/`5`/`7`/`8`, `-` sustained |
| Chord | `1`–`4` = root/3rd/5th/7th (combine); `0` = clear | hex `0`–`F`, `-` sustained |

- Plain numpad pad keys are live whenever the bass/rhythm/chord editor is open:
  bass pads play root/3rd/5th/6th-or-7th/root+8va (and `0` = rest) with the
  preset bass parameters; rhythm pads play the 13 pieces through the preset DrumMap; chord
  `1`–`4` play root/3rd/5th/7th through the current/last chord, combining while
  held, and `0` releases the live chord. In the chord grid, `1`–`4`/`0` keep
  their bit-toggle editing role. Live pads also record while recording.
  `+`/`−`/`Enter` keep their parameter actions. Chord-grid keys and direct
  parameter-modifying keymap actions remain active; the top-row strum/macro path
  stays disabled. The rhythm editor ignores `RhythmMute` and forces the rhythm
  unmuted; the bass editor ignores `BassEnable` and forces the bass enabled, so
  pads/audition always emit. The rhythm editor restores the preset's original
  mute/enable values when it closes; the bass editor leaves the bass enabled so
  the edited pattern plays through the normal bass engine.
- Recording uses the sequencer's one-bar hi-hat count-in and is quantized at
  capture time (`1/4`, `1/8`, or `1/16`); quantize is not stored in the file.
  Enter/Mute or Esc/Backspace stops a take or preview.
- Rhythm and chord recording are polyphonic; bass is monophonic. Recording is
  **overdub** (untouched steps/tracks are preserved); use **Clear** for a fresh
  take. Held bass/chord pads write `sustain_steps`.
- New patterns inherit the live rhythm meter (16 steps/bar in 4/4, 12 in 3/4).
  Record/preview require the live meter to match the working pattern.
- Files are written as compact `userN.json` in the matching directory and use
  the same schemas above. `New`/`Save As` create user files; `Save` overwrites the
  current file (built-ins require confirmation); only user patterns can be
  renamed or deleted. Built-in files that are deleted or corrupt are
  re-provisioned on boot.
- Limits: 32 total per kind; combined 72 KB all-pattern budget (48 KB user
  share); per-file caps 8 KB rhythm, 4 KB bass, 4 KB chord. The editor rejects a
  save that would exceed them. `bars` is inferred from
  `steps.length / steps_per_bar`.

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

To use the keyboard's media Play/Pause key to toggle the rhythm, add:

```json
{ "keys": ["PlayPause"], "action": { "type": "toggle", "param": "rhythm_enable", "a": 1, "b": 0 } }
```

### 7.3 A custom rhythm

Copy `examples/test_rhythm.json` to `/rhythms/test_rhythm.json`, reboot, then
`F4` to cycle to "Test Groove" (it appears after the 12 built-ins) and `F5` to
play it.
