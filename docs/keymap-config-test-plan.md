# KeybChord Pico — Configurable Keymap & Menu Rework — Hardware Test Plan

Covers the data-driven `/keymap.json` keymap, the reworked F1–F8 defaults, the
inverted menu navigation, the Ctrl/Alt combo shortcuts, and the Alt+F1–F12 drum
mute. Run after flashing `firmware.uf2`.

**Setup:** assembled unit with the keyboard on the PIO-USB host port, DIN →
synth/DAW, and a USB-CDC serial monitor at 115200. For Part F (customization /
validation) you will need to mount the FatFS drive on a PC: power off, hold
`Ctrl`, power on, then edit files on the "KeybChord" mass-storage drive.

**First boot:** if the flash is blank, the unit self-provisions `config.json`,
`/keymap.json`, `/presets/`, and `/rhythms/`. The default keymap file is written
automatically; there is nothing to prepare.

---

## 1. Default F1–F4 (common settings)

- [ ] `F1` cycles chord play mode Held → Press → Arpeggio → Silent → Held.
      LCD line 2 shows the new mode. In Press mode, pressing a chord key plays
      on press and releases on release.
- [ ] `F2` cycles arp mode Up → Down → Up-Down → Alternating → Random (LCD shows
      the mode; audible in Arpeggio play mode).
- [ ] `F3` cycles bass pattern Walking → Whole → Half → … (LCD shows the pattern).
- [ ] `F4` cycles rhythm pattern through the 12 patterns (LCD shows the new
      pattern; enabling rhythm with `F5` plays the new pattern).

## 2. Default F5–F8 (on/off toggles)

- [ ] `F5` toggles **Rhythm On/Off** (drums start/stop; LCD "Rhythm On/Off").
- [ ] `F6` toggles **Rhythm Mute** (drums keep the beat grid but go silent).
- [ ] `F7` toggles **Bass On/Off** (walking bass starts/stops).
- [ ] `F8` toggles **Clock Out** (MIDI clock 0xF8 starts/stops on DIN — confirm in
      the serial MIDI monitor).

## 3. Menus F9–F12

- [ ] `F9`/`F10`/`F11`/`F12` open Chord/Strum/Rhythm/Bass menus. Pressing the
      same key again (or `Esc`) closes. LCD line 1 = title, line 2 = selected
      parameter.
- [ ] `Super`+`F11` opens the **Drum** menu directly (was: only reachable via
      F8-inside-Rhythm). Verify the Drum menu still opens via `F11`→`F8`.

## 4. Inverted menu navigation

- [ ] Open a menu (e.g. `F9` Chord). `Up`/`Down` move the selection to the
      previous/next parameter (LCD line 2 changes). `Left`/`Right` change the
      selected parameter's value (LCD shows the value, then reverts to the menu).
- [ ] Confirm this is the **inverse** of the old behavior: `Up`/`Down` no longer
      change the value, `Left`/`Right` no longer move the selection.
- [ ] In the Chord menu, `Up` from the first parameter wraps to the last
      parameter, and `Down` from the last wraps to the first.
- [ ] F-keys still select parameters directly inside a menu (F1 = first param,
      F2 = second, …); `F8` inside the Rhythm menu still opens the Drum sub-menu.

## 5. Ctrl combos

- [ ] `Ctrl`+`F1` cycles chord voicing Root ↔ Smart (LCD shows Voicing).
- [ ] `Ctrl`+`F2` cycles chord inversion Root → 1st → 2nd → 3rd → Root.
- [ ] `Ctrl`+`F3` cycles chord arp mode (same as `F2`, confirmed on LCD).
- [ ] `Ctrl`+`F4` toggles chord roll between 0 and 50 ms (LCD shows "Chord Roll";
      holding a chord with roll 50 staggers note-ons by 50 ms).
- [ ] `Ctrl`+`F5` toggles the beat LED indicator on/off (the keyboard LED stops
      flashing on beats while off).
