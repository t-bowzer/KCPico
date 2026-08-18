# KeybChord Pico — M10 Hardware Test Plan

Covers the chord-engine upgrades, walking bass, strum mode/duration, rhythm drum
edits, and keymap refactor. Run after flashing `firmware.uf2` (see `scripts/install.md`).

**Setup:** assembled unit (spec §2.2) with the keyboard on the PIO-USB host port,
DIN → synth/DAW, and a USB-CDC serial monitor at 115200 for the MIDI monitor /
debug log. Optional: erase flash first (`picotool erase -a` or `flash_nuke.uf2`)
to verify first-boot self-provisioning.

---

## 1. Keymap & menus (AC-13, AC-27, AC-23)

- [ ] `F9` / `F10` / `F11` / `F12` open Chord / Strum / Rhythm / Bass menus.
      Pressing the same key again (or `Esc`) closes. LCD line 1 = menu title,
      line 2 = selected parameter.
- [ ] Main-menu hotkeys: `F1` cycles play mode, `F2` toggles voicing, `F3` toggles
      bass, `F4` toggles beat LED, `F5` rhythm on/off, `F6` clock out, `F7`
      pattern, `F8` mute.
- [ ] `PrtSc` / `ScLk` / `Pause` show Inversion 1st / 2nd / 3rd on the LCD and
      invert the next triggered chord.
- [ ] `Ctrl` / `Alt` / `Menu` keys have **no** runtime effect.
- [ ] **Boot key:** power off, hold `Ctrl`, power on → the FatFS drive mounts as
      "KeybChord"; the CDC serial log is still present. Without `Ctrl`, no drive.

## 2. Chord roll (AC-24)

- [ ] `F9` → `F7` (Roll) → `+` to +100 ms. Hold a chord: note-ons stagger
      ascending. Set −100: descending. Note-offs release together on chord change.

## 3. Minimum note count (AC-25)

- [ ] `F9` → `F8` (Min Notes) → 4. Play C major: `C E G C`. Set 5: `C E G C E`.
      Cmaj7 at 5: `C E G B C`.

## 4. Minimum interval spread (AC-26)

- [ ] `F9` → `F9` (Min Interval) → 5. Play C major: hear the wide `C3 G3 E4`
      spread. Set 0 → back to close `C E G`.

## 5. Manual inversions (AC-27)

- [ ] `PrtSc` then a chord: the 3rd is the lowest note. `ScLk` → 5th lowest,
      `Pause` → 7th lowest. Save (`Insert`→`Enter`) and reload; inversion persists.

## 6. Arpeggio modes (AC-28)

- [ ] `F1` to Arp mode. `F9` → `F11` (Arp Mode) → cycle Up / Down / Up-Down /
      Alternating / Random; confirm each step order on the synth.
- [ ] Enable rhythm (`F5`): arpeggio phase-locks to the beat (AC-6).

## 7. Held extensions (AC-13)

- [ ] Hold a chord and hold `Left`: add9 joins the sounding chord; release `Left`:
      add9 drops. Same for `Down` (add11) and `Right` (add13). Also verify on a
      latched Held chord (chord keys released).

## 8. Walking bass (AC-29)

- [ ] `F3` to enable bass; `F5` to enable rhythm; hold a chord. Hear
      root → 3rd → 5th → 6th/7th on each beat.
- [ ] Switch to the Waltz pattern (`F7`): bass cycles 3 beats.
- [ ] `F12` menu: octave / duration / velocity / channel are honored.
- [ ] Bass is silent when rhythm is stopped (`F5` off) and in Silent chord mode.

## 9. Strum updates (AC-4, AC-18, FR-S7/S8)

- [ ] **Duration minimum:** hold a numpad key — the note sustains past the
      duration; a quick tap sounds for the full minimum duration.
- [ ] `F10` → `F5` (Mode) → Scale; `F6` root, `F7` scale/mode; strum plays the
      scale. → Piano; strum is chromatic. → Follow-Chord restores the chord pool.
- [ ] Numpad strum is identical with Num Lock on and off.

## 10. Rhythm drum edits (FR-R9)

- [ ] `F11` → `F8` (Drums) → `F1` Kick; `+`/`-` changes the code and **auditions**
      it once. Set a velocity (`F2`) → non-zero forces that velocity; `0` (Auto)
      follows the pattern accents. Confirm note codes/velocities in the MIDI monitor.

## 11. Preset round-trip (AC-7, AC-20, AC-21)

- [ ] Edit chord/strum/bass/rhythm/drum params → `Insert` → `Enter` save →
      `Super+1..8` reload → all params persist. On-device `presets/bank*.json`
      should show `chord_roll_ms`, `min_notes`, `min_interval`, `inversion`,
      `arp_mode`, `strum.mode`/`root_pc`/`scale_type`, a `bass` block, and
      `drums.*_vel`.
- [ ] Load a legacy preset with `play_mode: 3` (old Rhythm) → behaves as
      Arpeggio, no crash (NFR-9).

## 12. Regressions

- [ ] Panic `Super+Esc` silences all channels and clears state.
- [ ] Hot-plug the keyboard mid-play: notes release, device keeps running.
- [ ] `Esc` in the main menu cancels chord/strum/bass but not rhythm.
- [ ] Beat LED (`F4`) flashes on the beat; removing the keyboard/LED does not
      affect timing or MIDI.
