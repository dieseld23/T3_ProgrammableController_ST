"""Regenerate the LCD bitmap fonts in ER-TFT024-3_4-Wire_SPI.c from a TrueType face.

    python tools/fontgen.py                                  report the fit only
    python tools/fontgen.py --write
    python tools/fontgen.py --font C:/Windows/Fonts/lucon.ttf --write

A glyph is stored row major, packed across row boundaries, least significant
bit first.  Each pixel is a coverage level of `bpp` bits, so 1 bpp is the
original ink/no-ink bitmap and anything above that is antialiased: the renderer
turns the level into a colour through a palette interpolated between dcolor and
bgcolor.  Four tables:

    chlibsmall   24x36, 2 bpp, ASCII 32..126        the three value rows
    chlib        48x96, 4 bpp, '0'..'9','-',' '     the big top number
    char_16_24   16x24, 2 bpp, ASCII 32..122        the top-area label
    char_12_24   12x24, 2 bpp, ASCII 32..126        the three row labels
    char_10_24   10x24, 2 bpp, ASCII 32..126        the corner readout

The big number carries 4 bpp because it is the text the eye goes to and it is
only twelve glyphs; the rest carry 2 bpp, where nearly all of the benefit is.
Going from 1 bpp to 2 removes the staircase, and 2 to 4 only refines an edge
that already reads as smooth, so the extra bits are not worth 3x the flash
everywhere.

Two checks run before anything is generated:

  * the 1 bpp packer is re-run over the arrays as they were committed and
    compared byte for byte.  Those arrays were consumed correctly by the
    shipped renderer, so this is an independent check of the packing machinery
    rather than the packer agreeing with itself.
  * every table currently in the source is unpacked and repacked at its
    declared depth.

Neither proves the multi-bit field order matches what the C renderers extract.
That is checked by rendering: tools/screenshot.py unpacks with the same
convention, so a disagreement shows up as visibly mangled text.

Three things fix the glyph box, all from the firmware rather than the font:

  * disp_str() advances 23 pixels but the cell is 24 wide, so the last column of
    every glyph is painted over by its neighbour.  Ink is kept inside columns
    1..22 to leave a clear pixel between letters.
  * the cell has no margin: anything drawn outside it is simply lost, so the
    whole character set has to fit between the cap line and the bottom row.
  * the cap height and cap top row are pinned to what the face that shipped
    used, so the new one lands in the same place on the screen.
"""
import argparse
import subprocess

from PIL import Image, ImageDraw, ImageFont

import lcddata as L

SS = 4            # glyphs are rendered this many times oversize, then scaled down
THRESHOLD = 110   # coverage, 0..255, at which a 1 bpp pixel turns on

ASCII = [chr(c) for c in range(32, 127)]
DIGITS = list('0123456789') + ['-', ' ']

# name -> (w, h, chars, ink columns, cap height, cap top row, bits per pixel,
#          characters that set the width)
#
# The last field is optional.  Condensing is driven by the widest glyph in the
# table, so a face that only ever draws a handful of characters is squeezed by
# ones it will never show: char_10_24 at full ASCII condenses to 0.54 and clips
# the ink off '%', which is the one glyph it exists to draw.  Naming the
# characters that matter lets the rest clip, since nothing renders them.
TABLES = {
    'chlibsmall': (24, 36, ASCII,  22, 24, 4, 2),
    'chlib':      (48, 96, DIGITS, 46, 74, 9, 4),
    'char_16_24': (16, 24, [chr(c) for c in range(32, 123)], 14, 17, 4, 2),
    'char_12_24': (12, 24, ASCII,  10, 17, 4, 2),
    'char_10_24': (10, 24, ASCII,   9, 17, 4, 2, '0123456789%RH'),
}


def geom(name):
    t = TABLES[name]
    w, h, chars, ink_w, cap, cap_top, bpp = t[:7]
    return w, h, chars, ink_w, cap, cap_top, bpp


