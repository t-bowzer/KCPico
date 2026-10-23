# Hardware Test Plan — CH559 USB-Host Coprocessor (M12 bring-up)

Detailed, step-by-step instructions to validate the **CH559 keyboard-host
coprocessor** design on a **breakout board**, before committing to the bare
CH559L chip on the custom PCB. Follow in order; record results in the checklist
at the end.

> **Scope.** This plan covers: (1) breakout wiring, (2) acquiring/developing the
> CH559 firmware, (3) flashing the CH559, (4) the RP2040 `InputCh559` adapter,
> (5) end-to-end verification, and (6) the changes required when we move to the
> bare chip. It does **not** re-verify USB MIDI out (covered by
> `hardware-test-plan-usb-midi.md`) or the DIN/TRS MIDI loop.

> **Note.** This document is **untracked** — a working scratch plan, not yet part
> of the committed docs. It intentionally records one open finding that affects
> the committed `docs/wiring.md`/schematic (see §5.4, the UART mismatch) and
> should be resolved before this plan is promoted into the repo.

---

## 1. Design verification summary

The committed design (`docs/wiring.md`, Spec §2.2, `KCPico.kicad_sch`) is
**correct**. Confirmed mapping:

| CH559 function | Pin | Design usage |
|---|---|---|
| `DP` (device D+) | 31 (P5.1) | programming header `DP` |
| `DM` (device D−) | 32 (P5.0) | programming header `DM` |
| `HP` (host D+) | 29 (P5.5) | keyboard USB-A `D+` |
| `HM` (host D−) | 30 (P5.4) | keyboard USB-A `D−` |
| `RXD1` | 27 (P2.6) | ← Pico `GP0` (TX) |
| `TXD1` | 28 (P2.7) | → Pico `GP1` (RX) |
| `VIN5` | 41 | 3V3 (LDO bypassed < 3.6 V) |
| `VDD33` | 42 | 3V3 |
| `GND` | 18 | GND |
| `RST` | 3 (P5.7) | RESET button → GND |
| `P0.2/RXD_` (UART0 RX), `P0.3/TXD_` (UART0 TX) | 38, 37 | **unused** |

So the three things you flagged are all correct:

- **5.0/5.1 for programming** — yes, that is the device port, used by the USB
  bootloader. 5.4/5.5 is the host port for the keyboard.
- **0.2/0.3 unused** — yes, they are UART0 (default console); the design talks to
  the Pico on **UART1** (`RXD1`/`TXD1`, pins 27/28).
- **3V3 → VIN5** — yes, `VIN5`+`VDD33` both to 3.3 V is the documented 3.3 V mode.

**One finding to resolve (§5.4):** the reference firmware we planned to build on
(`atc1441/CH559sdccUSBHost`) emits its protocol on **UART0 @ 1 Mbps on
P0.2/P0.3** — *not* UART1. That is a firmware-level difference, not a design
error; the plan below resolves it by forking the firmware to emit on **UART1**.

---

## 2. Breakout board inventory & pin map

Your breakout has:

- **2 USB ports** — one wired to `5.0/5.1` (**device/programming** port), one to
  `5.4/5.5` (**host** port). Identify which is which: the host port is the one
  you plug the **keyboard** into; the device port is the one you plug into the
  **PC** to flash the CH559.
- **4-pin header:** `Rx/0.2` (P0.2 = UART0 RX), `Tx/0.3` (P0.3 = UART0 TX),
  `5V` (= `VIN5`), `GND`.
- **All other pins** broken out and labelled (no `3V3`/`VDD33` exposed — its
  onboard LDO + decoupling handle `VDD33` internally).

Pin name reference (CH559 datasheet):

| Breakout label / pin | Function |
|---|---|
| `P2.6/RXD1` (27) | UART1 RX |
| `P2.7/TXD1` (28) | UART1 TX |
| `P0.2/RXD_` (38) | UART0 RX (alt) |
| `P0.3/TXD_` (37) | UART0 TX (alt) |
| `5V` | `VIN5` (41) |
| `GND` | `GND` (18) |
| `P5.7/RST` (3) | reset |

