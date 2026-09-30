"""Check the top area's erasing, which screenshot.py does not model.

    python tools/erase_check.py

Replays Top_area_display's draw-then-blank sequence on one canvas through a run
of readings and units, never clearing it, and after each call compares the top
area with a fresh render of that reading. Then it leaves out each of
t3oem_top_degrees()'s blank rectangles in turn, to show which of them the run
depends on. Exits 1 if the intact replay leaves a stale dot.

The sequence is a hand copy of ER-TFT024-3_4-Wire_SPI.c:
  - T3-OEM, C or F: t3oem_top_degrees(), the cells, ring and letter, then seven
    blank rectangles;
  - everything else: the three fixed cells, then the unit switch with
    unit_band_blank().
Change it with the C.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)                      # screenshot.py reads the firmware by relative path
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import screenshot as SS  # noqa: E402

k = SS.Screen().k
BG, CH = k['TSTAT8_BACK_COLOR'], k['TSTAT8_CH_COLOR']

BLANKS = ['left of the cells', 'right of the cells, above the unit', 'right of the cells, below the unit',
          'the dot before the ring', 'above the ring', 'below the ring', 'right of the letter']

SEQ = [(720, 'F'), (50, 'F'), (1000, 'F'), (90, 'F'), (100, 'F'), (-100, 'F'), (-50, 'F'), (-3, 'F'),
       (0, 'C'), (999, 'C'), (720, 'RH'), (50, 'F'), (820, 'PPM'), (-720, 'C'), (5, 'PERCENT'),
       (1000, 'F'), (45, 'NONE'), (50, 'C'), (123, 'Pa'), (720, 'F'), (99, 'kPa'), (-90, 'F')]


def cells_for(top, unit, t3oem):
    """The three right aligned cells Top_area_display builds."""
    n = SS.round_tenths(top) if unit in ('C', 'F', 'RH') else int(top)
    neg = top < 0 and (n != 0 or not t3oem)
    n = min(99 if neg else 999, abs(n))
    cells = [' ', ' ', chr(0x30 + n % 10)]
    if n >= 10:
        cells[1] = chr(0x30 + (n // 10) % 10)
    if n >= 100:
        cells[0] = chr(0x30 + n // 100)
    if neg:
        cells[1 if n < 10 else 0] = '-'
    return cells


def rect(s, x0, y0, x1, y1):
    """top_area_rect(): blank x0..x1-1, y0..y1-1 if it has any area."""
    if x1 > x0 and y1 > y0:
        s.null_icon(x1 - x0, y1 - y0, x0, y0, BG)


def unit_band_rect(s, x0, y0, x1, y1):
    bx0, bx1 = k['UNIT_BAND_XPOS'], k['UNIT_BAND_XPOS'] + k['UNIT_BAND_XDOTS']
    by0, by1 = k['UNIT_BAND_YPOS'], k['UNIT_BAND_YPOS'] + k['UNIT_BAND_YDOTS']
    rect(s, max(x0, bx0), max(y0, by0), min(x1, bx1), min(y1, by1))


def unit_band_blank(s, rx, ry, rw, rh, tx, chars):
    bx0, bx1 = k['UNIT_BAND_XPOS'], k['UNIT_BAND_XPOS'] + k['UNIT_BAND_XDOTS']
    by0, by1 = k['UNIT_BAND_YPOS'], k['UNIT_BAND_YPOS'] + k['UNIT_BAND_YDOTS']
    tx1 = tx + (chars - 1) * 23 + k['CHSMALL_XDOTS'] if chars else tx
    if chars == 0:
        tx = tx1 = bx1
    unit_band_rect(s, bx0, by0, rx if rw else tx, by1)
    if rw:
        unit_band_rect(s, rx, by0, rx + rw, ry)
        unit_band_rect(s, rx, ry + rh, rx + rw, by1)
        unit_band_rect(s, rx + rw, by0, tx, by1)
    if chars:
        unit_band_rect(s, tx, by0, tx1, k['UNIT_TEXT_YPOS'])
        unit_band_rect(s, tx, k['UNIT_TEXT_YPOS'] + k['CHSMALL_YDOTS'], tx1, by1)
    unit_band_rect(s, tx1, by0, bx1, by1)


def firmware(s, top, unit, t3oem, skip=None):
    """One call of Top_area_display. skip leaves out that blank of BLANKS."""
    cells = cells_for(top, unit, t3oem)
    if t3oem and unit in ('C', 'F'):
        first = 1 if cells[0] == ' ' else 0
        x0 = k['TOP_TWO_CELL_XPOS'] if first else k['FIRST_CH_POS']
        xc = x0 + (3 - first) * k['CHLIB_XDOTS']
        rx = xc + 1
        r = k['DEGREE_RING_XDOTS']
        tx = rx + r
        tx1 = tx + k['CHSMALL_XDOTS']
        ty0, ty1 = k['UNIT_TEXT_YPOS'], k['UNIT_TEXT_YPOS'] + k['CHSMALL_YDOTS']
        for c in range(first, 3):
            s.ch(0, x0 + (c - first) * k['CHLIB_XDOTS'], k['THERM_METER_POS'], cells[c], CH, BG)
        s.icon(r, r, 'degree_o', rx, k['UNIT_YPOS'])
        s.text(1, tx, k['UNIT_TEXT_YPOS'], unit, CH, BG)
        ax0, ax1, ay0, ay1 = k['TOP_AREA_XPOS'], k['TOP_AREA_XEND'], k['TOP_AREA_YPOS'], k['TOP_AREA_YEND']
        blanks = [(ax0, ay0, x0, ay1), (xc, ay0, ax1, ty0), (xc, ty1, ax1, ay1), (xc, ty0, rx, ty1),
                  (rx, ty0, tx, k['UNIT_YPOS']), (rx, k['UNIT_YPOS'] + r, tx, ty1), (tx1, ty0, ax1, ty1)]
        for i, b in enumerate(blanks):
            if i != skip:
                rect(s, *b)
        return
    for c, xk in enumerate(('FIRST_CH_POS', 'SECOND_CH_POS', 'THIRD_CH_POS')):
        s.ch(0, k[xk], k['THERM_METER_POS'], cells[c], CH, BG)
    text, ring = SS.UNITS[unit]
    if ring:
        s.icon(14, 14, 'degree_o', k['UNIT_POS'] - 14, k['UNIT_YPOS'])
        s.text(1, k['UNIT_POS'], k['UNIT_TEXT_YPOS'], text, CH, BG)
        unit_band_blank(s, k['UNIT_POS'] - 14, k['UNIT_YPOS'], 14, 14, k['UNIT_POS'], 1)
    elif len(text) == 1:
        s.text(1, k['UNIT_POS'], k['UNIT_TEXT_YPOS'], text, CH, BG)
        unit_band_blank(s, 0, 0, 0, 0, k['UNIT_POS'], 1)
    elif len(text) == 2:
        s.text(1, k['UNIT2_POS'], k['UNIT_TEXT_YPOS'], text, CH, BG)
        unit_band_blank(s, 0, 0, 0, 0, k['UNIT2_POS'], 2)
    else:
        unit_band_blank(s, 0, 0, 0, 0, 0, 0)


def top_area(im):
    return im.crop((k['TOP_AREA_XPOS'], k['TOP_AREA_YPOS'], k['TOP_AREA_XEND'], k['TOP_AREA_YEND'])).tobytes()


def fresh(top, unit, t3oem):
    s = SS.Screen()
    s.clear(BG)
    firmware(s, top, unit, t3oem)
    return s.im


def replay(t3oem, skip=None, verbose=False):
    """Run SEQ on one canvas; return how many steps left a stale dot."""
    bad = 0
    s = SS.Screen()
    s.clear(BG)
    for top, unit in SEQ:
        firmware(s, top, unit, t3oem, skip)
        want = fresh(top, unit, t3oem)
        if top_area(s.im) != top_area(want):
            bad += 1
            if verbose:
                diff = [(x, y) for y in range(k['TOP_AREA_YPOS'], k['TOP_AREA_YEND'])
                        for x in range(k['TOP_AREA_XPOS'], k['TOP_AREA_XEND'])
                        if s.im.getpixel((x, y)) != want.getpixel((x, y))]
                xs = [d[0] for d in diff]
                ys = [d[1] for d in diff]
                print('  after %s %s: %d stale dots, x %d..%d y %d..%d' % (top, unit, len(diff),
                      min(xs), max(xs), min(ys), max(ys)))
            s = SS.Screen()             # resync and carry on
            s.clear(BG)
            firmware(s, top, unit, t3oem, skip)
    return bad


def main():
    bad = 0
    for t3oem in (True, False):
        print('%s, %d readings:' % ('T3-OEM' if t3oem else 'Tstat10', len(SEQ)))
        n = replay(t3oem, verbose=True)
        print('  %d stale' % n)
        bad += n
    print('T3-OEM with one blank left out:')
    for i, name in enumerate(BLANKS):
        n = replay(True, skip=i)
        print('  %-36s %s' % (name, '%d steps stale' % n if n else 'never needed by this run'))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
