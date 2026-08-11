#!/usr/bin/env python3
"""genassets.py -- Trashavania asset pipeline.

All game art and level data is authored HERE as ASCII art and compiled into
C arrays (src/assets.c / src/assets.h). The generator also renders PNG
previews into build/preview/ so art can be reviewed without an emulator.

Conventions:
  - Sprite art charmap: '.'=transparent 'K'=color1 'G'=color2 'W'=color3
  - Background art charmap: '.'=color0 '1'=color1 '2'=color2 '3'=color3
  - Font glyphs: '.'=background '#'=color3
  - Metatiles are 16x16 px (2x2 CHR tiles) with one palette + one collision
    class each. Rooms are 16x15 grids of metatiles (one full NES screen,
    no scrolling). A metatile is exactly one attribute-table quadrant, so
    room attribute bytes are computed here at build time.

Run: python3 tools/genassets.py   (from repo root; also run by `make`)
"""

import os
import struct
import sys
import zlib

OUT_C = "src/assets.c"
OUT_H = "src/assets.h"
PREVIEW_DIR = "build/preview"

# ---------------------------------------------------------------------------
# NES master palette (classic FCEUX RGB approximation, for previews only)

NES_RGB = {
    0x00: (0x7C, 0x7C, 0x7C), 0x01: (0x00, 0x00, 0xFC), 0x02: (0x00, 0x00, 0xBC),
    0x03: (0x44, 0x28, 0xBC), 0x04: (0x94, 0x00, 0x84), 0x05: (0xA8, 0x00, 0x20),
    0x06: (0xA8, 0x10, 0x00), 0x07: (0x88, 0x14, 0x00), 0x08: (0x50, 0x30, 0x00),
    0x09: (0x00, 0x78, 0x00), 0x0A: (0x00, 0x68, 0x00), 0x0B: (0x00, 0x58, 0x00),
    0x0C: (0x00, 0x40, 0x58), 0x0F: (0x08, 0x08, 0x08),
    0x10: (0xBC, 0xBC, 0xBC), 0x11: (0x00, 0x78, 0xF8), 0x12: (0x00, 0x58, 0xF8),
    0x13: (0x68, 0x44, 0xFC), 0x14: (0xD8, 0x00, 0xCC), 0x15: (0xE4, 0x00, 0x58),
    0x16: (0xF8, 0x38, 0x00), 0x17: (0xE4, 0x5C, 0x10), 0x18: (0xAC, 0x7C, 0x00),
    0x19: (0x00, 0xB8, 0x00), 0x1A: (0x00, 0xA8, 0x00), 0x1B: (0x00, 0xA8, 0x44),
    0x1C: (0x00, 0x88, 0x88),
    0x20: (0xF8, 0xF8, 0xF8), 0x21: (0x3C, 0xBC, 0xFC), 0x22: (0x68, 0x88, 0xFC),
    0x23: (0x98, 0x78, 0xF8), 0x24: (0xF8, 0x78, 0xF8), 0x25: (0xF8, 0x58, 0x98),
    0x26: (0xF8, 0x78, 0x58), 0x27: (0xFC, 0xA0, 0x44), 0x28: (0xF8, 0xB8, 0x00),
    0x29: (0xB8, 0xF8, 0x18), 0x2A: (0x58, 0xD8, 0x54), 0x2B: (0x58, 0xF8, 0x98),
    0x2C: (0x00, 0xE8, 0xD8), 0x2D: (0x78, 0x78, 0x78),
    0x30: (0xFC, 0xFC, 0xFC), 0x31: (0xA4, 0xE4, 0xFC), 0x32: (0xB8, 0xB8, 0xF8),
    0x33: (0xD8, 0xB8, 0xF8), 0x34: (0xF8, 0xB8, 0xF8), 0x35: (0xF8, 0xA4, 0xC0),
    0x36: (0xF0, 0xD0, 0xB0), 0x37: (0xFC, 0xE0, 0xA8), 0x38: (0xF8, 0xD8, 0x78),
    0x39: (0xD8, 0xF8, 0x78), 0x3A: (0xB8, 0xF8, 0xB8), 0x3B: (0xB8, 0xF8, 0xD8),
    0x3C: (0x00, 0xFC, 0xFC), 0x3D: (0xF8, 0xD8, 0xF8),
}

# ---------------------------------------------------------------------------
# Game palettes (NES color indices). Background color (shared) is $0F black.

BG_PALETTES = [
    [0x0F, 0x02, 0x00, 0x10],  # 0: stone masonry (dk blue shadow, gray, lt gray)
    [0x0F, 0x0B, 0x19, 0x29],  # 1: sickly trash greens
    [0x0F, 0x03, 0x13, 0x30],  # 2: purples + white (moon, windows, text)
    [0x0F, 0x07, 0x17, 0x28],  # 3: warm food/HUD (browns, orange, yellow)
]
SPR_PALETTES = [
    [0x0F, 0x0F, 0x00, 0x30],  # 0: Jimothy (black outline/mask, gray fur, white face)
    [0x0F, 0x0F, 0x19, 0x30],  # 1: enemies (black, trash green, bone white)
    [0x0F, 0x0F, 0x2D, 0x28],  # 2: boss/effects (black, gray, yellow glow)
    [0x0F, 0x0F, 0x16, 0x28],  # 3: pickups/gnome (black, red, yellow)
]

