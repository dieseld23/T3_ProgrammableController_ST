"""Rebuild docs/idle-pages.png, the three idle pages the README opens with.

    python tools/idle_pages.py [out.png]

Each page is drawn by screenshot.render() as a T3-OEM draws it, at 2x, and the
three are laid side by side with a label over each.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)                      # screenshot.py reads the firmware by relative path
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import screenshot as SS  # noqa: E402

PAGES = [
    dict(labels=['SETPOINT', 'ROOM TMP', 'MODE'], values=['72', '71.4', 'HEAT'], top=710, unit='F',
         icons=['fan_auto', 'mode_heat', 'wall_up'], rh=64),
    dict(labels=['CO2 PPM', 'VENT', 'LIGHTS'], values=['820', 'OPEN', 'ON'], top=820, unit='PPM',
         icons=['fan_on', 'mode_cool', 'wall_down'], rh=None),
    dict(labels=['SOIL TMP', 'IRRIGATE', 'SLOPE'], values=['68', 'AUTO', '-13'], top=-90, unit='F',
         icons=['fan_off', 'mode_cool_ovr', 'wall_down'], rh=None),
]
GAP, STRIP, SCALE = 14, 20, 2
BACK = (12, 14, 18)


def main():
    out = Image.new('RGB', (3 * SS.W * SCALE + 2 * GAP, STRIP + SS.H * SCALE), BACK)
    draw = ImageDraw.Draw(out)
    font = ImageFont.load_default()
    for i, p in enumerate(PAGES):
        im = SS.render(SS.Screen(), p['labels'], p['values'], p['top'], p['unit'], i, len(PAGES),
                       'Sep 20 | 12:00 PM', 0, p['icons'], p['rh'])
        im = im.convert('RGB').resize((SS.W * SCALE, SS.H * SCALE), Image.NEAREST)
        x = i * (SS.W * SCALE + GAP)
        out.paste(im, (x, STRIP))
        draw.text((x + 3, 5), 'page %d' % (i + 1), fill=(210, 215, 225), font=font)
    dest = sys.argv[1] if len(sys.argv) > 1 else os.path.join('docs', 'idle-pages.png')
    out.save(dest)
    print('wrote', dest)


if __name__ == '__main__':
    main()