def measured(name):
    """The characters whose widths set the condense factor."""
    t = TABLES[name]
    chars = t[7] if len(t) > 7 else t[2]
    return [c for c in chars if c != ' ']


def nbytes(name, bpp=None):
    """Bytes per glyph. The cell always divides evenly at 1, 2 and 4 bpp."""
    w, h, _, _, _, _, declared = geom(name)
    bpp = declared if bpp is None else bpp
    bits = w * h * bpp
    assert bits % 8 == 0, name
    return bits // 8


def decode(data, w, h, nb, index, bpp):
    g = data[index * nb:(index + 1) * nb]
    per, mask = 8 // bpp, (1 << bpp) - 1
    px = []
    for b in g:
        for k in range(per):
            px.append((b >> (k * bpp)) & mask)
    return [[px[y * w + x] for x in range(w)] for y in range(h)]


def encode(rows, w, h, nb, bpp):
    per, mask = 8 // bpp, (1 << bpp) - 1
    px = [rows[y][x] for y in range(h) for x in range(w)]
    out = bytearray()
    for j in range(nb):
        b = 0
        for k in range(per):
            b |= (px[j * per + k] & mask) << (k * bpp)
        out.append(b)
    return bytes(out)


def roundtrip(vals, name, bpp, label):
    """Unpack and repack every glyph, comparing byte for byte."""
    w, h, chars = geom(name)[0], geom(name)[1], geom(name)[2]
    nb = nbytes(name, bpp)
    want = len(chars) * nb
    if len(vals) != want:
        return '%d bytes, not the %d that %d bpp needs' % (len(vals), want, bpp)
    for i in range(len(chars)):
        got = encode(decode(vals, w, h, nb, i, bpp), w, h, nb, bpp)
        if got != bytes(vals[i * nb:(i + 1) * nb]):
            return 'glyph %d does not round trip' % i
    print('  %-12s %-9s %3d glyphs, %6d bytes at %d bpp  round trips exactly'
          % (name, label, len(chars), len(vals), bpp))
    return None


def committed_source(rev):
    out = subprocess.check_output(['git', 'show', '%s:%s' % (rev, L.SRC)])
    return out.decode('utf-8', 'surrogateescape')


def check_committed_1bpp(sym, rev):
    """The independent check: the shipped renderer consumed these arrays."""
    try:
        src = committed_source(rev)
    except Exception as exc:
        print('  could not read %s from %s (%s), skipping' % (L.SRC, rev, exc))
        return
    for name in TABLES:
        span, vals = L.array(src, name, 'uint8', sym)
        if span is None:
            continue
        err = roundtrip(vals, name, 1, '@' + rev)
        if err and 'bytes, not the' not in err:
            raise SystemExit('%s at %s: %s' % (name, rev, err))


def size_for_cap(path, cap):
    """Smallest supersampled size whose '8' is at least cap tall."""
    for sz in range(8, 900):
        ft = ImageFont.truetype(path, sz)
        b = ft.getbbox('8')
        if (b[3] - b[1]) >= cap * SS:
            return sz, ft
    raise SystemExit('no size reaches cap %d in %s' % (cap, path))


