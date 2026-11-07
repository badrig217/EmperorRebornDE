"""Build the Emperor Spice large-font pack.

Renders the game's two text fonts ("Small" and "Standard") at twice their original size from a free
TrueType font (DejaVu Sans) and writes them as two extra game archives:

  fonts/UI.RFH + UI.RFD          FONT\\SMALLHD0.MAP, FONT\\SMALLHD1.MAP, FONT\\STANDARDHD.MAP
  fonts/3DDATA.RFH + 3DDATA.RFD  Textures\\SmallHD0.tga, Textures\\SmallHD1.tga, Textures\\StandardHD.tga

The fonts get new names, so the archives change nothing by themselves. With "Large in-game text"
on, EmperorHooks points the game's font table at the HD names and doubles the row heights.
Nothing from the original game goes into these files.

How the game reads a font sheet (CFont3D, Game.exe 1.09): the .MAP is a UTF-16 list of characters,
the .TGA holds the glyphs in rows of a fixed height (from the font table in Game.exe). The loader
walks each row left to right; a glyph is a run of columns that contain any non-black pixel, glyphs
are separated by fully black columns. The very first run (a thin marker line at the top left) is
skipped, as in the original sheets. Text is drawn additively, so glyphs are white on black.

usage: python build_fontpack.py PATH_TO_DejaVuSans.ttf OUTPUT_DIR
"""
import os, struct, sys, zlib
import numpy as np
from PIL import Image, ImageDraw, ImageFont

SMALL0 = ('!"#$%&\'[]*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ(\\)^_`abcdefghijklmnopqrstuvwxyz{|}~'
          '\xac\xe4\xf6\xfc\xc4\xd6\xdc\xdf\xe0\xe1\xe2\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf2\xf3\xf4\xf5'
          '\xf9\xfa\xfb\xfd\xff\xc0\xc1\xc2\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd2\xd3\xd4\xd5\xd9\xda\xdb'
          '\xdd\xe3\xc3\xe5\xc5\xae\xa9\xf1\xbf\xa1\xd1')
SMALL1 = 'Œœ…\xab\xbb\xa7\xb2\xb0\xa3\xb5'
STANDARD = ('!"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
            '\xac\xe4\xf6\xfc\xc4\xd6\xdc\xdf\xe0\xe1\xe2\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf2\xf3\xf4\xf5'
            '\xf9\xfa\xfb\xfd\xff\xc0\xc1\xc2\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd2\xd3\xd4\xd5\xd9\xda\xdb'
            '\xdd\xe3\xc3\xe5\xc5\xae\xa9\xf1\xbf\xa1\xd1…Œœ\xbb\xab\xa7\xb2\xb0\xa3\xb5')

# name, characters, row height, cap top, cap bottom (rows inside a glyph row), sheet width.
# Row heights are 2x the originals (Small 16 -> 32, Standard 30 -> 60); cap positions are 2x the
# originals' 'H', so the text sits in the row the way the game expects.
FONTS = [
    ('SmallHD0', SMALL0, 32, 6, 23, 512),
    ('SmallHD1', SMALL1, 32, 6, 23, 128),
    ('StandardHD', STANDARD, 60, 14, 43, 512),
]

TIMESTAMP = 0x60000000  # any fixed value keeps the build reproducible; the names are new, nothing to override


def render(ttf, chars, rowh, captop, capbot, texw):
    caph = capbot - captop + 1
    size = next(s for s in range(caph, caph * 3)
                if (lambda b: b[3] - b[1])(ImageFont.truetype(ttf, s).getbbox('H')) >= caph)
    font = ImageFont.truetype(ttf, size)
    base = capbot + 1 - font.getbbox('H')[3]

    def glyph(ch):
        im = Image.new('L', (rowh * 3, rowh), 0)
        ImageDraw.Draw(im).text((rowh, base), ch, font=font, fill=255)
        a = np.asarray(im).copy()
        a[a < 9] = 0                       # below one step of 16-bit colour: would vanish anyway
        cols = np.where(a.any(0))[0]
        a = a[:, cols[0]:cols[-1] + 1]
        a[rowh // 2, ~a.any(0)] = 9        # bridge inner gaps (e.g. '"') so the glyph stays one run
        return a

    sheet = np.zeros((rowh * 32, texw), np.uint8)
    sheet[0:rowh - 1, 0] = 255             # leading marker run, skipped by the loader
    x, y, pos = 2, 0, []
    for ch in chars:
        g = glyph(ch)
        if x + g.shape[1] > texw:
            x, y = 0, y + rowh
        sheet[y:y + rowh, x:x + g.shape[1]] = g
        pos.append(x)
        x += g.shape[1] + 1
    texh = max(texw, 1 << (y + rowh - 1).bit_length())
    sheet = sheet[:texh] if sheet.shape[0] >= texh else np.vstack([sheet, np.zeros((texh - sheet.shape[0], texw), np.uint8)])
    return sheet, pos, size


def scan(sheet, rowh):
    """Emulate the game's glyph scanner; returns the start column of every run."""
    on = sheet > 0
    h, w = on.shape
    runs, y = [], 0
    while y + rowh <= h:
        x = 0
        while x < w:
            if on[y:y + rowh, x].any():
                x0 = x
                while x < w and on[y:y + rowh, x].any():
                    x += 1
                runs.append(x0)
            x += 1
        y += rowh
    return runs


def tga555(sheet):
    v = (sheet.astype(np.uint32) * 31 + 127) // 255
    px = ((v << 10) | (v << 5) | v).astype('<u2')[::-1]   # bottom-up like the original sheets
    h, w = sheet.shape
    return bytes([0, 0, 2]) + bytes(9) + struct.pack('<HH', w, h) + bytes([16, 0]) + px.tobytes()


def write_archive(base, files):
    rfh, rfd = bytearray(), bytearray()
    for name, data in files:
        blob = struct.pack('<I', len(data)) + zlib.compress(data, 9)
        nb = name.encode('latin1') + b'\0'
        rfh += struct.pack('<6I', len(nb), TIMESTAMP, 2, len(blob), len(data), len(rfd)) + nb
        rfd += blob
    open(base + '.RFH', 'wb').write(rfh)
    open(base + '.RFD', 'wb').write(rfd)


def main(ttf, outdir):
    os.makedirs(outdir, exist_ok=True)
    ui, tex = [], []
    for name, chars, rowh, captop, capbot, texw in FONTS:
        sheet, pos, size = render(ttf, chars, rowh, captop, capbot, texw)
        quant = ((sheet.astype(np.uint32) * 31 + 127) // 255).astype(np.uint8)
        runs = scan(quant, rowh)
        assert runs[1:] == pos, f'{name}: glyph scan does not match the layout'
        ui.append((f'FONT\\{name.upper()}.MAP', '﻿'.encode('utf-16-le') + chars.encode('utf-16-le')))
        tex.append((f'Textures\\{name}.tga', tga555(sheet)))
        Image.fromarray(sheet).save(os.path.join(outdir, name + '.png'))
        print(f'{name}: {len(chars)} glyphs, {sheet.shape[1]}x{sheet.shape[0]}, DejaVu Sans {size}px')
    write_archive(os.path.join(outdir, 'UI'), ui)
    write_archive(os.path.join(outdir, '3DDATA'), tex)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
