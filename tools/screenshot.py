"""Render the idle screen off the firmware's own data, as a PNG.

    python tools/screenshot.py out.png --labels SET,ROO,MOD --values 45.0,45,HEAT

Reads the fonts, the icon bitmaps and the colour constants straight out of
ER-TFT024-3_4-Wire_SPI.c and LCD_TSTAT.h and replays the drawing calls
MenuIdle_display() makes, through the same primitives (disp_ch, disp_str,
disp_icon, disp_null_icon).  Nothing about the appearance is written down twice:
change a colour constant or regenerate a font and the picture follows.

It is a drawing check, not an emulator -- it does not run the menu task, so the
row contents are given on the command line.
"""
import argparse

from PIL import Image

import lcddata as L

W, H = 240, 320


class Screen:
    def __init__(self):
        self.src = L.read(L.SRC)
        self.k = L.defines()
        self.im = Image.new('RGB', (W, H), (0, 0, 0))
        self.icons = {}
        for name in L.icon_names(self.src):
            _, vals = L.array(self.src, name, 'uint16', self.k)
            self.icons[name] = vals
        self.fonts = {}
        for name in ('chlibsmall', 'chlib', 'char_16_24'):
            _, vals = L.array(self.src, name, 'uint8', self.k)
            self.fonts[name] = vals

    def put(self, x, y, colour):
        if 0 <= x < W and 0 <= y < H:
            self.im.putpixel((x, y), L.to_rgb(colour))

    def clear(self, colour):
        for y in range(H):
            for x in range(W):
                self.put(x, y, colour)

    def null_icon(self, cp, pp, x, y, bg):
        for j in range(pp):
            for i in range(cp):
                self.put(x + i, y + j, bg)

    def icon(self, cp, pp, name, x, y):
        data = self.icons[name]
        for j in range(min(cp * pp, len(data))):
            self.put(x + j % cp, y + j // cp, data[j])

    def _glyph(self, table, nbytes, w, h, index, x, y, fg, bg):
        base = index * nbytes
        for j in range(nbytes):
            b = table[base + j]
            for i in range(8):
                idx = j * 8 + i
                self.put(x + idx % w, y + idx // w, fg if (b >> i) & 1 else bg)

    def ch(self, form, x, y, c, fg, bg):
        if form == 0:
            idx = 10 if c == '-' else 11 if c == ' ' else ord(c) - 48
            self._glyph(self.fonts['chlib'], 576, 48, 96, idx, x, y, fg, bg)
        else:
            self._glyph(self.fonts['chlibsmall'], 108, 24, 36, ord(c) - 32, x, y, fg, bg)

    def text(self, form, x, y, s, fg, bg):
        for n, c in enumerate(s):
            self.ch(form, x + n * (31 if form == 0 else 23), y, c, fg, bg)

    def text_16_24(self, x, y, s, fg, bg):
        for n, c in enumerate(s):
            self._glyph(self.fonts['char_16_24'], 48, 16, 24, ord(c) - 32, x + n * 16, y, fg, bg)

    def tangle(self, x, y):
        k = self.k
        self.icon(8, 8, 'leftup', x, y)
        self.null_icon(113, 1, x + 6, y + 2, k['TSTAT8_MENU_COLOR'])
        self.null_icon(2, 28, x + 2, y + 8, k['TSTAT8_MENU_COLOR'])
        self.icon(8, 8, 'leftdown', x, y + 34)
        self.null_icon(113, 1, x + 6, y + 39, k['TSTAT8_MENU_COLOR'])
        self.icon(8, 8, 'rightdown', x + 115, y + 34)
        self.icon(8, 8, 'rightup', x + 115, y)
        self.null_icon(1, 28, x + 120, y + 8, k['TSTAT8_MENU_COLOR'])
        self.null_icon(115, 2, x + 5, y + 40, k['TANGLE_COLOR'])
        self.null_icon(115, 2, x + 5, y, k['TANGLE_COLOR'])
        self.null_icon(2, 32, x, y + 6, k['TANGLE_COLOR'])
        self.null_icon(2, 32, x + 121, y + 6, k['TANGLE_COLOR'])

    def page_marks(self, current, count):
        k = self.k
        if 'PAGE_MARK_XPOS' not in k:
            return
        self.null_icon(k['PAGE_MARK_XDOTS'], k['PAGE_MARK_STRIP_YDOTS'],
                       k['PAGE_MARK_XPOS'], k['PAGE_MARK_YPOS'], k['TSTAT8_BACK_COLOR'])
        if count < 2:
            return
        dim = k.get('PAGE_MARK_DIM_COLOR', k.get('BUTTON_DARK_COLOR'))
        for i in range(count):
            self.null_icon(k['PAGE_MARK_XDOTS'], k['PAGE_MARK_YDOTS'], k['PAGE_MARK_XPOS'],
                           k['PAGE_MARK_YPOS'] + i * k['PAGE_MARK_PITCH'],
                           k['SCH_COLOR'] if i == current else dim)


def render(s, labels, values, top, unit, page, pages, clock, selected):
    k = s.k
    BG, CH, SCHC = k['TSTAT8_BACK_COLOR'], k['TSTAT8_CH_COLOR'], k['SCH_COLOR']
    M2, HL = k['TSTAT8_MENU_COLOR2'], k['TSTAT8_BACK_COLOR1']
    s.clear(BG)

    s.icon(13, 26, 'cmnct_send', 0, 0)
    s.icon(13, 26, 'cmnct_rcv', 13, 0)
    s.icon(26, 26, 'wifi_4', 210, 0)

    whole, _, frac = top.partition('.')
    whole = whole.rjust(2)
    s.ch(0, k['FIRST_CH_POS'], k['THERM_METER_POS'], whole[0], CH, BG)
    s.ch(0, k['SECOND_CH_POS'], k['THERM_METER_POS'], whole[1], CH, BG)
    if frac:
        s.null_icon(8, 8, k['SECOND_CH_POS'] + 52, 85, CH)
        s.ch(0, k['THIRD_CH_POS'], k['THERM_METER_POS'], frac[0], CH, BG)
    s.text_16_24(k['UNIT_POS'] - 8, 26, unit, CH, BG)

    rows = (k['SETPOINT_POS'], k['FAN_MODE_POS'], k['SYS_MODE_POS'])
    for y in rows:
        s.tangle(102, y - 3)
    for i, y in enumerate(rows):
        back = HL if selected == i + 1 else BG
        s.text(1, k['SCH_XPOS'], y, labels[i][:3].ljust(3), SCHC, back)
        s.text(1, k['SCH_XPOS'] + 96, y, values[i][:5].ljust(5), SCHC, M2)
    s.page_marks(page, pages)

    s.null_icon(240, 36, 0, k['TIME_POS'], M2)
    s.text(1, 30, k['TIME_POS'], clock[:9], CH, M2)

    s.icon(k['ICON_XDOTS'], k['ICON_YDOTS'], 'sunicon', k['FIRST_ICON_POS'], k['ICON_POS'])
    s.icon(k['ICON_XDOTS'], k['ICON_YDOTS'], 'athome', k['SECOND_ICON_POS'], k['ICON_POS'])
    s.icon(k['ICON_XDOTS'], k['ICON_YDOTS'], 'heaticon', k['THIRD_ICON_POS'], k['ICON_POS'])
    s.icon(k['FANBLADE_XDOTS'], k['FANBLADE_YDOTS'], 'fanbladeA',
           k['FOURTH_ICON_POS'], k['ICON_POS'])
    s.icon(k['FANSPEED_XDOTS'], k['FANSPEED_YDOTS'], 'fanspeed2a',
           k['FIFTH_ICON_POS'], k['ICON_POS'])
    return s.im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--labels', default='SET,ROO,MOD')
    ap.add_argument('--values', default='45.0,45,HEAT')
    ap.add_argument('--top', default='21.5')
    ap.add_argument('--unit', default='C')
    ap.add_argument('--page', type=int, default=0)
    ap.add_argument('--pages', type=int, default=3)
    ap.add_argument('--selected', type=int, default=0)
    ap.add_argument('--clock', default='09-20 14:')
    ap.add_argument('--scale', type=int, default=2)
    args = ap.parse_args()

    im = render(Screen(), args.labels.split(','), args.values.split(','), args.top,
                args.unit, args.page, args.pages, args.clock, args.selected)
    if args.scale > 1:
        im = im.resize((W * args.scale, H * args.scale), Image.NEAREST)
    im.save(args.out)
    print('wrote', args.out)


if __name__ == '__main__':
    main()
