"""Generate the three state icons in ER-TFT024-3_4-Wire_SPI.c.

    python tools/icongen.py                    report the sizes only
    python tools/icongen.py --write
    python tools/icongen.py --sheet out.png    draw every state for inspection

The idle screen carries three icons: the circulation fan, what the room is
calling for, and where the greenhouse sidewalls are.  Each is stored as a 4 bit
per pixel index map with its own sixteen entry palette, in the same field order
the glyphs use -- two pixels per byte, low nibble first, packed straight across
the row boundaries, so disp_icon4() and screenshot.py unpack them the same way
the fonts are unpacked.

Indexed colour rather than the literal RGB565 the old icons used: a 72x45 cell
is 1620 bytes where a smaller 55x45 literal icon was 4950, so all ten states
together cost less than four of the old ones did, and each still has sixteen
colours to spend.

The palette is built rather than quantised.  Each icon names the two or three
hues it draws with, and the sixteen entries are the background plus an even ramp
from the background to each hue.  An antialiased edge is exactly a blend between
the background and one hue, so it lands on a ramp entry instead of on whatever a
median cut happened to pick, and the edges come out clean by construction.
"""
import argparse
import math

from PIL import Image, ImageDraw, ImageFont

import lcddata as L

SS = 4                      # everything is drawn this many times oversize
XDOTS, YDOTS = 72, 45       # one cell, matching ICON3_XDOTS / ICON3_YDOTS
BPP = 4

GREY = (0x6a, 0x7b, 0x8d)
GREEN = (0x3f, 0xd9, 0x7a)
AMBER = (0xff, 0xb0, 0x2e)
ORANGE = (0xff, 0x6a, 0x2a)
YELLOW = (0xff, 0xd2, 0x3f)
CYAN = (0x3f, 0xc9, 0xff)
ICE = (0xd8, 0xf3, 0xff)
BLUE = (0x4d, 0x8d, 0xff)
STEEL = (0x9f, 0xb4, 0xc7)


def ramp(bg, hue, steps):
    out = []
    for i in range(1, steps + 1):
        f = i / float(steps)
        out.append(tuple(int(round(b + (h - b) * f)) for b, h in zip(bg, hue)))
    return out


def palette_for(bg, hues):
    """Background first, then an even ramp to each hue, sixteen entries total."""
    n = len(hues)
    base, extra = divmod(15, n)
    pal = [bg]
    for i, hue in enumerate(hues):
        pal += ramp(bg, hue, base + (1 if i < extra else 0))
    while len(pal) < 16:
        pal.append(bg)
    return pal[:16]


def nearest(pal, px):
    best, bd = 0, None
    for i, c in enumerate(pal):
        d = sum((a - b) ** 2 for a, b in zip(c, px))
        if bd is None or d < bd:
            best, bd = i, d
    return best


# --------------------------------------------------------------- shapes

def poly(d, pts, cx, cy, sx, sy, colour):
    d.polygon([(cx + x * sx, cy + y * sy) for x, y in pts], fill=colour)


def ring(d, cx, cy, r, w, colour):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=colour, width=int(w))


def fan(d, cx, cy, r, colour, blades=3):
    """Three swept blades: each one runs from the hub to the rim on a curved
    centre line, widest in the middle and pointed at both ends."""
    sweep, wide = 1.20, 0.60
    for k in range(blades):
        a0 = k * 2 * math.pi / blades - math.pi / 2
        n = 36
        edge = []
        for side in (+1, -1):
            span = range(n + 1) if side > 0 else range(n, -1, -1)
            for t in span:
                f = t / float(n)
                rad = r * (0.14 + 0.86 * f)
                hw = wide * math.sin(math.pi * f) ** 0.8 * (1.0 - 0.45 * f)
                edge.append((a0 + f * sweep + side * hw, rad))
        d.polygon([(cx + rad * math.cos(a), cy + rad * math.sin(a)) for a, rad in edge],
                  fill=colour)
    d.ellipse([cx - r * 0.24, cy - r * 0.24, cx + r * 0.24, cy + r * 0.24], fill=colour)


FLAME = [(0.00, -1.00), (0.26, -0.62), (0.40, -0.24), (0.48, 0.14),
         (0.44, 0.52), (0.26, 0.84), (0.00, 0.96), (-0.26, 0.84),
         (-0.44, 0.52), (-0.48, 0.14), (-0.40, -0.24), (-0.24, -0.50),
         (-0.16, -0.20), (-0.04, -0.52)]
CORE = [(0.00, -0.22), (0.22, 0.14), (0.26, 0.48), (0.10, 0.80),
        (-0.10, 0.80), (-0.26, 0.48), (-0.22, 0.14)]