SPRITE_CHARMAP = {'.': 0, 'K': 1, 'G': 2, 'W': 3}
BG_CHARMAP = {'.': 0, '1': 1, '2': 2, '3': 3}
FONT_CHARMAP = {'.': 0, '#': 3}

# ---------------------------------------------------------------------------
# Tiny PNG writer (previews)


def write_png(path, w, h, pixels):
    raw = b''.join(
        b'\x00' + b''.join(struct.pack('BBB', *px) for px in row)
        for row in pixels)

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw)))
        f.write(chunk(b'IEND', b''))


# ---------------------------------------------------------------------------
# Art parsing helpers


def parse_art(art, charmap, width, height):
    """Multiline ASCII art -> grid of color indices [row][col]."""
    rows = [line for line in art.strip('\n').split('\n')]
    assert len(rows) == height, "art is %d rows, want %d:\n%s" % (len(rows), height, art)
    grid = []
    for r, line in enumerate(rows):
        assert len(line) == width, "row %d is %d cols, want %d: %r" % (r, len(line), width, line)
        grid.append([charmap[ch] for ch in line])
    return grid


def encode_tile(grid, ox, oy):
    """Extract the 8x8 tile at (ox,oy) from a color grid -> 16 CHR bytes."""
    data = bytearray()
    for plane in (0, 1):
        for y in range(8):
            b = 0
            for x in range(8):
                if (grid[oy + y][ox + x] >> plane) & 1:
                    b |= 0x80 >> x
            data.append(b)
    return bytes(data)


class TileBank:
    """One 4KB pattern table built from 8x8 tiles, deduplicated."""

    def __init__(self, name):
        self.name = name
        self.tiles = [bytes(16)]        # tile 0 is always blank
        self.index = {bytes(16): 0}

    def add(self, tile_bytes, dedupe=True):
        if dedupe and tile_bytes in self.index:
            return self.index[tile_bytes]
        idx = len(self.tiles)
        assert idx < 256, "%s pattern table overflow" % self.name
        self.tiles.append(tile_bytes)
        self.index.setdefault(tile_bytes, idx)
        return idx

    def add_grid(self, grid, w_tiles, h_tiles, dedupe=True):
        """Add all tiles of a multi-tile image, row-major. Returns first index
        and asserts the rest are consecutive (needed for metasprites)."""
        first = None
        for ty in range(h_tiles):
            for tx in range(w_tiles):
                idx = self.add(encode_tile(grid, tx * 8, ty * 8), dedupe=dedupe)
                if first is None:
                    first = idx
        return first

    def data(self):
        return b''.join(self.tiles)


spr_bank = TileBank("sprites")
bg_bank = TileBank("background")

# ===========================================================================
# FONT -- 8x8 glyphs, drawn in color 3. Placed at fixed indices in the
# background pattern table: ' '=0(blank), 'A'-'Z'=1..26, '0'-'9'=27..36,
# then punctuation (see FONT_PUNCT). Text rendering in C relies on this.
# ===========================================================================

FONT = {
    'A': ".###.\n#...#\n#...#\n#####\n#...#\n#...#\n#...#",
    'B': "####.\n#...#\n#...#\n####.\n#...#\n#...#\n####.",
    'C': ".####\n#....\n#....\n#....\n#....\n#....\n.####",
    'D': "####.\n#...#\n#...#\n#...#\n#...#\n#...#\n####.",
    'E': "#####\n#....\n#....\n####.\n#....\n#....\n#####",
    'F': "#####\n#....\n#....\n####.\n#....\n#....\n#....",
    'G': ".####\n#....\n#....\n#.###\n#...#\n#...#\n.###.",
    'H': "#...#\n#...#\n#...#\n#####\n#...#\n#...#\n#...#",
    'I': "#####\n..#..\n..#..\n..#..\n..#..\n..#..\n#####",
    'J': "..###\n...#.\n...#.\n...#.\n...#.\n#..#.\n.##..",
    'K': "#...#\n#..#.\n#.#..\n##...\n#.#..\n#..#.\n#...#",
    'L': "#....\n#....\n#....\n#....\n#....\n#....\n#####",
    'M': "#...#\n##.##\n#.#.#\n#.#.#\n#...#\n#...#\n#...#",
    'N': "#...#\n##..#\n#.#.#\n#..##\n#...#\n#...#\n#...#",
    'O': ".###.\n#...#\n#...#\n#...#\n#...#\n#...#\n.###.",
    'P': "####.\n#...#\n#...#\n####.\n#....\n#....\n#....",
    'Q': ".###.\n#...#\n#...#\n#...#\n#.#.#\n#..#.\n.##.#",
    'R': "####.\n#...#\n#...#\n####.\n#.#..\n#..#.\n#...#",
    'S': ".####\n#....\n#....\n.###.\n....#\n....#\n####.",
    'T': "#####\n..#..\n..#..\n..#..\n..#..\n..#..\n..#..",
    'U': "#...#\n#...#\n#...#\n#...#\n#...#\n#...#\n.###.",
    'V': "#...#\n#...#\n#...#\n#...#\n#...#\n.#.#.\n..#..",
    'W': "#...#\n#...#\n#...#\n#.#.#\n#.#.#\n##.##\n#...#",
    'X': "#...#\n#...#\n.#.#.\n..#..\n.#.#.\n#...#\n#...#",
    'Y': "#...#\n#...#\n.#.#.\n..#..\n..#..\n..#..\n..#..",
    'Z': "#####\n....#\n...#.\n..#..\n.#...\n#....\n#####",
    '0': ".###.\n#..##\n#.#.#\n#.#.#\n##..#\n#...#\n.###.",
    '1': "..#..\n.##..\n..#..\n..#..\n..#..\n..#..\n#####",
    '2': ".###.\n#...#\n....#\n..##.\n.#...\n#....\n#####",
    '3': "####.\n....#\n....#\n.###.\n....#\n....#\n####.",
    '4': "...#.\n..##.\n.#.#.\n#..#.\n#####\n...#.\n...#.",
    '5': "#####\n#....\n####.\n....#\n....#\n#...#\n.###.",
    '6': ".###.\n#....\n#....\n####.\n#...#\n#...#\n.###.",
    '7': "#####\n....#\n...#.\n..#..\n..#..\n..#..\n..#..",
    '8': ".###.\n#...#\n#...#\n.###.\n#...#\n#...#\n.###.",
    '9': ".###.\n#...#\n#...#\n.####\n....#\n....#\n.###.",
}
FONT_PUNCT = {
    '.': ".....\n.....\n.....\n.....\n.....\n.##..\n.##..",
    '!': "..#..\n..#..\n..#..\n..#..\n..#..\n.....\n..#..",
    '-': ".....\n.....\n.....\n.###.\n.....\n.....\n.....",
    "'": "..#..\n..#..\n.#...\n.....\n.....\n.....\n.....",
    ',': ".....\n.....\n.....\n.....\n.....\n.##..\n..#..",
    ':': ".....\n.##..\n.##..\n.....\n.##..\n.##..\n.....",
}


