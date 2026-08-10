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

**2026-08-10 update:** `src/crt0.s` — the one hand-written 6502 file
(iNES header, reset handler, NMI handler, `_oam`/`_nmi_flag` storage) —
had never actually been committed. The repo's `.gitignore` had a blanket
`*.s` rule meant for cc65's generated intermediate assembly (which lands
in `build/`, already covered by the `build/` ignore line), but it also
matched the hand-written source file, silently keeping it out of every
commit. It doesn't exist in git history at all. `.gitignore` has been
fixed (removed the redundant `*.o`/`*.s`/`*.map` lines) and `crt0.s` has
been rewritten from scratch to match `nes-uxrom.cfg`'s segments/exports.
Also fixed: `tools/smoke_test.lua` had a hardcoded path from a different
machine/user (`/Users/rickyb/...`), now a relative `build/smoke_test.png`.

The build pipeline is verified working:

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

**Runtime behavior verified via headless fceux + Lua (no GUI/screenshot
needed — see "Automated memory probing" below):** disassembling the built
ROM confirms the reset routine, NMI vector/handler, and the
`PPUCTRL = 0x80` / `PPUMASK = 0x1E` writes in `main()` all match the
source exactly. More importantly, a scripted test that holds the D-pad
Down button for 60 emulated frames shows `player_y` (and the OAM shadow
copy) increase from 120 to 180 — i.e. **the full loop is provably
running every frame**: NMI fires, `wait_vblank()` returns, `ppu_update()`
DMAs the sprite to real PPU OAM, and `pad_poll()` reads input correctly.
(A naive single-point memory read of `_nmi_flag` came back 0 and looked
alarming at first — that was just a sampling race against
`wait_vblank()`, which clears the flag almost immediately after NMI sets
it; it wasn't a real bug. Don't rely on a single `_nmi_flag` sample as a
liveness check — use an input-driven side effect like this instead.)

## 5. Known issue: screen appears blank/grey (unresolved, but game logic is not the suspect anymore)

Given the above, the CPU-side game logic (reset, NMI, main loop, PPU
register writes, OAM DMA, input) is confirmed correct by direct
disassembly and by observed behavior (sprite position responding to
input). The visual "blank"/"grey" report is therefore more likely a
**Mesen2 display/window issue, or the specific PPU *content* being
genuinely hard to see**, not a dead program. Next session should:

1. **Take an actual screenshot first** — this session couldn't
   (`screencapture` had no Screen Recording permission for the terminal
   app in use). Get that permission sorted (System Settings → Privacy &
   Security → Screen Recording → enable your terminal app, then fully
   quit/reopen it) before re-investigating this.
2. With a screenshot in hand, check the obvious things: is the Mesen2
   window actually unpaused / rendering (not stuck on a "load" dialog or
   paused state)? Does toggling anything change what's shown?
3. **Floor row placement** — still an open, unconfirmed suspicion.
   `draw_floor()` in `main.c` writes to nametable row 27 of 30, very
   close to the bottom edge. Mesen's log reports a 224-line video mode
   (NES visible area is 224 of 240 lines, 8 lines cropped top+bottom).
   Row 27 spans pixel Y 216–223, which should be just inside the visible
   window, but try moving it to row 14 (screen middle) to rule out an
   off-by-one in the crop math.
4. **PPU VRAM (nametable/attribute table/CHR RAM) is never explicitly
   cleared** by `crt0.s` — only CPU-side RAM is. `main.c` writes the
   floor row and one CHR tile explicitly but never touches the attribute
   table; if that's garbage on power-on it affects *color*, not whether
   the tile shows at all, but worth checking with the PPU viewer.
5. Use Mesen2's built-in debugger (Tools → Debugger, PPU viewer / nametable
   viewer) to directly inspect live nametable/pattern table/OAM/palette
   contents — much faster than guessing from source once you can actually
   see the Mesen window.

### Automated memory probing (this is what actually worked — use this, not screenshots, for headless verification)

`fceux --loadlua <script> build/trashavania.nes` runs fine even in this
sandboxed/headless-ish environment as long as the Lua script only uses
`memory.readbyte()` / `joypad.set()` / `io.write()` — i.e. avoid
`gui.savescreenshot()`, which hits Qt threading errors on this machine
("Cannot move to target thread") likely from the same `qt`/`qtbase`
Homebrew clash noted in the install steps above. Reading CPU RAM directly
(the `_oam` shadow buffer at `$0200`, `player_x`/`player_y` at `$0300`
(see `build/trashavania.map` for current addresses — they can shift
between builds), injecting `joypad.set(1, {down=true})` and checking for
movement) is a reliable, screenshot-free way to confirm the game loop is
alive. This is the same technique that untangled the false "NMI never
fires" alarm above — prefer it over a screenshot when you just need a
pass/fail liveness check rather than a visual layout check.

```sh
fceux --sound 0 --loadlua your_probe.lua build/trashavania.nes
```

`tools/smoke_test.lua` (the `gui.savescreenshot()`-based script) is left
as-is for a machine where the Qt conflict doesn't bite — try it first on
a clean machine before reaching for the memory-probe approach above.

## 6. Next steps

1. Get an actual screenshot (fix Screen Recording permission for the
   terminal app first) and root-cause the visual blank/grey report — see
   section 5. This is very likely a small, specific issue now (display
   setting or floor-row-position), not a fundamentally broken program.
2. Add actual "Hello World" (or in-universe: "Castle Refuse has awakened.")
   text rendering — requested as a clearer, more unambiguous visual smoke
   test than the current floor+sprite demo. Needs a small hand-authored
   bitmap font (a handful of 8x8 CHR tiles for the letters used) written
   into CHR RAM alongside the existing solid-block tile, then written into
   the nametable as a string.
3. Once both are confirmed visually working, continue with the brief's
   "Core play" milestone (section 18): movement/jumping, tile collision,
   swipe attack, damage/death/restart.
