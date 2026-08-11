# Trashavania Dev Log

Running build log for the hackathon ROM. Newest entries at the bottom.

## 2026-08-10 — Session start: plan + baseline

- Read `brief.md`. Target: complete Mapper 2/UxROM NES ROM — title, 5 rooms,
  Jimothy (walk/jump/crouch/swipe), 2 sub-weapons, 3 enemies, Count Dumpula
  boss, death/restart, victory rank, music + SFX.
- Decision: keep the previous session's *verified* infrastructure
  (`crt0.s`, `nes-uxrom.cfg`, `Makefile`, `nes.h`, `ppu.c`, `pad.c`) and
  rewrite all game code + assets from scratch.
- Root-caused the old "blank screen" report by inspection: the demo
  `main.c` set PPUADDR (which corrupts the scroll latch) and never wrote
  PPUSCROLL afterwards, and never cleared the nametable. The new engine
  resets scroll every frame in the vblank path.
- Verified `make` still produces a valid 65,552-byte ROM (iNES header:
  mapper 2, 4×16KB PRG, CHR RAM).
- Mesen2 `--testrunner` screenshot attempt produced no file on first try —
  will debug; fallback is the proven headless-fceux RAM-probe technique
  from SETUP.md.

### Asset pipeline decision

All art and level data is authored as ASCII art inside `tools/genassets.py`
and compiled to C arrays in `src/assets.c/.h`. Reasons: no image-editor
dependency, everything is code-reviewable/diffable, and the generator also
renders PNG previews to `build/preview/` so art can be checked visually
without an emulator in the loop.

## First light: engine core running

- New engine boots: title screen text renders, Garbage Grove (room 0) draws
  with correct scroll, Jimothy walks/jumps with tile collision (verified:
  he stops at the trash pile's solid edge).
- **Bug found + fixed**: boot deadlock — `enter_state()` called `ppu_off()`
  which waited on `nmi_flag` before NMI had ever been enabled. `ppu_off()`
  now tracks an `nmi_on` shadow flag and only waits when NMI is live.
- **Verification workflow that works headless on this machine**:
  `tools/probe.lua` (fceux, callback style) takes scripted input +
  screen dumps via `emu.getscreenpixel` (1.4ms/frame) into PPM, converted
  by `tools/screen2png.py`. Key gotchas: fceux's cwd is NOT the repo root
  (use absolute paths), and a Lua error inside the frame callback kills the
  script silently (the emulator keeps running -> looks like a hang).
  Mesen2 `--testrunner` doesn't work on this machine (exit 255 / hang).
- Art batch 2 done: bat/cat/gnome enemies, bag/candle containers, all
  pickups, projectiles, HUD tiles. Jimothy got his signature ringed tail.
