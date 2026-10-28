# CH559 Breakout Integration Guide

Step-by-step instructions to connect a **CH559 USB-host breakout board** to a
fully built **KeybChord Pico**, verify the hardware interfaces, flash the custom
firmware, and test end-to-end. This is the M12 bring-up path (see
`hardware-test-plan-ch559.md`): the CH559 replaces the PIO-USB keyboard host, and
the keyboard plugs into the **breakout's host port** instead of the Pico.

The **custom firmware is already developed** in this repo (see "Step 3 — Develop
& flash the custom firmware"). We go straight to that firmware path; the
quick-start / prebuilt-firmware (UART0 @ 1 Mbps) wiring is **not** used unless
the custom firmware cannot be debugged (see "Fallback" at the end).

---

## 1. Materials

| # | Item | Notes |
|---|------|-------|
| 1 | **CH559 USB-host breakout** | e.g. MatzElectronics CH559 USB Host Breakout (Tindie) or Electrodragon CH559 mini. Must break out `P2.6`/`P2.7` (UART1) and `5V`/`GND`. |
| 2 | Fully built **KeybChord Pico** | Pico + DIN/TRS MIDI out + LCD + the existing USB-A keyboard receptacle still in place. |
| 3 | USB keyboard | A plain HID keyboard. Later also a wireless-dongle or hub-connected keyboard (the "problem" cases). |
| 4 | 4× **dupont jumper wires** (male-to-female or male-to-male as your headers dictate) | `5V`, `GND`, `GP0`↔`P2.6`, `GP1`↔`P2.7`. |
| 5 | USB-A → micro-USB cable | Pico → PC (power + CDC debug log). |
| 6 | USB cable for the **CH559 device/programming port** | Matches the breakout's device connector (micro-B, USB-C, etc.). |
| 7 | Breadboard (optional) | To keep the four wires tidy and to host any jumpers. |
| 8 | Multimeter | Continuity + 5 V rail checks (Step 2). |
| 9 | PC with **Python 3** + `ch55xtool` | Flashes the CH559 (`pip install ch55xtool`). |
| 10 | **SDCC ≥ 4.x** | Builds the CH559 firmware (`sudo apt install sdcc`). |
| 11 | **PlatformIO** | Builds the RP2040 firmware. |
| 12 | MIDI monitor | A hardware synth on DIN, or the Pico's USB-MIDI device + DAW/`mido` (`tools/midi_test/`). |
| 13 | USB-serial adapter (3.3 V logic) | *Optional* — to watch the CH559 UART1 frames directly (Step 2). |

---

## 2. Breakout board orientation

Identify the parts of the breakout (labels vary by vendor):

- **Host USB port** → wired to `HP`/`HM` (P5.5/P5.4). This is where the **keyboard** plugs in.
- **Device/programming USB port** → wired to `DP`/`DM` (P5.1/P5.0). This is where you plug the **PC** to flash the CH559.
- **4-pin header** (`Rx/0.2`, `Tx/0.3`, `5V`, `GND`) → UART0 + power. **Not** used in the primary wiring (we use UART1).
- **Broken-out pins** labelled `P2.6` / `RXD1` and `P2.7` / `TXD1` → UART1, used below.

| Breakout label / pin | Function |
|---|---|
| `P2.6` / `RXD1` (27) | UART1 RX ← Pico TX |
| `P2.7` / `TXD1` (28) | UART1 TX → Pico RX |
| `5V` (= `VIN5`, 41) | 5 V power in (powers the host port's VBUS) |
| `GND` (18) | ground |
| `P5.7` / `RST` (3) | reset (tie to a RESET button → GND if present) |

---

## 3. Step 1 — Connect the breakout to the keybchord

> **Important re-wiring note.** A stock KeybChord Pico currently uses `GP0`/`GP1`
> as the PIO-USB host (`GP0` = D+, `GP1` = D− to the keyboard receptacle). For the
> CH559 build those two pins become the **UART bridge**. Disconnect the keyboard
> receptacle's D+/D− from `GP0`/`GP1` (unplug the duponts, or leave the receptacle
> unused), and instead wire `GP0`/`GP1` to the breakout. The keyboard now plugs
> into the **breakout's host port**.

Wiring (4 wires):

| Pico pin | Direction | Breakout pin |
|---|---|---|
| `VBUS` (40, 5 V) | → | `5V` (`VIN5`, 41) |
| `GND` (3 or 8) | ↔ | `GND` (18) |
| `GP0` (1, UART0 TX) | → | `P2.6` / `RXD1` (27) |
| `GP1` (2, UART0 RX) | ← | `P2.7` / `TXD1` (28) |

Then:

1. Plug the **keyboard** into the breakout's **host** USB port.
2. Plug the Pico into the PC via its **micro-USB** (this powers the Pico; `VBUS`
   then powers the breakout and, through it, the keyboard).
3. Leave the breakout's **device** USB port unconnected for now (used only for
   flashing, Step 3).