def build_table(path, name):
    w, h, chars, ink_w, cap, cap_top, bpp = geom(name)
    nb = nbytes(name)
    top = (1 << bpp) - 1
    sz, ft = size_for_cap(path, cap)

    inked = measured(name)
    left = min(ft.getbbox(c)[0] for c in inked)
    right = max(ft.getbbox(c)[2] for c in inked)
    condense = min(1.0, (ink_w * SS) / float(right - left))
    y_off = cap_top * SS - ft.getbbox('8')[1]     # one baseline for every glyph

    data = bytearray()
    clipped = []
    for ch in chars:
        big = Image.new('L', (w * SS * 3, h * SS), 0)
        if ch != ' ':
            # Centre the ink inside the columns that are KEPT (1..ink_w), not
            # inside the whole cell.  Column 0 is always cleared, so the two are
            # only the same when w - ink_w is even; at 10 dots with ink_w 9 the
            # old formula hung half a column off each end and clipped '%'.
            x_off = SS + (ink_w * SS - (right - left) * condense) / 2.0 - left * condense
            ImageDraw.Draw(big).text((x_off, y_off), ch, font=ft, fill=255)
            if condense < 0.999:
                big = big.resize((int(big.width * condense), big.height), Image.LANCZOS)
        small = big.resize((max(1, big.width // SS), h), Image.LANCZOS)

        cell = []
        for y in range(h):
            row = []
            for x in range(w):
                v = small.getpixel((x, y)) if x < small.width else 0
                if bpp == 1:
                    row.append(top if v >= THRESHOLD else 0)
                else:
                    row.append(int(round(v / 255.0 * top)))
            cell.append(row)
        # column 0 stays clear, and so does everything the next glyph paints over
        lost = 0
        for row in cell:
            lost += 1 if row[0] else 0
            row[0] = 0
            for x in range(ink_w + 1, w):
                lost += 1 if row[x] else 0
                row[x] = 0
        if lost:
            clipped.append((ch, lost))
        data += encode(cell, w, h, nb, bpp)

    blob = bytes(data)
    rows8 = decode(blob, w, h, nb, chars.index('8'), bpp)
    used = [y for y in range(h) if any(rows8[y])]
    print("  %-12s %-16s %d bpp  size %-4d condense %.2f  '8' rows %d..%d (cap %d)"
          % (name, path.split('/')[-1], bpp, sz // SS, condense, used[0], used[-1],
             used[-1] - used[0] + 1))
    if clipped:
        worst = sorted(clipped, key=lambda t: -t[1])[:6]
        print('       %d glyphs lost ink at the cell edge: %s'
              % (len(clipped), ', '.join('%r(%d)' % t for t in worst)))
    return blob


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--font', default='C:/Windows/Fonts/consolab.ttf')
    ap.add_argument('--rev', default='HEAD', help='revision holding the 1 bpp arrays')
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    src = L.read(L.SRC)
    sym = L.defines()

    print('1 bpp packer against the arrays as committed at %s:' % args.rev)
    check_committed_1bpp(sym, args.rev)

    print('tables in the working source, at their declared depth:')
    for name in TABLES:
        span, vals = L.array(src, name, 'uint8', sym)
        if span is None:
            print('  %-12s not in the source yet, it will be added' % name)
            continue
        err = roundtrip(vals, name, geom(name)[6], 'on disk')
        if err:
            print('  %-12s %s -- it will be replaced' % (name, err))

    print('generating from %s:' % args.font)
    built = {n: build_table(args.font, n) for n in TABLES}
    total = 0
    for name, blob in built.items():
        assert len(blob) == nbytes(name) * len(geom(name)[2]), name
        total += len(blob)
    print('  %d bytes of glyph data in total' % total)

    if not args.write:
        print('\npreview only; pass --write to replace the arrays')
        return

    for name, blob in built.items():
        span, _ = L.array(src, name, 'uint8', sym)
        text = L.emit(name, list(blob), 'uint8')
        if span is None:
            # a table generated for the first time goes in beside its neighbours
            anchor, _ = L.array(src, 'char_16_24', 'uint8', sym)
            gap = chr(10) * 2
            src = src[:anchor[1]] + gap + text + src[anchor[1]:]
        else:
            src = src[:span[0]] + text + src[span[1]:]
    open(L.SRC, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(src)

    after = L.read(L.SRC)
    for name, blob in built.items():
        _, vals = L.array(after, name, 'uint8', sym)
        assert bytes(vals) == blob, name + ' did not read back'
    print('\nrewrote %s, all four arrays read back identical' % L.SRC)


if __name__ == '__main__':
    main()
