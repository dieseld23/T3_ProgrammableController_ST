# T3 Programmable Controller — STM32

Work to update this firmware for the older STM32 based Temco products, mainly
around the display. This is a fork; everything here is developed and reviewed on
the fork rather than upstream.

## The idle screen

![Three pages of the idle screen](docs/idle-pages.png)

Rendered by `tools/screenshot.py` from the firmware's own font tables, icon
arrays and colour constants — not a mockup.

`rev68VPF` boots on hardware and draws the screen. What that confirms is that it
runs and renders; it does not confirm the layout matches these pictures pixel for
pixel, and the state icons and the corner humidity have not been driven yet
because nothing writes VAR25-28.

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

Two notes on the toolchain:

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
- Building dirties the checked-in artifacts under `arm/OBJ/`. Those are not part
  of any commit on this branch, with one exception:
  `arm/OBJ/Tstat10_arm_revxx.hex` is committed so the branch carries something
  flashable. It covers `0x08008000`-`0x08054a30` with the entry point at
  `0x08008131` — the application only. The bootloader lives below `0x08008000`,
  is not in this repository and is not touched by flashing this file, so a bad
  application still leaves the device recoverable through the ISP window.
  Re-run the build before trusting it after any source change.

Current state of the `Tstat10_wifi` target: **0 errors, 482 warnings**.

| Region | Used | Of | Free |
| --- | --- | --- | --- |
| `ER_IROM1` flash | `0x4c6f0` | `0x60000` | ~78 KB |
| `RW_RAM1` external SRAM | `0x76360` | `0x80000` | ~39 KB |
| `RW_IRAM1` internal SRAM | `0x4370` | `0xe000` | ~39 KB |

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
`xTaskCreate` (`common/product.h:170`). `WifiSTACK_SIZE` was set to 1000 with
2048 commented out beside it, which left `WIFI_task` 344 bytes short of the
deepest chain the linker can trace through it.

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
seriously. `operand`/`veval_exp` and `get_ay_elem`/`veval_exp` in the Control
Basic interpreter are mutually recursive, with depth set by how deeply a
user-authored program nests its expressions. Nothing bounds it.

**Half the tree is not compiled.** The project builds 93 files. The whole `asix/`
directory, `arm/uIP*`, `common/comm.c` and `arm/USER/TestTool_MiniT/` are not
among them, and `asix/` in particular holds near-copies of `modbus.c`,
`flash_user.c` and others that are easy to edit by mistake. The compiled set is
whatever `<FilePath>` entries appear in `arm/USER/Tstat10_wifi.uvprojx`; resolve
them relative to `arm/USER/`.

## Memory safety

FreeRTOS now reports rather than absorbs two classes of failure.
`configCHECK_FOR_STACK_OVERFLOW` is `2` and `configUSE_MALLOC_FAILED_HOOK` is
`1`; both hooks are in `common/main.c` and both record to a global and then
`SoftReset()`. Neither was enabled before, so a task that ran off its stack
corrupted whatever the heap had placed below it and carried on, and a task that
could not get a stack simply never ran — every `xTaskCreate` here discards its
return value.

**This converts silent faults into visible restarts.** A board that starts
cycling after this change is reporting a fault it always had.

Task stacks currently take 39,616 bytes of the 60 KB heap (64.5%), leaving about
21 KB for TCBs, queues and semaphores. `heap_2` cannot coalesce freed blocks,
which is survivable only because every task here is created once at startup and
never deleted.

Unbounded copies reachable from the network were closed at the same time: three
in `decode.c`, where a one-byte length from program bytecode was memcpy'd into a
94-byte buffer; two in `ptransfer.c`, where a 16-bit `total_length` off the wire
sized copies into 300-byte buffers; and a `strcpy` in `alarm.c` that put that
94-byte message into a 59-byte field. Two format buffers that could not hold
their own output were resized (`alarm.c`, `scan.c`); a `memcpy(..., 0, ...)`
reading from address 0 became the `memset` it was meant to be; `GET_PANEL_INFO`
stopped dereferencing a pointer one line before assigning it; two call sites
stopped indexing `remote_points_list[128]` with the `0xff` "idle" sentinel; and
an `if(count >= 0)` on an unsigned count stopped guarding a division by it.

The `decode.c` clamp deliberately leaves `prog += len` using the original
length: that byte is part of the instruction stream whether or not it was sane,
and clamping the step too would desynchronise the decoder on exactly the input
the fix exists to survive.

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
`set_subnet_parameters` now do — `USART_IT_RXNE` only, so transmit interrupts
sharing the vector keep working, and a byte arriving meanwhile still sets RXNE
and is taken on unmask.

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

