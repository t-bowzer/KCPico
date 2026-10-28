# CH559 USB-host firmware

Keyboard USB-host coprocessor firmware for the CH559 (WCH 8051 MCU). It
enumerates a USB keyboard on the host port and forwards raw HID reports to the
RP2040 over a UART, using the `atc1441` framing protocol (see
`hardware-test-plan-ch559.md` §4.4).

This is the firmware that replaces the PIO-USB keyboard host in KeybChord Pico
(M12). It runs on a **separate CH559 chip** and is **GPL-3.0** (upstream
license); because it is an independent program on its own chip, it does not
affect the MIT-licensed RP2040 firmware.

## Provenance

- Forked from [`atc1441/CH559sdccUSBHost`](https://github.com/atc1441/CH559sdccUSBHost)
  (GPL-3.0), imported via `git subtree` at upstream commit
  `6b132f0543bd4027b3c65af046157bcb7704fbeb`.
- Vendored here as **source only**; the upstream bundled SDCC toolchain
  (`SDCC/`), `chflasher.exe`, `compile.bat`, and the Arduino example sketches
  were dropped.
- `compiler.h` (referenced by upstream `CH559.h` via `#include <compiler.h>`)
  is provided by SDCC itself (`<mcs51/compiler.h>`, which defines the `SFR`/
  `SBIT`/`SFR16` macros); no extra header is vendored.

## Build

Requires [SDCC](https://sdcc.sourceforge.net/) ≥ 4.x (`mmcs51`):

```bash
sudo apt install sdcc     # Debian/Ubuntu
make                      # produces ch559.hex + ch559.bin
```

The build mirrors the upstream memory layout: `--model-large`, xram at
`0x0600` (0x0800 bytes), code up to `0xEFFF`, `FREQ_SYS=48000000`.

## Flash

Flash `ch559.bin` (or `.hex`) to the CH559 over its **device** USB port
(`DP`/`DM`, pins 31/32) using [`ch55xtool`](https://github.com/MarsTechHAN/ch552tool):

```bash
python3 ch55xtool.py -f ch559.bin
```

See `hardware-test-plan-ch559.md` §5 for bootloader entry and the bare-chip
programming header.

## Divergences from upstream

Implemented (tracked against the bring-up plan):

- [x] Emit the protocol on **UART1** (`RXD1`/`TXD1`, P2.6/P2.7) at 115200 baud
      (`CH559_UART_BAUD` in `config.h`; upstream emits on UART0 @ 1 Mbps on
      P0.2/P0.3).
- [x] Relay keyboard **LED SET_REPORT** (output report) so the Pico's beat-LED
      indicator works. The Pico sends `0xED` + 1 LED byte over UART1; the
      firmware issues a SET_REPORT(output) on the enumerated keyboard.
- [x] Set the HID **boot protocol** during enumeration for the standard 8-byte
      report format.
- [x] Detect a keyboard that is already attached at power-up/reset by polling
      the root-hub attach levels, rather than waiting for a `UIF_DETECT` edge.

The LED SET_REPORT and startup-detection paths are validated on hardware.

## Files

| File | Purpose |
|------|---------|
| `main.c` | Entry point, clock init, host init, main loop |
| `USBHost.c/.h` | USB host enumeration + HID polling |
| `uart.c/.h` | UART framing protocol (`0xFE`-delimited messages) |
| `util.c/.h` | Clock, delay, UART0, `putchar` |
| `CH559.h` | WCH register definitions (includes SDCC's `compiler.h`) |
| `config.h` | User overrides |
| `Makefile` | SDCC build |
| `LICENSE` | GPL-3.0 (upstream) |
