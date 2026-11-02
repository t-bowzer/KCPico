---
name: keybchord-ch559
description: Use when changing firmware/ch559, the CH559 USB-host coprocessor, the UART bridge, boot protocol, keyboard LED relay, or CH559 build/flash docs. Distinguishes real source from generated SDCC artifacts.
---

# CH559 USB-host coprocessor

The CH559 is an 8051-class MCU on its own chip. It enumerates the USB keyboard
and forwards HID frames to the RP2040 over UART1. It is a GPL-3.0 fork of
`atc1441/CH559sdccUSBHost`; keep upstream code style and license intact.

## Real source vs build outputs

Edit these:

- `main.c` — clock init, host init, main loop.
- `USBHost.c` / `USBHost.h` — enumeration and HID polling.
- `uart.c` / `uart.h` — framing protocol and LED command.
- `util.c` / `util.h` — clock, delay, UART0, `putchar`.
- `config.h` — user overrides.
- `CH559.h` — WCH register definitions.
- `Makefile` — SDCC build.

Never edit these; `make` generates them:

`.asm`, `.lst`, `.rel`, `.rst`, `.sym`, `.map`, `.mem`, `.ihx`, `.hex`,
`.bin`, `.lk`.

`make clean` deletes those generated files, including `.asm`/`.lst`.

## Build

```bash
cd firmware/ch559 && make    # ch559.hex + ch559.bin
```

Requires SDCC 4.x (`mmcs51`). Memory layout: `--model-large`, xram `0x0600`
(0x0800 bytes), code up to `0xEFFF`, `FREQ_SYS=48000000`.

## Protocol

The framing and message types are implemented on both sides and must stay in
sync:

- CH559 side: `firmware/ch559/uart.c`, `uart.h`.
- RP2040 side: `keybchord/lib/hw/ch559_protocol.h`,
  `keybchord/lib/hw/input_ch559.cpp`.

Frame: `0xFE`, length LE, message type, type/HID usage, device, endpoint,
4 vendor/product bytes, payload, `0x0A`.

Message types: `CONNECTED 0x01`, `DISCONNECTED 0x02`, `ERROR 0x03`,
`DEVICE_POLL 0x04`, `DEVICE_STRING 0x05`, `DEVICE_INFO 0x06`,
`HID_INFO 0x07`, `STARTUP 0x08`.

Key details:

- HID usage in `DEVICE_POLL` is `0x06` for keyboard and `0x0C` for Consumer
  Control, so the RP2040 picks the right report parser.
- LED relay: RP2040 sends `0xED` plus the 1-byte LED output report
  (bit0 NumLock, bit1 CapsLock, bit2 ScrollLock); CH559 issues
  SET_REPORT(output) to the enumerated keyboard.
- Enumeration sets HID boot protocol for the standard 8-byte report.
- At power-up the CH559 polls root-hub attach levels so an already-connected
  keyboard is detected.

## RP2040 side

- `keybchord/lib/hw/input_ch559.*` parses frames into `KeyEvent`s and relays
  LED state.
- `keybchord/lib/hw/hid_consumer.h` handles Consumer Control descriptors,
  including QMK-style shared endpoints.
- `keybchord/test/test_ch559.cpp` covers the native protocol/parser behavior.
- Run `cd keybchord && pio test -e native` after protocol changes.

## Docs and hardware

- `docs/ch559-breakout-integration-guide.md` — integration and flashing.
- `hardware-test-plan-ch559.md` — bring-up and verification steps.
- `docs/wiring.md` — pins and connections.

## Token discipline

- Read only `.c`/`.h`/`Makefile`; skip generated artifacts and vendored
  upstream noise.
- Use the guides for wiring and flashing instead of reading the whole repo.