## Not done yet

- **Only one image is confirmed on hardware, and it is not the newest.**

  | image | what it adds | hardware |
  | --- | --- | --- |
  | `rev68VPF` | the display work | **boots and draws** |
  | `rev68VPF2` | `RW_IRAM1` based at `0x20002000` | untested |
  | `rev68VPF3` | the memory-safety fixes | untested |

  Each builds on the one above, so `rev68VPF3` carries two unproven changes at
  once and a failure would not say which. Prove `rev68VPF2` first. `rev68VPF` is
  the fallback.

  Nobody has yet checked the layout against these renders by eye, stepped the
  pages with the RIGHT key, or driven VAR25-28 to see the state icons and the
  humidity readout change. The value most likely to need nudging by eye is
  `LABEL_YOFF`.

- **`main` cannot currently produce a bootable image.** It still carries
  `RW_IRAM1 0x20000000 / 0x10000`, the map described under Building as the one
  that would not boot. Building from `main` and flashing the result will put a
  device in its bootloader. Fixed by the memory-map branch, not yet merged.

- **Tuned PID loops will behave differently.** The derivative term was using the
  negated previous error instead of the change in error, so a loop holding a
  steady offset carried a constant derivative push and a rising error was pushed
  the wrong way. Fixed — see Memory safety — but any controller with `rate > 0`
  was tuned around the old behaviour and wants revisiting. Controllers with
  `rate == 0` are unaffected.

- **Found and confirmed, not yet fixed.** Two defects are left, both judged to
  need design rather than a patch: the power-loss window in
  `arm/FLASH/flash_user.c`, where one sector erase covers several separate
  writes so an interruption loses everything after the erase, and the unbounded
  `veval_exp` recursion described above. The `#186-D pointless comparison of
  unsigned integer with zero` warnings are worth reading as a group: a vacuous
  lower-bound check on a network-derived index is how this kind of thing gets
  interesting, and one of them was already hiding a division by zero.
- **The first hardware attempt did not boot at all**, and the cause was the
  memory map rather than anything on screen — see the `RW_RAM1` note under
  Building. The display code had not run when the device hung.
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
  of the ~78 KB free inside the region there is another 96 KB that is not
  allocated at all, out of the 480 KB the bootloader leaves.

  RAM is a different story, and it is the real argument for a bigger part. The
  board carries one IS61L5128L, which is 512K x 8 — 512 KB, byte wide. RW and ZI
  come to about 501 KB, so the external SRAM runs at roughly 92% full with
  around 39 KB spare. The largest consumers are `tsm.o` (169 KB, the BACnet
  transaction state machine), `user_data.o` (~71 KB, the point database) and the
  60 KB FreeRTOS heap. Internal SRAM cannot substitute: the heap alone would take
  94% of the 64 KB on chip.

  Before reaching for a bigger part, note that **a third of the RAM is in a
  prebuilt library and is not being used.** `TSM_List` is exactly
  `255 x 662 = 168,810` bytes, while `bacnet/bacnet.h:75` says

  ```c
  #define MAX_TSM_TRANSACTIONS  20//255 //????????????? changed by chelsea
  ```

  Someone already cut 255 to 20 and it changed nothing, because `tsm.o` comes
  from `bacnet/bacnet_ARM_revXX_T10.lib` and the map records its source as
  `bacnet\src\tsm.c`, which is not in this repository. The header only steers
  code compiled *here*. So about 155 KB is transaction slots the firmware's own
  configuration says should not exist. Recovering it means sourcing an upstream
  `tsm.c` that matches `MAX_APDU 600` and adding it to the project so the linker
  prefers it over the library's copy; eleven exported symbols have to match.
  Recompiled from the repo headers the entry works out at 674-676 bytes rather
  than the library's 662, so budget roughly 150 KB, not a precise figure.

  `address.o` is in the same library, and `Address_Cache` is `255 x 31 = 7,905`
  bytes. There `MAX_ADDRESS_CACHE 255` at `bacnet.h:83` does agree with the
  binary — that one is merely large, not a failed edit.

  The real obstacle is the bootloader. It is not in this repository, it is
  flashed below `0x08008000`, and changing silicon needs one that runs on the
  new part. Flash wait states and the clock tree in
  `arm/USER/system_stm32f10x.c` would need review too. T3000's reported MCU type
  (`T3_chip_type`) is display only and gates nothing, so it is not a concern.
