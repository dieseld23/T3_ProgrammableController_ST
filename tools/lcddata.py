"""Shared reader for the LCD tables in ER-TFT024-3_4-Wire_SPI.c.

The arrays are not uniformly written: the four corner pieces used by
draw_tangle() mix hex literals with #define'd colour names, so picking out only
the 0x.... tokens quietly drops six of leftup's sixty four pixels.  Everything
here tokenises on commas and insists every entry resolves, so a shortened array
is an error rather than a corrupted corner.
"""
import re

SRC = 'arm/HARDWARE/LCD/ER-TFT024-3_4-Wire_SPI.c'
HDR = 'arm/HARDWARE/LCD/LCD_TSTAT.h'

HEX = re.compile(r'0[xX][0-9a-fA-F]+')
DEC = re.compile(r'\d+')


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def read(path):
    return open(path, encoding='utf-8', errors='surrogateescape').read()


def defines(hdr=None):
    """#define NAME <number>, including ones written as expressions."""
    hdr = hdr if hdr is not None else read(HDR)
    out = {}
    for m in re.finditer(r'^#define[ \t]+(\w+)[ \t]+([^/\r\n]+)', hdr, re.M):
        name, val = m.group(1), m.group(2).strip()
        if HEX.fullmatch(val):
            out[name] = int(val, 16)
        elif DEC.fullmatch(val):
            out[name] = int(val)
    for _ in range(3):                       # a few refer to each other
        for m in re.finditer(r'^#define[ \t]+(\w+)[ \t]+([^/\r\n]+)', hdr, re.M):
            name, val = m.group(1), m.group(2).strip()
            if name in out or not re.fullmatch(r'[\w \t+\-*/()]+', val):
                continue
            try:
                out[name] = int(eval(val, {'__builtins__': {}}, out))
            except Exception:
                pass
    return out


def values(inner, symbols):
    """Every comma separated entry of an initialiser, hex, decimal or a name."""
    out = []
    for tok in strip_comments(inner).split(','):
        tok = tok.strip()
        if not tok:
            continue
        if HEX.fullmatch(tok):
            out.append(int(tok, 16))
        elif DEC.fullmatch(tok):
            out.append(int(tok))
        elif tok in symbols:
            out.append(symbols[tok])
        else:
            raise SystemExit('unparsable array entry %r' % tok)
    return out


def array(src, name, kind, symbols):
    """(span of the whole declaration, values). kind is 'uint16' or 'uint8'."""
    if kind == 'uint16':
        pat = r'uint16 const ' + name + r'\[\]\s*=[^{]*\{.*?\n\};'
    else:
        pat = r'(?:uint8|unsigned char) const ' + name + r'\[\]\s*=[^{]*\{.*?\n\};'
    m = re.search(pat, src, re.S)
    if not m:
        return None, None
    body = m.group(0)
    return m.span(), values(body[body.index('{') + 1:body.rindex('}')], symbols)


def icon_names(src):
    return re.findall(r'uint16 const (\w+)\[\]\s*=', src)


def emit(name, vals, kind, per_line=12):
    width = 4 if kind == 'uint16' else 2
    decl = ('uint16' if kind == 'uint16' else 'uint8')
    lines = ['%s const %s[] = ' % (decl, name), '{']
    for i in range(0, len(vals), per_line):
        lines.append('\t' + ''.join('0x%0*x,' % (width, v) for v in vals[i:i + per_line]))
    lines.append('};')
    return '\n'.join(lines)


def to_rgb(c):
    return (((c >> 11) & 0x1F) * 255 // 31,
            ((c >> 5) & 0x3F) * 255 // 63,
            (c & 0x1F) * 255 // 31)


def to565(r, g, b):
    r = max(0, min(255, int(round(r))))
    g = max(0, min(255, int(round(g))))
    b = max(0, min(255, int(round(b))))
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def parse_hex_colour(s):
    s = s.lstrip('#')
    return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)
