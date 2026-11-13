# Hardware Test Plan — Pattern Editors (M17)

Step-by-step verification for the v0.2.0 **bass, rhythm, and chord pattern
editors**, including the default keymap remap. Follow the sections in order and
record results in the checklist at the end.

> **Scope.** This plan verifies: Pattern Edit entry/exit, the keymap remap,
> sequencing-style count-in recording, rhythm polyphony/overdub, bass and chord
> sustain capture, quantize, preview, grid editing, save/new/rename/delete,
> built-in overwrite, caps/budget, power-cycle persistence, configurator
> compatibility, and exclusion against sequencer/MSC/panic. It does not
> re-verify the general chord/rhythm/bass engines, CH559 input, or DIN/TRS
> wiring.

---

## 1. Prerequisites

Hardware:
- Assembled KeybChord Pico running v0.2.0 firmware.
- Hosted USB keyboard, LCD1602, and a MIDI monitor on **USB MIDI** or **DIN**
  (a DAW, MIDI-OX, or the repo's `tools/midi_test` harness).
- A USB data cable for the Pico's native USB port (CDC log + USB MIDI + MSC).
- A keyboard with a numpad. Note whether it is 6KRO or NKRO for the polyphony
  checks.

Software:
- PlatformIO for builds/tests.
- Python 3.9+ with the configurator dependencies (`pip install -e ".[dev]"`
  from `configurator/`, or run the packaged configurator).
- Optional: a backup of `/rhythms`, `/bass`, and `/chordpatterns` before
  overwriting built-ins.

---

## 2. Controls used in this plan

These are the **default** keymap bindings. If `keymap.json` was customized,
substitute your bindings. The pattern editor's numpad pads are hard-coded and
Num-Lock-independent.

| Control | Default binding | Meaning |
|---|---|---|
| Chord Edit menu | `F9` | Contains **Edit Pattern** for chord patterns. |
| Rhythm Edit menu | `F10` | Contains **Edit Pattern** for rhythm patterns. |
| Strum Edit menu | `F11` | No pattern editor. |
| Bass Edit menu | `F12` | Contains **Edit Pattern** for bass patterns. |
| Chord pattern editor | `Super+F9` | Opens the **Chord Pattern** editor. |
| Rhythm pattern editor | `Super+F10` | Opens the **Rhythm Pattern** editor. |
| Drum Edit menu | `Super+F11` | Unchanged. |
| Bass pattern editor | `Super+F12` | Opens the **Bass Pattern** editor. |
| Seq Edit menu | `Ctrl+F12` | Moved from `Super+F12`. |
| Record | **Record** command | Start a take; Enter/Mute or Esc/Backspace stops it. |
| Preview | **Preview** command | Start a loop; Enter/Mute or Esc/Backspace stops it. |
| Grid | **Grid** command | Enter the grid; Esc/Backspace leaves it. |
| Live numpad | Numpad pad keys | Bass degrees (`1`–`4`/`0`) or the 13 drum pieces, depending on the editor. `+`/`−`/Enter keep their parameter actions. |
| Parameter keys | Normal keymap | Direct parameter actions remain active while the editor is open. |
| Chord keys | Letter grid | Remain live while the editor is open. |
| Back/exit | `Esc` / `Backspace` | Leave the grid or editor; discard prompt if dirty. |
| Panic | `Super+Esc` | Stops recording/preview and all sound. |

### 2.1 Numpad pads (recording and live)

| Kind | Pads |
|---|---|
| Rhythm | `1`–`9`, `0`, `.`, `/`, `*` = kick, snare, hi-hat, open-hat, rimshot, clap, crash, ride, bongo, conga-lo, conga-hi, clave, shaker. Live play uses the preset DrumMap/channel. |
| Bass | `1`–`5` = root/3rd/5th/6th-or-7th/root+8va; `0` = rest. Live play uses the current chord and preset bass parameters. |
| Chord | `1`–`4` = root/3rd/5th/7th (combine); `0` = clear (grid/record only). |

### 2.2 Expected LCD messages

| Situation | Line 1 | Line 2 |
|---|---|---|
| Command screen | `Chord Pattern` / `Rhythm Pattern` / `Bass Pattern` | `Pat: …`, `Bars: …`, `Quant: …`, … |
| Value selected | `Pattern` / `Bars` / `Quantize` | pending value (changes before Enter/Mute) |
| Bars value | `Bars` | `1` / `2` / `4` / `8` |
| Quantize value | `Quantize` | `1/4` / `1/8` / `1/16` |
| Record armed | `Rec arm` | `Wait bar` |
| Count-in | `Count-in` | `1 bar` |
| Recording bar N | `Recording N/M` | (blank) |
| Meter mismatch | `Meter mismatch` | `Select rhythm` |
| Clock stalled | (editor title) | `Rhythm stalled` |
| Saved | `Pattern saved` | (blank) |
| New created | `New Pat N` | `Created` |
| Limit reached | `Pattern limit` | (blank) |
| File too large | `Pattern too large` | (blank) |
| Discard prompt | `Discard?` | `Ent/Bk` |
| Overwrite prompt | `Overwrite?` | `Ent/Bk` |
| Delete prompt | `Delete pattern?` | `Ent/Bk` |

---

## 3. Build and automated checks

| # | Check | Command | Expected |
|---|-------|---------|----------|
| 1 | Native tests | `cd keybchord && pio test -e native` | All tests pass, including pattern serializers/quantize/overdub/budget. |
| 2 | Firmware build | `cd keybchord && pio run -e pico` | Builds without errors. |
| 3 | Configurator tests | `cd configurator && pytest` | All tests pass, including keymap/budget validation. |

---

## 4. Entry, editors, and discard

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 4 | Chord menu entry | `F9` → highlight `Edit Pattern` → `Enter` | The **Chord Pattern** editor opens with the preset's current chord pattern. |
| 5 | Rhythm menu entry | `F10` → `Edit Pattern` → `Enter` | The **Rhythm Pattern** editor opens with the preset's current rhythm. |
| 6 | Bass menu entry | `F12` → `Edit Pattern` → `Enter` | The **Bass Pattern** editor opens with the preset's current bass pattern. |
| 7 | Direct keys | `Super+F9`, `Super+F10`, `Super+F12` | Open the matching kind directly. |
| 8 | Per-kind screen | Open each editor from its menu/direct key | Line 1 shows `Chord Pattern`, `Rhythm Pattern`, or `Bass Pattern`; the command list has no Kind item. |
| 9 | Discard | Edit a step, then `Esc` | `Discard? Ent/Bk`; `Backspace` keeps editing, `Enter` discards. |
| 10 | Keymap remap | `F11` | Opens **Strum Edit**, not Rhythm. |
| 11 | Seq menu move | `Ctrl+F12` | Opens **Seq Edit**; `Super+F12` opens Bass Pattern Edit instead. |
| 12 | Drum unchanged | `Super+F11` | Opens **Drum Edit**. |

### 4.1 Command-menu navigation

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 66 | Browse with all keys | In any editor, press Up/Down/Left/Right and Volume +/− | The command highlight moves and wraps; no value changes. |
| 67 | Enter/Mute selects | Highlight `Bars`, press `Enter` or `Mute` | The Bars value screen appears and stays selected; the command list is unchanged. |
| 68 | Change pending value | While `Bars` is selected, press Up/Down/Left/Right/Volume +/− | The pending Bars value cycles; the working pattern/bar count is not changed yet. |
| 69 | Apply selection | With a changed Bars value, press `Enter` or `Mute` | The new Bars value is applied and the command menu returns. |
| 70 | Cancel selection | Select `Bars`, change it, press `Esc` or `Backspace` | The pending change is discarded and the command menu returns; Bars is unchanged. |
| 71 | Action item | Highlight `Grid` or `Preview`, press `Enter`/`Mute` | The action opens immediately; returning shows the command menu again. |

### 4.2 Live pads and parameter pass-through

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 72 | Live rhythm pads | Open the rhythm editor but do not record; press `KP1`/`KP2` | Drums play through the preset DrumMap note/velocity and rhythm channel; no strum notes. |
| 73 | Live bass pads | Open the bass editor but do not record; press `KP1`–`KP4`, then play a different chord and repeat | Bass follows the current chord and preset octave/velocity/duration/channel; no strum notes. |
| 74 | Live pads while value selected | Select `Bars`, then press `KP1` | The bass/drum/chord note plays and the Bars selection stays active; Esc cancels normally. |
| 75 | Modified numpad | While the editor is open, press `Alt+KpPlus` or `Ctrl+KpPlus` | The mapped strum parameter changes as normal; the key does not become a live pad. |
| 76 | Parameter keys | While the editor is open, press `F1`, `F2`, `F3`, `F4`, `F5`, `F8`, and a few Ctrl/Alt parameter combos | The parameters change and the editor screen is restored after the transient value display; no parameter menu opens. |
| 77 | Parameter keys in value selection | Select `Bars`, then press `F1`/`F2` | The parameter changes and the editor returns to the selected-value screen; Esc then cancels the Bars edit. |
| 78 | Chord keys | Play chord keys while each editor is open, including while a value is selected and during preview | Chords play normally; preview stops on a live chord press and can be restarted. |
| 79 | Stop take/preview | Start a take or preview, then press Enter/Mute; repeat with Esc/Backspace | The take/preview stops and the command menu returns. |
| 80 | F2/F3/F4 are parameter keys | While the editor is open, press `F2`, `F3`, `F4` | Arp mode, bass pattern, and rhythm pattern change as normal; the editor stays open. |
| 81 | Mute/enable override | Open the rhythm editor and press `F6`; open the bass editor and press `F7` | The matching parameter is ignored and pads/preview keep emitting. The rhythm editor restores its original mute value on exit; the bass editor leaves the bass enabled. |
| 82 | Live chord pads | Open the chord editor but do not record; press `KP1`, then add `KP2`, then release `KP1` | Each pad plays its degree through the current chord; held pads combine, released degrees drop out, `KP0` releases the audition. In Grid, `KP1`–`KP4`/`KP0` still toggle step bits. |
| 83 | Playback priority | Start rhythm/sequence/bass playback, then navigate menus and change settings for several bars | Playback stays on-tempo; no audible late-note bursts. `[PERF] rhythm-dispatch-us` stays low and `rhythm-queue` overflow is zero. |
| 84 | Storage stall recovery | While playback is running, perform an explicit preset/pattern save | Playback pauses briefly during the flash write, then resumes on the current grid without a catch-up burst of old notes. |

---

## 5. Rhythm recording

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 13 | Count-in | Rhythm editor, Bars=1, Record | Pattern is suppressed; one bar of quarter-note closed hi-hats (downbeat accented), then `Recording 1/1`. |
| 14 | Auto-stop | Let the bar finish | Recording stops automatically; returns to the command screen; no normal pattern resumes mid-take. |
| 15 | Single pad | Record; press kick on beat 1 | Kick track has a hit at step 0; the pad auditions the kick. |
| 16 | Polyphony | Record; press kick+snare+hi-hat together | All three tracks have hits at the same step (subject to keyboard rollover). |
| 17 | Overdub | Finish a kick take; record again and add snare | Kick remains; snare is added; other steps are untouched. |
| 18 | Same-pad replace | Record two kick hits at one step | One hit remains (last velocity wins), not a doubled hit. |
| 19 | Accent | Grid-edit a hit to `X` | Playback uses velocity 127; `x` uses default velocity. |
| 20 | Quantize 1/16 | Record slightly off-grid | Hits snap to the nearest 16th. |
| 21 | Quantize 1/8 | Repeat with Quantize=1/8 | Hits snap to even steps. |
| 22 | Quantize 1/4 | Repeat with Quantize=1/4 | Hits snap to beats (0/4/8/12). |
| 23 | Clear | Use **Clear** after a take | Working buffer becomes rests; bars/meter unchanged. |

---

## 6. Bass recording

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 24 | Degree pads | Bass editor, record; press `1`, `2`, `3`, `4` on separate steps | Steps contain root/3rd/5th/6th-7th respectively; each auditions against the selected chord. |
| 25 | Monophonic | Press two degree pads on one step | Last degree wins. |
| 26 | Sustain | Hold `1` across four steps | The attack step writes a sustain covering the held steps; the grid shows `-` on the sustained steps. |
| 27 | Percussive | Tap a degree pad | Sustain is 0; playback uses `bass.note_duration_ms`. |
| 28 | Rest/clear | Press `0` on a step | The step and any sustain starting there are cleared. |
| 29 | Overdub | Record a second take | Untouched steps remain; pressed steps replace. |

---

## 7. Chord pattern recording

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 30 | Single degree | Chord editor, record; press `1` | Step bit is root; preview plays the root through the selected chord. |
| 31 | Combine bits | Press `1`+`2`+`3` at one step | Step is root+3rd+5th (`7`), displayed as hex `7`. |
| 32 | Add a degree later | Overdub `4` onto a step with `7` | Step becomes `15` (root+3rd+5th+7th). |
| 33 | Clear step | Press `0` on a step | Step becomes `0` (rest). |
| 34 | Sustain | Hold the degree pads across steps | `sustain_steps` covers the held duration; grid shows `-`. |
| 35 | Preview chord | Hold a different chord and preview | Degrees resolve through the new chord; live chord takes over and preview can be restarted. |
| 36 | Triad 7th wrap | Preview a pattern with bit `8` on a triad | 7th degree wraps to the root an octave up, per 6.10. |

---

## 8. Preview and grid

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 37 | Preview start | Select **Preview** | Loops from the next bar; normal rhythm/bass playback is suppressed. |
| 38 | Preview stop | Press Enter/Mute or Esc/Backspace | Preview stops cleanly; no stuck notes. |
| 39 | Preview before save | Edit, preview, exit without saving | Preview uses the unsaved working buffer. |
| 40 | Rhythm grid | Select **Grid** | `>` cursor; Left/Right moves steps/bars; Up/Down selects the 13 pieces; Enter cycles rest/hit/accent. |
| 41 | Bass grid | Select **Grid** | Enter cycles rest/root/3rd/5th/6th-7th; `-` sustained steps display. |
| 42 | Chord grid | Select **Grid** | Hex `0`–`F`; line 1 decodes the cursor step; numpad `1`–`4`/`0` toggles bits. |
| 43 | Cursor/bar wrap | Move Left/Right across bar edges | Cursor wraps into the adjacent bar; bar indicator updates. |
| 44 | 12-step pattern | Edit a 3/4 (Walz) pattern | 12 cells per bar; grid and playback use 12 steps; `Meter mismatch` appears if the live rhythm is 4/4. |
| 45 | 16-step pattern | Edit a 4/4 pattern | 16 cells per bar; works normally. |

---

## 9. Save, new, rename, delete

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 46 | New | New → enter name → choose Bars → decline `Record now?` → save | Prompts in order (name, Bars, Record now?); declining shows `New Pat N` / `Created`; creates `userN.json` with the chosen name/length. |
| 47 | Save overwrite | Edit a user pattern → Save | Same file is rewritten; name/index remain. |
| 48 | Save As | Save As | Creates a new user file; the preset selects the new pattern. |
| 49 | Rename user | Rename → type → Enter | Name changes; file name stays; presets referencing the old name are affected as documented. |
| 50 | Delete user | Delete → Enter | File is removed; next pattern is selected. |
| 51 | Built-in overwrite | Edit a built-in → Save | `Overwrite?`; Enter writes the built-in file, Backspace cancels. |
| 52 | Built-in rename/delete | Select a built-in → Rename/Delete | Refused; built-ins cannot be renamed/deleted. |
| 53 | Unique name | Rename to an existing name | Editor uniquifies it or shows an error; no duplicate names in the library. |
| 54 | Power cycle | Save, power off/on | Pattern file loads; preset name reference still resolves. |
| 55 | Configurator sees file | Mount the drive / open the configurator | User pattern appears in the matching editor; can be edited/saved. |

---

## 10. Limits, exclusion, and regression

| # | Check | Steps | Expected |
|---|-------|-------|----------|
| 56 | Per-kind cap | Create up to 32 patterns | The 33rd is rejected with `Pattern limit`; existing files are intact. |
| 57 | Per-file cap | Save an oversized rhythm/bass/chord pattern | Rejected with `Pattern too large`. |
| 58 | Combined budget | Fill user patterns toward 48 KB | Saves beyond the budget are rejected; no corruption. |
| 59 | Configurator budget | Exceed 32/budget in configurator | Validation warns/errors at the same thresholds. |
| 60 | Sequencer exclusion | Start a sequence, then open Pattern Edit | Sequence stops; pattern editor is usable. |
| 61 | Reverse exclusion | Start recording/preview, then `Ctrl+F9` / `Ctrl+F10` | Sequence actions do not start while the editor is active. |
| 62 | MSC gating | Hold Ctrl at power-on, open Pattern Edit | `USB drive active`; save/record write actions are refused. |
| 63 | Panic | Record/preview, then `Super+Esc` | Recording/preview stops; all notes off. |
| 64 | Live chord regression | Play/strum while the editor is open | Chords remain live; top-row strum/macros are not triggered by the editor's numpad; editing still works. |
| 65 | Normal playback regression | Exit the editor, play a saved pattern | Selected pattern plays through the normal rhythm/bass/chord path. Bass remains enabled and still plays with the rhythm stopped. |

---

## 11. Results checklist

**Verification record:** passed **2026-11-13** on the assembled v0.2.0 PCB
with v0.2.0 firmware. All checks below passed.

| # | Check | Pass / Fail |
|---|-------|-------------|
| 1 | Native tests pass | Pass |
| 2 | Firmware builds | Pass |
| 3 | Configurator tests pass | Pass |
| 4 | Chord menu entry | Pass |
| 5 | Rhythm menu entry | Pass |
| 6 | Bass menu entry | Pass |
| 7 | Direct keys | Pass |
| 8 | Per-kind screens | Pass |
| 9 | Discard prompt | Pass |
| 10 | Keymap remap (F10/F11/Ctrl+F12) | Pass |
| 11 | Rhythm count-in | Pass |
| 12 | Rhythm auto-stop | Pass |
| 13 | Rhythm polyphony | Pass |
| 14 | Rhythm overdub | Pass |
| 15 | Rhythm accent | Pass |
| 16 | Quantize 1/4, 1/8, 1/16 | Pass |
| 17 | Bass degree pads | Pass |
| 18 | Bass monophonic | Pass |
| 19 | Bass sustain | Pass |
| 20 | Chord bitmask combine | Pass |
| 21 | Chord sustain | Pass |
| 22 | Chord preview resolution | Pass |
| 23 | Preview start/stop | Pass |
| 24 | Grid editing (all kinds) | Pass |
| 25 | 12-step meter | Pass |
| 26 | New/Save/Save As | Pass |
| 27 | Rename/Delete user | Pass |
| 28 | Built-in overwrite | Pass |
| 29 | Power-cycle persistence | Pass |
| 30 | Configurator sees files | Pass |
| 31 | Caps/budget rejection | Pass |
| 32 | Sequencer exclusion | Pass |
| 33 | MSC gating | Pass |
| 34 | Panic | Pass |
| 35 | Normal playback regression | Pass |
| 36 | Browse with all keys | Pass |
| 37 | Enter/Mute selects | Pass |
| 38 | Change pending value | Pass |
| 39 | Apply selection | Pass |
| 40 | Cancel selection | Pass |
| 41 | Action item | Pass |
| 42 | Live rhythm pads | Pass |
| 43 | Live bass pads | Pass |
| 44 | Live pads while value selected | Pass |
| 45 | Modified numpad | Pass |
| 46 | Parameter keys | Pass |
| 47 | Parameter keys in value selection | Pass |
| 48 | Chord keys | Pass |
| 49 | Stop take/preview | Pass |
| 50 | F2/F3/F4 parameter keys | Pass |
| 51 | Mute/enable override | Pass |
| 52 | Live chord pads | Pass |
| 53 | Playback priority | Pass |
| 54 | Storage stall recovery | Pass |

---

## 12. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Meter mismatch` | Live rhythm meter differs from the pattern's `steps_per_bar` | Select a 3/4 rhythm (e.g. Waltz) for a 12-step pattern, or a 4/4 rhythm for 16 steps. |
| Pads record nothing | Not in Record, or before the first recorded step | Wait for `Recording 1/N`; pads before the downbeat are ignored. |
| Only some simultaneous pads record | Keyboard rollover | Use an NKRO keyboard or record in multiple overdub takes. |
| `Pattern limit` | 32-pattern cap or combined budget reached | Delete user patterns, or overwrite an existing user/built-in pattern instead of creating another. |
| `Pattern too large` | File exceeds 8/4/4 KB | Shorten bars or remove tracks/steps. |
| Saved pattern not selectable | Name collision or file not reloaded | Check for a unique name; power-cycle if the runtime library did not reload. |
| Built-in edit vanished | Built-in file was deleted/corrupt | A deleted built-in is re-provisioned from the template on boot; overwrite it again. |
| Sequence controls do nothing while editing | Editor is active | Exit the editor first; sequence and pattern editor are mutually exclusive. |
| Save/record refused | MSC drive is active | Eject the drive (the device reboots) and retry. |
