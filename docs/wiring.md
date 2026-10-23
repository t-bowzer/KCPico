# KeybChord (Pico Edition) — Wiring Diagram

Visual companion to the canonical pin map. The pin/power tables in
**Spec §2.2** ([KeybChord_Pico_Spec.md](KeybChord_Pico_Spec.md)) remain
**authoritative** for exact pins and component values; this page is a rendered
connection view of those tables.

This is a **logical connection diagram** (which wire goes where), not a physical
breadboard layout. All resistors are **1/4 W (0.25 W)**.

## Connection tables

The tables below are the routing view — "which terminal goes where". For exact
pin functions and power specs, see Spec §2.2.

### Raspberry Pi Pico (RP2040)

| Pico pin | Signal | Routes to |
|---|---|---|
| `VBUS` (40) | 5 V | keyboard USB-A receptacle `VBUS` (pin 1) |
| `3V3 OUT` (36) | 3.3 V rail | CH559 `VIN5` (41) + `VDD33` (42); LCD `VCC`; programming header `3V3`; MIDI source (`DIN 4` / `TRS ring`) via **33 Ω**; BOOT pull-up via **10 kΩ** |
| `GND` (3, 8, …) | ground | CH559 `GND` (18); keyboard `GND` (pin 4); LCD `GND`; programming header `GND`; MIDI `GND` (`DIN 2` / `TRS sleeve`); RESET switch |
| `GP0` (1) | UART0 TX | CH559 `RXD1` (27) |
| `GP1` (2) | UART0 RX | CH559 `TXD1` (28) |
| `GP4` (6) | I2C0 SDA | LCD/PCF8574 `SDA` |
| `GP5` (7) | I2C0 SCL | LCD/PCF8574 `SCL` |
| `GP8` (11) | UART1 TX | `DIN 5` via **10 Ω**; `TRS tip` via **10 Ω** (shared) |

### CH559L

| CH559 pin | Signal | Routes to |
|---|---|---|
| `RXD1` (27) | UART RX | Pico `GP0` (TX) |
| `TXD1` (28) | UART TX | Pico `GP1` (RX) |
| `HP` (29) | host D+ | USB-A `D+` (pin 3) via **22 Ω** |
| `HM` (30) | host D− | USB-A `D−` (pin 2) via **22 Ω** |
| `DP` (31) | device D+ | programming header `DP`; BOOT button via **10 kΩ** to `3V3` |
| `DM` (32) | device D− | programming header `DM` |
| `VIN5` (41) | 3.3 V | Pico `3V3 OUT`; **0.1 µF** decoupling to `GND` |
| `VDD33` (42) | 3.3 V | Pico `3V3 OUT`; **0.1 µF** decoupling to `GND` |
| `GND` (18) | ground | Pico `GND` |
| `RST` (3) | reset | RESET button → `GND` |

### USB-A receptacle (keyboard host)

| Receptacle pin | Routes to |
|---|---|
| 1 `VBUS` | Pico `VBUS` (5 V) |
| 2 `D−` | CH559 `HM` (30) via **22 Ω** |
| 3 `D+` | CH559 `HP` (29) via **22 Ω** |
| 4 `GND` | Pico `GND` |

### Programming header (4-pin 0.1")

| Header pin | Routes to |
|---|---|
| `DP` | CH559 `DP` (31) |
| `DM` | CH559 `DM` (32) |
| `GND` | Pico `GND` |
| `3V3` | Pico `3V3 OUT` (reference only) |

### DIN-5 MIDI OUT

| Pin | Routes to |
|---|---|
| 2 `GND` | Pico `GND` |
| 4 (source) | Pico `3V3 OUT` via **33 Ω** |
| 5 (data) | Pico `GP8` via **10 Ω** |

### 3.5 mm TRS MIDI OUT (Type-A, shared with DIN)

| Contact | Routes to |
|---|---|
| tip (data) | Pico `GP8` via **10 Ω** (shared) |
| ring (source) | Pico `3V3 OUT` via **33 Ω** (shared) |
| sleeve (GND) | Pico `GND` |

### LCD1602 + PCF8574

| Pin | Routes to |
|---|---|
| `SDA` | Pico `GP4` |
| `SCL` | Pico `GP5` |
| `VCC` | Pico `3V3 OUT` |
| `GND` | Pico `GND` |

## Wiring diagram

A single consolidated diagram renders below, with every wire in one place. It is drawn
with **Graphviz `dot`** using orthogonal (right-angle) edges, so the wires never cross
and read like a schematic; each box is one board/connector. The DOT source is the
editable source of truth — regenerate the PNG after editing (see "Regenerating the PNG").