- [ ] `Ctrl`+`=` increments chord velocity by 1; `Ctrl`+`-` decrements by 1
      (LCD shows "Chord Velocity"; audible as louder/softer chord attacks).

## 6. Alt / Ctrl combos on the keypad

- [ ] `Ctrl`+`KpEnter` cycles strum layout Full ↔ Limited (LCD "Strum Layout").
- [ ] `Alt`+`KpEnter` cycles strum mode Chord → Scale → Piano (LCD "Strum Mode").
- [ ] `Ctrl`+`Kp+` / `Ctrl`+`Kp-` cycle strum scale up/down (LCD shows the scale).
- [ ] `Alt`+`Kp+` / `Alt`+`Kp-` cycle strum root up/down (LCD shows the root note).
- [ ] `Alt`+`=` / `Alt`+`-` step the bass octave up/down (LCD shows "Bass Octave").

## 7. Alt+F1–F12 — drum mute (dynamic)

- [ ] Load a pattern with ≥3 drums (e.g. Rock 1: kick, snare, hi-hat). Enable
      rhythm with `F5`.
- [ ] `Alt`+`F1` mutes the **1st drum of the current pattern** (Rock 1 = kick).
      LCD shows `Kick Mute` / `On`. The kick drops out of the pattern.
- [ ] `Alt`+`F1` again unmutes; LCD shows `Kick Mute` / `Off`; the kick returns.
- [ ] `Alt`+`F2` / `Alt`+`F3` mute/unmute the 2nd/3rd drum (snare, hi-hat).
- [ ] Switch patterns (`F4`) to one with a different track order (e.g. Bossa Nova:
      kick, rimshot, ride, shaker). `Alt`+`F2` now mutes the rimshot (the 2nd drum
      of that pattern), confirming the mapping follows the current pattern.
- [ ] `Alt`+`F4` on a 3-drum pattern (no 4th drum) does nothing.

## 8. Preset navigation & reserved keys (regression)

- [ ] `Home`/`End` move the preset cursor backward/forward; `Super`+`Home`/`End`
      move banks; `Super`+`1..8` load a slot; `Insert`→`Enter` saves; `Delete`→
      `Enter` clears. (`Enter`/`Backspace`/`Esc` cancel a prompt.)
- [ ] `PrtSc`/`ScLk`/`Pause` = inversions; `=`/`-` = chord octave; `Kp+`/`Kp-` =
      strum octave; `Space` = tap tempo; `PgUp`/`PgDn` = tempo up/down.
- [ ] Chord grid, strum keys (number row / keypad), backtick, and the held
      `Left`/`Down`/`Right` extensions still play/sound as before.
- [ ] `Super`+`Esc` = panic (all notes off on all 16 channels, rhythm stops).
- [ ] `Esc` in the main menu cancels sounding chord/strum notes.

## 9. Custom `/keymap.json` (positive path)

1.  Power off, hold `Ctrl`, power on → the "KeybChord" drive mounts.
2.  Open `/keymap.json` and change the `F1` binding action to
    `{ "type": "inc", "param": "tempo" }`.
3.  Eject the drive, power-cycle without `Ctrl`.
4.  Confirm `F1` now increments tempo (LCD "Rhythm Tempo"), and it no longer
    cycles chord mode. Then revert `F1` to `chord_mode` (or restore the file).

## 10. Validation failure → halt + error log (negative path)

1.  With the drive mounted, edit `/keymap.json` to bind a **reserved** key, e.g.
    `{ "keys": ["Q"], "action": { "type": "cycle", "param": "chord_mode" } }`.
2.  Eject and reboot without `Ctrl`.
3.  Expected: the unit **halts** — LCD shows `Keymap error` / `Remount to fix`
    and no keys produce sound or menu changes.
