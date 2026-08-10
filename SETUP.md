# Trashavania — Dev Environment Setup

Handoff notes for continuing this build on a new machine. Read `brief.md`
first for the game design; this file is just toolchain + current status.

## 1. Install the toolchain

Tested on macOS (Apple Silicon) with Homebrew. Adjust package manager for
other platforms — cc65 and fceux are both cross-platform.

```sh
brew install cc65 fceux
```

This gives you `cc65`/`ca65`/`ld65` (compiler/assembler/linker) and `fceux`
(secondary emulator, has Lua scripting/debugger).

**If `brew install fceux` fails to link with a `qtbase`/`qt` conflict**: that
means another Homebrew formula (`qt`) is already installed and owns some of
the same symlinks. Don't force-link over it — just re-run
`brew install fceux`; the formula itself still builds and installs even if
the *link* step warns, and a second `install` call usually succeeds cleanly
once the dependency is already in the Cellar. This conflict is
machine-specific (whatever else is installed here), not something in this
repo.

### Mesen2 (primary emulator)

Not in Homebrew — download it directly. This is the emulator the project
brief wants for accuracy and sprite-limit/timing diagnostics.

```sh
mkdir -p tools
cd tools
curl -sL -o mesen2.zip \
  https://github.com/SourMesen/Mesen2/releases/latest/download/Mesen_2.1.1_macOS_ARM64_AppleSilicon.zip
  # For Intel Macs, use the _macOS_x64_Intel.zip asset instead.
  # Check https://github.com/SourMesen/Mesen2/releases for the current version
  # if 2.1.1 is no longer "latest".
unzip -o -q mesen2.zip -d Mesen2
unzip -o -q Mesen2/Mesen.app.zip -d Mesen2
rm Mesen2/Mesen.app.zip mesen2.zip
xattr -dr com.apple.quarantine Mesen2/Mesen.app
cd ..
```

`tools/Mesen2/` is gitignored (399MB, not project source) — this step has
to be redone on every machine.

## 2. Project layout

```
src/
  crt0.s          startup/interrupt code — the one hand-written 6502 asm file.
                  iNES header (mapper 2/UxROM, CHR RAM), power-on sequence,
                  RAM clear, NMI handler. Exports _oam, _nmi_flag.
  nes-uxrom.cfg   custom ld65 linker script. cc65's stock nes.cfg only
                  targets NROM (mapper 0); the brief wants Mapper 2/UxROM
                  with CHR RAM, so this defines 4x16KB PRG banks (bank 3
                  fixed at $C000, banks 0-2 reserved/unused so far for
                  future room content) and the internal RAM layout
                  (zero page, page-aligned OAM buffer at $0200, BSS, cc65
                  parameter stack).
  nes.h           hardware register addresses + PPUCTRL/PPUMASK/pad bit
                  masks, as volatile pointers. No assembly needed for
                  register access.
  ppu.c / pad.c   small support lib: vblank wait, OAM DMA, PPU address/
                  write helpers, controller polling.
  main.c          Foundation-phase demo: loads a palette + one CHR tile,
                  draws a background floor row, shows one sprite, moves it
                  with the D-pad.
Makefile          `make` builds build/trashavania.nes. `make run` (Mesen2)
                  / `make run-fceux`.
tools/smoke_test.lua   fceux Lua script for scripted testing (see below —
                  currently doesn't fully work in this environment).
```

`build/` and `tools/Mesen2/` are gitignored. Everything else is checked in.

## 3. Build

```sh
make          # -> build/trashavania.nes
make run      # opens it in Mesen2
make run-fceux
```

## 4. Current status

The build pipeline itself is verified working:

- `make` produces a 65,552-byte ROM (16-byte header + 4×16KB PRG, no CHR
  ROM data — matches expected size exactly).
- Header bytes are correct: `4E 45 53 1A 04 00 21 00 ...` → iNES magic, 4
  PRG banks, 0 CHR banks (CHR RAM), mapper 2 low nibble + vertical
  mirroring bit, mapper high nibble 0.