def snowflake(d, cx, cy, r, colour, w):
    for k in range(6):
        a = k * math.pi / 3
        dx, dy = math.cos(a), math.sin(a)
        d.line([cx, cy, cx + dx * r, cy + dy * r], fill=colour, width=int(w))
        for at, blen in ((0.52, 0.30), (0.80, 0.22)):
            bx, by = cx + dx * r * at, cy + dy * r * at
            for s in (+1, -1):
                b = a + s * math.pi / 4
                d.line([bx, by, bx + math.cos(b) * r * blen, by + math.sin(b) * r * blen],
                       fill=colour, width=int(w))


def thermometer(d, cx, cy, h, colour, w, fill_frac=0.0, fill_colour=None):
    bulb = h * 0.20
    top = cy - h / 2
    bot = cy + h / 2 - bulb
    d.ellipse([cx - bulb, cy + h / 2 - bulb * 2, cx + bulb, cy + h / 2], fill=colour)
    d.rounded_rectangle([cx - bulb * 0.55, top, cx + bulb * 0.55, bot],
                        radius=bulb * 0.55, outline=colour, width=int(w))
    if fill_frac > 0:
        col = top + (bot - top) * (1.0 - fill_frac)
        d.rounded_rectangle([cx - bulb * 0.22, col, cx + bulb * 0.22, bot],
                            radius=bulb * 0.22, fill=fill_colour or colour)


def arrow(d, cx, cy, half, colour, w, up):
    """Head at the top when up, at the bottom when down."""
    tip = cy - half if up else cy + half
    tail = cy + half if up else cy - half
    barb = tip + (half * 0.55 if up else -half * 0.55)
    d.line([cx, tail, cx, tip], fill=colour, width=int(w))
    d.line([cx - half * 0.60, barb, cx, tip], fill=colour, width=int(w))
    d.line([cx + half * 0.60, barb, cx, tip], fill=colour, width=int(w))


def greenhouse(d, walls_up, hoop, panel, S):
    """A hoop house seen end on: ground, arch, and the two roll up sidewalls."""
    w = 3 * S
    left, right = 7 * S, 65 * S
    ground = 41 * S
    spring = 25 * S                      # where the arch meets the walls
    d.line([4 * S, ground, 68 * S, ground], fill=hoop, width=int(w))
    d.arc([left, spring - (right - left) / 2 * 0.62, right, spring + (right - left) / 2 * 0.62],
          180, 360, fill=hoop, width=int(w))
    d.line([left, spring, left, ground], fill=hoop, width=int(w))
    d.line([right, spring, right, ground], fill=hoop, width=int(w))
    for x in (left, right):
        if walls_up:                             # rolled into a bundle under the eave
            d.rounded_rectangle([x - 6 * S, spring + 1 * S, x + 6 * S, spring + 8 * S],
                                radius=3.5 * S, fill=panel)
        else:                                    # let down, closing the side to the ground
            d.rectangle([x - 4.5 * S, spring + 1 * S, x + 4.5 * S, ground - w / 2], fill=panel)
    arrow(d, 36 * S, 32 * S, 7.5 * S, panel, 3 * S, walls_up)


def badge(d, cx, cy, r, colour, letter, bg):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=colour)
    try:
        ft = ImageFont.truetype('C:/Windows/Fonts/consolab.ttf', int(r * 1.7))
    except Exception:
        ft = ImageFont.load_default()
    b = d.textbbox((0, 0), letter, font=ft)
    d.text((cx - (b[2] - b[0]) / 2.0 - b[0], cy - (b[3] - b[1]) / 2.0 - b[1]),
           letter, font=ft, fill=bg)


def frame(d, colour, S):
    """The override marker: the symbol boxed in amber."""
    d.rounded_rectangle([1.5 * S, 1.5 * S, (XDOTS - 1.5) * S, (YDOTS - 1.5) * S],
                        radius=6 * S, outline=colour, width=int(2.5 * S))


# --------------------------------------------------------------- the icons

def draw_fan(d, S, state, bg):
    colour = GREY if state == 'off' else GREEN
    cx, cy, r = 34 * S, 22 * S, 17 * S
    fan(d, cx, cy, r, colour)
    ring(d, cx, cy, r + 4 * S, 2.5 * S, colour)
    if state == 'auto':
        badge(d, 60 * S, 33 * S, 11 * S, AMBER, 'A', bg)
    elif state == 'off':
        b = (r + 4 * S) * 0.78
        d.line([cx - b, cy + b, cx + b, cy - b], fill=bg, width=int(5 * S))
        d.line([cx - b, cy + b, cx + b, cy - b], fill=GREY, width=int(2.5 * S))


def draw_mode(d, S, state, bg):
    cx, cy = 36 * S, 22 * S
    if state == 'idle':
        thermometer(d, cx, cy, 41 * S, GREY, 2.5 * S, 0.30, GREY)
    elif state.startswith('heat'):
        poly(d, FLAME, cx, cy, 21 * S, 20.5 * S, ORANGE)
        poly(d, CORE, cx, cy, 21 * S, 20.5 * S, YELLOW)
    else:
        snowflake(d, cx, cy, 20.5 * S, CYAN, 3 * S)
        d.ellipse([cx - 3 * S, cy - 3 * S, cx + 3 * S, cy + 3 * S], fill=ICE)
    if state.endswith('_ovr'):
        frame(d, AMBER, S)


