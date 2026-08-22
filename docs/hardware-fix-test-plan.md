# KeybChord Pico — M11 Hardware-Fix Test Plan

Validates the fixes made in response to `hw-test-feedback.txt` (see
`KeybChord_Pico_Roadmap.md` §M11). Run after flashing the latest `firmware.uf2`.

**Setup:** assembled unit (spec §2.2) with the keyboard on the PIO-USB host port,
DIN → synth/DAW, and a USB-CDC serial monitor at 115200 for the MIDI monitor /
debug log. A fresh flash (`picotool erase -a` or `flash_nuke.uf2`) is optional to
confirm first-boot defaults.

Each fix maps to the feedback line it addresses. Work top to bottom; each
section is independent.

---

## 1. Chord roll is now audible (feedback: "roll isn't large enough")

- [ ] `F9` → `F7` (Roll) → hold `+` (auto-repeat) up to +2000 ms.
- [ ] Hold a chord: note-ons stagger ascending with a clearly audible delay
      between each note (at +2000 the top note lands ~2 s after the first on a
      3-note chord).
- [ ] Set −2000: note-ons stagger descending.
- [ ] Set 0: notes sound together again.
- [ ] Confirm the LCD shows the value in the ±2000 range (step 10).

## 2. Min-interval + inversion is correct for every root (feedback: "E is all wrong")

- [ ] `F9` → `F9` (Min Interval) → 4. `F9` → `F10` (Inversion) → 1st.
- [ ] Play **C major** (`T`): expect `E G C`-family spread (E lowest).
- [ ] Play **F major** (`R`): expect **A in the bass** (the 3rd), NOT the root F.
- [ ] Play **Bb major** (`E`): expect **D in the bass** (the 3rd), NOT Bb.
- [ ] Set Min Interval → 0 and confirm the same chords are close-voiced again
      (no change to inversion bass).
- [ ] Confirm in the MIDI monitor that the lowest note of each inversion is the
      chord's 3rd, regardless of root.

## 3. 2nd and 3rd inversions are distinct (feedback: "2nd and 3rd are the same")

- [ ] `F9` → `F10` (Inversion). Set Min Interval → 0 (root position).
- [ ] Play **C major** with `ScLk` (2nd inversion): `G` is the lowest note.
- [ ] Play **C major** with `Pause` (3rd inversion): a **different** voicing
      (root position raised an octave — `C` is the lowest, an octave up), not
      identical to 2nd inversion.
- [ ] Play a **7th chord** (e.g. `T`+`B` = Cmaj7) with `Pause`: the **7th** (`B`)
      is the lowest note.
- [ ] Save (`Insert`→`Enter`) and reload (`Super+1..8`): inversion persists.

## 4. Held extensions don't re-attack the chord (feedback: "shouldn't retrigger the whole chord")

- [ ] Hold a chord (`T` = C major) and keep it sounding.
- [ ] Press and hold `Left` (add9): only the added 9th (`D`) is heard as a new
      note — the C/E/G of the base chord do **not** re-attack (no "chop").
- [ ] Release `Left`: the 9th drops out; the base chord keeps sustaining without
      re-attacking.
- [ ] Repeat for `Down` (add11) and `Right` (add13).
- [ ] Verify on a latched Held chord (release the chord key first, then press
      the arrow): same add/remove behavior.

## 5. All drum instruments are editable (feedback: "only kick/snare/hats editable")

- [ ] `F11` → `F8` (Drums). `F1`–`F8` cover Kick/Snare/Hi-Hat/Open-Hat (note +
      velocity); use `Left`/`Right` to reach the remaining pieces.
- [ ] Scroll through and confirm the full list is present: Rimshot, Clap, Crash,
      Ride, Bongo, Conga Lo, Conga Hi, Clave, Shaker (each with note + velocity).
- [ ] Select **Ride** note and change its code (`+`/`-`): it auditions once.
- [ ] Load the **Bossa Nova** pattern (`F7` to cycle to it, then `F5` to run) and
      remap the Ride; confirm the ride in the pattern now sounds the new code.
- [ ] Repeat a velocity override on one piece and confirm non-zero forces that
      velocity while `0` (Auto) follows the pattern accents.
- [ ] `Insert`→`Enter` save; `Super+1..8` reload: drum note/velocity edits persist.

## 6. Num Lock, `/`, `*` are strum keys (feedback: "add num lock, /, and *")

- [ ] Ensure strum layout is **Full** (`F10` → `F4`).
- [ ] With a chord selected, press numpad **`/`**, **`*`**, and **Num Lock**:
      each strums a note (top 3 notes of the pool).
- [ ] Confirm `Keypad +` / `Keypad -` still adjust strum octave (not strum).
- [ ] Toggle Num Lock LED on/off and confirm strum behavior is unchanged
      (FR-S6).

## 7. Walking bass is in time with the drums (feedback: "bass slightly before drums")

- [ ] `F3` to enable bass; `F5` to enable rhythm; hold a chord.
- [ ] Listen carefully: each bass "thump" lands **on** the drum beat (the
      downbeat kick and the bass root are simultaneous), not one 16th-note early.
- [ ] Confirm in the MIDI monitor that the bass note-on and the kick note-on for
      the same beat appear at essentially the same timestamp.
- [ ] Switch to the Waltz pattern (`F7`): bass still locks to the 3-beat bar.

## 8. Arpeggio stays in time with the rhythm (feedback: "arp should also be synced")

- [ ] `F1` to Arp mode. Enable rhythm (`F5`). Hold a chord.
- [ ] Confirm arp notes step in time with the rhythm (one per rhythm step), with
      no drift or freeze.
- [ ] Toggle rhythm off (`F5`) mid-arp: the arp continues free-running instead of
      stopping.

## 9. Configurable bass patterns (Upgrade-Plan: "bass should be configurable")

- [ ] `F12` → `F6` (Pattern). Cycle through the patterns with a chord held and
      rhythm running; confirm each:
  - **Walking** (default): root-3rd-5th-6th/7th per beat.
  - **Whole**: root sustained for a whole bar.
  - **Half**: root on beats 1 and 3, each sustaining half a bar.
  - **Quarter**: root on every beat.
  - **Half Alt**: root/5th alternating half notes.
  - **Quarter Alt**: root/5th alternating quarter notes.
  - **3/4 Alt**: root on beat 1, 5th on the last beat.
  - **Hold**: root sustains while the chord key is held (Press/Arp) or
    indefinitely (Held); stops when the chord is released.
- [ ] Confirm bass is silent when rhythm is stopped (all except Hold) and in
      Silent chord mode.
- [ ] `Insert`→`Enter` save; `Super+1..8` reload: `bass.pattern` persists.
- [ ] Inspect `presets/bank*.json`: `bass.pattern` and the extended `drums`
      block are present.

## 10. Regressions

- [ ] Panic `Super+Esc` silences all channels and clears state.
- [ ] Hot-plug the keyboard mid-play: notes release, device keeps running.
- [ ] `Esc` in the main menu cancels chord/strum/bass but not rhythm.
- [ ] Beat LED (`F4`) flashes on the beat; removing the keyboard/LED does not
      affect timing or MIDI.