---

## 3. Wiring the breakout

Power the breakout from **5 V** (its native mode). This is deliberate: even
though the final design runs the CH559 at 3.3 V, the breakout's **host USB port
supplies the keyboard's 5 V VBUS from its `VIN5` rail**, so feeding it 3.3 V
would starve a standard keyboard. Signal levels remain safe either way — the
CH559 I/O is powered from `VDD33` (3.3 V) and is 5 V-tolerant, so its
`TXD1`/`RXD1` swing 3.3 V regardless.

### 3.1 Primary wiring — UART1 (validates the committed design)

Use this once the forked firmware (§5.4) is flashed.

| Pico pin | →/← | Breakout pin |
|---|---|---|
| `VBUS` (40, 5V) | → | `5V` |
| `GND` (3/8) | → | `GND` |
| `GP0` (UART0 TX) | → | `P2.6/RXD1` (pin 27) |
| `GP1` (UART0 RX) | ← | `P2.7/TXD1` (pin 28) |

- Keyboard → breakout **host** USB port.
- Pico → PC via its native micro-USB (power + CDC debug).

### 3.2 Quick-start wiring — UART0 (prebuilt firmware, no code changes)

Optional Phase-0 sanity check before writing any firmware. The stock
`CH559USB.bin` emits on UART0 @ **1 Mbps** on the alt pins:

| Pico pin | →/← | Breakout pin |
|---|---|---|
| `VBUS` (40) | → | `5V` |
| `GND` (3/8) | → | `GND` |
| `GP0` (TX) | → | `Rx/0.2` (P0.2/RXD_) |
| `GP1` (RX) | ← | `Tx/0.3` (P0.3/TXD_) |

> 1 Mbps requires a USB-serial adapter or a Pico sketch that opens `Serial1` at
> 1,000,000 baud. Prefer §3.1 at 115,200 baud for the real run; use §3.2 only to
> confirm "the CH559 enumerates a keyboard and emits frames" quickly.

---

## 4. CH559 firmware — acquire vs develop

### 4.1 Prebuilt firmware (quick start)

