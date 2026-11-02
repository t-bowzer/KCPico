# Hardware Test Plan — Chord Sequencer (M14)

Step-by-step verification for the v0.2.0 **chord sequencer**, preset-count
change, and configurator sequence editor. Follow the sections in order and record
results in the checklist at the end.

> **Scope.** This plan verifies: 10 sequence slots per preset, 1/2/4/8-bar
> recording, looping playback synced to the rhythm clock, live-chord override,
> recorded/live settings mixing, per-preset storage, clearing, the 40-preset
> migration, and the configurator editor. It does not re-verify the general chord
> engine, rhythm engine, CH559 input, USB MIDI, or DIN/TRS wiring.

---

## 1. Prerequisites

Hardware:
- Assembled KeybChord Pico running v0.2.0 firmware.
- Hosted USB keyboard, LCD1602, and a MIDI monitor on **USB MIDI** or **DIN**
  (a DAW, MIDI-OX, or the repo's `tools/midi_test` harness).
- A USB data cable for the Pico's native USB port (CDC log + USB MIDI + MSC).

Software:
- PlatformIO for builds/tests.
- Python 3.9+ with the configurator dependencies (`pip install -e ".[dev]"`
  from `configurator/`, or run the packaged configurator).
- Optional: a folder backup of the device before upgrading.

> **Important v0.2 migration:** the preset count changed from **80 (10 banks)**
> to **40 (5 banks)**. On first boot, the firmware removes
> `/presets/bank6.json` … `/presets/bank10.json` (and any legacy sequence files)
> to reclaim flash. Back up banks 6–10 with the configurator **before**
> upgrading if you need them. `startup_preset` values above B5 fall back to
> B1:P1.

---

## 2. Controls used in this plan

These are the **default** keymap bindings. If `keymap.json` was customized,
substitute your bindings; `Ctrl+1`…`Ctrl+0` sequence selection is hard-coded.

| Control | Default binding | Meaning |
|---|---|---|
| Rhythm on/off | `F5` | Must be **on** for recording/playback; it may be muted. |
| Rhythm mute | `F6` | Mutes drums while the clock keeps running. |
| Bass on/off | `F7` | Keep off for a clean chord-only test. |
| Clock Out | `F8` | MIDI clock transmit; enabling waits for the next beat boundary. Double-tap `F8` to resync (MIDI Start at the next boundary). |
| Record | `Ctrl+F9` | Arm/stop recording. If rhythm is off, enables and mutes it automatically. Prompts before overwriting an existing slot. Mutes the pattern, plays a hi-hat count-in, then records. |
| Play/Stop | `Ctrl+F10` | Arm/stop playback. If rhythm is off, enables and mutes it automatically. Starts at the next bar. |
| Clear sequence | `Ctrl+F11` | Prompt; `Enter` confirms, `Backspace`/`Esc` cancels. |
| Select sequence | `Ctrl+1` … `Ctrl+9`, `Ctrl+0` | Slots 1–10. `Ctrl+0` = slot 10. |
| Seq Edit menu | `Super+F12` | Opens the Slot / Bars / Settings menu. |
| Save preset | `Insert` | Saves preset parameters (sequences already auto-save). |
| Clear preset | `Delete` | Clears preset parameters **and removes its sequence file**. |
| Preset cursor | `Home` / `End`, then `Enter` | Browse/load presets. |
| Direct preset load | `Super+1`…`Super+8` | Load slot N in the current bank. |
| Cancel live chord / stop sound | `Esc` (main screen) | Clears a latched live chord so sequence playback resumes. |
| Panic | `Super+Esc` | Stops sequence playback and all sound. |

### 2.1 Expected LCD messages

| Situation | Line 1 | Line 2 |
|---|---|---|
| Seq Edit menu | `Seq Edit` | `Slot` (or `Bars` / `Settings`) |
| Slot value | `Sequence Slot` | `1/10` |
| Bars value | `Sequence Bars` | `4` |
| Settings value | `Sequence Settings` | `Recorded` or `Live` |
| Record armed (waiting) | `Rec arm` | `Wait bar` |
| Count-in | `Count-in` | `1 bar` |
| Recording started | `Recording 1/4` | (blank) |
| Recording bar N | `Recording N/4` | (blank) |
| Recording saved | `Seq saved` | (blank) |
| Play armed | `Play arm` | `Wait bar` |
| Playback started | `Seq 1` | `play` |
| Playback stopped | `Seq stop` | (blank) |
| Slot selected | `Seq N` | (blank) |
| Empty slot | `Seq empty` | (blank) |
| Event cap reached | `Seq full` | (blank) |
| Overwrite confirmation | `Overwrite Seq in` | `slot N? Ent/Bk` |
| Clear confirmation | `Clear Seq N?` | `Enter=Yes Bk=No` |
| Sequence cleared | `Seq cleared` | (blank) |
| MSC drive active + write command | `USB drive active` | `Seq write off` |

> The LCD is 16×2. Timestamps/values may be shortened or split across two lines.
> The count-in/recording screen is persistent: it should stay on `Rec arm` /
> `Wait bar`, then `Count-in` / `1 bar`, then `Recording N/M` without flashing
> back to the idle screen between bar updates. Sequence write commands (record, clear, opening Seq Edit) are
> blocked while the Mass Storage drive is presented; eject the drive and reboot
> first.

---

## 3. Build and automated checks

Run these on the development host before hardware testing.

| # | Check | Command / action | Expected |
|---|---|---|---|
| 1 | Native unit tests | `cd keybchord && pio test -e native` | All tests pass. |
| 2 | Firmware build | `cd keybchord && pio run -e pico` | Build succeeds. |
| 3 | Configurator tests | `cd configurator && python -m pytest -q` | All tests pass. |
| 4 | Flash budget guard | Native test `Sequence.WorstCaseFileFitsBudget` | Passes; a full 10-slot/64-event file is under 7 KB. |
| 5 | Pico size | Build output | Firmware fits; note flash/RAM usage. |

---

## 4. Migration and preset-count checks

| # | Check | Steps | Expected |
|---|---|---|---|
| 6 | Backup before upgrade | Put the old device in MSC mode (hold `Ctrl` at power-on); copy the `KeybChord` drive to a folder; eject. | Folder contains `presets/bank1.json`…`bank10.json`. |
| 7 | Flash v0.2.0 | Build/flash `firmware.uf2` (BOOTSEL or Esc-at-boot). | Device boots; LCD shows the idle screen. |
| 8 | Legacy banks removed | Inspect the drive in MSC mode (hold `Ctrl` at power-on). | `bank6.json`…`bank10.json` are gone; `bank1`…`bank5` remain. |
| 9 | 40-preset navigation | `Home` repeatedly; watch B#:P# wrap. | Wraps within banks 1–5 only (B5:P8 → B1:P1). |
| 10 | Old startup preset | Set `"startup_preset": "B10:P8"` in `config.json`, reboot. | Falls back to B1:P1 (check the debug log). |
| 11 | Configurator warning | Open a folder containing legacy banks 6–10. | Warning explains only banks 1–5 are loaded and to back up first. |

> If step 6 was skipped, banks 6–10 are intentionally unrecoverable after the
> first v0.2 boot. This is the accepted flash-budget tradeoff.

---

## 5. Menu and slot selection

| # | Check | Steps | Expected |
|---|---|---|---|
| 12 | Open Seq Edit | Press `Super+F12`. | LCD `Seq Edit` / `Slot`. |
| 13 | Browse params | Use arrows/volume. | Cycles `Slot` → `Bars` → `Settings` → `Slot`. |
| 14 | View Slot | `Enter`/`Mute` on Slot. | `Sequence Slot` / `1/10`; arrows change it. `Enter` commits; `Backspace` restores. |
| 15 | View Bars | Select Bars. | `Sequence Bars` / `4`; arrows cycle 1 → 2 → 4 → 8. |
| 16 | View Settings | Select Settings. | `Sequence Settings` / `Recorded` or `Live`; arrows toggle. |
| 17 | Select slots | Press `Ctrl+1`, `Ctrl+3`, `Ctrl+0`. | LCD `Seq 1`, `Seq 3`, `Seq 10`; no strum note is produced. |
| 18 | Strum-release safety | Hold a number key (strum), press `Ctrl` while holding, release the number, release `Ctrl`. | No stuck note; the sequence slot selection changes. |
| 19 | Menu timeout | Leave the Seq menu unselected for 10 s. | Menu auto-exits to idle. A selected parameter never times out. |

---

## 6. Record a sequence

Use a simple 4-bar take first.

1. Load a preset (e.g. B1:P1).
2. Press `F5` to enable rhythm, or skip this: record/play enables the rhythm
   automatically and mutes the drums if it was off. You can leave the pattern
   unmuted; the count-in/recording guide overrides it automatically.
3. Press `F7` to make sure bass is off.
4. Press `Ctrl+1` to select sequence slot 1.
5. Press `Ctrl+F9`; the pattern mutes and the LCD shows `Rec arm` / `Wait bar`.
   At the next bar, exactly one bar of count-in hi-hats plays (`Count-in` /
   `1 bar`), then recording starts.
6. Play chord changes on the grid:
   - bar 1: `T` (C major)
   - bar 2: release `T`, press `Y` (G major)
   - bar 3: release `Y`, press `U` (D major)
   - bar 4: release `U`, press `I` (A major)

| # | Check | Expected |
|---|---|---|
| 20 | Arm + mute | Immediately after `Ctrl+F9`: `Rec arm` / `Wait bar`. If rhythm was off, it is now enabled and muted. The normal rhythm pattern stops, and there are **no hi-hat beats yet**. |
| 21 | Exactly one count-in bar | At the next bar boundary: `Count-in` / `1 bar` and exactly one bar of closed hi-hat quarter notes (downbeat accented). No hats play before this bar. The LCD does **not** flash back to idle. |
| 22 | Start on bar | After the one-bar count-in: `Recording 1/4`; the hi-hats continue. |
| 23 | Bar progress | Each bar: `Recording 2/4`, `3/4`, `4/4`. The screen stays on the recording line between bars (test at 40 BPM to make the gaps long). |
| 24 | Auto-stop | After bar 4: `Seq saved`; the hi-hat guide stops and the normal pattern resumes. The file `/sequences/b1p1.json` exists. |
| 25 | Chord-only recording | Strum the number row during recording; those notes are **not** added to the sequence (only chord-grid changes are). |
| 26 | Quantization | Press a chord slightly before/after a beat. On playback it lands on the nearest 16th. |
| 27 | Held-chord start | Arm, hold a chord during the count-in, and keep holding when `Recording 1/N` appears. | The held chord becomes step 0. |
| 28 | Stop early | Press `Ctrl+F9` mid-take. | Recording stops, the partial take is saved (`Seq saved`), and the normal pattern resumes. |
| 29 | Rhythm disabled | Press `F5` to disable rhythm, then `Ctrl+F9` on an empty slot. | Rhythm is enabled and muted automatically; `Rec arm` / `Wait bar`, the count-in, and recording all proceed. |
| 30 | Pattern mute restored | Record with the pattern unmuted, then with it muted (`F6`). | The guide hats play in both cases; after the take the pattern returns to its previous mute state. |
| 30a | Overwrite prompt | Record into a slot that already has events, then press `Ctrl+F9`. | LCD `Overwrite Seq in` / `slot N? Ent/Bk`; nothing is armed until confirmed. |
| 30b | Overwrite cancel | On the overwrite prompt, press `Backspace`; repeat with `Esc`. | Prompt closes; the existing sequence is unchanged; no recording starts. |
| 30c | Overwrite confirm | On the overwrite prompt, press `Enter`; repeat with `Mute`. | The count-in/recording starts and the old take is replaced only after confirmation. |

> If the LCD is in the Seq menu, transient messages revert to the menu after the
> configured display timeout; the action still occurs.

---

## 7. Playback and looping

| # | Check | Steps | Expected |
|---|---|---|---|
| 31 | Arm playback | `Ctrl+F10`. | `Play arm` / `Wait bar`. |
| 32 | Start/loop | Wait for the downbeat. | `Seq 1` / `play`; MIDI monitor shows the recorded chords repeating every bar. |
| 33 | Step timing | Compare the MIDI timestamps to the rhythm beat. | Chord changes land on the recorded 16th steps (within the normal main-loop jitter). |
| 34 | Stop | Press `Ctrl+F10` again. | `Seq stop`; all sequence notes receive note-offs. |
| 35 | Empty slot | Select an empty slot (`Ctrl+2`) and press `Ctrl+F10`. | `Seq empty`; no playback. |
| 36 | Rhythm disabled | Press `F5` off, then `Ctrl+F10` on a recorded slot. | Rhythm is enabled and muted automatically; `Play arm` / `Wait bar`, then playback starts at the next bar. |
| 37 | Rhythm stopped mid-play | Press `F5` off while playing. | Playback stops; `Rhythm off`; notes release. |
| 38 | Bass/strum independence | Playback a sequence and strum the numpad. | The strum is live and is never recorded/replayed. |
| 38a | Strum follows sequence | Play a sequence whose chords differ from the last live chord, then strum the numpad during playback. | Strummed notes match the sequence chord currently sounding, not the last physically pressed chord. Strum notes are still not recorded/replayed. |
| 39 | Preset load during playback | Load another preset (`Super+2` or cursor + `Enter`). | Old playback stops; the new preset's sequences load. |
| 39a | Select a new slot during playback | While a sequence loops, press `Ctrl+2` (or another slot). | The current sequence finishes its current loop; the new slot starts on its downbeat and continues looping. The old chord is released at the switch. Selecting an empty slot switches and stops with `Seq empty`. |

---

## 8. Beat LED and MIDI clock sync

| # | Check | Steps | Expected |
|---|---|---|---|
| 40 | LED on the beat | Enable rhythm (`F5`) and watch the keyboard LED against the hi-hat guide or a drum hit. | The LED pulses on the quarter-note beat, not between beats. The count-in hats and LED agree. |
| 41 | Count-in alignment | Arm recording (`Ctrl+F9`). | The first hi-hat and the LED flash on the same beat; the downbeat hi-hat is accented. |
| 42 | MIDI clock start phase | Connect a DAW/device, set it to external MIDI clock, then enable Clock Out (`F8`). | The first incoming clock starts on the next beat boundary; the device's beat/bar aligns with the LED and the hi-hat guide. Clock Out sends clocks only (no automatic Start/Stop). |
| 43 | Toggle clock out off/on | Toggle `F8` off and on several times at random moments. | Switching off stops clocks immediately and sends no transport message. Switching on waits for the next beat boundary, then streams clocks. |
| 43a | Clock Resync after local stop | With Clock Out on, press Stop on the synth, then Play again. If the beat is offset, double-tap `F8`. | Double-tap ensures Clock Out is on and sends MIDI Start at the next beat boundary, immediately before that clock. The synth's beat re-aligns with the LED. |
| 44 | Clock out while rhythm is off | Disable rhythm (`F5`) but leave Clock Out on. | Clock and LED keep running on the master timebase; the external beat stays aligned. |
| 45 | Re-enable rhythm | Re-enable rhythm (`F5`). | The next downbeat lands on a clock beat, and the LED/hats stay aligned. |
| 46 | Tempo change | Change tempo with `PgUp`/`PgDn` while the LED and external clock run. | The pattern re-slaves to the next beat (a deliberate restart), the LED stays on the beat, and the external device follows the tempo. If the LED visibly shifts off the beat after a tempo change, record it as a failure and report the tempo values. |

> Clock Out is clock-only (no automatic Start/Stop). Double-tap the Clock Out
> key after stopping/restarting the slave locally to send MIDI Start at the next
> beat boundary. A device that ignores incoming clocks or transport messages
> cannot be phase-aligned by this test.

## 9. Live-chord override

Record a sequence with two clearly different chords, e.g. slot 2:

- step 0 (bar 1): C major (`T`)
- step 16 (bar 2): G major (`Y`)
- bars = 4

Then:

| # | Check | Steps | Expected |
|---|---|---|---|
| 47 | Start playback | `Ctrl+F10`, wait for the downbeat. | `Seq 2` / `play`; C major sounds on step 0. |
| 48 | Live chord takes over | Press `T` (or another chord) before the step-16 event. | The live chord sounds; the sequence does not change to G at step 16 while live is active. |
| 49 | Latched live chord | Release the live keys (Held mode latches). | The live chord stays; sequence events stay suppressed. |
| 50 | Resume | Press `Esc` to clear the live chord (or press another live chord and clear it). | The sequence resumes at the next event; G sounds on the next step-16 event/loop. |
| 51 | Panic | `Super+Esc` during playback. | Playback and all sound stop; rhythm is disabled. |
| 52 | Hot-unplug | Unplug the keyboard during playback, then replug. | Playback stops cleanly (no stuck MIDI notes); keyboard recovers. |

> Live override is monophonic at the chord engine: the live chord replaces the
> sequence chord. The sequence keeps advancing silently and resumes at the next
> event after the live chord ends. In Held mode you must clear the latched live
> chord (`Esc`) before the sequence becomes audible again.

---

## 10. Bars and settings

| # | Check | Steps | Expected |
|---|---|---|---|
| 53 | Change bars | `Super+F12`, Bars, set `2`, `Enter`. | `Bars` / `2`; the file updates. Events beyond 2 bars are dropped. |
| 54 | Bar choices | Cycle Bars. | Only 1, 2, 4, 8. |
| 55 | Record with 1 bar | Set Bars=1, record one chord. | Playback loops every bar. |
| 56 | Record with 8 bars | Set Bars=8, record. | Playback loops every 8 bars. |
| 57 | Global Settings = Recorded | Set Settings=Recorded; play; change a chord parameter live (e.g. `Ctrl+=` velocity). | The change is heard **only** if that setting's Live bit is set in the configurator; otherwise the recorded snapshot wins. |
| 57a | Recorded play mode | Record with chord mode `Arpeggio` (and a distinctive arp rate), set Settings=Recorded, then play. | The sequence arpeggiates with the recorded arp mode/rate, even if the live chord mode later changes. |
| 58 | Global Settings = Live | Set Settings=Live; play; change velocity/voicing live. | All captured settings follow the live preset values, including play mode and arp rate; channel stays live. |
| 58a | Live play mode | Set Settings=Live; play; switch the live chord mode between Held and Arpeggio. | Playback follows the live play mode after each sequence event. |
| 59 | Settings reset | Reboot. | The global Settings switch returns to `Recorded` (runtime-only). |

### 9.1 Per-setting Live bits (configurator)

1. Put the device in MSC mode (hold `Ctrl` at power-on).
2. Open the device in the configurator.
3. Select the preset → **Sequences** tab → the recorded slot.
4. Tick **Live** beside e.g. **Velocity** (leave others unticked).
5. Save to device, eject, and reboot.
6. On the device, set Settings=Recorded and play. Change chord velocity live.
7. Verify velocity follows the live value while the other settings still use the
   recorded snapshot.

| # | Check | Expected |
|---|---|---|
| 60 | Per-setting Live | The ticked setting follows live; unticked settings use the snapshot. |
| 61 | Capture settings | In the configurator, **Capture settings from preset** fills the snapshot from the preset's chord parameters. |
| 62 | Captured play mode | The snapshot includes play mode and arp rate. An old 11-value recorded array loads as Held/16th. |

---

## 11. Clear, persistence, and preset association

| # | Check | Steps | Expected |
|---|---|---|---|
| 63 | Clear prompt | `Ctrl+F11`. | `Clear Seq N?` / `Enter=Yes Bk=No`. |
| 64 | Cancel clear | `Backspace` or `Esc`. | Prompt closes; sequence unchanged. |
| 65 | Confirm clear | `Ctrl+F11`, `Enter`. | `Seq cleared`; the selected slot is empty; the file is removed if all slots are empty. |
| 66 | Per-preset slots | Record slot 1 in B1:P1; load B1:P2; check its slot 1. | B1:P2's slot 1 is independent/empty. |
| 67 | Return to preset | Load B1:P1 again. | Its sequences are still there. |
| 68 | Save preset | `Insert`, `Enter`, finish naming. | Preset parameters save; sequences are already saved and are not duplicated/copied. |
| 69 | Clear preset | `Delete`, `Enter` on B1:P1. | Preset resets; `/sequences/b1p1.json` is removed. |
| 70 | No copy action | Inspect the configurator preset UI. | There is no "copy sequence" action; sequences stay tied to bank/slot. |
| 71 | MSC write block | Hold `Ctrl` while powering on (MSC drive active), then press `Ctrl+F9`, `Ctrl+F11`, or `Super+F12`. | `USB drive active` / `Seq write off`; no filesystem writes. Eject/reboot to resume. |

> To inspect files: power off, hold `Ctrl`, power on, open the `KeybChord` drive,
> and look in `/sequences/`. Ejecting reboots the firmware automatically.

---

## 12. Event cap and storage stress

| # | Check | Expected |
|---|---|---|
| 72 | 64-event cap | A sequence can hold at most 64 events. Adding a 65th shows `Seq full` (on device) or is rejected by the configurator. |
| 73 | Duplicate steps | Writing two events on the same step keeps the last one (configurator/firmware). |
| 74 | Compact files | A full 10-slot file is compact JSON (one line) and under ~7 KB. |
| 75 | Worst-case budget | Native `Sequence.WorstCaseFileFitsBudget` passes; fill several presets and check the drive has free space. |
| 76 | Corrupt file | Corrupt a `/sequences/*.json` file (invalid JSON), reboot. | The preset loads with empty sequences; no crash; the firmware logs the issue. |
| 77 | Missing file | Delete a sequence file, reboot. | Empty sequences; recording creates a new file. |
| 78 | 40-preset layout | Confirm only `bank1`…`bank5` are written/provisioned. | 5×8 = 40 presets. |

---

## 13. Configurator editor

1. Put the device in MSC mode; open it in the configurator.
2. Select a preset → **Sequences**.
3. Choose a slot; set Bars.
4. Click a step; pick root, type, add9/add11/add13; **OK**.
5. Right-click a step to clear it.
6. Use **Capture settings from preset**; adjust the recorded settings; tick Live
   where desired.
7. **Clear slot** (confirm).
8. Save to device; eject; verify playback on the device.
9. Run **Validate** on the folder/tree.

| # | Check | Expected |
|---|---|---|
| 79 | Editor opens | Sequences tab shows 10 slots and the selected preset's B#:P#. |
| 80 | Chord picker | Selecting root/type/extensions updates the preview (e.g. `Fm7+9+13`). |
| 81 | Grid editing | Left-click adds/replaces; right-click clears; bar rows and beat shading are visible. |
| 82 | Bars/steps | Changing bars/steps trims out-of-range events and redraws the grid. |
| 83 | Settings snapshot | Editing the values and per-setting Live checkboxes updates the sequence JSON. |
| 84 | Capture | Copies the preset's current chord params into the snapshot. |
| 85 | Validation | Valid files pass; >64 events, invalid chord IDs, out-of-range steps/settings are reported. |
| 86 | Save/load | Save, reload the folder, and re-open: all edits and live bits round-trip. |
| 87 | Device round-trip | Save to device, reboot, play the configurator-authored sequence. |
| 88 | Legacy warning | Opening a folder with bank6–10 shows the migration warning. |

---

## 14. Regression checks

| # | Check | Expected |
|---|---|---|
| 89 | Chord/strum | Chord grid and strum still work normally. |
| 90 | Rhythm/bass | Rhythm and walking bass still play; mute/bass toggles work. |
| 91 | Macros | Top-row macros still trigger when `number_keys` is `macro`; numpad still strums. |
| 92 | Presets | Save/load/clear, cursor navigation, bank navigation, and naming work with 5 banks. |
| 93 | Keymap | Existing customized keymaps still load; sequence menu/actions are optional. |
| 94 | Panic | `Super+Esc` stops sequence playback/recording and all sound. |
| 95 | Arp rate | In Arpeggio/Arp-Hold, edit Arp Rate in the `F9` menu. | Whole/Half/Quarter/Eighth/16th/32nd change the arp speed in musical subdivisions; 16th matches the previous behavior. Notes stay locked to the beat LED with the rhythm both on and off. |
| 95a | Arp early-start quantization | In Arpeggio mode, press a chord about 10 ms before an arp step boundary. Repeat with a press well before the boundary. | The near-boundary press waits and sounds its first note on the boundary, with no second note immediately after. The early press starts immediately as before. |
| 96 | Chord grouping | Press `T` then `B` for Cmaj7, first with a deliberate gap and then as close to simultaneous as possible. | A deliberate gap sounds two chords; a near-simultaneous press (within ~15 ms) sounds only Cmaj7, with no C-major attack before it. |

---

## 15. Results checklist

| # | Check | Pass / Fail |
|---|---|---|
| 1 | Native tests pass | |
| 2 | Pico build succeeds | |
| 3 | Configurator pytest passes | |
| 4 | Legacy banks 6–10 removed after boot | |
| 5 | B1–B5 navigation wraps correctly | |
| 6 | `Super+F12` opens Seq Edit (Slot/Bars/Settings) | |
| 7 | Ctrl+1…Ctrl+0 select slots without strum/stuck notes | |
| 8 | Record arms, mutes the pattern, and waits silently for the count-in bar | |
| 9 | Exactly one bar of hi-hat count-in, then persistent Recording 1/N…N/N | |
| 10 | Strum is not recorded | |
| 11 | Playback loops in sync with the rhythm | |
| 12 | Playback stops cleanly (note-offs) | |
| 13 | Live chord overrides sequence events | |
| 14 | Sequence resumes after the live chord ends | |
| 15 | Rhythm disabled → auto-enable + mute, then record/play | |
| 16 | Muted rhythm still records/plays with the hi-hat guide | |
| 17 | Bars 1/2/4/8 save and trim events | |
| 18 | Global Recorded/Live settings switch works | |
| 19 | Configurator per-setting Live works | |
| 20 | Clear prompt/cancel/confirm works | |
| 21 | Sequences are per-preset and survive load | |
| 22 | Clearing a preset removes its sequence file | |
| 23 | MSC drive blocks sequence writes | |
| 24 | 64-event cap enforced | |
| 25 | Corrupt/missing files fall back safely | |
| 26 | Configurator editor round-trips | |
| 27 | Configurator validation catches bad sequences | |
| 28 | Chord/strum/rhythm/bass/macros regressions pass | |
| 29 | Beat LED is on the beat; MIDI clock starts/re-syncs on a beat boundary | |
| 30 | Recording LCD stays on `Recording N/M` between bars (no idle flash) | |
| 31 | Hi-hat guide stops and the normal pattern resumes after the take | |
| 32 | FollowChord strum tracks the sequence chord during playback | |
| 33 | Selecting a new sequence during playback queues it to the next loop | |
| 34 | Recording over an existing slot prompts before overwriting | |
| 35 | Overwrite cancel preserves the existing sequence | |
| 36 | Enter/Mute confirm the overwrite prompt | |
| 37 | Recorded arp/pattern play mode reproduces on playback | |
| 38 | Play mode and arp rate round-trip through the configurator | |
| 39 | Legacy sequences still play as held chords | |
| 40 | Arp early-start quantization works within 30 ms before a boundary | |

---

## 16. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| `Seq empty` | The selected slot has no events. Record it first, or select another slot (`Ctrl+1`…`Ctrl+0`). |
| `Seq full` | The sequence already has 64 events. Remove steps in the configurator or clear the slot. |
| `Seq save fail` | The filesystem is full or write-protected. Free space (remove user patterns/sequences) and retry. |
| `USB drive active` / `Seq write off` | The Mass Storage drive is presented (Ctrl-at-boot). Eject the drive and reboot before recording/clearing sequences. |
| Sequence does not change chords | A live chord is held/latched. Press `Esc` to clear it; the sequence resumes at the next event. |
| Recorded arp/pattern plays as held chords | In Settings=Recorded the sequence may predate play-mode capture and load as Held; re-record it or set Settings=Live. In Settings=Live, check the preset's current play mode. |
| Arp first note seems delayed | Expected when the chord starts within 30 ms before an arp step boundary; the first note is quantized to that boundary to avoid two notes bunching together. |
| Newly selected sequence does not start immediately | The previous sequence always finishes its current loop. The new slot starts on its next downbeat; selecting an empty slot stops playback at that boundary with `Seq empty`. |
| Record starts late | Arming waits for the next bar, then plays exactly one bar of count-in hi-hats. Recording starts after that bar; watch for `Recording 1/N`. |
| No count-in hi-hats | Record/play enables the rhythm automatically and mutes the drums if it was off. The guide overrides mute; check the rhythm channel and that the mapped closed hi-hat note is audible. |
| LED looks off the beat | The LED and hi-hat guide should share the master clock. Verify the guide hats align; if the LED is consistently early/late, report the LED target (`led.led`) and keyboard model. |
| External clock starts off-beat | Clock Out starts at the next 24-tick beat boundary. After a local stop/restart, double-tap `F8` to send MIDI Start on a boundary. Also check that the device is set to follow external MIDI clock (not free-run). |
| Recording screen flashes idle | The recording status is now persistent. If it still flashes, verify the v0.2.0 firmware containing the persistent-status fix is flashed. |
| No MIDI from playback | Check the MIDI monitor, the chord channel (preset `chord.channel`), USB MIDI (`Ctrl+F8`), and the DIN/USB routing. Rhythm mute only mutes drums, not sequence chords. |
| Ctrl+number strums instead of selecting | Verify the keyboard is sending Ctrl; `Ctrl+number` is hard-coded and overrides strum. If a custom keymap is loaded, sequence selection is unaffected. |
| Sequence file not visible | Sequences are compact JSON in `/sequences/b<N>p<M>.json`; make sure you are looking at the correct preset bank/slot. Empty libraries remove the file. |
| Configurator cannot find the device | Power on while holding `Ctrl` to present the `KeybChord` drive; install PySide6 and run from `configurator/`. |
