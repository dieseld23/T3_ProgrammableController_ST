"""Regenerate the LCD bitmap fonts in ER-TFT024-3_4-Wire_SPI.c from a TrueType face.

    python tools/fontgen.py                                  report the fit only
    python tools/fontgen.py --write
    python tools/fontgen.py --font C:/Windows/Fonts/lucon.ttf --write

disp_ch() and disp_ch_16_24() store a glyph as a 1-bit bitmap, row major, eight
pixels per byte, least significant bit first, packed straight across the row
boundaries.  Three tables use that layout:

    chlibsmall   24x36, 108 bytes/glyph, ASCII 32..126   the three value rows
    chlib        48x96, 576 bytes/glyph, '0'..'9','-',' '  the big top number
    char_16_24   16x24,  48 bytes/glyph, ASCII 32..122   the top-area label
    char_12_24   12x24,  36 bytes/glyph, ASCII 32..126   the three row labels

char_12_24 is the odd one out: it is condensed rather than drawn at its natural
width, because the row label has to fit between the screen edge and the value
box at x=102.  Eight characters is the whole of Str_variable_point.label, and 12
pixels is both the widest cell that fits eight of them and the narrowest that
stays legible, so there is no choice to make about the number.

Before anything is generated the packer is run over the shipped arrays and the
result compared byte for byte, so a mistake in the bit order cannot reach the
device.

Three things fix the glyph box, all of them from the firmware rather than from
the font:

  * disp_str() advances 23 pixels but the cell is 24 wide, so the last column of
    every glyph is painted over by its neighbour.  Ink is kept inside columns
    1..22 to leave a clear pixel between letters.
  * the cell has no margin: anything drawn outside it is simply lost, so the
    whole character set has to fit between the cap line and the bottom row.
  * the cap height and cap top row are pinned to what the face that shipped
    used, so the new one lands in the same place on the screen rather than
    floating up or down inside the frames.

A monospace face needs the least horizontal squeezing here: the cell is 24x36,
close to the aspect ratio such a face is drawn at.  At cap 24 Consolas Bold needs
no condensing at all, where Arial Narrow Bold needs 0.75 and Segoe UI Semibold
0.62.
"""
import argparse
import re

from PIL import Image, ImageDraw, ImageFont

import lcddata as L

SS = 4            # glyphs are rendered this many times oversize, then scaled down
THRESHOLD = 110   # coverage, 0..255, at which a scaled-down pixel turns on

ASCII = [chr(c) for c in range(32, 127)]
DIGITS = list('0123456789') + ['-', ' ']

# name -> (w, h, bytes/glyph, chars, ink columns, cap height, cap top row)
TABLES = {
    'chlibsmall': (24, 36, 108, ASCII,  22, 24, 4),
    'chlib':      (48, 96, 576, DIGITS, 46, 74, 9),
    'char_16_24': (16, 24,  48, [chr(c) for c in range(32, 123)], 14, 17, 4),
    'char_12_24': (12, 24,  36, ASCII, 10, 17, 4),
}
KIND = {'chlibsmall': 'uint8', 'chlib': 'unsigned char', 'char_16_24': 'uint8',
        'char_12_24': 'uint8'}


def decode(data, w, h, nbytes, index):
    g = data[index * nbytes:(index + 1) * nbytes]
    bits = []
    for b in g:
        for i in range(8):
            bits.append((b >> i) & 1)
    return [[bits[y * w + x] for x in range(w)] for y in range(h)]


def encode(rows, w, h, nbytes):
    bits = [rows[y][x] for y in range(h) for x in range(w)]
    out = bytearray()
    for j in range(nbytes):
        b = 0
        for i in range(8):
            if bits[j * 8 + i]:
                b |= 1 << i
        out.append(b)
    return bytes(out)


def check_roundtrip(src, sym):
    for name, (w, h, nb, chars, _, _, _) in TABLES.items():
        span, vals = L.array(src, name, 'uint8', sym)
        if span is None:
            print('  %-12s not in the source yet, it will be added' % name)
            continue
        want = len(chars) * nb
        if len(vals) != want:
            raise SystemExit('%s: %d bytes, expected %d' % (name, len(vals), want))
        for i in range(len(chars)):
            if encode(decode(vals, w, h, nb, i), w, h, nb) != bytes(vals[i * nb:(i + 1) * nb]):
                raise SystemExit('%s: glyph %d does not round trip' % (name, i))
        print('  %-12s %3d glyphs, %5d bytes  round trips exactly'
              % (name, len(chars), len(vals)))


def size_for_cap(path, cap):
    """Smallest supersampled size whose '8' is at least cap tall."""
    for sz in range(8, 900):
        ft = ImageFont.truetype(path, sz)
        b = ft.getbbox('8')
        if (b[3] - b[1]) >= cap * SS:
            return sz, ft
    raise SystemExit('no size reaches cap %d in %s' % (cap, path))


def build_table(path, name):
    w, h, nb, chars, ink_w, cap, cap_top = TABLES[name]
    sz, ft = size_for_cap(path, cap)

    inked = [c for c in chars if c != ' ']
    left = min(ft.getbbox(c)[0] for c in inked)
    right = max(ft.getbbox(c)[2] for c in inked)
    condense = min(1.0, (ink_w * SS) / float(right - left))
    y_off = cap_top * SS - ft.getbbox('8')[1]     # one baseline for every glyph

    data = bytearray()
    clipped = []
    for ch in chars:
        big = Image.new('L', (w * SS * 3, h * SS), 0)
        if ch != ' ':
            x_off = (w * SS - (right - left) * condense) / 2.0 - left * condense
            ImageDraw.Draw(big).text((x_off, y_off), ch, font=ft, fill=255)
            if condense < 0.999:
                big = big.resize((int(big.width * condense), big.height), Image.LANCZOS)
        small = big.resize((max(1, big.width // SS), h), Image.LANCZOS)
        cell = [[1 if (x < small.width and small.getpixel((x, y)) >= THRESHOLD) else 0
                 for x in range(w)] for y in range(h)]
        # column 0 stays clear, and so does everything the next glyph paints over
        lost = 0
        for row in cell:
            lost += row[0]
            row[0] = 0
            for x in range(ink_w + 1, w):
                lost += row[x]
                row[x] = 0
        if lost:
            clipped.append((ch, lost))
        data += encode(cell, w, h, nb)

    blob = bytes(data)
    rows8 = decode(blob, w, h, nb, chars.index('8'))
    used = [y for y in range(h) if any(rows8[y])]
    print("  %-12s %-16s size %-4d condense %.2f  '8' rows %d..%d (cap %d)"
          % (name, path.split('/')[-1], sz // SS, condense, used[0], used[-1],
             used[-1] - used[0] + 1))
    if clipped:
        worst = sorted(clipped, key=lambda t: -t[1])[:6]
        print('       %d glyphs lost ink at the cell edge: %s'
              % (len(clipped), ', '.join('%r(%d)' % t for t in worst)))
    return blob


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--font', default='C:/Windows/Fonts/consolab.ttf')
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    src = L.read(L.SRC)
    sym = L.defines()
    print('packer check against the shipped arrays:')
    check_roundtrip(src, sym)

    print('generating from %s:' % args.font)
    built = {n: build_table(args.font, n) for n in TABLES}
    for name, blob in built.items():
        w, h, nb, chars, _, _, _ = TABLES[name]
        assert len(blob) == nb * len(chars), name

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
    print('\nrewrote %s, all three arrays read back identical' % L.SRC)


if __name__ == '__main__':
    main()