- Source: [`atc1441/CH559sdccUSBHost`](https://github.com/atc1441/CH559sdccUSBHost)
  — `CH559USB.bin` (and `.hex`) in the repo root.
- GPL-3.0. Acceptable: it runs on a **separate chip**, so it does not contaminate
  the MIT RP2040 firmware.
- Emits the framing in §4.4 on **UART0 @ 1 Mbps (P0.2/P0.3)**. No LED
  SET_REPORT (output) support. This is fine for a "keys work" sanity check only.

### 4.2 Develop the fork (recommended for the final design)

Fork `atc1441/CH559sdccUSBHost` into the project (or a sibling repo). Toolchain:
**SDCC** (`compile.bat` on Windows bundles it; `sdcc` + `makebin`/`packihx` on
Linux/macOS). Edit `config.h`/`main.c`/`uart.c` for the changes in §4.3/§4.5.

### 4.3 Required firmware changes (to match the design)

1. **Output on UART1** (`RXD1`/`TXD1` = P2.6/P2.7, pins 27/28), not UART0.
   - Replace `putchar`/`SBUF` (UART0) with a `UART1` send via `SER1_THR`
     (`SER1_FIFO`), and route all `sendProtocolMSG`/`sendHidPollMSG` output
     through it.
   - Init UART1: set `SER1_IER` pin mode `010` (RXD1/TXD1 on P2.6/P2.7),
     `SER1_LCR` 8N1, divisor via `SER1_DLL`/`SER1_DLM`/`SER1_DIV` for the target
     baud. Reference the WCH `CH559EVT` UART1 example for the exact divisor math
     (`Fsys` = 48 MHz after `initClock()`).
2. **Baud rate.** Recommend **115,200** (plenty for key events, works with every
   serial tool). The stock 1 Mbps is also fine on the RP2040 but is awkward for
   USB-serial adapters. Keep the value in one `#define` and make sure the RP2040
   `Serial1.begin(…)` matches exactly.
3. **LED SET_REPORT relay (needed for the beat-LED feature).** The stock firmware
   only polls the interrupt-IN endpoint. Add a small host→device OUT transfer
   that sends the 8-byte keyboard LED output report (bit 0=NumLock, 1=CapsLock,
   2=ScrollLock) when commanded. Simplest: define a one-byte UART command from
   the Pico (e.g. `0xED` + LED byte) that triggers the SET_REPORT; the Pico
   `InputCh559::setLed()` sends it.
4. *(Optional but recommended)* **Set the boot protocol** during enumeration
   (`SET_PROTOCOL` = 0x00) so keyboards report the standard 8-byte boot format.
   If skipped, the RP2040 parser must handle report IDs (§5.3), which it does.

### 4.4 Protocol framing (what the Pico must parse)

Each message is a byte stream delimited by `0xFE` … `0x0A`:

| offset | field |
|---|---|
| 0 | `0xFE` start |
| 1–2 | payload length, 16-bit little-endian |
| 3 | message type (below) |
| 4 | `type` (HID usage for `DEVICE_POLL`, e.g. `0x06`=keyboard) |
| 5 | `device` |
| 6 | `endpoint` |
| 7–10 | 4 bytes — zeros, or VID/PID for `DEVICE_POLL` |
| 11.. | payload (length bytes) |
| last | `0x0A` (`\n`) |

Message types (`MSG_TYPE_*`):

| value | name | meaning |
|---|---|---|
| `0x01` | CONNECTED | a device attached |
| `0x02` | DISCONNECTED | a device detached |
| `0x03` | ERROR | enumeration error |
| `0x04` | DEVICE_POLL | **raw HID report** (the key data) |
| `0x05` | DEVICE_STRING | descriptor string |
| `0x06` | DEVICE_INFO | config descriptor |
| `0x07` | HID_INFO | HID report descriptor |
| `0x08` | STARTUP | boot banner |

`DEVICE_POLL` (`0x04`) payload = raw interrupt-IN report. Boot keyboards: 8
bytes = `[modifiers, reserved, key0..key5]`. Report-protocol keyboards prefix a
**Report ID** — the RP2040 parser skips it (same as the current PIO-USB adapter).

### 4.5 Bootloader entry points (for flashing convenience)

- **USB bootloader:** pull `DP` high through ~10 kΩ to 3.3 V during reset.
- **Stock firmware has two extras:** the `kb` command over its UART (see
  `processUart()`), and holding **P4.6 low** (`main()` checks `P4_IN & (1<<6)`)
  enters `runBootloader()`. On the breakout P4.6 is accessible if you want a
  hardware BOOT button without the DP pull-up.
- After the first successful flash, `ch55xtool`/`chflasher` re-enter the
  bootloader over USB automatically.

---

## 5. Flashing the CH559

The breakout's **device** USB port carries `DP`/`DM`, so flashing is just a USB
cable to the PC — no 4-pin header needed (that header is UART0 + power, and it
is **not** a programming header).

1. **Power during flash:** disconnect the Pico `VBUS`→`5V` jumper so the CH559 is
   powered only by the PC through the device port (avoids two 5 V sources).
2. **First flash — enter bootloader:** hold the breakout's **BOOT** button (or a
   DP→3.3 V 10 kΩ pull-up) while tapping **RESET**, release. If there is no BOOT
   button, tie a 10 kΩ from `DP` to 3.3 V and tap RESET.
