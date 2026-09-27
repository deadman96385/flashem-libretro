#!/usr/bin/env python3
"""tex.py: decode a GE texture from a RAM dump (GEREPLAY_RAM or VFLASH_RAMDUMP).

  tex.py <ram> <out.png> tx ty w h [px py bpp] [--surface 0x0FFD0000] [--scale n]

The same rules as texel() in src/ge.c: the texture is a region of the tiled
1024-wide 16 bpp surface at (tx, ty) (--surface: where row 0 would be, as
GE.surface; the default is the layout both the kernel and games use); 8 bpp packs two texels and 4 bpp four
into a surface pixel, low bits first; the palette is a row of surface pixels
at (px, py). Without a palette the region is shown as 16 bpp pixels.
"""
import sys
import numpy as np
from PIL import Image

args = [a for a in sys.argv[1:] if not a.startswith('--')]
opts = dict(a[2:].split('=') for a in sys.argv[1:] if a.startswith('--'))
surface = int(opts.get('surface', '0x0FFD0000'), 0)
scale = int(opts.get('scale', '1'))
ram = np.frombuffer(open(args[0], 'rb').read(), dtype=np.uint8)
tx, ty, w, h = (int(v) for v in args[2:6])
bpp = int(args[8]) if len(args) > 8 else 16
px, py = (int(args[6]), int(args[7])) if bpp != 16 else (0, 0)


def addr(x, y):
    return (surface - 0x10000000 + (y >> 3) * 16384 + (x >> 5) * 512 +
            (y & 7) * 64 + (x & 31) * 2)


def pix(x, y):
    a = addr(x, y)
    return ram[a].astype(np.uint32) | ram[a + 1].astype(np.uint32) << 8


def rgb(c):
    return np.stack([(c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3], -1).astype(np.uint8)


u = np.arange(w)[None, :].repeat(h, 0)
v = np.arange(h)[:, None].repeat(w, 1)
if bpp == 16:
    img = rgb(pix(tx + u, ty + v))
else:
    per = 16 // bpp
    word = pix(tx + u // per, ty + v)
    idx = word >> ((u % per) * bpp) & ((1 << bpp) - 1)
    pal = rgb(pix(px + np.arange(1 << bpp), np.full(1 << bpp, py)))
    img = pal[idx]
im = Image.fromarray(img)
if scale > 1:
    im = im.resize((w * scale, h * scale), Image.NEAREST)
im.save(args[1])