def add_font():
    order = [chr(c) for c in range(ord('A'), ord('Z') + 1)] + \
            [chr(c) for c in range(ord('0'), ord('9') + 1)] + \
            list(FONT_PUNCT.keys())
    for i, ch in enumerate(order):
        art = FONT.get(ch) or FONT_PUNCT[ch]
        # pad each 5-wide row to 8, add blank 8th row
        rows = [(line + '...')[:8] for line in art.split('\n')] + ['........']
        grid = parse_art('\n'.join(rows), FONT_CHARMAP, 8, 8)
        idx = bg_bank.add(encode_tile(grid, 0, 0), dedupe=False)
        assert idx == 1 + i, "font tile %r landed at %d, want %d" % (ch, idx, 1 + i)
    return order


FONT_ORDER = add_font()

# ---------------------------------------------------------------------------
# HUD tiles (8x8, background bank, drawn on nametable rows 0-1 which the
# room attribute generator forces to palette 3 = warm)

HUD_TILES = {}


def hud_tile(name, art):
    grid = parse_art(art, BG_CHARMAP, 8, 8)
    HUD_TILES[name] = bg_bank.add(encode_tile(grid, 0, 0))


hud_tile("PIP_FULL", """\
..1111..
.133331.
13333331
13333331
13333331
13333331
.133331.
..1111..
""")

hud_tile("PIP_EMPTY", """\
..1111..
.1....1.
1......1
1......1
1......1
1......1
.1....1.
..1111..
""")

hud_tile("SNACK_ICON", """\
.111111.
12333321
13222231
13222231
12333321
.111111.
........
........
""")

hud_tile("ICON_CAP", """\
.11111..
1232321.
1323231.
1232321.
.11111..
........
........
........
""")

hud_tile("ICON_TOMATO", """\
...11...
..1221..
.123321.
12333321
12333321
.123321.
..1221..
...11...
""")

# ===========================================================================
# JIMOTHY -- 16x24 px frames (2x3 tiles), facing RIGHT. The engine flips
# horizontally for left-facing. Assembled from shared head/body/leg pieces
# so the frames stay consistent.
# ===========================================================================

# Head + hunched torso, rows 0..15 (16 rows). Ringed tail curls up behind
# (left, cols 0-3, alternating gray/black rings); body cols 4-13.
JIM_TOP = """\
...KK.....KK....
..KGGK...KGGK...
..KGGGK.KGGGK...
..KGGGGGGGGGK...
.KGGGGGGGGGGGK..
.KKKKKKKKKKKGGK.
.KWWKKWWKKKKGGK.
.KKWKKKWKKKGGGGK
.KKKKKKKKKWWWWKK
..KGGGGGGKWWWWWK
.KKGGGGGGGKKKKK.
KGGKGGGGGGGGK...
KGGKGGGGGGGGK...
KKKKGGGGGGGGK...
KGGKGGGGGGGGK...
KGGKGGGGWWGGK...
"""

JIM_LEGS_IDLE = """\
KKKKGGGGWWWGK...
.KKGGGGGWWWGK...
..KGGGGGGGGK....
..KGGKKKGGK.....
..KGGK.KGGK.....
..KGGK.KGGK.....
.KKGGK.KGGKK....
.KKKK...KKKK....
"""

JIM_LEGS_WALK1 = """\
KKKKGGGGWWWGK...
.KKGGGGGWWWGK...
..KGGGGGGGGK....
..KGKKGGKKGK....
.KGGK...KGGK....
KGGK.....KGGK...
KGGK......KGGK..
KKKK......KKKK..
"""

JIM_LEGS_WALK2 = """\
KKKKGGGGWWWGK...
.KKGGGGGWWWGK...
..KGGGGGGGGK....
..KGGKKKGGK.....
...KGGKGGK......
...KGGKGGK......
...KGGKGGK......
...KKK.KKK......
"""

