# T3 Programmable Controller — STM32

Work to update this firmware for the older STM32 based Temco products, mainly
around the display. This is a fork; everything here is developed and reviewed on
the fork rather than upstream.

## The idle screen

![Three pages of the idle screen](docs/idle-pages.png)

Rendered by `tools/screenshot.py` from the firmware's own font tables, icon
arrays and colour constants — not a mockup. **Nothing here has been verified on
a physical panel.**

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
`PAGE_MARK_MAX * IDLE_PAGE_ROWS` = 24 values instead of three.

- **RIGHT** steps to the next page and wraps. This key did nothing on the idle
  screen before.
- **LEFT** still picks a row within the page; **LEFT+RIGHT** still opens the menu.
- Pages past the last VAR that carries a label are not offered, so a panel that
  labels VAR1-VAR6 gets two pages rather than eight.
- A column of marks in the top right corner shows which page is up, one per
  page that exists. It is hidden when there is only one page.

### Dark palette

`TSTAT8_BACK_COLOR` and friends were retuned to a dark scheme. Because
`disp_icon()` ignores its colour arguments and blits literal RGB565, every icon
carried the old background baked into it, so all 25 icon arrays were recomposited
against the new background rather than colour-replaced — see `recolour_icons.py`
below.

### A real typeface, antialiased

The bitmap fonts were regenerated from Consolas Bold at the sizes the firmware
already used, keeping the cap height and cap top row so nothing moved on screen.

They also stopped being one bit per pixel. A glyph now stores a coverage level
per pixel and the renderer turns it into a colour through a palette interpolated
between `dcolor` and `bgcolor`, built once per character, so a pixel costs a
table lookup rather than a blend. The big top number carries 4 bits per pixel
because it is the text the eye goes to and it is only twelve glyphs; the three
smaller tables carry 2, where nearly all of the benefit is. Going from 1 bit to 2
removes the staircase; 2 to 4 only refines an edge that already reads as smooth.

### Full length row labels

`Str_variable_point.label` is nine bytes, but the rows only ever drew three of
them, so a point called `SETPOINT` read as `SET`. They now draw all eight.

`draw_tangle()` puts the value box at x=134, so the label owns x=0..133; eight
12-dot cells from x=2 end at x=97. Twelve is both the widest cell that fits eight
characters and about the narrowest that stays legible, so `char_12_24` is the same
face at the same cap height as `char_16_24`, condensed to 0.68.

Labels come from the VAR label field in T3000 — until you set them there, the rows
still read `SET`, `ROO`, `MOD`, just in the narrower face.

### Whole degrees and four character values

The displayed temperatures are only ever whole numbers, so the big top number
drops the decimal point: it rounds the tenths the sensor path carries, clamps to
three digits and right aligns with leading blanks. `THIRD_CH_POS` moved back to
`SECOND_CH_POS + 48` now that nothing sits between the digits.

The value boxes hold four characters (`VALUE_CHARS`) instead of five, which is
enough for `HEAT`, `AUTO`, `OPEN`, `72`. `draw_tangle()` takes a width rather
than assuming one, and the box is anchored to the right edge because the page
marks no longer live there. The 44 state words were shortened to fit, which also
retired the original `NAORM` typo.

### The link corner, the unit and the value boxes

The wifi bars moved from the top right to the top left, with the RS485 send and
receive arrows stacked underneath them, so everything about the link is in one
corner. Both sit left of `FIRST_CH_POS`, which is x=39, so the column is clear
of the big number for its whole height.

The unit moved from the foot of the number to its cap line. The digits are drawn
from the top of a 96 dot cell but their ink only starts nine rows in, so
`UNIT_YPOS` is `THERM_METER_POS + CHLIB_CAP_TOP` rather than `THERM_METER_POS`.

The value text drops two dots (`VALUE_YOFF`) so that its ink centres in the box:
`draw_tangle()` runs the box from y-3 to y+40 and the 15x30 face inks rows 4 to
28 of its cell, which sits two dots high without the offset.

A value shorter than four characters is now right justified, so the units column
of a number stays in one place as the number grows. That also fixed a repaint
bug: `sprintf` leaves a terminator part way along the buffer, which stopped
`disp_str()` before it repainted the rest of the box, so going from `AUTO` to
`72` left `TO` behind. `justify_value()` squares the full width off with spaces
before shifting.

### The corner strip

The big number moved left to x=30, which centres a two digit reading on the
screen and opens a 30 dot strip down the left edge. The strip carries the wifi
bars, the RS485 arrows under them, and a humidity readout under those. Three
characters will not fit across 30 dots, so the percent sign sits on its own line
under the value.

Moving the number also moved the minus sign. It used to be drawn on its own at
x=6, which is now underneath the RS485 arrows, so it rides in the cell left of
the first digit instead -- the 48x96 table already carries a `-` glyph. A three
digit negative has no cell left for the sign, so the magnitude is capped at two
digits when the value is negative.

Humidity comes from `TOP_RH_VAR` (VAR28), following the three icon VARs and
carrying whole percent in `value/1000`, the same convention. A value outside
0..99 draws blank rather than a wrong number.

### A clock instead of scrolling text

`display_clock()` draws `Sep 20 | 12:00 PM` in the 12-dot face — date and time on
one line, in the band the scrolling message used to occupy.

### Three state icons

![Every icon state](docs/state-icons.png)