![KeybChord Pico wiring diagram](wiring.png)

<details>
<summary>DOT source (editable)</summary>

```dot
digraph KeybChord {
    graph [rankdir=LR, splines=ortho, nodesep=0.5, bgcolor="white", ranksep=0.9, fontname="Helvetica"];
    node [shape=box, style="rounded", fontname="Helvetica", fontsize=11];
    edge [fontname="Helvetica", fontsize=9, color="#333333"];

    subgraph cluster_power {
        label="Power";
        SRC [label="5V supply\n≥1A"];
        USBC [label="Pico\nmicro-USB"];
        VBUS [label="VBUS (40)"];
        D1 [label="Schottky D1"];
        VSYS [label="VSYS (39)"];
        V33 [label="3V3 OUT (36)"];
        GND [label="GND (3,8,…)"];
        CAP [label="100µF"];
    }

    subgraph cluster_pico {
        label="Raspberry Pi Pico";
        GP0 [label="GP0\nUART0 TX (1)"];
        GP1 [label="GP1\nUART0 RX (2)"];
        GP4 [label="GP4\nSDA (6)"];
        GP5 [label="GP5\nSCL (7)"];
        GP8 [label="GP8\nUART1 TX (11)"];
    }

    subgraph cluster_ch559 {
        label="CH559L host";
        CRX [label="RXD1 (27)"];
        CTX [label="TXD1 (28)"];
        CHP [label="HP (29)"];
        CHM [label="HM (30)"];
        CDP [label="DP (31)"];
        CDM [label="DM (32)"];
        CV5 [label="VIN5 (41)"];
        CVD [label="VDD33 (42)"];
        C_VIN [label="0.1µF"];
        C_VDD [label="0.1µF"];
        CGN [label="GND (18)"];
        CRST [label="RST (3)"];
    }

    subgraph cluster_kbd {
        label="USB-A receptacle";
        KVB [label="VBUS (1)"];
        KDP [label="D+ (3)"];
        KDM [label="D− (2)"];
        KGN [label="GND (4)"];
    }

    subgraph cluster_prog {
        label="Programming header (4-pin)";
        PDP [label="DP"];
        PDM [label="DM"];
        PGN [label="GND"];
        PV3 [label="3V3"];
    }

    BOOT [label="BOOT button"];
    RESET [label="RESET button"];
    R_HP [label="22Ω"];
    R_HM [label="22Ω"];
    R_BOOT [label="10kΩ"];

    subgraph cluster_midi {
        label="MIDI OUT (DIN-5 + TRS, parallel)";
        R10 [label="10Ω"];
        R33 [label="33Ω"];
        MDATA [label="data bus"];
        MSRC [label="source bus"];
        DIN5 [label="DIN 5"];
        DIN4 [label="DIN 4"];
        DIN2 [label="DIN 2 GND"];
        TIP [label="TRS tip"];
        RING [label="TRS ring"];
        SLEEVE [label="TRS sleeve"];
    }

    subgraph cluster_lcd {
        label="LCD1602 + PCF8574";
        LSDA [label="SDA"];
        LSCL [label="SCL"];
        LVCC [label="VCC"];
        LGND [label="GND"];
    }

    SRC -> USBC -> VBUS;
    VBUS -> D1 -> VSYS -> V33;
    VBUS -> KVB [label="5V"];
    VBUS -> CAP -> GND;

    GP0 -> CRX [label="TX"];
    GP1 -> CTX [label="RX"];
    CHP -> R_HP;
    R_HP -> KDP;
    CHM -> R_HM;
    R_HM -> KDM;
    CDP -> PDP;
    CDM -> PDM;
    GP8 -> R10;
    R10 -> MDATA;
    MDATA -> DIN5;
    MDATA -> TIP;
    GP4 -> LSDA;
    GP5 -> LSCL;

    V33 -> CV5;
    V33 -> CVD;
    CV5 -> C_VIN -> GND;
    CVD -> C_VDD -> GND;
    V33 -> LVCC;
    V33 -> PV3;
    V33 -> R33;
    R33 -> MSRC;
    MSRC -> DIN4;
    MSRC -> RING;
    V33 -> R_BOOT;
    R_BOOT -> BOOT;
    BOOT -> CDP;

    GND -> CGN;
    GND -> KGN;
    GND -> LGND;
    GND -> PGN;
    GND -> DIN2;
    GND -> SLEEVE;
    GND -> RESET;
    RESET -> CRST;
}
```