JIM_LEGS_JUMP = """\
KKKKGGGGWWWGK...
.KKGGGGGWWWGK...
..KGGGGGGGGK....
.KGGKKKKKGGK....
KGGK....KGGK....
KGGK.....KGGK...
.KK.......KK....
................
"""

JIM_IDLE = JIM_TOP + JIM_LEGS_IDLE
JIM_WALK1 = JIM_TOP + JIM_LEGS_WALK1
JIM_WALK2 = JIM_TOP + JIM_LEGS_WALK2
JIM_JUMP = JIM_TOP + JIM_LEGS_JUMP

# Crouch -- 16x16 raccoon scrunch (signature pose per the brief)
JIM_CROUCH = """\
...KK.....KK....
..KGGK...KGGK...
..KGGGK.KGGGK...
..KGGGGGGGGGK...
.KGGGGGGGGGGGK..
.KKKKKKKKKKKGGK.
.KWWKKWWKKKKGGK.
.KKWKKKWKKKGGGGK
.KKKKKKKKKWWWWKK
.KGGGGGGGKWWWWWK
KKGGGGGGGGKKKKK.
KGGKGGGGGGGGGK..
KKKKGGGGGGGGGK..
KGGKGGGGGGGGGK..
.KKGGKKKKKKGGK..
.KKKK......KKKK.
"""

# Swipe -- 16x24, arm thrust forward; pair with the slash effect sprite
JIM_SWIPE = """\
...KK.....KK....
..KGGK...KGGK...
..KGGGK.KGGGK...
..KGGGGGGGGGK...
.KGGGGGGGGGGGK..
.KKKKKKKKKKKGGK.
.KWWKKWWKKKKGGK.
.KKWKKKWKKKGGGGK
.KKKKKKKKKWWWWKK
..KGGGGGGKWWWWWK
.KKGGGGGGGKKKKK.
KGGKGGGGGGGGKK..
KGGKGGGGGGKGGGKK
KKKKGGGGGGGGKKK.
KGGKGGGGGGGGK...
KGGKGGGGWWGGK...
""" + JIM_LEGS_IDLE

# Hurt -- knocked back, squint eyes, legs flail
JIM_HURT = JIM_TOP.replace(".KWWKKWWKKKKGGK.", ".KWKWKWKWKKKGGK.") + """\
.KKKGGGGWWWGGK..
KGGGGGGGWWWGGGK.
KGKGGGGGGGGKGK..
KK.KGGKKKKGGK.KK
...KGK...KGK....
..KGK.....KGK...
..KK.......KK...
................
"""

# Victory -- arms raised holding the moment (and soon, the Golden Garbage)
JIM_VICTORY = """\
.KK..........KK.
KGGK.KK..KK.KGGK
KGGKKGGKKGGKKGGK
.KGGKGGGGGGKGGK.
..KGGGGGGGGGGK..
..KKKKKKKKKKKK..
..KWWKKWWKKKKK..
..KKWKKKWKKKKK..
..KKKKKKKKKKK...
..KGGWWWWGGGK...
..KGGWWWWGGGK...
..KGGGGGGGGGK...
.KGGGGGGGGGGGK..
.KGGGGWWGGGGGK..
.KGGGGWWWGGGGK..
..KGGGGGGGGGK...
..KGGGGGGGGGK...
..KGGGGGGGGGK...
...KGGGGGGGK....
...KGGKKKGGK....
...KGGK.KGGK....
...KGGK.KGGK....
..KKGGK.KGGKK...
..KKKK...KKKK...
"""