def draw_walls(d, S, state, bg):
    greenhouse(d, state == 'up', STEEL, GREEN if state == 'up' else BLUE, S)


ICONS = {
    'fan_off':   (draw_fan, 'off', [GREY]),
    'fan_on':    (draw_fan, 'on', [GREEN]),
    'fan_auto':  (draw_fan, 'auto', [GREEN, AMBER]),
    'mode_idle': (draw_mode, 'idle', [GREY]),
    'mode_heat': (draw_mode, 'heat', [ORANGE, YELLOW]),
    'mode_cool': (draw_mode, 'cool', [CYAN, ICE]),
    'mode_heat_ovr': (draw_mode, 'heat_ovr', [ORANGE, YELLOW, AMBER]),
    'mode_cool_ovr': (draw_mode, 'cool_ovr', [CYAN, ICE, AMBER]),
    'wall_down': (draw_walls, 'down', [STEEL, BLUE]),
    'wall_up':   (draw_walls, 'up', [STEEL, GREEN]),
}
ORDER = ['fan_off', 'fan_on', 'fan_auto',
         'mode_idle', 'mode_heat', 'mode_cool', 'mode_heat_ovr', 'mode_cool_ovr',
         'wall_down', 'wall_up']


def build(name, bg):
    fn, state, hues = ICONS[name]
    pal = palette_for(bg, hues)
    big = Image.new('RGB', (XDOTS * SS, YDOTS * SS), bg)
    fn(ImageDraw.Draw(big), SS, state, bg)
    im = big.resize((XDOTS, YDOTS), Image.LANCZOS)

    idx = [nearest(pal, im.getpixel((x, y))) for y in range(YDOTS) for x in range(XDOTS)]
    data = bytearray()
    for i in range(0, len(idx), 2):
        data.append(idx[i] | (idx[i + 1] << 4))
    return bytes(data), pal, idx


def sheet(path, bg, built):
    zoom, cols, gap, cap = 4, 5, 8, 18
    rows = (len(ORDER) + cols - 1) // cols
    sh = Image.new('RGB', (cols * (XDOTS * zoom + gap) - gap,
                           rows * (YDOTS * zoom + gap + cap) - gap), (12, 14, 18))
    d = ImageDraw.Draw(sh)
    try:
        ft = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 14)
    except Exception:
        ft = ImageFont.load_default()
    for n, name in enumerate(ORDER):
        _, pal, idx = built[name]
        im = Image.new('RGB', (XDOTS, YDOTS))
        im.putdata([pal[i] for i in idx])
        x = (n % cols) * (XDOTS * zoom + gap)
        y = (n // cols) * (YDOTS * zoom + gap + cap)
        d.text((x + 2, y + 1), name, fill=(210, 215, 225), font=ft)
        sh.paste(im.resize((XDOTS * zoom, YDOTS * zoom), Image.NEAREST), (x, y + cap))
    sh.save(path)
    print('wrote', path, sh.size)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--sheet')
    args = ap.parse_args()

    k = L.defines()
    bg = L.to_rgb(k['TSTAT8_BACK_COLOR'])
    src = L.read(L.SRC)

    built = {n: build(n, bg) for n in ORDER}
    per = XDOTS * YDOTS * BPP // 8
    for n in ORDER:
        assert len(built[n][0]) == per, n
    print('%d states, %d bytes each plus a 32 byte palette, %d bytes in total'
          % (len(ORDER), per, len(ORDER) * (per + 32)))

    if args.sheet:
        sheet(args.sheet, bg, built)
    if not args.write:
        print('preview only; pass --write to replace the arrays')
        return

    for n in ORDER:
        data, pal, _ = built[n]
        for sym, vals, kind in (('icon_' + n, list(data), 'uint8'),
                                ('pal_' + n, [L.to565(*c) for c in pal], 'uint16')):
            span, _ = L.array(src, sym, kind, k)
            text = L.emit(sym, vals, kind, 12 if kind == 'uint16' else 16)
            if span is None:
                anchor, _ = L.array(src, 'chlibsmall', 'uint8', k)
                src = src[:anchor[0]] + text + chr(10) * 2 + src[anchor[0]:]
            else:
                src = src[:span[0]] + text + src[span[1]:]
    open(L.SRC, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(src)

    after = L.read(L.SRC)
    for n in ORDER:
        _, vals = L.array(after, 'icon_' + n, 'uint8', k)
        assert bytes(vals) == built[n][0], n + ' did not read back'
        _, vals = L.array(after, 'pal_' + n, 'uint16', k)
        assert vals == [L.to565(*c) for c in built[n][1]], n + ' palette did not read back'
    print('rewrote %s, all %d arrays read back identical' % (L.SRC, len(ORDER) * 2))


if __name__ == '__main__':
    main()