> **Power rule.** Feed the breakout **5 V** from Pico `VBUS` (not 3.3 V). The
> breakout's host port sources the keyboard's 5 V VBUS from its `VIN5` rail, so a
> 3.3 V feed would starve the keyboard. The CH559 I/O is 3.3 V (from its internal
> `VDD33`) and 5 V-tolerant, so the UART signals to the Pico stay 3.3 V-safe.

---

## 4. Step 2 — Verify the hardware interfaces

Do these **before** flashing any firmware (they catch wiring mistakes cheaply).

1. **Power off** everything. With the multimeter in continuity mode:
   - Pico `GND` ↔ breakout `GND` → beep.
   - Pico `GP0` ↔ breakout `P2.6/RXD1` → beep.
   - Pico `GP1` ↔ breakout `P2.7/TXD1` → beep.
   - Pico `VBUS` ↔ breakout `5V` → beep.
   - No unintended shorts: `5V` ↔ `GND` on the breakout must **not** beep.
2. **Power on** (Pico to PC). With the multimeter in DC volts:
   - Breakout `5V` ↔ `GND` ≈ **5.0 V**.
   - Breakout host-port `VBUS` ↔ `GND` ≈ **5.0 V** (keyboard powered).
   - Pico `3V3(OUT)` ≈ 3.3 V (sanity).
3. **No magic smoke, no excessive heat** on the CH559. The CH559 runs off its
   internal oscillator — it draws little current and stays cool.

### 4.1 Verify the CH559 UART1 bridge (standalone, optional but recommended)

This isolates the CH559 from the Pico so a fault is easy to localize. You need a
USB-serial adapter at 3.3 V logic and a serial terminal at **115200 8N1**.

1. Flash the CH559 first (Step 3) if you haven't yet, or use the already-flashed
   chip.
2. Tie the USB-serial adapter's GND to breakout `GND`; adapter RX ← breakout
   `P2.7/TXD1`.
3. On reset the CH559 emits `STARTUP` (`0x08`) frames — a stream of bytes
   beginning `0xFE 0x00 0x00 0x08 … 0x0A`.
4. Plug the keyboard into the breakout host port → expect `CONNECTED` (`0x01`)
   and `DEVICE_INFO` (`0x06`) frames. Press keys → `DEVICE_POLL` (`0x04`) frames
   whose payload is the 8-byte boot report. Unplug → `DISCONNECTED` (`0x02`).

If you see the `0xFE … 0x0A` framing, the CH559 hardware + firmware are good.
(If nothing appears, see "Fallback".)

---

## 5. Step 3 — Develop & flash the custom firmware

The custom firmware is **already implemented** in this repo. This section
summarizes what was changed and how to (re)build and flash it.

### 5.1 CH559 firmware (`firmware/ch559/`)

Key changes from the upstream `atc1441/CH559sdccUSBHost` fork:

1. **UART1 @ 115200** — the protocol now emits on UART1 (`RXD1`/`TXD1` =
   P2.6/P2.7) instead of UART0 @ 1 Mbps. `initUART1()` (in `util.c`) sets the pin
   mode (`010`), 8N1, and the baud divisor; `putchar()`/`processUart()` now use
   `SER1_*`. Baud is `CH559_UART_BAUD` in `config.h`.
