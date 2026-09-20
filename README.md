# T3 Programmable Controller — STM32

Work to update this firmware for the older STM32 based Temco products, mainly
around the display. This is a fork; everything here is developed and reviewed on
the fork rather than upstream.

## Scope

The MCU is an STM32F103ZE. The application is linked at `0x08008000` — the
bootloader lives below that and is **not** part of this repository.

There are three Keil targets under `arm/USER/`:

| Project | Target |
| --- | --- |
| `Tstat10_wifi.uvprojx` | `ARM_TSTAT_WIFI` — the thermostat with the LCD |
| `mini_arm.uvprojx` | `ARM_MINI` |
| `CM5_arm.uvprojx` | `ARM_CM5` |

All display work so far is confined to the `Tstat10_wifi` build: `arm/MENU/*` and
`arm/HARDWARE/LCD/ER-TFT024-3_4-Wire_SPI.c` are only compiled there. The panel is
240x320 over SPI in RGB565.

The screen only runs on a board whose `mini_type` is `MINI_TSTAT10` (9) or
`MINI_T10P` (11) — see the gate in `common/main.c` around `LCD_Intial()`. T3000
labels type 11 "T3-OEM".

## What has changed

### Paged idle screen

The main screen kept its layout, but the three rows are no longer locked to
VAR1-VAR3. Each page shows the next three VARs, so a program can drive up to
`PAGE_MARK_MAX * 3` values instead of three.

- **RIGHT** steps to the next page and wraps. This key did nothing on the idle
  screen before.
- **LEFT** still picks a row within the page; **LEFT+RIGHT** still opens the menu.
- Pages past the last VAR that carries a label are not offered, so a panel that
  labels VAR1-VAR6 gets two pages rather than eight.
- A strip of marks down the right hand side (x=227, beside the rows) shows which
  page is up. It is hidden when there is only one page.

### Dark palette

`TSTAT8_BACK_COLOR` and friends were retuned to a dark scheme. Because
`disp_icon()` ignores its colour arguments and blits literal RGB565, every icon
carried the old background baked into it, so all 25 icon arrays were recomposited
against the new background rather than colour-replaced — see `recolour_icons.py`
below.

### A real typeface

The three original bitmap fonts were regenerated from Consolas Bold at the sizes
the firmware already used, keeping the cap height and cap top row so nothing
moved on screen.

### Full length row labels

`Str_variable_point.label` is nine bytes, but the rows only ever drew three of
them, so a point called `SETPOINT` read as `SET`. They now draw all eight.

`draw_tangle()` puts the value box at x=102, so the label owns x=0..101; eight
12-dot cells from x=2 end at x=97. Twelve is both the widest cell that fits eight
characters and about the narrowest that stays legible, so `char_12_24` is the same
face at the same cap height as `char_16_24`, condensed to 0.68.

Labels come from the VAR label field in T3000 — until you set them there, the rows
still read `SET`, `ROO`, `MOD`, just in the narrower face.

## Tools

`tools/` holds the scripts that generate the tables in
`arm/HARDWARE/LCD/ER-TFT024-3_4-Wire_SPI.c`. They are run from the repository
root and need Python 3 with Pillow.

| Script | What it does |
| --- | --- |
| `lcddata.py` | Shared reader for the arrays and `#define`s. Not run directly. |
| `fontgen.py` | Regenerates the four bitmap font tables from a TrueType face. |
| `recolour_icons.py` | Recomposites the icon bitmaps for a new screen background. |
| `screenshot.py` | Renders the idle screen to a PNG from the real arrays. |

```bash
python tools/fontgen.py                       # report the fit, change nothing
python tools/fontgen.py --write               # replace the arrays
python tools/recolour_icons.py --bg "#0D1520" --write
python tools/screenshot.py out.png --labels "SETPOINT,ROOM TMP,MODE" --values "72.0,71.4,HEAT"
```

Two things about these are worth knowing before changing them.

`fontgen.py` proves its bit packing by re-encoding every shipped array and
comparing byte for byte **before** it generates anything, so a mistake in the bit
order cannot reach the device. That check stands in for hardware testing and
should be kept.

`lcddata.py` tokenises arrays on commas and resolves `#define`d names, raising on
anything it cannot resolve. This matters because the four corner pieces used by
`draw_tangle()` mix hex literals with colour names — a hex-only parser silently
returns 58 of `leftup`'s 64 pixels and corrupts the frame corners.

## Building

Keil MDK, ARMCC V5.06. Headless:

