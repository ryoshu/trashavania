#!/usr/bin/env python3
"""Convert the PPM screen dumps written by tools/probe.lua into PNGs.
Usage: python3 tools/screen2png.py build/*.ppm  (writes .png next to each)"""
import struct
import sys
import zlib


def convert(path):
    with open(path, 'rb') as f:
        assert f.readline().strip() == b'P6'
        w, h = map(int, f.readline().split())
        f.readline()
        data = f.read()
    raw = b''.join(b'\x00' + data[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, d):
        c = struct.pack('>I', len(d)) + tag + d
        return c + struct.pack('>I', zlib.crc32(tag + d) & 0xFFFFFFFF)

    out = path.rsplit('.', 1)[0] + '.png'
    with open(out, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw)))
        f.write(chunk(b'IEND', b''))
    print(out)


for p in sys.argv[1:]:
    convert(p)