4.  Power off, hold `Ctrl`, power on → mount the drive and open
    `/keymap_error.log`. Confirm it lists the reserved-key error (one line per
    binding error).
5.  Fix (or delete) `/keymap.json`, eject, reboot without `Ctrl` → normal boot.

Repeat step 1–4 with each of these, confirming a distinct error line each time:

- [ ] Duplicate binding (same key twice).
- [ ] Unknown key name or unknown parameter name.
- [ ] Missing coverage: delete the `Super`+`F11` drum-menu binding (or a
      preset navigation binding) → "missing required binding" error.
- [ ] Malformed JSON (e.g. delete a closing brace).

## 11. First-boot provisioning

- [ ] On an erased flash, after first boot the drive (or serial log) shows
      `/keymap.json` exists and matches the default keymap; the device boots
      normally and all defaults in sections 1–8 work.

## 12. Voicing modes (Root / Smart / Down / Up)

- [ ] Open the Chord menu (`F9`), select **Voicing** (F3), and cycle with `Right`
      through Root → Smart → Down → Up → Root. `Ctrl`+`F1` cycles the same modes.
- [ ] **Down**: in the Chord menu set Voicing to `Down`. Play C major, then G
      major — the G chord voices *below* the C voicing (e.g. `B3 D4 G4` instead
      of root position). Play a few chords walking down the circle of fifths:
      each next chord is voiced lower, never higher.
- [ ] **Up**: set Voicing to `Up`. Repeat the C→G walk; each next chord voices
      *above* the previous.
- [ ] The first chord after switching mode (or after reset) plays root position
      at the configured octave (the direction applies once a previous voicing
      exists).

## 13. Voicing reset (`Backspace`, main screen)

- [ ] Play a couple of chords in `Smart`/`Down`/`Up` so the voice has walked away
      from root position.
- [ ] From the main screen (no menu/cursor/prompt), press `Backspace`. LCD shows
      a brief "Voicing Reset".
- [ ] The next chord plays in root position at the configured octave (not
      voice-led from the previous chord).
- [ ] While holding/sustaining a chord, press `Backspace`: the current chord is
      unchanged; the reset applies to the next chord.
- [ ] `Backspace` still cancels the save/clear prompt (Section 8) — it only
      resets voicing from the plain main screen.

## 14. Preset naming

- [ ] With no custom name, the LCD top-left shows `B#:P#` (e.g. `B1:P1`) when
      browsing presets (`Home`/`End`) and, in the main view, after ~3 s with no
      chord played.
- [ ] While a chord has been played in the last 3 s, the top-left shows the
      chord name (as before).
- [ ] **Name a preset:** `Insert` → `Enter` (save) → the LCD enters "Rename".
      Type with the chord-grid keys (`Q`→`q`, … `A`→`a`, … `Z`→`z`, `Tab`→space);
      `Backspace` deletes the last character; `Enter` commits the name; `Esc`
      cancels (keeps the previous name).
- [ ] After committing, browse to that slot (`Home`/`End`): the top-left shows
      the new name. Reboot and confirm the name persists (it is stored in
      `/presets/bank*.json`).

---

### Notes

- The default `Ctrl`/`Alt` combo layout is defined in `keymap_config.cpp`
  (`KeymapConfig::defaults()` and `keymapDefaultJson()`). If a physical combo
  does not behave as this document describes, read `/keymap.json` on the drive —
  it is the source of truth for what the firmware loads.
- Drum mute targets only **standard GM** drums (kick, snare, hi-hat, open-hat,
  rimshot, clap, crash, ride, bongo, conga-lo, conga-hi, clave, shaker). A
  pass-through (non-standard) track shows `No mute` and is unaffected.
- Voicing `Down`/`Up` are directional: `Down` picks the nearest voicing whose
  notes are all ≤ the previous voicing (≥1 strictly lower); `Up` is the
  symmetric ≥ case. When no candidate exists (range edge) it falls back to the
  nearest overall voicing.
