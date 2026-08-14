# KeybChord Pico — Session 2026-08-12 Summary

## Goal: M2 Hardware Verification (T19/T20 from 08-08 session)

Following the 08-08 instructions, the objective was to flash the firmware and
verify on-device keyboard input → MIDI output over the USB-CDC debug log.

## Result: SUCCESS — M2 sign-off reached

- Firmware flashed and running on the Pico.
- USB-CDC serial monitor shows every keyboard press/release (`[t] KEY DN/UP usage=... mods=...`).
- A **C4 Note-On (ch1, vel 100)** was heard on the synth when pressing keys.
- The full on-device path — PIO-USB host (GP0/GP1) → key parsing → MidiRouter → Serial2 (GP8) → DIN — is confirmed working on real hardware.

## Blocking issue resolved: WSL2 USB forwarding

The session was spent getting the Pico's USB-CDC visible inside WSL2:

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| `pio device monitor` listed only `/dev/ttyS0..7` | WSL2 doesn't auto-forward USB | usbipd-win on Windows |
| "running scripts is disabled" | PowerShell execution policy | `-ExecutionPolicy Bypass` |
| "usbipd not found on PATH" | usbipd-win not installed | `winget install --id dorssel.usbipd-win` |
| `Permission denied: /dev/ttyACM0` | device is `root:dialout` 660, user not in group | `sudo usermod -aG dialout $USER` |

### New file created
- `tools/pico-usb-to-wsl.ps1` — Windows PowerShell script to auto-detect the Pico
  (RP2 BOOTSEL mass-storage or USB-CDC serial), bind + attach it to WSL via usbipd.
  Modes: default (attach), `-Detach`, `-List`, `-BusId <id>`.

### Environment changes made (non-repo)
- Installed `usbipd-win` on Windows.
- Added `tyler` to the `dialout` group in WSL.
- `keybchord/99-platformio-udev.rules` (under `/etc/udev/rules.d/`) is **still empty
  (0 bytes)** — the dialout group fixed it, so the rule file was not populated.

## Observations noted (NOT fixed — deferred by user request)

1. **Stuck C4 note.** `MidiRouter::sendTestNote()` (`lib/engines/midi_router.cpp:33`)
   sends only a `noteOn`; `main.cpp:33` calls it only on `ev.pressed`. There is no
   NoteOff anywhere, so C4 sounds on key-down and is never released. This is the
   M2 "proof of MIDI path" scaffold (D6), not a bug in the MIDI path itself. Full
   note lifecycle (incl. NoteOff) is owned by **M3**.

2. **usbipd forwarding dropped after a few minutes.** Not a firmware problem — the
   Pico kept outputting to the synth. Likely causes: WSL2 VM idle timeout, Windows
   USB selective suspend, or Pico re-enumeration under keyboard current draw.
   Mitigations if it recurs: `usbipd attach --wsl --auto-attach`, disable USB
   selective suspend, larger bulk cap (220µF) / powered hub. Non-blocking.

## Uncommitted work (carried over)

`git status` shows M2 code (from the 08-08 session) is still uncommitted, plus the
new `tools/` dir and this session's summaries. Recommend committing M2 + tools
before starting M3 (see Next Steps).

---

## Next Steps: Resume Plan for Next Session

### 0. Commit M2 before starting M3
- Stage `keybchord/`, `tools/`, `wiring-cheatsheet.txt`, and the session summaries.
- Message style follows existing history (e.g. "Add session summary: M1 complete, M2 preview").

### 1. M3 — Chord Engine (roadmap line 389; spec §12.3)
Pure logic first (native GoogleTest), then on-device via USB-CDC monitor.

Deliverables:
1. **Keymap Resolver** — grid + modifier convention, HID usage → logical action
   (`lib/core/keymap.*`).
2. **Root/quality resolution** — modifiers as chord roots (AC-11).
3. **Combination matrix** — same-column + left-adjacent + leftmost-`` ` `` combos (AC-14).
4. **Voicing** — root-position & smart voicing (integer math) (AC-12).
5. **Extensions** — independent add9/add11/add13 (AC-13).
6. **Play modes + note lifecycle** — held/arp/rhythm/silent/press-to-play, correct
   NoteOn/NoteOff pairing (AC-3). **This resolves Observation #1 (stuck C4).**
7. **Held-mode latching** — snapshot vs pending (AC-15).

AC covered: AC-1, AC-2, AC-3, AC-11, AC-12, AC-13, AC-14, AC-15.

### 2. Verification approach
- **Native:** add GoogleTest cases for keymap, chord resolution, combination matrix,
  voicing, extensions, and note lifecycle. Run `pio test -e native`.
- **On-device:** replace the `sendTestNote()` scaffold in `main.cpp` with the M3
  engine; verify chords/voicings on DIN + USB-CDC monitor. Use the workflow from
  today: flash via BOOTSEL drag-and-drop, forward USB via `tools/pico-usb-to-wsl.ps1`,
  monitor at 115200.

### 3. After M3 sign-off
- M4 Strum Plate (note-pool, layouts, strum params) — roadmap line 394.
- Keep the USB-CDC debug log as the on-device instrument through M5; LCD is not
  required until M6.

### Reference
- Roadmap: `KeybChord_Pico_Roadmap.md` (M3 at line 389).
- Spec: `KeybChord_Pico_Spec.md` §12.3.
- Prior summaries: `SESSION_SUMMARY_2026-08-06.md`, `SESSION_SUMMARY_2026-08-08.md`.