2. **HID boot protocol** — during enumeration the firmware issues
   `SET_PROTOCOL = boot` so keyboards report the standard 8-byte boot format (no
   report ID). See `setHidBootProtocol()` in `USBHost.c`.
3. **LED SET_REPORT relay** — the RP2040 sends `0xED` + 1 LED byte over UART1;
   `processUart()` (in `uart.c`) calls `setHidKeyboardLed()`, which issues a
   `SET_REPORT(output)` through `hostCtrlTransfer()` with the 1-byte LED report.
   This powers the beat-LED.
4. **Startup/hot-plug detection** — `checkRootHubConnections()` polls the USB
   attach levels instead of relying only on the `UIF_DETECT` edge, so a keyboard
   already connected when the CH559 powers up still enumerates.

Build (SDCC):

```bash
sudo apt install sdcc        # Debian/Ubuntu
cd firmware/ch559
make                         # produces ch559.hex + ch559.bin
```

Flash the CH559 over its **device** USB port (see `docs/wiring.md` §"Flashing
the CH559" and `hardware-test-plan-ch559.md` §5):

```bash
# Clone the cross-platform flasher, then flash the built binary:
git clone https://github.com/MarsTechHAN/ch552tool.git
python3 ch552tool/ch55xtool.py -f firmware/ch559/ch559.bin
```

(`ch55xtool` is also available via `pip install ch55xtool`; `WCHISPTool` is the
Windows GUI alternative.)

Bootloader entry for the first flash: hold the breakout's **BOOT** button (or
pull `DP` high through 10 kΩ to 3.3 V) while tapping **RESET**, then release.
The CH559 enumerates as a USB bootloader device on the PC; `ch55xtool` flashes
and resets it. If you are powering the CH559 from Pico `VBUS` during flashing,
disconnect the Pico `VBUS`→`5V` jumper first so only the PC powers it.

### 5.2 RP2040 firmware (`keybchord/`)

The RP2040 firmware has a single input path: the CH559 UART bridge.

- `lib/hw/ch559_protocol.h` — streaming `0xFE`-delimited frame parser (pure C++).
- `lib/hw/hid_report.h` — shared boot-report parser (`report → KeyEvent` logic).
- `lib/hw/input_ch559.{h,cpp}` — `InputCh559` adapter: `Serial1` at 115200 on
  `GP0`/`GP1`, feeds frames to the parser, emits `KeyEvent`s, and relays
  `setLed()` as the `0xED` command.
- `lib/hw/pins.h` — `PIN_CH559_TX 0`, `PIN_CH559_RX 1`, `CH559_UART_BAUD 115200`.
- `lib/hw/factory.cpp` — instantiates `InputCh559`.
- `platformio.ini` — `[env:pico]` is the CH559 build; the Pico PIO USB
  dependency and the legacy PIO-USB build have been removed.
- `test/test_ch559.cpp` — native unit tests for the frame + report parsers.

Build and flash the RP2040:

```bash
cd keybchord
pio run -e pico                # builds .pio/build/pico/firmware.uf2
```

Flash the Pico: hold **BOOTSEL**, plug into USB, drag `firmware.uf2` onto the
RPI-RP2 drive. (Enclosed build: power off, hold **Esc** on the keyboard, power on.)

### 5.3 Verify the builds (CI-style)

```bash
cd keybchord
pio test -e native              # 427 tests incl. Ch559FrameParser + HidBootReport
cd ../firmware/ch559 && make    # ch559.bin builds clean (warnings only)
```

---

## 6. Step 4 — Test the keybchord with the breakout

After both firmwares are flashed and wired as in Step 1:

1. Open the Pico CDC serial monitor at **115200** (`pio device monitor`).
   On boot you should see the `KeybChord Pico` banner; the CH559 `STARTUP` frame
   is consumed by the adapter (not echoed raw).
2. **Keyboard connect:** plug the keyboard into the breakout host port. The
   adapter flips `connected()`; press keys and the log shows `[KEY DN/UP
   usage=0x… mods=0x…]`.
3. **Chord + strum:** hold a chord key (e.g. `T`) and strum the number row
   (`1`) → MIDI notes appear on the USB-MIDI monitor and/or the DIN synth.