def add_sprite(name, art, w, h):
    grid = parse_art(art, SPRITE_CHARMAP, w, h)
    first = spr_bank.add_grid(grid, w // 8, h // 8, dedupe=False)
    return (name, first, w // 8, h // 8, grid)


# ---------------------------------------------------------------------------
# Enemies (palette 1: black/green/white unless noted)

# Trash Bat: a garbage bag twisted into a bat. Two wing frames.
BAT1 = """\
................
..K..........K..
.KGK........KGK.
KGGGK......KGGGK
KGGGGK.KK.KGGGGK
KGGGGKKGGKKGGGGK
.KGGKGWGGWGKGGK.
..KKGGGGGGGGKK..
....KGKGGKGK....
.....KKGGKK.....
.......KK.......
................
................
................
................
................
"""

BAT2 = """\
................
................
................
................
.......KK.......
..KK..KGGK..KK..
.KGGKKGGGGKKGGK.
KGGGGKGWWGKGGGGK
KGGGGGGGGGGGGGGK
.KKGGKGGGGKGGKK.
...KKKGGGGKKK...
.....KGKKGK.....
......K..K......
................
................
................
"""

# Skeletal Alley Cat: bones + cutlery. Two pace frames. White bones.
CAT1 = """\
................
................
.KK.............
KWWK......KKKK..
KWWWK....KWWWWK.
.KWKWKKKKKWKKWK.
..KWWWWWWWWKKWK.
...KWKKKKWK.KK..
...KWK..KWK.....
..KWK...KWK.....
..KWK....KWK....
.KWK......KWK...
.KK........KK...
................
................
................
"""

CAT2 = """\
................
................
.KK.............
KWWK......KKKK..
KWWWK....KWWWWK.
.KWKWKKKKKWKKWK.
..KWWWWWWWWKKWK.
...KWKKKKWK.KK..
....KWKKWK......
....KWKKWK......
....KWKKWK......
....KWK.KWK.....
....KK...KK.....
................
................
................
"""

# Haunted Garden Gnome (palette 3: black/red/yellow + white beard via...
# only 3 colors: red hat, yellow face/beard, black outline)
GNOME1 = """\
......KK........
.....KGGK.......
....KGGGGK......
...KGGGGGGK.....
..KGGGGGGGGK....
..KKKKKKKKKK....
..KWWWWWWWWK....
..KWKWWWWKWK....
..KWWWWWWWWK....
...KWWWWWWK.....
..KWWWWWWWWK....
.KWWKWWWWKWWK...
.KWKKWWWWKKWK...
.KWK.KWWK.KWK...
..KK..KKKK.KK...
................
"""

GNOME2 = """\
......KK........
.....KGGK.......
....KGGGGK......
...KGGGGGGK.....
..KGGGGGGGGK....
..KKKKKKKKKK....
..KWWWWWWWWK....
..KWKWWWWKWK....
..KWWWWWWWWK....
...KWWWWWWK...W.
..KWWWWWWWWK.W..
.KWWKWWWWKWWW...
.KWKKWWWWKKWK...
.KWK.KWWK.KWK...
..KK..KKKK.KK...
................
"""

# Breakable container: knotted trash bag (8x16 would be thin; 16x16)
BAG = """\
......KKK.......
.....KGGGK......
......KGK.......
.....KGGGK......
....KGGGGGK.....
...KGGGGGGGK....
..KGGGWGGGGGK...
..KGGGGGGGGGK...
.KGGGGGGWGGGGK..
.KGGWGGGGGGGGK..
.KGGGGGGGGGGGK..
.KGGGGGWGGGGGK..
..KGGGGGGGGGK...
...KKGGGGGKK....
.....KKKKK......
................
"""

# Candle on a stubby holder (8x16, palette 2: gray + yellow flame)
CANDLE = """\
........
...WW...
...WW...
..GWWG..
..GGGG..
...KK...
..KGGK..
..KGGK..
..KGGK..
..KGGK..
..KGGK..
..KGGK..
.KGGGGK.
.KKKKKK.
........
........
"""

# ---------------------------------------------------------------------------
# Projectiles + effects

CAP = """\
.KKK....
KGWGK...
KWGWK...
KGWGK...
.KKK....
........
........
........
"""

TOMATO = """\
..KK....
.KGKK...
KGWGGK..
KGGGGK..
KGGGGK..
.KGGK...
..KK....
........
"""

SPLAT = """\
................
....G...G.......
..G.KGGGK..G....
.KGGGGGGGGGK....
GGGGGGGGGGGGGG..
................
................
................
"""

SHARD = """\
..K.....
.KWK....
KWKWK...
.KWK....
..K.....
........
........
........
"""

SLASH = """\
.....KK.
...KKWK.
..KWWWK.
.KWWKK..
KWWK....
KWK.....
KWK.....
.K......
"""

POOF1 = """\
...KK...
..KGGK..
.KGWWGK.
.KGWWGK.
..KGGK..
...KK...
........
........
"""

POOF2 = """\
K..KK..K
.KKGGKK.
.KG..GK.
KG....GK
.KG..GK.
.KKGGKK.
K..KK..K
........
"""

# ---------------------------------------------------------------------------
# Pickups (palette 3: black/red/yellow)

SNACK = """\
.KKKKK..
KGWGGGK.
KWGGGGK.
KGGGGGK.
.KKKKK..
........
........
........
"""

BIGSNACK = """\
..KKKKKKKKKK....
.KGGGGGGGGGGK...
KGWWGGGGGGGGGK..
KGWGGGGGGGGGGK..
KGGGGGGGGGGGGK..
KGGGGWWWWGGGGK..
KGGGGWKKWGGGGK..
KGGGGWWWWGGGGK..
KGGGGGGGGGGGGK..
.KGGGGGGGGGGK...
..KKKKKKKKKK....
................
................
................
................
................
"""

BURRITO = """\
..KKKKK.
.KWWWWKK
KWGGGGWK
KWGGGGWK
.KWWWWK.
..KKKK..
........
........
"""

GOLD = """\
....KKKKKKKK....
...KWWWWWWWWK...
..KWGGGGGGGGWK..
.KKKKKKKKKKKKKK.
.KGWGGWGGWGGWGK.
.KGGWGGWGGWGGGK.
.KGWGGWGGWGGWGK.
.KKKKKKKKKKKKKK.
..KGGGGGGGGGGK..
..KGGGGGGGGGGK..
..KGGGGGGGGGGK..
..KKKKKKKKKKKK..
................
................
................
................
"""

MYSTERY = """\
.KKKKKK.
KGGGGGGK
KGWWWWGK
KGWKKWGK
KGGGKWGK
KGGWWGGK
KGGGGGGK
.KKKKKK.
"""

SPRITES = [
    add_sprite("JIM_IDLE", JIM_IDLE, 16, 24),
    add_sprite("JIM_WALK1", JIM_WALK1, 16, 24),
    add_sprite("JIM_WALK2", JIM_WALK2, 16, 24),
    add_sprite("JIM_JUMP", JIM_JUMP, 16, 24),
    add_sprite("JIM_CROUCH", JIM_CROUCH, 16, 16),
    add_sprite("JIM_SWIPE", JIM_SWIPE, 16, 24),
    add_sprite("JIM_HURT", JIM_HURT, 16, 24),
    add_sprite("JIM_VICTORY", JIM_VICTORY, 16, 24),
    add_sprite("BAT1", BAT1, 16, 16),
    add_sprite("BAT2", BAT2, 16, 16),
    add_sprite("CAT1", CAT1, 16, 16),
    add_sprite("CAT2", CAT2, 16, 16),
    add_sprite("GNOME1", GNOME1, 16, 16),
    add_sprite("GNOME2", GNOME2, 16, 16),
    add_sprite("BAG", BAG, 16, 16),
    add_sprite("CANDLE", CANDLE, 8, 16),
    add_sprite("CAP", CAP, 8, 8),
    add_sprite("TOMATO", TOMATO, 8, 8),
    add_sprite("SPLAT", SPLAT, 16, 8),
    add_sprite("SHARD", SHARD, 8, 8),
    add_sprite("SLASH", SLASH, 8, 8),
    add_sprite("POOF1", POOF1, 8, 8),
    add_sprite("POOF2", POOF2, 8, 8),
    add_sprite("SNACK", SNACK, 8, 8),
    add_sprite("BIGSNACK", BIGSNACK, 16, 16),
    add_sprite("BURRITO", BURRITO, 8, 8),
    add_sprite("GOLD", GOLD, 16, 16),
    add_sprite("MYSTERY", MYSTERY, 8, 8),
]

# ===========================================================================
# METATILES -- 16x16 px background building blocks.
# collision: 0=none 1=solid 2=platform(top only) 3=hazard 4=door
# ===========================================================================

COLL_NAMES = {0: "NONE", 1: "SOLID", 2: "PLATFORM", 3: "HAZARD", 4: "DOOR"}

METATILES = {}          # name -> (id, palette, collision)
MT_ORDER = []


def metatile(name, palette, collision, art):
    grid = parse_art(art, BG_CHARMAP, 16, 16)
    tiles = [bg_bank.add(encode_tile(grid, tx * 8, ty * 8))
             for ty in range(2) for tx in range(2)]
    mid = len(MT_ORDER)
    METATILES[name] = (mid, palette, collision, tiles, grid)
    MT_ORDER.append(name)
    return mid


# id 0 must be empty sky (all color 0)
metatile("sky", 2, 0, "\n".join(["." * 16] * 16))

metatile("brick", 0, 1, """\
2222222212222222
2111211121112111
2222222222222122
1211121112111211
2222122222222222
2111211121112111
2222222222212222
1211121112111211
2222222212222222
2111211121112111
2222222222221222
1211121112111211
2221222222222222
2111211121112111
2222222222222212
1211121112111211
""")

metatile("dirt", 1, 1, """\
2222222222222222
2122212221222122
1111111111111111
1112111121111211
1111111111111111
1211112111121111
1111111111111111
1111211111211112
1111111111111111
1121111211111121
1111111111111111
1112111121111211
1111111111111111
1211111211121111
1111111111111111
1111111111111111
""")

metatile("platform", 0, 2, """\
3333333333333333
2222222222222222
1212121212121212
1111111111111111
................
................
................
................
................
................
................
................
................
................
................
................
""")

metatile("moon", 2, 0, """\
.....333333.....
...3333333333...
..333333333333..
.33333333333333.
.33333233333333.
3333332233333333
3333333333333333
3333333333333333
3333333333333333
3333333233333333
.33333322333333.
.33333333333333.
..333333333333..
...3333333333...
.....333333.....
................
""")

metatile("fence", 0, 0, """\
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
2222222222222222
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
2222222222222222
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
.2.2.2.2.2.2.2.2
""")

metatile("trashpile", 1, 1, """\
......22........
...222332.......
..23322222......
.2232222322.....
2222332222222...
2322222233222...
2222322222222222
2233222232222332
2222223322232222
2322222222332222
2223322222222232
2222222332222222
2332223222233222
2222322222222222
2223222332222322
2222222222222222
""")

metatile("doortop", 2, 4, """\
....11111111....
..111111111111..
.11221111112211.
.12211111111221.
1122111111112211
1221111111111221
1221111111111221
1122111111112211
1221111111111221
1221111111111221
1221111111111221
1122111111112211
1221111111111221
1221111111111221
1221111111111221
1221111111111221
""")

metatile("doorbottom", 2, 4, """\
1221111111111221
1122111111112211
1221111111111221
1221111111111221
1221111111111221
1221111111112211
1221111111111221
1221111111111221
1221111111111221
1122111111112211
1221111111111221
1221111111111221
1221111111111221
1221111111112211
1221111111111221
1221111111111221
""")

metatile("window", 2, 0, """\
......1111......
....11333311....
...1333333331...
..133333333331..
..133333333331..
..113333333311..
..131133331131..
..133311113331..
..133333333331..
..113333333311..
..133333333331..
..133311113331..
..131133331131..
..133333333311..
..133333333331..
..111111111111..
""")

# ===========================================================================
# ROOMS -- 16x15 metatile grids + spawns.
# Spawn types (shared constants with the engine, see assets.h):
# ===========================================================================

SPAWN_TYPES = [
    "SP_BAT", "SP_CAT", "SP_GNOME", "SP_BAG", "SP_CANDLE",
    "SP_SNACK", "SP_BIGSNACK", "SP_BURRITO", "SP_GOLD", "SP_WEAPON_CAP",
    "SP_WEAPON_TOMATO",
]

ROOM_LEGEND = {
    '.': "sky",
    'B': "brick",
    '#': "dirt",
    'p': "platform",
    'M': "moon",
    'f': "fence",
    'T': "trashpile",
    'd': "doortop",
    'D': "doorbottom",
    'W': "window",
}


class Room:
    def __init__(self, name, art, spawns, player_start, exits):
        """art: 15 rows x 16 cols of legend chars.
        spawns: list of (SP_type, mt_x, mt_y).
        player_start: (mt_x, mt_y) -- feet-on-this-tile position.
        exits: dict {'door': next_room_index or 'WIN'}"""
        self.name = name
        rows = art.strip('\n').split('\n')
        assert len(rows) == 15, "%s: %d rows" % (name, len(rows))
        self.map = []
        for r in rows:
            assert len(r) == 16, "%s: bad row %r" % (name, r)
            self.map.extend(METATILES[ROOM_LEGEND[ch]][0] for ch in r)
        self.spawns = spawns
        self.player_start = player_start
        self.exits = exits

    def attributes(self):
        """64 attribute bytes; each metatile is one attribute quadrant."""
        attr = []
        for ay in range(8):
            for ax in range(8):
                def pal(mx, my):
                    if my < 2:
                        return 3        # HUD strip: warm palette, all rooms
                    if mx >= 16 or my >= 15:
                        return 0
                    return METATILES[MT_ORDER[self.map[my * 16 + mx]]][1]
                b = (pal(ax * 2, ay * 2)
                     | (pal(ax * 2 + 1, ay * 2) << 2)
                     | (pal(ax * 2, ay * 2 + 1) << 4)
                     | (pal(ax * 2 + 1, ay * 2 + 1) << 6))
                attr.append(b)
        return attr


ROOMS = [
    Room("Garbage Grove", """\
................
................
................
...M..........BB
..............BB
.............BBB
.............BBB
............BBBB
............BBBB
...........BBBBB
..........BBBBBB
.....TT...BBdBBB
.f..TTTT.fBBDBBB
################
################
""",
         spawns=[("SP_BAT", 6, 5), ("SP_BAG", 3, 12), ("SP_SNACK", 7, 10)],
         player_start=(1, 12),
         exits={'door': 1}),
]

# ===========================================================================
# Emission
# ===========================================================================


def preview_all():
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    def render_grid(grid, palette):
        return [[NES_RGB[palette[v]] for v in row] for row in grid]

    # sprites (on dark background so transparent shows as bg color), 4x scale
    preview_pal = {"BAT": 1, "CAT": 1, "CANDLE": 2, "BAG": 1, "GNOME": 3,
                   "SNACK": 3, "BIGSNACK": 3, "BURRITO": 3, "GOLD": 3,
                   "MYSTERY": 3, "TOMATO": 3, "SPLAT": 3, "SHARD": 1,
                   "POOF": 2, "CAP": 2, "SLASH": 2}
    for name, first, tw, th, grid in SPRITES:
        pal = SPR_PALETTES[0]
        for prefix, p in preview_pal.items():
            if name.startswith(prefix):
                pal = SPR_PALETTES[p]
                break
        px = [[NES_RGB[0x0C] if v == 0 else NES_RGB[pal[v]] for v in row
               for _ in range(4)] for row in grid for _ in range(4)]
        write_png("%s/spr_%s.png" % (PREVIEW_DIR, name.lower()),
                  tw * 8 * 4, th * 8 * 4, px)

    # metatiles
    for name in MT_ORDER:
        mid, palette, coll, tiles, grid = METATILES[name]
        write_png("%s/mt_%s.png" % (PREVIEW_DIR, name),
                  16, 16, render_grid(grid, BG_PALETTES[palette]))

    # rooms
    for i, room in enumerate(ROOMS):
        px = [[None] * 256 for _ in range(240)]
        for my in range(15):
            for mx in range(16):
                name = MT_ORDER[room.map[my * 16 + mx]]
                _, palette, _, _, grid = METATILES[name]
                for y in range(16):
                    for x in range(16):
                        px[my * 16 + y][mx * 16 + x] = \
                            NES_RGB[BG_PALETTES[palette][grid[y][x]]]
        write_png("%s/room%d.png" % (PREVIEW_DIR, i), 256, 240, px)

    # font sheet
    sheet = [[(0, 0, 0)] * (16 * 8) for _ in range((len(FONT_ORDER) // 16 + 1) * 8)]
    for i in range(1, len(FONT_ORDER) + 1):
        tile = bg_bank.tiles[i]
        gx, gy = ((i - 1) % 16) * 8, ((i - 1) // 16) * 8
        for y in range(8):
            for x in range(8):
                v = ((tile[y] >> (7 - x)) & 1) | (((tile[8 + y] >> (7 - x)) & 1) << 1)
                if v:
                    sheet[gy + y][gx + x] = (255, 255, 255)
    write_png("%s/font.png" % PREVIEW_DIR, 16 * 8, len(sheet), sheet)


def c_bytes(data, per_line=16):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + per_line]) + ",")
    return "\n".join(lines)


def emit():
    h = []
    c = []
    h.append("/* Generated by tools/genassets.py -- DO NOT EDIT BY HAND */")
    h.append("#ifndef ASSETS_H")
    h.append("#define ASSETS_H")
    h.append("")
    c.append("/* Generated by tools/genassets.py -- DO NOT EDIT BY HAND */")
    c.append('#include "assets.h"')
    c.append("")

    # CHR banks
    spr_data = spr_bank.data()
    bg_data = bg_bank.data()
    h.append("#define CHR_SPRITES_LEN %d" % len(spr_data))
    h.append("#define CHR_BG_LEN %d" % len(bg_data))
    h.append("extern const unsigned char chr_sprites[CHR_SPRITES_LEN];")
    h.append("extern const unsigned char chr_bg[CHR_BG_LEN];")
    c.append("const unsigned char chr_sprites[CHR_SPRITES_LEN] = {\n%s\n};" % c_bytes(spr_data))
    c.append("const unsigned char chr_bg[CHR_BG_LEN] = {\n%s\n};" % c_bytes(bg_data))

    # palettes
    pal = []
    for p in BG_PALETTES:
        pal.extend(p)
    for p in SPR_PALETTES:
        pal.extend(p)
    h.append("extern const unsigned char game_palette[32];")
    c.append("const unsigned char game_palette[32] = {\n%s\n};" % c_bytes(pal))

    # sprite frame indices
    h.append("")
    for name, first, tw, th, _ in SPRITES:
        h.append("#define SPR_%s %d  /* %dx%d tiles */" % (name, first, tw, th))

    # font mapping info
    h.append("")
    h.append("#define FONT_A 1")
    h.append("#define FONT_0 27")
    punct = list(FONT_PUNCT.keys())
    for i, ch in enumerate(punct):
        cname = {'.': 'PERIOD', '!': 'BANG', '-': 'DASH', "'": 'APOS',
                 ',': 'COMMA', ':': 'COLON'}[ch]
        h.append("#define FONT_%s %d" % (cname, 37 + i))

    # HUD tiles
    h.append("")
    for name, idx in HUD_TILES.items():
        h.append("#define TILE_%s %d" % (name, idx))

    # metatile tables
    h.append("")
    h.append("#define MT_COUNT %d" % len(MT_ORDER))
    for name in MT_ORDER:
        mid = METATILES[name][0]
        h.append("#define MT_%s %d" % (name.upper(), mid))
    for part, sel in (("tl", 0), ("tr", 1), ("bl", 2), ("br", 3)):
        arr = bytes(METATILES[n][3][sel] for n in MT_ORDER)
        h.append("extern const unsigned char mt_%s[MT_COUNT];" % part)
        c.append("const unsigned char mt_%s[MT_COUNT] = {\n%s\n};" % (part, c_bytes(arr)))
    coll = bytes(METATILES[n][2] for n in MT_ORDER)
    h.append("extern const unsigned char mt_coll[MT_COUNT];")
    c.append("const unsigned char mt_coll[MT_COUNT] = {\n%s\n};" % c_bytes(coll))

    # collision classes
    h.append("")
    for v, n in COLL_NAMES.items():
        h.append("#define COLL_%s %d" % (n, v))

    # spawn type constants
    h.append("")
    for i, n in enumerate(SPAWN_TYPES):
        h.append("#define %s %d" % (n, i + 1))

    # rooms
    h.append("")
    h.append("#define ROOM_COUNT %d" % len(ROOMS))
    h.append("typedef struct {")
    h.append("    const unsigned char *map;    /* 240 metatile ids */")
    h.append("    const unsigned char *attr;   /* 64 attribute bytes */")
    h.append("    const unsigned char *spawns; /* type,x,y triples, 0xFF end */")
    h.append("    unsigned char start_x, start_y; /* player start, metatile coords */")
    h.append("    unsigned char door_dest;     /* room index, 0xFF = victory */")
    h.append("} RoomDef;")
    h.append("extern const RoomDef rooms[ROOM_COUNT];")
    for i, room in enumerate(ROOMS):
        c.append("/* Room %d: %s */" % (i, room.name))
        c.append("static const unsigned char room%d_map[240] = {\n%s\n};"
                 % (i, c_bytes(bytes(room.map))))
        c.append("static const unsigned char room%d_attr[64] = {\n%s\n};"
                 % (i, c_bytes(bytes(room.attributes()))))
        sp = []
        for (t, x, y) in room.spawns:
            sp.append(t)
            sp.append(str(x))
            sp.append(str(y))
        sp.append("0xFF")
        c.append("static const unsigned char room%d_spawns[] = { %s };"
                 % (i, ", ".join(sp)))
    entries = []
    for i, room in enumerate(ROOMS):
        dest = room.exits.get('door')
        dest = "0xFF" if dest == 'WIN' else str(dest)
        entries.append("    { room%d_map, room%d_attr, room%d_spawns, %d, %d, %s }"
                       % (i, i, i, room.player_start[0], room.player_start[1], dest))
    c.append("const RoomDef rooms[ROOM_COUNT] = {\n%s\n};" % ",\n".join(entries))

    h.append("")
    h.append("#endif /* ASSETS_H */")

    with open(OUT_H, 'w') as f:
        f.write("\n".join(h) + "\n")
    with open(OUT_C, 'w') as f:
        f.write("\n".join(c) + "\n")

    print("assets: %d sprite tiles (%dB), %d bg tiles (%dB), %d metatiles, %d rooms"
          % (len(spr_bank.tiles), len(spr_data), len(bg_bank.tiles), len(bg_data),
             len(MT_ORDER), len(ROOMS)))


if __name__ == "__main__":
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    preview_all()
    emit()