</details>

## Legend & notes

- **All resistors 1/4 W (0.25 W)** — actual dissipation is only milliwatts.
- **USB host (CH559L coprocessor on UART0):** the keyboard is hosted by a dedicated
  **CH559L** (WCH 8051 MCU with a hardware USB host controller). It enumerates the
  keyboard (hubs + report/NKRO), parses HID reports, and forwards key events to the Pico
  over **UART0** — `GP0`=TX (→ CH559 `RXD1` pin 27), `GP1`=RX (← CH559 `TXD1` pin 28).
  The CH559 **HP** (pin 29) / **HM** (pin 30) run to the receptacle through **22 Ω** series
  resistors (the only series elements; the keyboard supplies device-side pull-ups). `DP`
  (pin 31) / `DM` (pin 32) go to a **4-pin programming header** for flashing the CH559 via
  its USB bootloader. **Power:** `VIN5` (pin 41) and `VDD33` (pin 42) both to `3V3(OUT)`
  (0.1 µF decoupling; at a <3.6 V supply the internal LDO is bypassed), `GND` (pin 18) to
  ground. Runs off its **internal oscillator** (no crystal). `RST` is pin 3.
  **Programming aids (drawn):** a **BOOT** tactile button pulls `DP` up to `3V3` through a
  **10 kΩ** resistor (hold it during reset to enter the USB bootloader), and a **RESET**
  tactile button grounds `RST` (pin 3) to reset the CH559.
- **DIN MIDI OUT (3.3V loop):** driven directly from the 3.3V `GP8` TX pin —
  **10 ohm** in series with `GP8` to DIN pin 5, **33 ohm** from `3V3(OUT)` to DIN pin 4,
  GND to DIN pin 2. Standards-compliant per the MIDI 1.0 Electrical Spec update (2014).
  In firmware, call `Serial2.setTX(8)` before `Serial2.begin(31250)`. The RP2040 UART TX
  is push-pull (not open-drain) — a benign deviation that works with standard
  opto-isolated inputs; optional ferrite beads on the DIN signal pins reduce RFI.
- **TRS MIDI OUT (3.5mm, Type-A):** wired **in parallel** with the DIN loop and
  shares the same 10 ohm / 33 ohm resistors (not duplicated). Type-A: **tip** =
  DIN pin 5 (data, `GP8`), **ring** = DIN pin 4 (source, `3V3`), **sleeve** =
  DIN pin 2 (GND). Both jacks drive a downstream synth simultaneously.
- **LCD1602/PCF8574:** I2C0 on `GP4` (SDA) / `GP5` (SCL); **powered at 3.3V** so the I2C
  bus stays 3.3V-safe. Pull-ups from the backpack or 4.7 kohm to 3.3V. Firmware
  auto-probes address `0x27` then `0x3F`, falling back to the null LCD adapter if absent.
- **Power (canonical):** native micro-USB -> `VBUS` powers the Pico; the keyboard draws
  5V from `VBUS` (upstream of onboard Schottky **D1**, so keyboard current bypasses D1).
  Use a **>=1 A** supply sized for the cable/connector combined current, and a **100 uF**
  bulk cap on the 5V rail near the receptacle (hot-plug inrush / brownout protection).
- **Back-feed rule:** do **not** inject a regulated 5V into `VSYS` while also powered from
  native USB unless that external `VSYS` feed has **its own series Schottky diode**. Pick
  one power path. (The onboard D1 only covers `VBUS`->`VSYS`.)
- **Master power switch (recommended, not required):** the Pico's micro-USB is soldered
  directly to `VBUS`, so there is no on-board break point. A switch that cuts the Pico *and*
  the keyboard lives **off-board** — an inline USB power switch or a switched PSU (not drawn).
- **3V3(OUT) budget:** the LCD runs off `3V3(OUT)` (~300 mA rail shared with the RP2040);
  keep a backlit LCD within budget or power its backlight separately.

For reference designs and precedent (rppicomidi, Adafruit MIDI FeatherWing, PJRC/Teensy,
MIDI.org 2014 update, CH559 datasheet / HIDman / atc1441 CH559sdccUSBHost, Pico datasheet
power section), see the spec.

## Flashing the CH559

The CH559 is programmed over **USB** (its `DP`/`DM` device port), not UART. Only **D+,
D-, GND** are needed from the PC — the board self-powers at 3.3 V, so do **not** connect
the PC's 5 V `VBUS` into the board.