4. **Rhythm + bass:** `F5` (rhythm) → drums on ch10; `F7` (bass) → bass on ch3.
5. **Boot-key gestures:** power off, hold **Esc**, power on → the Pico enters the
   RPI-RP2 bootloader. Power off, hold **Ctrl**, power on → the "KeybChord" MSC
   drive mounts for config editing.
6. **Beat LED:** with a keyboard that has a CapsLock/NumLock/ScrollLock LED, the
   beat indicator should flash it (see checklist #10 — this exercises the LED
   SET_REPORT relay).
7. **Hot-plug:** unplug/replug the keyboard mid-session → it recovers without a
   reset.
8. **Problem keyboards:** a hub-connected keyboard and a long-descriptor/NKRO
   keyboard that failed on PIO-USB should now enumerate and type.

### Verification checklist

| # | Check | Pass / Fail |
|---|---|---|
| 1 | CH559 flashes over its device USB port | |
| 2 | On reset it emits `STARTUP` (`0x08`) frames on UART1 @ 115200 | |
| 3 | Keyboard → `CONNECTED` (`0x01`) + `DEVICE_INFO` (`0x06`) | |
| 4 | Keys → `DEVICE_POLL` (`0x04`, usage `0x06`) frames | |
| 5 | Unplug → `DISCONNECTED` (`0x02`) | |
| 6 | Pico debug log shows `[KEY DN/UP …]` | |
| 7 | Chord (`T`) + strum (`1`) → MIDI notes | |
| 8 | Rhythm (`F5`) + bass (`F7`) → drums (ch10) + bass (ch3) | |
| 9 | Boot keys: `Esc` → bootloader, `Ctrl` → MSC drive | |
| 10 | Beat LED flashes the keyboard LED | |
| 11 | Hot-plug recovers without reset | |
| 12 | Hub/NKRO/long-descriptor keyboards enumerate | |
| 13 | `pio test -e native` green (427 tests) | |

---

## 7. Fallback (only if the custom firmware cannot be debugged)

If the CH559 does not produce `0xFE … 0x0A` frames on UART1 after flashing, the
quick-start path can confirm the chip enumerates a keyboard at all, using the
stock prebuilt firmware on **UART0 @ 1 Mbps**:

| Pico pin | →/← | Breakout pin |
|---|---|---|
| `VBUS` (40) | → | `5V` |
| `GND` | → | `GND` |
| `GP0` (TX) | → | `Rx/0.2` (P0.2/RXD_, pin 38) |
| `GP1` (RX) | ← | `Tx/0.3` (P0.3/TXD_, pin 37) |

Flash the stock `CH559USB.bin` (`atc1441/CH559sdccUSBHost`) and read UART0 at
**1 Mbps** with a USB-serial adapter or a Pico sketch opening `Serial1` at
1,000,000 baud. If keys enumerate here but not on UART1, the fault is in the
fork's UART1 setup; if nothing enumerates here either, the fault is powering or
the breakout itself.

---

## 8. Next step (breakout → bare CH559L on the PCB)

The RP2040 firmware and the CH559 firmware are **unchanged** when moving to the
bare chip (same UART1, same baud, same protocol). Only the board changes: power
`VIN5`/`VDD33` both from `3V3(OUT)` (0.1 µF decoupling), take the keyboard's
VBUS from Pico `VBUS` directly, add the 22 Ω series resistors on `HP`/`HM`, and
add a 4-pin `DP`/`DM`/`GND`/`3V3` programming header. See
`hardware-test-plan-ch559.md` §8 and `docs/wiring.md`.

---

## References

- `hardware-test-plan-ch559.md` — the working bring-up plan (framing §4.4, flash §5).
- `docs/wiring.md` — final board wiring (bare-chip) and CH559 flashing.
- `docs/KeybChord_Pico_Spec.md` §2.2 — canonical pin map.
- `firmware/ch559/README.md` — CH559 fork provenance, build, divergences.
- `firmware/ch559/` (`util.c`, `uart.c`, `USBHost.c`, `config.h`) — the firmware changes.
- `keybchord/lib/hw/` (`ch559_protocol.h`, `hid_report.h`, `input_ch559.*`) — the adapter.
