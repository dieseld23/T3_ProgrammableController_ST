# T3 Programmable Controller — STM32

Work to update this firmware for the older STM32 based Temco products. It started
with the display, then became a pass over memory safety, the UART receive paths,
the PID loop and the Control Basic interpreter. This is a fork; everything here is
developed and reviewed on the fork rather than upstream.

## Status

As of 2026-09-24, `main` carries all of the work below (PRs #1-#8), and no other
branches are open.

- **Build:** `Tstat10_wifi`, 0 errors, 474 warnings. `tools/checkmap.py` passes.
- **Hardware:** only `rev68VPF` has run on a device. Everything after it is built
  and reviewed but **not yet flashed**.

| image | adds | md5 | hardware |
| --- | --- | --- | --- |
| `rev68VPF` | the display work | `03889122…` | **boots and draws** |
| `rev68VPF2` | `RW_IRAM1` based at `0x20002000` (#5) | `21e7c5cc…` | untested |
| `rev68VPF3` | memory safety, UART races, PID derivative (#6, #7) | `43889d83…` | untested |
| `rev68VPF4` | cap on nested array indexes, three index bounds (#8) | `de1f5e18…` | untested — **this is `main`** |

All four are in `arm/OBJ/` as `Tstat10_arm_rev68VPF*.hex`. Each builds on the one
above, so a failure in `rev68VPF4` would not say which layer caused it. Flash them
in order, with `rev68VPF` as the fallback; [To do](#to-do) lists what to check on
each. The application is linked above the bootloader, so a bad image leaves the
device recoverable through the bootloader's ISP window at power-on.

The most urgent open item is that **network write commands can overflow the
point tables**; see [To do](#to-do).

## The idle screen

![Three pages of the idle screen](docs/idle-pages.png)

Rendered by `tools/screenshot.py` from the firmware's own font tables, icon
arrays and colour constants — not a mockup.

`rev68VPF` boots on hardware and draws the screen. That confirms it runs and
renders. It does not confirm the layout matches these pictures pixel for pixel.
The state icons and the corner humidity have not been driven yet, because nothing
writes VAR25-28.

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

## Display changes

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

The unit moved from the foot of the number to its cap line. That needs two
constants, not one, because both faces ink below the top of their cell by
different amounts. `UNIT_YPOS` is the digits' cap line, nine rows into their 96
dot cell; the degree ring is a raw 14x14 bitmap with no padding, so it is drawn
straight at that line, while the letter beside it is a 24x36 cell that inks four
rows in and so starts at `UNIT_TEXT_YPOS`, four rows above. All three ink tops
then land on the same row.

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
bars, the RS485 arrows under them, and a humidity readout under those, labelled
`RH` with the value and its percent sign together on one line.

Three characters do not fit across 30 dots in the 12 dot face: the third cell
would start at x=36 and the first digit cell repaints from x=30 on every refresh,
erasing it. `char_10_24` is the same face at 10 dots, where three cells land
exactly on 0..29.

Moving the number also moved the minus sign. It used to be drawn on its own at
x=6, which is now underneath the RS485 arrows, so it rides in the cell left of
the first digit instead -- the 48x96 table already carries a `-` glyph. A three
digit negative has no cell left for the sign, so the magnitude is capped at two
digits when the value is negative.

Humidity comes from `TOP_RH_VAR` (VAR28), following the three icon VARs and
carrying whole percent in `value/1000`, the same convention. A value outside
0..99 draws blank rather than a wrong number.

Two things in `fontgen.py` came out of fitting that face. A table can now name
the characters that set its condense factor, because squeezing a 10 dot cell to
hold `@` and `W` clipped the ink off `%`, which is the one glyph the readout
exists to draw. And the ink is centred inside the columns that are kept, 1 to
`ink_w`, rather than inside the whole cell — column 0 is always cleared, so the
two only agree when `w - ink_w` is even, and at 10 dots it hung half a column off
each end. That change is a no-op for the four older tables, which was checked by
regenerating them against the committed arrays.

### Three things the layout work uncovered

None of these were introduced by the new layout; all three were made reachable
or visible by it.

**The value format overran the box.** `"%.1f"` needs five cells for `-12.5` and
for `123.4` alike, and the old code let `sprintf` write them and then cut at
four, leaving `-12.` and `123.` -- a dangling point that reads as a broken
number rather than a rounded one. `format_value()` picks the number of decimals
from what will fit, measures the result because rounding can carry into a new
digit (`9.996` at two places is `10.00`), and drops places until it does.

**A two character unit sat on the hundreds digit.** `"%R"`, `"pp"`, `"kP"` and
`"Pa"` were drawn at `UNIT_POS - 23`, which is x=166 -- inside the third digit
cell. They start at `UNIT2_POS` now, where the digits end, and still finish six
dots clear of the page marks. The whole unit band is wiped before each draw, so
switching from a two character unit to a one character one cannot leave a tail.

**`display_dec()` cut a notch out of the third digit.** It painted an 8x8
rectangle at x=130 to hide the decimal point, which was between the digits in
the old geometry. Moving the number left put that rectangle inside the third
digit cell, where it erased rows 85..87 of the glyph. There is no decimal point
on the big number any more, so the function is gone rather than repositioned.

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
| `fontgen.py` | Regenerates the five bitmap font tables from a TrueType face. |
| `icongen.py` | Generates the ten state icons and their palettes. |
| `recolour_icons.py` | Recomposites the legacy RGB565 icon bitmaps for a new screen background. |
| `screenshot.py` | Renders the idle screen to a PNG from the real arrays. |
| `checkmap.py` | Fails a link that puts data where the stack lives. Run before flashing. |

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

What it does model, it has to model exactly, because it is the only check this
work has. It carries mirrors of `format_value()`, `justify_value()` and the
tenths rounding rather than Python equivalents of them: `str.rjust()` is not
`justify_value()`, because it keeps the trailing spaces a memcpy'd literal like
`"ON  "` carries, and `round()` is not `(value + 5) / 10`, because it rounds half
to even and disagrees on every `.5`. `--unit` takes the firmware's own branch
names so that `RH` draws `%R` rather than `RH`, and `--values` sends anything
numeric through `format_value` instead of accepting a pre-formatted string. A
short icon array and a page count over `PAGE_MARK_MAX` are errors rather than
silently clipped, since both corrupt the panel write on hardware.

## Building

Keil MDK, ARMCC V5.06. Headless:

```bash
"C:/Keil_v5/UV4/UV4.exe" -r arm/USER/Tstat10_wifi.uvprojx -o build.log
```

Use `-r` (full rebuild) rather than `-b` when comparing warning counts, or you are
comparing an incremental build against a full one. Do not pass `-j1`; UV4 does not
accept it and blocks on a dialog instead of failing.

Notes on the toolchain:

- The prebuilt BACnet library links from `bacnet/bacnet_ARM_revXX_T10.lib`
  inside the repository. Temco's projects named it as
  `..\..\..\BACLIB\bacnet_ARM_revXX_T10.lib`, a folder *next to* the repository,
  so a fresh clone could not link until someone created that folder by hand.
  `Tstat10_wifi.uvprojx` now points at the in-repo copy (same md5; the rebuilt hex
  was byte-identical). `mini_arm.uvprojx` and `CM5_arm.uvprojx` still point at
  `BACLIB`, and CM5's `bacnet_ARM_rev10_mini.lib` is not in the repository at all.
- UV4 rewrites `Tstat10_wifi.uvprojx` on build to match the locally installed
  compiler and device pack. That drift should not be committed.
- The scatter file is **generated** from the target dialog, not hand written, so
  the memory map is edited through the `OCR_RVCT` entries in the project rather
  than in `arm/OBJ/Tstat10_arm_revxx.sct`. The `<ScatterFile>` entry naming
  `..\OBJ	est.sct` is a stale leftover; no such file exists and nothing reads it.
- **The stack is invisible to the linker, and the memory map has to work around
  it.** `arm/CORE/startup_stm32f10x_hd.s` builds with `DATA_IN_ExtSRAM EQU 1`
  and writes an absolute stack pointer rather than using the one the linker
  placed:

  ```asm
  __initial_sp EQU 0X20000000 + Stack_Size    ; 0x20002000
  ```

  Nothing then references `Stack_Mem`, so the linker discards the section --
  `Removing startup_stm32f10x_hd.o(STACK), (8192 bytes)` in the map -- and stops
  believing anything occupies internal SRAM. The stack is real: 8 KB growing
  down from `0x20002000`. The linker just cannot see it.

  This used to be hidden by `RW_RAM1` being declared as `0x800000` against a
  512 KB part, which swallowed every byte of RW and ZI and left internal SRAM
  empty by accident. Declaring it at its real size ended the accident: the
  linker filled `0x20000000`-`0x20005358` straight through the stack, `__main`
  zeroed live stack frames during its ZI sweep, and the first device flashed
  with that build never left its bootloader.

  The fix is to reserve the stack rather than to hide it. `RW_IRAM1` is based at
  `0x20002000` with size `0xE000`, so the 8 KB below it belongs to the stack
  alone, and `RW_RAM1` carries its real `0x80000` -- which puts the overflow
  check back, on a region that runs about 92% full.

  **Run `tools/checkmap.py` before handing anyone a hex.** It fails the build
  configuration that would not boot, by both the region base and the placement
  addresses, and warns when a region passes 95% full.
- The `Tstat10_wifi` build outputs are committed with the source that produced
  them: `arm/OBJ/Tstat10_arm_revxx.hex`, `.axf`, `.map`, the call graph `.htm` and
  `.build_log.htm`. So every commit carries its own map and stack analysis, and a
  change that moves memory or stack shows up in the diff. Rebuild and commit them
  together with any source change.

  The hex covers `0x08008000`-`0x08054bdb`, with the entry point at `0x08008131`.
  That is the application only. The bootloader lives below `0x08008000`, is not
  in this repository and is not touched by flashing this file, so a bad
  application still leaves the device recoverable through the ISP window. Images
  meant for a device are also copied to `Tstat10_arm_rev68VPF*.hex`; see
  [Status](#status).

Current state of the `Tstat10_wifi` target: **0 errors, 474 warnings**.

| Region | Used | Of | Free |
| --- | --- | --- | --- |
| `ER_IROM1` flash | `0x4c8f8` (313,592) | `0x60000` | ~78 KB |
| `RW_RAM1` external SRAM | `0x763d8` (484,312) | `0x80000` | ~39 KB (92.4% full) |
| `RW_IRAM1` internal SRAM | `0x43dc` (17,372) | `0xe000` | ~39 KB |

`RW_IRAM1`'s `Of` column is `0xe000` rather than the `0x10000` the chip carries,
because the 8 KB below `0x20002000` is reserved for the stack the linker cannot
see. See the note above.

Flash went down about 25 KB across the icon change: the twelve literal RGB565
icons that nothing draws any more came to roughly 41 KB, against 16 KB for the
ten indexed ones that replaced them. They were deleted from the source rather
than left to the linker, which does not reliably strip unreferenced const data.

Internal SRAM is in use again, and that part is a genuine win: about 17 KB now
lands there, on a region that had been sitting entirely empty because `.ANY`
swept everything into the oversized external one. Internal SRAM is single cycle
where the FSMC part is not.

What matters is *where* it lands. An earlier revision of this file said the same
thing about simply capping `RW_RAM1`, and that advice was wrong in a way that
cost a device: capping it alone let the linker fill from `0x20000000` upward,
straight through the invisible stack, and the board never left its bootloader.
The speed was never the problem; the placement was. Basing `RW_IRAM1` at
`0x20002000` keeps the benefit and puts the data above the stack instead of on
top of it.

## Things that are not what they look like

Four traps in this tree have each cost real time, and none of them announce
themselves.

**A sizing constant in a header may not reach the binary.** `tsm.o` and
`address.o` come from `bacnet/bacnet_ARM_revXX_T10.lib`, a prebuilt 29 MB static
library whose sources are not in this repository. Editing `MAX_TSM_TRANSACTIONS`
in `bacnet/bacnet.h` does not change its RAM, and somebody has already tried.
Before quoting any `bacnet.h` constant as though it describes the image, check
the actual symbol size in `arm/OBJ/Tstat10_arm_revxx.map`.

**Task stack sizes are in words, not bytes.** `portSTACK_TYPE` is `uint32_t`, so
`xTaskCreate(..., 1000, ...)` asks for 4,000 bytes. `sTaskCreate` is just
`xTaskCreate` (`common/product.h:170`). Reading it as bytes is how two tasks ended
up short: `WifiSTACK_SIZE` was 1000 with 2048 commented out beside it, and
`MainSerialSTACK_SIZE` was 1024. Both are 2048 now; see
[Memory safety](#memory-safety).

**The Keil call graph holds the stack analysis, and it is checked in.**
`arm/OBJ/Tstat10_arm_revxx.htm` carries `Maximum Stack Usage` plus a per-root
`[Stack]Max Depth` for every entry point, so each task can be measured against
its own stack:

```bash
sed 's/<[^>]*>//g' arm/OBJ/Tstat10_arm_revxx.htm > cg.txt
grep -iE 'Maximum Stack Usage|Mutually Recursive' cg.txt
grep -oE '\[Stack\]Max Depth = [0-9]+Call Chain = [A-Za-z_][A-Za-z_0-9]*' cg.txt
```

Read the `+ Unknown(Cycles, Untraceable Function Pointers)` on that figure
seriously. The Control Basic interpreter recurses once per nested array index,
so its depth is set by the program a user downloads. That is now capped — see
[Nested array indexes](#nested-array-indexes) — and the call graph's only
interpreter cycle is `eval_index ⇒ veval_exp`. If another interpreter cycle
appears there, it is a recursion path that goes around the cap.

**Half the tree is not compiled.** The project compiles 91 source files (89 C,
2 assembly) and links two prebuilt libraries. The whole `asix/` directory,
`arm/uIP*`, `common/comm.c` and `arm/USER/TestTool_MiniT/` are not among them, and `asix/` in particular holds near-copies of `modbus.c`,
`flash_user.c` and others that are easy to edit by mistake. The compiled set is
whatever `<FilePath>` entries appear in `arm/USER/Tstat10_wifi.uvprojx`; resolve
them relative to `arm/USER/`.

## Memory safety

FreeRTOS now reports rather than absorbs two classes of failure.
`configCHECK_FOR_STACK_OVERFLOW` is `2` and `configUSE_MALLOC_FAILED_HOOK` is
`1`; both hooks are in `common/main.c` and both `SoftReset()`. They record
nothing, because RAM does not survive the reset: to see which task overran, set a
breakpoint in `vApplicationStackOverflowHook` and read `pcTaskName`. Neither
check was enabled before, so a task that ran off its stack
corrupted whatever the heap had placed below it and carried on, and a task that
could not get a stack simply never ran — every `xTaskCreate` here discards its
return value.

**This converts silent faults into visible restarts.** A board that starts
cycling after this change is reporting a fault it always had.

Twelve tasks are linked into this build, and their stacks take 51,232 bytes of
the 60 KB heap (83.4%). TCBs, queues and semaphores come out of the other
10,208 — roughly 2 KB of it — which leaves about 8 KB. `heap_2` cannot coalesce
freed blocks, which is survivable only because every task here is created once
at startup and never deleted. The heap is only drawn on at boot, so a stack that
does not fit shows up as a reset loop on the first power-up rather than later.

| task | stack (bytes) | deepest traced call chain | margin |
| --- | ---: | ---: | ---: |
| `main_dealwithData` | 8,192 | 4,160 | 2.0× |
| `WIFI_task` | 8,192 | 4,344 | 1.9× |
| `Master_Node_task` | 8,192 | 4,312 | 1.9× |
| `Bacnet_Control` | 8,192 | 3,104 + 208 per nested index, 4,768 at the cap | 1.7× at the cap |
| `ScanTask` | 8,192 | 476 | 17× |
| `Monitor_Task_task` | 4,000 | 388 | 10× |
| `refresh_Input_Task` | 2,000 | 312 | 6.4× |
| `MenuTask` | 1,536 | 144 | 11× |
| `Common_task` | 1,024 | 136 | 7.5× |
| `refresh_Output_Task` | 800 | 80 | 10× |
| idle | 512 | 88 | 5.8× |
| `Key_Process` | 400 | 128 | 3.1× |

Depths are the `[Stack]Max Depth` figures from Keil's call graph
(`arm/OBJ/Tstat10_arm_revxx.htm`), which cannot see through function pointers
or recursion, so they are floors. Each interrupt taken while a task runs also
pushes 32 bytes onto that task's stack. Find the tasks from the callers of
`xTaskGenericCreate` in the same file rather than by grepping the source: an
earlier count from the source missed five of them, and one of the five,
`main_dealwithData`, had 4,096 bytes for a 4,160-byte chain. It now has 2048
words.

Unbounded copies reachable from the network were closed at the same time: three
in `decode.c`, where a one-byte length from program bytecode was memcpy'd into a
94-byte buffer; two in `ptransfer.c`, where a 16-bit `total_length` off the wire
sized copies into 300-byte buffers; and a `strcpy` in `alarm.c` that put that
94-byte message into a 59-byte field. Two format buffers that could not hold
their own output were resized (`alarm.c`, `scan.c`); a `memcpy(..., 0, ...)`
reading from address 0 became the `memset` it was meant to be; `GET_PANEL_INFO`
stopped dereferencing a pointer one line before assigning it, and now assembles
the device instance with its two halves the right way round; three places
stopped indexing `remote_points_list[128]` with the `0xff` "idle" sentinel, two
reads in `ptransfer.c` and a block of writes in `main.c` that landed ~3.5 KB past
the end of the table after every panel scan; and an
`if(count >= 0)` on an unsigned count stopped guarding a division by it. The
sound level still reads the table's floor of 50 in a silent room, as it always
has; it just no longer gets there by dividing by zero.

That pass did not reach every copy the network drives. The private-transfer
writes into the other point tables are still unbounded; see [To do](#to-do).

The `decode.c` clamp covers the copy into `message[]` only. `prog += len` still
steps by the original length, because that byte is part of the instruction
stream whether or not it was sane, and `ALARM` and `DALARM` write to the program
just past the message — a state byte and a 4-byte delay counter. Both now check
that step against the end of the program's 2,000-byte row and abandon the scan
with `-1`, the decoder's existing answer to malformed code, rather than write
past it. `ALARM` also stopped writing through a `NULL` when its comparison
operator is missing.

Alarm messages are stored truncated to 58 characters, so the lookups that find
an existing alarm (`checkforalarm`, `dalarmrestore`) now compare on the same 58.
Against the full text a long message never matched its own stored copy: it was
filed again on every scan until the table filled, and could never be restored.

One of those fixes was wrong the first time, in a way worth repeating. The
`ptransfer.c` guard was put inside `Get_Pkt_Bac_to_Modbus`, which is called
*after* the caller has already filled `bacnet_to_modbus` — so it ran after the
overflow it was meant to stop. A bounds check is only a bounds check if it
precedes the write it guards.

### A critical section does not mask the UART here

`uart0/1/2_rece_count` are written by the USART receive ISRs and read by tasks,
and are now `volatile`. That is the smaller half of the problem. The receive ISR
appends to its buffer and, on a complete frame, sets the count back to zero and
starts the next frame at index 0 — so a `memcpy` out of that buffer which
straddles the reset returns the tail of one frame spliced onto the head of the
next. `volatile` does nothing about interleaving.

Neither does `taskENTER_CRITICAL`. USART1, USART2 and USART3 are all installed at
**NVIC pre-emption priority 0** (`usart.c:129`, `223`, `321`), while
`configMAX_SYSCALL_INTERRUPT_PRIORITY` is 191. A critical section only raises
`BASEPRI` to that threshold, which leaves a priority 0 handler free to run
straight through it. Anything that must be atomic against a receive ISR has to
mask that interrupt by name, which `wait_subnet_response` and
`set_subnet_parameters` now do for all three ports through `uart_rx_take` and
`uart_rx_arm` in `common/modbus.c`. A byte arriving meanwhile waits in the data
register and is taken on unmask.

Mask it **at the NVIC** (`NVIC_DisableIRQ`/`NVIC_EnableIRQ`), not by clearing
`RXNEIE` with `USART_ITConfig`. The first version of this fix did the latter,
and `USART_ITConfig` is a read-modify-write of `CR1`. The USART1 ISR clears
`TXEIE` in that same register when it finishes sending a frame, so if it lands
between the task's read and write, the task writes the stale `TXEIE` back, the
ISR fires again and puts a stray byte on the RS-485 bus just as the slave starts
to answer. `ICER` and `ISER` are write-one-to-clear and write-one-to-set, so
there is nothing to lose.

The same priority arrangement means those ISRs **must not call any FreeRTOS
`…FromISR` function**. They currently do not. Keep it that way, or lower the
priority to at or below the threshold first.

### The PID derivative

`pid_controller` zeroed `erp` between the proportional and integral terms, so the
derivative block — which wants `erp - old_err * 100 / prop` — got
`0 - old_err * 100 / prop`: the negated previous error rather than the change.
The term tracked the level of the error instead of its movement, which meant a
constant push at a steady offset and the wrong sign on a rising error. It also
divided by `prop` without the guard the proportional term applies to itself.

It now works from the change in the **measurement**, not the change in error.
With a fixed setpoint the two are the same, but a setpoint step (an
occupied/unoccupied schedule switch, say) moves the error in one sample and
would kick the output for a whole 10 s period; the measurement does not jump.
The loop's history is also re-seeded on the first sample after power-up and
after any spell in manual, so returning to auto does not see the whole drift
while it was off as a single step. The integral's trapezoid uses the same
re-seeded history.

### Nested array indexes

The Control Basic interpreter in `bacnet/private/decode.c` evaluates postfix
bytecode on a bounded value stack, so ordinary expressions do not recurse
however many brackets they have. Array indexes do: reading `AY1[expr]` evaluates
`expr` in a fresh `veval_exp`, and an index can itself contain an array read.
Nothing limited how deep that goes. T3000's compiler accepts any depth, and
`WRITEPROGRAMCODE_T3000` stores whatever bytes it is sent. A 2,000-byte program
row has room for well over a hundred levels.

Each level costs 208 bytes of the `Bacnet_Control` task's 8 KB stack, which
already runs 3,104 deep without any, so somewhere past 25 levels it overflows.
With the overflow hook that means a reset, the program runs again once the
board is back, and after five such resets `bac_control.c` switches every program
off — a unit that has quietly stopped running its programs.

Nesting is now capped at `MAX_INDEX_DEPTH` (8), about 1.7 KB. Both recursion
edges go through `eval_index`, which counts. At the cap it `longjmp`s back to
`exec_program`, which abandons the scan and raises
`PRG n error : indexes nest too deep`. The statements before the one that nested
too deep have already run that scan, as they do when the decoder meets malformed
code, and the program stays on.

It jumps rather than returning because a normal return from the middle of an
expression leaves `prog` pointing into the middle of it. The statement that
asked for the value would then act on it: write 0 to an output, or raise an
alarm with whatever text `prog` now points at. The frames jumped over are
interpreter frames holding no locks. The one piece of state to undo is the
`Alarm` statement's habit of overwriting its comparison operator with `0xFF`
while it evaluates; the byte is recorded and put back.

The same pass bounded three indexes that came straight from the program:
- `PIDPROP`, `PIDDERIV` and `PIDINT` wrote `controllers[i - 1]` for any
  controller number the program computed.
- `get_ay_elem` read `arrays_address[]` with an unchecked byte out of the
  bytecode and then dereferenced what it found.
- `ALARM-AT` appended its panel list to the 5-byte `alarm_panel[]` on every scan.
  Nothing reset the count (the reset at the top of the scan is commented out), so
  a program using it wrote past the array within a few scans. Once the signed
  8-bit count wrapped negative, it wrote below the array too. Each `ALARM-AT`
  now replaces the list, keeping the five panels `putmessage` reads.

## To do

Ordered by risk to a unit in the field. Checked against `main` on 2026-09-24.

### Code

1. **Network write commands can overflow the point tables.** This is the most
   urgent item. `bacnet/private/ptransfer.c` handles the T3000 private-transfer
   writes, which need no authentication. `WRITEINPUT`, `WRITEOUTPUT` and
   `WRITEVARIABLE` check both ends of the range and cap the copy. Every other table
   write does neither: weekly and annual routines, programs, program code,
   controllers, monitors, groups, remote points, alarms, units and passwords.
   - It checks only `point_end_instance <= MAX_…`. The `<=` admits one entry past
     the end of the table, and `point_start_instance` is never checked at all.
   - The generic copy then writes `total_length - 7` bytes. The only check is that
     this equals `entitysize × count`, and `entitysize` is a 9-bit field from the
     same packet. So a request can put up to about 500 bytes past the end of
     `programs[]`, `controllers[]` and the rest.
   - `WRITEPROGRAMCODE_T3000` also offsets into the program row by a 7-bit
     `packet_index`, which puts the copy up to 50 KB past the row. The check on
     `packet_index` at ~`ptransfer.c:2092` runs after the copy.

   The fix is the `WRITEINPUT` pattern applied to every table: `start < MAX`,
   `end < MAX`, `start <= end`, and cap the copy at `(MAX - start) × sizeof`.
   Plus a bound on `packet_index`. Reads use the same headers and want the same
   check.
2. **Program code and four settings pages are still lost on a power cut.** The
   point tables are saved through a shadow page and a commit record
   (`flash_replace_page`, `flash_finish_pending_commit` in
   `arm/FLASH/flash_user.c`), so a cut mid-save is recovered at boot. Two kinds of
   save bypass that:
   - `Flash_Store_Code` erases each program's page and rewrites it in place.
   - The `FLASH_OTHER_ADDR…ADDR4` pages are erased and then written directly, in
     several separate writes for some of them. These hold multi-state values, the
     device name and SNTP settings, output priority arrays, and email settings.

   Routing both through `flash_replace_page` would close it.
3. **Downloaded program bytecode is not checked when it arrives.** The
   interpreter now bounds what it can at run time: message lengths, jump targets
   within the row, nesting depth, table indexes. A malformed program is still
   stored and saved to flash as sent, though. Checking the row when
   `WRITEPROGRAMCODE_T3000` receives it would turn a program that fails every
   scan into a rejected download. The larger half of the job is a parser that
   agrees exactly with `veval_exp` on operand sizes.
4. **Eight `#186-D` warnings deserve a read as a group.** Each is an unsigned
   value compared with zero, and one of them was already hiding a division by
   zero:
   - `ptransfer.c:705`, `ptransfer.c:2288`
   - `user_data.c:1485`
   - `modbus.c:4488`, `modbus.c:4608`
   - `tstat_wifi.c:380`, `tstat_wifi.c:401`
   - `menuSet.c:83`

   A useless lower bound on an index that came off the network is how an overflow
   hides.
5. **Alarms are never forwarded to other panels.** `sendalarm` and its callers in
   `bacnet/private/alarm.c` are commented out. The `where1…where5` destinations
   that `ALARM-AT` sets are stored with each alarm and shown in T3000, but go
   nowhere. Relatedly, `alarm_at_all` is set by `ALARM-AT ALL` and never cleared.
   The reset at the top of the scan is commented out, so it stays set until
   reboot. That is harmless while forwarding is off, and needs deciding before
   forwarding is turned back on.
6. **The `mini_arm` and `CM5_arm` targets have not been built since this work
   began.** They compile the same `decode.c`, `ptransfer.c`, `alarm.c`,
   `modbus.c` and `main.c`. Nothing here has checked that they still build, or
   that the memory-map and stack reasoning holds for them. Both still link the
   BACnet library from `..\BACLIB`, and CM5's library is missing (see
   [Building](#building)).

### On the bench

Flash the images in [Status](#status) in order. Each row assumes the one above
it worked.

| image | check |
| --- | --- |
| `rev68VPF2` | Boots and draws, the same as `rev68VPF`. This proves the memory map alone. |
| `rev68VPF3` | Stays up past a minute. A stack or heap shortfall now shows as a reset loop in the first seconds. RS-485 master polling returns sane values on each port in use, which covers the UART change. Loops with `rate > 0` are watched through a setpoint change (see below). |
| `rev68VPF4` | A program that reads `AY1[AY1[AY1[…]]]` nine deep raises `PRG n error : indexes nest too deep` instead of resetting the board. A program using `ALARM-AT` with panel numbers runs for several minutes without trouble. |

Also, on whichever image boots:

- Check the layout against the renders by eye. `LABEL_YOFF` is the value most
  likely to need nudging.
- Step the pages with RIGHT.
- Drive VAR25-28 to see the state icons and the humidity readout change.

### In the field

- **Re-tune PID loops that use a derivative.** The derivative term used the
  negated previous error; it now uses the change in the measurement (see
  [The PID derivative](#the-pid-derivative)). Any controller with `rate > 0` was
  tuned around the old behaviour. Every loop also sees one smaller change: the
  first integral step after boot or after leaving manual uses the current error
  twice rather than a stale one.
- **The icon VARs are a proposal, not a convention.** VAR25-27 were chosen
  because they sit just past the paged range. Nothing else in the firmware or in
  T3000 knows about them yet, and a Control Basic program has to be written to
  drive them.

### Memory

- **Recover about 150 KB of external SRAM from the BACnet library.** `TSM_List`
  is `255 × 662 = 168,810` bytes, while `bacnet/bacnet.h:75` asks for 20
  transactions:

  ```c
  #define MAX_TSM_TRANSACTIONS  20//255 //????????????? changed by chelsea
  ```

  The edit changed nothing, because `tsm.o` comes from the prebuilt library, and
  the map records its source as `bacnet\src\tsm.c`, which is not in this
  repository. Recovering it means compiling an upstream `tsm.c` that matches
  `MAX_APDU 600` into the project, so the linker prefers it over the library's
  copy. Eleven exported symbols have to match. Recompiled from the repo headers,
  an entry works out at 674-676 bytes rather than 662, so budget roughly 150 KB.
  That would take the external SRAM from 92% full to about 63%.

  `Address_Cache` in the same library is `255 × 31 = 7,905` bytes, and there
  `MAX_ADDRESS_CACHE 255` at `bacnet.h:83` does agree with the binary. It is
  merely large.

### Elsewhere

- **The ESP32 port still has bugs that are fixed here.** In
  `T3-programmable-controller-on-ESP32/temco_bacnet/private/`, as of 2026-09-24:
  - `decode.c` has the unbounded array-index recursion.
  - `PIDPROP`, `PIDDERIV` and `PIDINT` use an unchecked controller index.
  - `ALARM-AT` has the appending overflow.
  - `alarm.c` still `strcpy`s the program's message into its 59-byte field.

## Options considered

### Moving to the GD32F103

Recorded as an option, with the figures it should be weighed against. Not being
pursued.

GigaDevice's part is pin and largely register compatible with the STM32F103 and
clocks to 108 MHz against 72, so the speed argument is real. It would not do much
for the screen, though: the drawing loops are bound by `Write_Data()` and the SPI
clock rather than by the core.

Flash is not the binding constraint. The scatter file claims `0x60000` at
`0x08008000`, while the device holds `0x80000` from `0x08000000`. So on top of the
~78 KB free inside the region, there is another 96 KB that is not allocated at
all, out of the 480 KB the bootloader leaves.

RAM is a different story, and it is the real argument for a bigger part. The
board carries one IS61L5128L, which is 512K × 8: 512 KB, byte wide. RW and ZI
come to about 501 KB. Of that, 484 KB is in the external SRAM, which runs 92.4%
full with about 39 KB spare, and 17 KB is in internal SRAM. The largest consumers
are:
- `tsm.o`: 169 KB, the BACnet transaction state machine.
- `user_data.o`: about 71 KB, the point database.
- the FreeRTOS heap: 60 KB.

Internal SRAM cannot substitute: the heap alone would take 94% of the 64 KB on
chip. And before reaching for a bigger part, a third of that RAM can be recovered
without one (see [Memory](#memory)).

The real obstacle is the bootloader. It is not in this repository, it is flashed
below `0x08008000`, and changing silicon needs one that runs on the new part.
Flash wait states and the clock tree in `arm/USER/system_stm32f10x.c` would need
review too. T3000's reported MCU type (`T3_chip_type`) is display only and gates
nothing, so it is not a concern.
