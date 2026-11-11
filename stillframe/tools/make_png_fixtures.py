#!/usr/bin/env python3
"""Writes PNG files in many colour types plus .raw (RGBA bytes as Pillow decodes them) for decoder tests."""
import random, sys
from PIL import Image

out = sys.argv[1]
random.seed(1)


def rnd(w, h, mode):
    im = Image.new(mode, (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            if mode == 'RGB': px[x, y] = tuple(random.randrange(256) for _ in range(3))
            elif mode == 'RGBA': px[x, y] = tuple(random.randrange(256) for _ in range(4))
            elif mode == 'L': px[x, y] = random.randrange(256)
            elif mode == 'LA': px[x, y] = (random.randrange(256), random.randrange(256))
            elif mode == 'P': px[x, y] = random.randrange(16)
    return im


def save(name, im, **kw):
    im.save(f'{out}/{name}.png', **kw)
    open(f'{out}/{name}.raw', 'wb').write(Image.open(f'{out}/{name}.png').convert('RGBA').tobytes())


for mode in ['RGB', 'RGBA', 'L', 'LA']:
    save(mode, rnd(37, 23, mode))
im = rnd(37, 23, 'P')
im.putpalette([random.randrange(256) for _ in range(48)] + [0] * 720)
save('P', im, transparency=3)
g = Image.new('L', (16, 4)); g.putdata([x * 16 for x in range(16)] * 4)
save('L4', g, bits=4)
save('G16', Image.new('I;16', (5, 5), 40000))
# Pillow can't write interlaced PNGs: flag a normal one as interlaced (and fix the header CRC) so the decoder's refusal is testable.
import struct, zlib
data = bytearray(open(f'{out}/RGB.png', 'rb').read())
data[28] = 1
data[29:33] = struct.pack('>I', zlib.crc32(bytes(data[12:29])))
open(f'{out}/interlaced.png', 'wb').write(bytes(data))