**Board-side connector (recommended):** a **4-pin 0.1" (2.54 mm) male header** — `DP`
(pin 31), `DM` (pin 32), `GND`, and `3V3` (the 4th pin is optional, used only as a power
reference). Through-hole and trivial to hand-solder. The header's `GND`/`3V3` are the
Pico's ground and `3V3(OUT)` (the same rail that powers the CH559); the `3V3` pin is a
reference only — do not feed power into it and never connect the PC's 5 V `VBUS`.

**Cable/programmer:** a **USB-A male plug breakout** (a small board with a USB-A plug and
labelled `VBUS`/`D+`/`D-`/`GND` pads). Solder a 4-pin female dupont/JST-XH pigtail to it —
`D+`→DP, `D-`→DM, `GND`→GND, and leave `VBUS` unconnected — then plug the USB-A straight
into the PC. Avoid a raw USB-A↔A cable (host-to-host hazard); a labelled breakout or a cut
cable is the safe route.

**Bootloader entry (first flash):** CH55x enters its USB bootloader by pulling **D+ (DP)
high through ~10 kΩ** during reset. So also place two tactile buttons (or two shortable
pads) on the board: **BOOT** (10 kΩ `DP`→3V3) and **RESET** (pin 3). After the first
flash, `ch55xtool`/ch55xduino re-enters the bootloader over USB automatically, so the
buttons/pads are mainly for initial programming.

**Software:**
- [`ch55xtool`](https://github.com/MarsTechHAN/ch552tool) — Python, cross-platform:
  `python3 ch55xtool.py -f ch559.bin`.
- [`WCHISPTool`](https://www.wch.cn/downloads/WCHISPTool_Setup_exe.html) — WCH's Windows tool.
- Firmware source: **`firmware/ch559/`** in this repo (a fork of
  [`atc1441/CH559sdccUSBHost`](https://github.com/atc1441/CH559sdccUSBHost), built
  with SDCC — `make` produces `ch559.hex`/`ch559.bin`). WCH's `CH559EVT` host
  examples are an alternative base.

**Parts / sources:**
- **USB-A plug breakout** (the PC-side plug with labelled pads):
  [Adafruit 4448 — USB Type A Plug Breakout Cable w/ female jumpers](https://www.adafruit.com/product/4448)
  (ready-made pigtail, recommended) or
  [Adafruit 1387 — USB DIY Connector Shell (Type A Male Plug)](https://www.adafruit.com/product/1387)
  (bare shell; solder your own wires). Generic "USB A plug breakout" modules are also
  available from AliExpress/Amazon.
- **Board header + pigtail:** a 1×4 **0.1" (2.54 mm) male pin header** — e.g.
  [Adafruit 392 — Break-away 0.1" 36-pin strip male header](https://www.adafruit.com/product/392)
  (snap off 4 pins) — plus female dupont jumpers to mate with it, e.g.
  [Adafruit 1951 — Premium Female/Female Jumper Wires, 20×3"](https://www.adafruit.com/product/1951)
  or [Adafruit 266 — 40×6"](https://www.adafruit.com/product/266). (JST-XH pigtails are an
  alternative from Digi-Key/LCSC.)
- **Tactile switches:** 2× 6×6 mm tactile pushbutton (BOOT/RESET) — e.g.
  [Adafruit 367 — Tactile Switch Buttons](https://www.adafruit.com/product/367) or any
  Digi-Key/LCSC equivalent.
- **Passives (board):** 1× 10 kΩ resistor (BOOT pull-up) and 2× 0.1 µF capacitors
  (`VIN5`/`VDD33` decoupling) — generic parts.

**Procedure:** hold **BOOT**, tap **RESET**, release **BOOT** → the CH559 enumerates as a
USB bootloader device on the PC → run `ch55xtool -f <firmware>.bin` → done.

## Regenerating the PNG

`wiring.png` is rendered from the DOT block above using Graphviz `dot` (orthogonal
splines). Easiest is the Kroki web API:

```bash
# Extract the ```dot block to wiring.dot, then:
curl -s -X POST https://kroki.io/graphviz/png \
  -H "Content-Type: text/plain" --data-binary @wiring.dot -o docs/wiring.png
```

Or render locally if Graphviz is installed:

```bash
dot -Tpng -Gbgcolor=white -o docs/wiring.png wiring.dot
```

The `rankdir=LR` + `splines=ortho` combination is what keeps the edges at right angles
and crossing-free; preserve it when editing.
