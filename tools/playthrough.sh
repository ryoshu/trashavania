#!/bin/sh
# Full scripted playthrough: power-on -> title -> all 5 rooms -> boss ->
# victory screen. Exercises every mechanic. Zero-page addresses:
#   07 game_state  08 cur_room  0F pixx  10 pixy  16 hp  17 snacks  18 weapon
# BSS: 3FA boss_active  3FB boss_hp  3FC boss_defeated  3FD boss phase
# Boss phase 0x0B = BP_OPEN (weak point vulnerable), 0x04 = BP_DROP.
# The tower climb waits on pixy after each hop (platform_top - 24):
#   cans A8, r11 98, r10 88, r9 78, r7 58, r6 48, r5 38, r3 18
cd "$(dirname "$0")/.." || exit 1
P="$PWD/build"

S="wait:90;shot:$P/p_title.ppm;tap:start;waitmem:08:00;wait:20"

# --- Room 0: Garbage Grove -- jump the trash piles, enter the castle
S="$S;hold:right;wait:50;hold:right,a;wait:20;hold:right;wait:20"
S="$S;hold:right,a;wait:20;hold:right;wait:30;hold:right,a;wait:20;hold:right;wait:60"
S="$S;waitmem:08:01;wait:20;shot:$P/p_room1.ppm;read:hp:16"

# --- Room 1: Recycling Crypt -- hop ONTO the cans (bottle cap pickup),
# snipe the cat from there, jump the glass, door
S="$S;hold:right;wait:34;release;hold:right,a;wait:18;release;wait:20"
S="$S;read:weapon:18;read:snacks:17"
S="$S;hold:up;wait:2;tap:b,up;wait:44;hold:up;wait:2;tap:b,up;wait:44;release;wait:6"
S="$S;shot:$P/p_catdead.ppm"
S="$S;hold:right;wait:26;release;hold:right,a;wait:26;hold:right;wait:44"
S="$S;waitmem:08:02;wait:20;shot:$P/p_room2.ppm;read:hp:16"

# --- Room 2: Tower of Cans -- checkpointed climb. Every jump gets a
# settle wait BEFORE the pixy checkpoint (pixy is crossed mid-air too).
S="$S;hold:right;wait:70;release"
S="$S;hold:right,a;wait:20;hold:right;wait:6;release;wait:16;waitmem:10:A8"   # cans stack
S="$S;hold:left,a;wait:18;hold:left;wait:6;release;wait:16;waitmem:10:98"     # row11
S="$S;hold:left;wait:26;hold:left,a;wait:24;hold:left;wait:6;release;wait:16;waitmem:10:88"  # row10
S="$S;hold:left;wait:24;hold:left,a;wait:22;hold:left;wait:4;release;wait:16;waitmem:10:78"  # row9
S="$S;hold:right;wait:6;release;hold:a,right;wait:22;release;wait:20;waitmem:10:58"          # row7
S="$S;hold:left;wait:8;release;wait:6"                                # snack
S="$S;hold:right;wait:20;hold:right,a;wait:24;hold:right;wait:6;release;wait:16;waitmem:10:48" # row6
S="$S;tap:b;wait:20;hold:right;wait:6;release;wait:8"                 # candle
S="$S;hold:a,up;wait:5;hold:a,up,b;wait:2;hold:up;wait:44;release"    # gnome shot 1
S="$S;hold:a,up;wait:5;hold:a,up,b;wait:2;hold:up;wait:44;release;wait:4"  # shot 2
S="$S;shot:$P/p_tower.ppm"
S="$S;hold:right;wait:16;hold:right,a;wait:22;hold:right;wait:6;release;wait:20;waitmem:10:38" # row5
S="$S;hold:right;wait:12;hold:right,a;wait:24;hold:right;wait:12;release;wait:20" # row3/door
S="$S;hold:right;wait:40"
S="$S;waitmem:08:03;wait:20;shot:$P/p_room3.ppm;read:hp:16"

# --- Room 3: Chapel -- tomato pickup, eat one cat hit and sprint the
# glass during invincibility, swipe the gnome, burrito, door
S="$S;hold:right;wait:44;hold:right,a;wait:24;hold:right;wait:36;release;wait:4"
S="$S;read:weapon:18;read:hp:16"
S="$S;hold:right;wait:30;release;wait:4"
S="$S;tap:b;wait:26;tap:b;wait:26;tap:b;wait:26;shot:$P/p_chapel.ppm"
S="$S;hold:right;wait:60"
S="$S;waitmem:08:04;wait:20;shot:$P/p_boss.ppm;read:hp:16;dump:3FA:12"

# --- Room 4: Count Dumpula. Per cycle: when the lid drops (phase 4) run
# left from under it; when the weak point opens (phase 0x0B) run right to
# the dumpster and swipe. 12 hits needed at ~4 hits per cycle; allow 5.
# Re-approach (hold right) before EVERY swipe, not just at window start:
# a bat clip or garbage hit knocks Jimothy out of range and a fixed-position
# barrage then whiffs the whole cycle. 26-frame spacing clears hit_cool=24.
for i in 1 2 3 4 5 6; do
  S="$S;waitmem:3FD:04;hold:left;wait:34;release"
  S="$S;waitmem:3FD:0B;hold:right;wait:34;release"
  S="$S;hold:right;wait:6;release;tap:b;wait:18"
  S="$S;hold:right;wait:6;release;tap:b;wait:18"
  S="$S;hold:right;wait:6;release;tap:b;wait:18"
  S="$S;hold:right;wait:6;release;tap:b;wait:18"
  S="$S;hold:right;wait:6;release;tap:b;wait:18"
  S="$S;read:bosshp:3FB;read:hp:16"
done
S="$S;waitmem:3FC:01;shot:$P/p_dying.ppm;wait:240"
S="$S;hold:left;wait:30;release;hold:right;wait:24;release;wait:30"
S="$S;read:state:07;shot:$P/p_victory.ppm;wait:10;quit"

PROBE_SCRIPT="$S" timeout 900 fceux --sound 0 --loadlua tools/probe.lua \
  build/trashavania.nes 2>&1 | grep -E "SHOT|VAR|DUMP|DONE|WAITMEM"
python3 tools/screen2png.py build/p_*.ppm >/dev/null 2>&1