The old row of five icons — day/night, home/away, heat/cool, a fan blade and a
fan speed bar — is replaced by three larger ones: what the circulation fans are
doing, what the room is calling for, and where the greenhouse sidewalls are.

Each is a 4 bit per pixel index map with its own sixteen entry palette, packed
the way the glyphs are: two pixels to a byte, low nibble first, straight across
the row boundaries. Indexed colour costs a third of what the literal RGB565
icons cost even at a larger cell: 1,620 bytes against the 4,950 a 55x45 literal
icon took. All ten states together come to 16 KB, against the 41 KB the twelve
arrays they replace came to — and each one still has sixteen colours to spend.

The palettes are built rather than quantised. Each icon names the two or three
hues it draws with, and the sixteen entries are the background plus an even ramp
to each hue. An antialiased edge is exactly a blend between the background and
one hue, so it lands on a ramp entry by construction instead of on whatever a
median cut happened to pick.

Which state each icon shows comes from a VAR, so a Control Basic program or T3000
drives them with no protocol change. These sit just past the paged rows, so a
page can never reach them. A digital VAR contributes its control bit, an
analogue one the whole part of its value, and anything out of range reads as
state 0 — an unconfigured panel shows fan off, idle, walls down.

| VAR | Shows | States |
| --- | --- | --- |
| VAR25 | circulation fan | 0 off, 1 on, 2 auto |
| VAR26 | call | 0 idle, 1 heat, 2 cool, 3 heat override, 4 cool override |
| VAR27 | sidewalls | 0 down, 1 up |

The override states draw the same flame or snowflake inside an amber frame, so
"cooling" and "cooling because someone said so" are one glance apart.

## Tools

`tools/` holds the scripts that generate the tables in
`arm/HARDWARE/LCD/ER-TFT024-3_4-Wire_SPI.c`. They are run from the repository
root and need Python 3 with Pillow.

| Script | What it does |
| --- | --- |
| `lcddata.py` | Shared reader for the arrays and `#define`s. Not run directly. |
| `fontgen.py` | Regenerates the four bitmap font tables from a TrueType face. |
| `icongen.py` | Generates the ten state icons and their palettes. |
| `recolour_icons.py` | Recomposites the legacy RGB565 icon bitmaps for a new screen background. |
| `screenshot.py` | Renders the idle screen to a PNG from the real arrays. |

```bash
python tools/fontgen.py                       # report the fit, change nothing
python tools/fontgen.py --write               # replace the arrays
python tools/icongen.py --sheet icons.png     # draw every state, change nothing
python tools/icongen.py --write
python tools/recolour_icons.py --bg "#0D1520" --write
python tools/screenshot.py out.png --labels "SETPOINT,ROOM TMP,MODE" --values "72,71,HEAT" --icons "fan_auto,mode_heat,wall_up"
```

Three things about these are worth knowing before changing them.

`fontgen.py` proves its bit packing by re-encoding the arrays **as they were
committed at 1 bit per pixel** and comparing byte for byte, before it generates
anything. Those arrays were consumed correctly by the shipped renderer, so this
is an independent check of the packing machinery rather than the packer agreeing
with itself. That check stands in for hardware testing and should be kept.

Nothing in that proves the multi-bit field order matches what the C renderers
extract. That is checked by rendering: `screenshot.py` unpacks with the same
convention, so a disagreement shows up as visibly mangled text or icons.

`lcddata.py` tokenises arrays on commas and resolves `#define`d names, raising on
anything it cannot resolve. This matters because the four corner pieces used by
`draw_tangle()` mix hex literals with colour names — a hex-only parser silently
returns 58 of `leftup`'s 64 pixels and corrupts the frame corners.

`recolour_icons.py` only applies to the literal RGB565 icons. The state icons
need no recolouring: `icongen.py` reads `TSTAT8_BACK_COLOR` and rebuilds the
ramps against it, so changing the background means re-running it.

`screenshot.py` models the drawing, not the erasing. It will not catch a clear
rectangle left at the wrong size or position, so any change to the cell geometry
has to update every `disp_null_icon()` that wipes it by hand.

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
| `ER_IROM1` flash | `0x4ae68` | `0x60000` | ~84 KB |
| `RW_RAM1` external SRAM | `0x75368` | `0x80000` | ~43 KB |
| `RW_IRAM1` internal SRAM | `0x5358` | `0x10000` | ~43 KB |

Flash went down about 25 KB across the icon change: the twelve literal RGB565
icons that nothing draws any more came to roughly 41 KB, against 16 KB for the
ten indexed ones that replaced them. They were deleted from the source rather
than left to the linker, which does not reliably strip unreferenced const data.

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
- **The icon VARs are a proposal, not a convention.** VAR25-27 were chosen
  because they sit just past the paged range; nothing else in the firmware or in
  T3000 knows about them yet, and a Control Basic program has to be written to
  drive them.

- **A possible move to the GD32F103** for more memory and speed. Recorded as an
  option, with the figures it should be weighed against.

  GigaDevice's part is pin and largely register compatible with the STM32F103
  and clocks to 108 MHz against 72, so the speed argument is real. It would not
  do much for the screen, though: the drawing loops are bound by `Write_Data()`
  and the SPI clock rather than by the core.

  Flash is not the binding constraint. The scatter file claims `0x60000` at
  `0x08008000`, while the device holds `0x80000` from `0x08000000` — so on top
  of the ~84 KB free inside the region there is another 96 KB that is not
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
