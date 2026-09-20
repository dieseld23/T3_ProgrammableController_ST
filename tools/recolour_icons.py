"""Recolour the LCD icon bitmaps for a new screen background.

    python tools/recolour_icons.py                 report what would change
    python tools/recolour_icons.py --bg #0D1520 --write

disp_icon() ignores its dcolor/bgcolor arguments and blits the array straight
out, so every icon carries literal RGB565 pixels -- including whatever screen
background it was drawn against.  Change the background constant on its own and
every icon keeps a rectangle of the old colour around it.

The icons are antialiased, so an exact-match replace leaves a halo.  Each pixel
is instead treated as artwork composited over the old background:

    p = a * ink + (1 - a) * oldbg

The coverage a is estimated from how far the pixel sits from the old background,
normalised against the icon's own strongest pixel.  Recompositing over a new
background then needs neither ink nor a solved separately:

    p' = a * ink + (1 - a) * newbg
       = p + (1 - a) * (newbg - oldbg)

so a pure background pixel lands exactly on the new background, a pure ink pixel
is untouched, and the antialiased edge moves smoothly between the two.
"""
import argparse

import lcddata as L

# the screen colours the icons were drawn against; 0x8618 is the TX/RX pair's own
OLD_BACKGROUNDS = (0x7E19, 0x7E17, 0x8618,   # what the icons shipped with
                   0x08a4)                   # this fork, before the darkening


def recolour(vals, newbg):
    counts = {}
    for v in vals:
        counts[v] = counts.get(v, 0) + 1
    oldbg = max(OLD_BACKGROUNDS, key=lambda c: counts.get(c, 0))
    if counts.get(oldbg, 0) == 0:
        return None, None, 0

    ob = L.to_rgb(oldbg)

    def dist(c):
        r, g, b = L.to_rgb(c)
        return abs(r - ob[0]) + abs(g - ob[1]) + abs(b - ob[2])

    ref = max((dist(v) for v in set(vals)), default=0)
    if ref == 0:
        return [L.to565(*newbg)] * len(vals), oldbg, counts[oldbg]

    out = []
    for v in vals:
        a = min(1.0, dist(v) / float(ref))
        r, g, b = L.to_rgb(v)
        out.append(L.to565(r + (1 - a) * (newbg[0] - ob[0]),
                           g + (1 - a) * (newbg[1] - ob[1]),
                           b + (1 - a) * (newbg[2] - ob[2])))
    return out, oldbg, counts[oldbg]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--bg', default='#0D1520', help='new screen background, #rrggbb')
    ap.add_argument('--write', action='store_true')
    args = ap.parse_args()

    newbg = L.parse_hex_colour(args.bg)
    src = L.read(L.SRC)
    sym = L.defines()
    print('new background %s -> 0x%04X' % (args.bg, L.to565(*newbg)))

    edits, skipped = [], []
    for name in L.icon_names(src):
        span, vals = L.array(src, name, 'uint16', sym)
        new, oldbg, n = recolour(vals, newbg)
        if new is None:
            skipped.append(name)
            continue
        assert len(new) == len(vals)
        print('  %-12s %5d px, old bg 0x%04X on %5.1f%%'
              % (name, len(vals), oldbg, 100.0 * n / len(vals)))
        edits.append((span, L.emit(name, new, 'uint16')))

    if skipped:
        print('  left alone (no known background): ' + ', '.join(skipped))
    if not args.write:
        print('\npreview only; pass --write to rewrite the arrays')
        return

    for (a, b), replacement in sorted(edits, reverse=True):
        src = src[:a] + replacement + src[b:]
    open(L.SRC, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(src)

    after = L.read(L.SRC)
    for name in L.icon_names(after):
        _, vals = L.array(after, name, 'uint16', sym)
        assert vals is not None, name
    print('\nrewrote %d arrays in %s, all re-read cleanly' % (len(edits), L.SRC))


if __name__ == '__main__':
    main()