3. **Tool (any one):**
   - `ch55xtool` (Python, cross-platform): `python3 ch55xtool.py -f firmware.bin`
   - `chflasher.exe` (bundled in the atc1441 repo; needs libusb-win32 via Zadig)
   - `WCHISPTool` (WCH's Windows GUI)
4. **Verify** the CH559 enumerates as a bootloader device and the tool reports
   success.
5. **Reconnect** the Pico `VBUS`→`5V` jumper for run mode.

> The committed `docs/wiring.md` "Flashing the CH559" section describes the
> bare-chip path (a 4-pin `DP`/`DM`/`GND`/`3V3` header + USB-A plug breakout).
> On the breakout, the device USB port replaces that header; everything else in
> that section still applies.

---

## 6. Updating the keybchord firmware (RP2040)

The current keyboard path is `InputUsbHost` (PIO-USB, `lib/hw/input_usbhost.*`).
Add a parallel **UART-bridge** adapter. Suggested steps:

1. **Extract the report parser.** Pull the report→`KeyEvent` logic out of
   `InputUsbHost::onReport()` (modifier byte + 6 keycodes, report-ID skip,
   `modifierBitToUsage`) into a small shared helper (e.g.
   `lib/hw/hid_report.h` / `.cpp`) so `InputUsbHost` and the new adapter both
   use it. Keep the `MAX_KEYS = 6` boot-format semantics for now.
2. **New adapter** `lib/hw/input_ch559.h/.cpp` implementing `InputAdapter`:
   - `begin()` — `Serial1.begin(115200)` (GP0/GP1).
   - `poll()` — read `Serial1`, re-sync on `0xFE`, parse the §4.4 frame, and on
     each `DEVICE_POLL` (`0x04`) with HID-usage keyboard run the shared parser,
     accumulating `KeyEvent`s to return.
   - `connected()` / `reportReceived()` — track `CONNECTED`/`DISCONNECTED`
     (`0x01`/`0x02`) and first report, so the boot-key scan (`detectBootMode()`)
     keeps working unchanged.
   - `setLed()` — send the §4.3(3) LED command to the CH559; return `false`
     until firmware supports it (the beat-LED degrades gracefully, as today).
3. **`pins.h`** — add `PIN_CH559_TX 0` / `PIN_CH559_RX 1` (or rely on `Serial1`
   defaults); document alongside the existing `PIN_DIN_TX`.
4. **`factory.cpp`** — instantiate `InputCh559` instead of `InputUsbHost`. Prefer
   a build flag (e.g. `KEYBCHORD_CH559`) so the legacy PIO-USB build stays
   intact, mirroring how `KEYBCHORD_NATIVE`/`KEYBCHORD_TEST_HOOK` are gated.
5. **`platformio.ini`** — for the CH559 build: drop `sekigon-gonnoc/Pico PIO
   USB` and `-DPIO_USB_DP_PIN_DEFAULT=0`; add the `KEYBCHORD_CH559` flag.
6. **Tests** — the frame parser + report parser are pure logic, so unit-test them
   natively (GoogleTest, `KEYBCHORD_NATIVE`) with captured frames (feed a
   `DEVICE_POLL` byte array, assert `KeyEvent`s). Keep the existing native suite
   green.

---

## 7. Verification checklist

Run in order. Use the Pico's CDC debug log (`[KEY DN/UP …]`, `[INFO] …`) and a
MIDI monitor on the device's USB-MIDI input.

### 7.1 Firmware + flash (standalone)

| # | Check | Pass / Fail |
|---|---|---|
| 1 | CH559 flashes over its device USB port | |
| 2 | On reset it emits `STARTUP` (`0x08`) frames on UART1 (see §3.1) | |

### 7.2 CH559 hosts the keyboard

| # | Check | Pass / Fail |
|---|---|---|
| 3 | Plug a keyboard → `CONNECTED` (`0x01`) + `DEVICE_INFO` (`0x06`) frames | |
| 4 | Press keys → `DEVICE_POLL` (`0x04`, HID usage `0x06`) frames appear | |
| 5 | Unplug → `DISCONNECTED` (`0x02`) | |

### 7.3 RP2040 end-to-end

| # | Check | Pass / Fail |
|---|---|---|
| 6 | Pico debug log shows `[KEY DN/UP usage=0x…]` for real key presses | |
| 7 | Chord press (`T`) + strum (`1`) produce MIDI notes (USB MIDI monitor) | |
| 8 | Rhythm (`F5`) + bass (`F7`) produce drums (ch10) + bass (ch3) | |
| 9 | **Boot-key gestures:** power-on `Esc` → RPI-RP2 bootloader; power-on `Ctrl` → MSC drive | |
| 10 | **Beat LED** on the keyboard flashes (only after §4.3(3) is implemented) | |
| 11 | **Hot-plug:** unplug/replug the keyboard mid-session recovers without reset | |
| 12 | **Problem keyboards:** a hub-connected keyboard and a long-descriptor/NKRO keyboard that failed on PIO-USB now enumerate and type | |
| 13 | Native GoogleTest suite green (`pio test -e native`) | |

---

## 8. Switching to the bare chip (breakout → custom PCB)

When moving to the bare CH559L, the **RP2040 firmware and the CH559 firmware are
unchanged** (same UART1, same baud, same protocol). Only the physical board
changes:

1. **Power.** The bare chip runs at **3.3 V** (the design): tie `VIN5` (41) and
   `VDD33` (42) **both** to `3V3(OUT)`, each with a **0.1 µF** decoupling cap to
   GND. Do **not** feed 5 V to `VIN5` on the final board (the breakout was 5 V
   only to power the keyboard VBUS through its host port).
2. **Keyboard VBUS.** The keyboard now gets 5 V from **Pico `VBUS` directly**
   (receptacle pin 1 → `VBUS`), *not* through the CH559. This is already in
   `docs/wiring.md`; on the breakout the host port handled VBUS for you.
3. **Host-port passives.** Add the **22 Ω** series resistors on `HP` (29) and
   `HM` (30) to the receptacle (the only series elements), plus the **100 µF**
   bulk cap on the 5 V rail near the receptacle.
4. **Programming header.** `DP` (31)/`DM` (32) go to the **4-pin
   `DP`/`DM`/`GND`/`3V3` header** (not a USB connector). Flashing then uses a
   USB-A plug breakout (`D+`→`DP`, `D−`→`DM`, `GND`→`GND`, leave `VBUS`
   floating) per `docs/wiring.md` §"Flashing the CH559".
5. **BOOT/RESET buttons.** Place the two tactile buttons — **BOOT**
   (10 kΩ `DP`→3V3) and **RESET** (`RST` pin 3 → GND) — for first-flash
   bootloader entry.
6. **Unused pins.** `P0.2`/`P0.3` (UART0) and the device-port connector disappear
   entirely; they were only needed by the stock firmware/breakout.
7. **Re-verify.** Re-run §7.2/§7.3 after assembly; pay special attention to #12
   (the problem keyboards) since the bare layout's grounding/decoupling differs.

---

## 9. Open decisions to record before committing this plan

- **UART choice:** confirmed **UART1** (matches the committed wiring). Lock the
  **baud = 115,200** here unless a latency measurement says otherwise.
- **LED SET_REPORT protocol** (§4.3(3)): exact UART command byte(s) from Pico →
  CH559.
- **Report-ID vs boot protocol:** whether the firmware forces boot protocol
  (§4.3(4)) or the RP2040 parser always handles report IDs.
- Whether the CH559 firmware fork lives in this repo (e.g. `firmware/ch559/`) or
  a separate repo, and how its `.bin` is built/stored.

## References

- `docs/wiring.md`, `docs/KeybChord_Pico_Spec.md` §2.2, `KCPico.kicad_sch`
- [`atc1441/CH559sdccUSBHost`](https://github.com/atc1441/CH559sdccUSBHost) (firmware base, GPL-3.0)
- WCH `CH559EVT` (UART1 example, SET_REPORT/control-transfer examples)
- `hardware-test-plan-usb-midi.md` (manual smoke test + MIDI harness, reuse §7)
