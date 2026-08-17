# KeybChord Pico — Session 2026-08-16 (M9) Summary

## Goal: M9 — USB Mass Storage + dev serial (roadmap §6)

Make `config.json`/presets/rhythms drag-drop editable from a PC via the native
USB port, without losing the USB-CDC serial debug log. Keyboard stays on the
PIO-USB host port (GP0/GP1).

## Result: SUCCESS

- **Native tests:** `pio test -e native` → **261/261 passing**.
- **Pico build:** `pio run -e pico` → SUCCESS (RAM 12.7%, Flash 11.5%).
  `firmware.uf2` is now the only artifact to flash (no `buildfs`/`buildunified`).

## Storage migration: LittleFS → FatFS

- A PC cannot mount LittleFS, so the storage backend moved to the arduino-pico
  **FatFS** library (FAT16 over the onboard flash FTL, wear-leveled). New
  `lib/hw/storage_fatfs.*` replaces `storage_littlefs.*` (drop-in `StorageAdapter`;
  `FatFS` shares the `FS`/`File` API). `factory.cpp` constructs `StorageFatFs`.
- `board_build.filesystem_size = 0.5m` is retained (FatFS uses the same flash
  region via `_FS_start`/`_FS_end`).

## Self-provisioning (replaces the shipped `data/` image)

- `data/` (LittleFS image) is removed; the PlatformIO builder only makes
  LittleFS images, so defaults are now embedded in code.
- New `lib/core/defaults.*` — `provisionDefaults()` writes `config.json`,
  10 banks × 8 presets, and all 12 rhythms (JSON strings embedded in
  `defaults.cpp`). `StorageFatFs::begin()` auto-formats on empty and provisions
  on first boot (NFR-9). New `test/test_defaults.cpp` verifies it.

## Mass Storage bridge

- New `lib/hw/msc_fatfs.*` — bridges **`Adafruit_USBD_MSC`** (already compiled
  in; `CFG_TUD_MSC` was enabled) to FatFS's low-level `fatfs::disk_read` /
  `disk_write` / `disk_ioctl`. Sets capacity from `GET_SECTOR_COUNT`, registers
  read/write/flush/start-stop callbacks; on eject (`load_eject`) it remounts
  FatFS so the firmware sees the host's edits.
- `FatFSUSB` was ruled out (it `#error`s under `USE_TINYUSB`, which the
  keyboard host requires — Adafruit TinyUSB's `hcd_pio_usb.c` is the only PIO-USB
  host driver).

## Boot-key composite USB (F11)

- Hold **F11** at power-on to also present the Mass Storage drive; the CDC
  serial log is present in both modes. `detectBootKey()` polls the keyboard
  during a short window after enumeration (breaks early once the initial report
  is read, so normal boot is unaffected).
- When the drive is added the device re-enumerates (`tud_disconnect` +
  `tud_connect`). While the drive is presented, preset save/clear is suppressed
  (log a warning) to avoid FAT corruption.

## Modified / new files

- New: `lib/core/defaults.*`, `lib/hw/storage_fatfs.*`, `lib/hw/msc_fatfs.*`,
  `test/test_defaults.cpp`.
- Removed: `lib/hw/storage_littlefs.*`, `data/` (config.json + rhythms/*.json).
- Modified: `lib/hw/factory.cpp`, `src/main.cpp`, `KeybChord_Pico_Roadmap.md`.

## Verification approach

- Native: GoogleTest; `pio test -e native` green (261).
- On-device: `pio run -e pico` → drag `firmware.uf2` (BOOTSEL) → monitor 115200.

## Next steps

1. **Hardware test plan** (this session): verify first-boot self-provisioning,
   the F11 boot-key drive, drag-drop editing, serial in both modes, and the
   save-suppression / eject-remount behavior on real hardware.

---

# Post-M9 feedback refinements (2026-08-16)

1. **Pretty-printed JSON.** `AppConfig::save` and `savePreset` now use
   `serializeJsonPretty`; the embedded rhythm JSON in `lib/core/defaults.cpp` is
   multi-line. On-device `config.json`/presets/rhythms are now human-readable.
2. **Volume label "KeybChord".** `StorageFatFs::begin()` sets the FAT volume
   label via `fatfs::f_setlabel` (read-then-set, so it self-heals on existing
   devices). The drive now shows as "KeybChord" instead of "USB drive".
3. **LCD feedback when save/clear is blocked.** While the MSC drive is active,
   Insert/Delete now shows `USB drive active / Save/Clear off` on the LCD in
   addition to the serial warning.
4. **Eject stays ejected.** On eject (`START-STOP load_eject`) the drive is now
   withdrawn (`setUnitReady(false)`) so the host stops re-mounting it; FatFS is
   remounted so the firmware sees the host's edits and preset save/clear work
   again. The drive returns only on the next F11 boot.

Note: the pretty JSON + label only materialize on a *fresh* filesystem (or once
the label self-heals on the next boot). To fully re-test provisioning, erase
the flash first (e.g. `flash_nuke.uf2` from the PlatformIO platform's
`misc/binaries/`, or `picotool erase -a`) then flash `firmware.uf2`.

(End of file - total 61 lines)