- fceux's own ROM loader independently parses this as `Mapper #: 2, Mapper
  name: UNROM`, `CHR ROM: 0 x 8KiB = 0 KiB`, and powers on without error.
- The linker map (`build/trashavania.map`) shows clean segment placement —
  no overlaps, nothing overflowing the fixed PRG bank, all symbols
  (`_main`, `_oam`, `_nmi_flag`, cc65 runtime helpers) resolved.

## 5. Known issue: blank screen (unresolved)

Running `make run` opens Mesen2 successfully (process launches, renders),
but the screen shows **blank instead of the expected floor row + moving
sprite**. Not yet root-caused. Ruled out so far:

- Not a link/layout bug (map file is clean, see above).
- Not a build-reproducibility issue (rebuilds are deterministic, same
  output).
- Header/mapper detection is independently confirmed correct by fceux.

Not yet checked / worth trying next, roughly in order of suspicion:

1. **PPU VRAM (nametable/attribute table/CHR RAM) is never explicitly
   cleared.** `crt0.s` only zeroes CPU-side internal RAM ($0000-$07FF);
   the *PPU's* VRAM (nametables, attribute tables, CHR RAM) is separate
   memory reachable only through `$2006`/`$2007`, and its power-on content
   is emulator/hardware-dependent. `main.c` writes the floor row and one
   CHR tile explicitly, but never touches the attribute table — if that
   comes up as unexpected garbage it *shouldn't* cause a fully blank
   screen (the explicitly-written tile should still show), but it's the
   most likely thing to check first with a debugger/memory viewer.
2. **Floor row placement.** `draw_floor()` in `main.c` writes to nametable
   row 27 of 30 — very close to the bottom edge. Mesen's log reported a
   224-line video mode (NES visible area is 224 of 240 lines, i.e. 8 lines
   cropped top and bottom). Row 27 spans pixel Y 216–223, which should
   still be just inside the visible window, but this is a guess, not
   confirmed — try moving the floor row to the middle of the screen (e.g.
   row 14) to rule out an off-by-one in the crop math.
3. **Rule out "no rendering happened at all" vs "specific content
   missing"**: temporarily set the backdrop color (`palette[0]`) to
   something loud like white, or fill the *entire* nametable with tile 1
   instead of just one row, to see whether *anything* changes on screen.
   If the screen stays truly blank even then, the bug is in the
   PPUCTRL/PPUMASK enable sequence or vblank timing in `main.c`, not the
   content-drawing calls.
4. Use Mesen2's built-in debugger (Tools → Debugger, or the PPU viewer) to
   directly inspect nametable/pattern table/OAM contents at runtime rather
   than guessing from source — much faster than the automated-screenshot
   approach below.

### Automated screenshot attempt (didn't pan out here — may work for you)

`tools/smoke_test.lua` runs 120 frames then calls `gui.savescreenshot(...)`
via `fceux --loadlua tools/smoke_test.lua build/trashavania.nes`. On this
machine it failed with Qt threading errors ("Cannot move to target
thread", "loading two sets of Qt binaries") — almost certainly caused by
this machine's pre-existing `qt` Homebrew formula clashing with `fceux`'s
`qtbase` dependency at the dynamic-linker level (see the fceux install
note above). This is machine-specific and may just work on a clean
machine:

```sh
fceux --sound 0 --loadlua tools/smoke_test.lua build/trashavania.nes
ls build/smoke_test.png
```

If it works, you get an actual screenshot to inspect instead of relying on
a human looking at the Mesen window.

## 6. Next steps

1. Root-cause and fix the blank screen (see above).
2. Add actual "Hello World" (or in-universe: "Castle Refuse has awakened.")
   text rendering — requested as a clearer, more unambiguous visual smoke
   test than the current floor+sprite demo. Needs a small hand-authored
   bitmap font (a handful of 8x8 CHR tiles for the letters used) written
   into CHR RAM alongside the existing solid-block tile, then written into
   the nametable as a string.
3. Once both are confirmed visually working, continue with the brief's
   "Core play" milestone (section 18): movement/jumping, tile collision,
   swipe attack, damage/death/restart.