```bash
"C:/Keil_v5/UV4/UV4.exe" -r arm/USER/Tstat10_wifi.uvprojx -o build.log
```

Use `-r` (full rebuild) rather than `-b` when comparing warning counts, or you are
comparing an incremental build against a full one. Do not pass `-j1`; UV4 does not
accept it and blocks on a dialog instead of failing.

Two notes on the toolchain:

- UV4 rewrites `Tstat10_wifi.uvprojx` on build to match the locally installed
  compiler and device pack. That drift should not be committed.
- The scatter file is **generated** from the target dialog, not hand written, so
  the memory map is edited through the `OCR_RVCT` entries in the project rather
  than in `arm/OBJ/Tstat10_arm_revxx.sct`. The `<ScatterFile>` entry naming
  `..\OBJ\test.sct` is a stale leftover; no such file exists and nothing reads it.
  Both RAM regions were declared larger than the silicon and have been
  corrected: `RW_IRAM1` to `0x10000`, the STM32F103ZE's internal SRAM, and
  `RW_RAM1` to `0x80000`, the IS61L5128L on the board. `RW_RAM1` had been
  declared as `0x800000` — sixteen times the real part — on a region that runs
  about 92% full, so an overflow would have linked cleanly and corrupted at run
  time. It now fails the link instead.
- Building dirties the checked-in artifacts under `arm/OBJ/`. Those are not part
  of any commit on this branch.

Current state of the `Tstat10_wifi` target: **0 errors, 492 warnings** (down from
510 — the 18 that went were `char*` / `unsigned char*` mismatches removed by
explicit casts).

| Region | Used | Of | Free |
| --- | --- | --- | --- |
| `ER_IROM1` flash | `0x47bb8` | `0x60000` | ~97 KB |
| `RW_RAM1` external SRAM | `0x75368` | `0x80000` | ~43 KB |
| `RW_IRAM1` internal SRAM | `0x5360` | `0x10000` | ~43 KB |

Capping `RW_RAM1` at the real 512 KB had a side effect worth knowing: the linker
now spills about 21 KB into internal SRAM, which had been sitting entirely unused
because `.ANY` swept everything into the oversized external region. Internal SRAM
is single cycle where the FSMC part is not, so that is free speed rather than a
regression.

## Not done yet

- **Nothing here has been verified on hardware.** The font packing is proven by
  round-trip and the layout by rendering from the real arrays and constants, but
  no physical panel has been driven. The value most likely to need nudging by eye
  is `LABEL_YOFF`.
- **Antialiasing.** The glyph renderers are 1 bit per pixel, so every edge is
  hard. Two bits per pixel with a four entry palette computed once per character
  would cost about 24 KB across the four tables, against roughly 97 KB free. The
  whole surface is three near-identical inner loops in
  `ER-TFT024-3_4-Wire_SPI.c` (`disp_ch`, `disp_ch_16_24`, `disp_ch_12_24`).

- **A possible move to the GD32F103** for more memory and speed. Recorded as an
  option, with the figures it should be weighed against.

  GigaDevice's part is pin and largely register compatible with the STM32F103
  and clocks to 108 MHz against 72, so the speed argument is real. It would not
  do much for the screen, though: the drawing loops are bound by `Write_Data()`
  and the SPI clock rather than by the core.

  Flash is not the binding constraint. The scatter file claims `0x60000` at
  `0x08008000`, while the device holds `0x80000` from `0x08000000` — so on top
  of the ~97 KB free inside the region there is another 96 KB that is not
  allocated at all, out of the 480 KB the bootloader leaves.

  RAM is a different story, and it is the real argument for a bigger part. The
  board carries one IS61L5128L, which is 512K x 8 — 512 KB, byte wide. RW and ZI
  come to about 501 KB, so the external SRAM runs at roughly 92% full with
  around 44 KB spare. The largest consumers are `tsm.o` (~169 KB, the BACnet
  transaction state machine at `MAX_TSM_TRANSACTIONS 20`), `user_data.o`
  (~71 KB, the point database) and the 60 KB FreeRTOS heap. Internal SRAM cannot
  substitute: the heap alone would take 94% of the 64 KB on chip.

  The real obstacle is the bootloader. It is not in this repository, it is
  flashed below `0x08008000`, and changing silicon needs one that runs on the
  new part. Flash wait states and the clock tree in
  `arm/USER/system_stm32f10x.c` would need review too. T3000's reported MCU type
  (`T3_chip_type`) is display only and gates nothing, so it is not a concern.
