/* main.c -- Trashavania: state machine + fixed-step main loop.
   Frame order (brief section 16): input -> player -> entities -> collision
   -> animation -> build OAM; PPU commits happen at the top of the next
   frame inside vblank (OAM DMA, buffered VRAM writes, scroll reset). */
#include "game.h"

#pragma bss-name (push, "ZEROPAGE")
unsigned char frame_cnt;
unsigned char oam_idx;
unsigned char pad, pad_prev, pad_new;
unsigned char game_state;
unsigned char cur_room;
unsigned int px, py;
int pvy;
unsigned char pixx, pixy;
unsigned char on_ground, facing_left, crouching;
unsigned char jump_buf, coyote;
unsigned char hp, snacks, weapon;
unsigned char inv_timer, attack_timer, knock_timer, knock_dir;
unsigned char anim_timer, anim_frame;
#pragma bss-name (pop)

#define PPUCTRL_GAME (PPUCTRL_NMI_ENABLE | PPUCTRL_BG_PT_1000)

unsigned char victory_pending;
static unsigned int play_sec;
static unsigned char sec_frames;

/* ------------------------------------------------------------------ */

static void new_game(void) {
    cur_room = 0;
    hp = MAX_HP;
    snacks = 0;
    weapon = 0;
    gold_count = 0;
    victory_pending = 0;
    play_sec = 0;
    sec_frames = 0;
}

static void load_room(void) {
    ppu_off();
    clear_nametable();
    load_palette();
    draw_room(cur_room);
    player_init(rooms[cur_room].start_x, rooms[cur_room].start_y);
    spawn_room_entities(rooms[cur_room].spawns);
    if (cur_room == ROOM_COUNT - 1) {
        boss_init();
        audio_song(SONG_BOSS);
    } else {
        boss_active = 0;
        audio_song(SONG_CASTLE);
    }
    vbuf_reset();
    hud_dirty = 1;
    ppu_on();
}

static void hud_update(void) {
    unsigned char i;
    unsigned char row[8];
    if (!hud_dirty) return;
    hud_dirty = 0;
    for (i = 0; i < MAX_HP; ++i) {
        row[i] = (i < hp) ? TILE_PIP_FULL : TILE_PIP_EMPTY;
    }
    vbuf_run(2, 1, row, 8);
    row[0] = TILE_SNACK_ICON;
    row[1] = FONT_0 + snacks / 10;
    row[2] = FONT_0 + snacks % 10;
    vbuf_run(12, 1, row, 3);
    if (weapon) {
        vbuf_tile(17, 1, (weapon == 1) ? TILE_ICON_CAP : TILE_ICON_TOMATO);
    }
}

void enter_state(unsigned char st) {
    game_state = st;
    switch (st) {
    case ST_TITLE: {
        unsigned char r, tc;
        audio_song(SONG_TITLE);
        ppu_off();
        clear_nametable();
        text_screen_palette();
        /* retint BG palette 0 for the portrait: gray outline, medium-gray
           fur, white face (restored by load_palette on room load) */
        ppu_set_addr(0x3F01);
        PPUDATA = 0x00; PPUDATA = 0x2D; PPUDATA = 0x30;
        draw_text(10, 4, "TRASHAVANIA");
        draw_text(3, 6, "THE ADVENTURES OF JIMOTHY");
        /* hunched hero portrait, 4x6 bg tiles, centered */
        for (r = 0; r < TITLE_JIM_H; ++r) {
            ppu_set_addr(0x2000 + (10 + r) * 32 + 14);
            for (tc = 0; tc < TITLE_JIM_W; ++tc) {
                PPUDATA = TILE_TITLE_JIM + r * TITLE_JIM_W + tc;
            }
        }
        /* portrait area -> BG palette 0 (raccoon grays); the rest of the
           text screen stays palette 2 */
        ppu_set_addr(0x23D3); PPUDATA = 0x00; PPUDATA = 0x00;
        ppu_set_addr(0x23DB); PPUDATA = 0x00; PPUDATA = 0x00;
        draw_text(4, 18, "IT WAS A MISERABLE NIGHT");
        draw_text(7, 19, "TO HAVE A CURSE.");
        draw_text(10, 22, "PRESS START");
        ppu_on();
        break;
    }
    case ST_GAME:
        load_room();
        break;
    case ST_DEATH:
        audio_song(SONG_DEATH);
        ppu_off();
        clear_nametable();
        text_screen_palette();
        draw_text(6, 10, "JIMOTHY HAS EXPIRED.");
        draw_text(2, 12, "THE TRASH REMAINS UNCLAIMED.");
        draw_text(8, 18, "PRESS START");
        ppu_on();
        break;
    case ST_VICTORY: {
        unsigned char score, m;
        unsigned char buf[5];
        audio_song(SONG_VICTORY);
        ppu_off();
        clear_nametable();
        text_screen_palette();
        draw_text(2, 6, "THE GOLDEN GARBAGE IS YOURS!");
        draw_text(4, 9, "JIMOTHY HAS NO MASTER.");
        draw_text(4, 10, "JIMOTHY HAS SNACKS.");

        m = (unsigned char)(play_sec / 60);
        if (m > 9) m = 9;
        draw_text(8, 13, "TIME    :");
        buf[0] = FONT_0 + m;
        ppu_set_addr(0x2000 + 13 * 32 + 15);
        PPUDATA = buf[0];
        PPUDATA = FONT_COLON;
        PPUDATA = FONT_0 + (unsigned char)((play_sec % 60) / 10);
        PPUDATA = FONT_0 + (unsigned char)(play_sec % 10);
        draw_text(8, 14, "HEALTH");
        ppu_set_addr(0x2000 + 14 * 32 + 15);
        PPUDATA = FONT_0 + hp;
        draw_text(8, 15, "GOLD");
        ppu_set_addr(0x2000 + 15 * 32 + 15);
        PPUDATA = FONT_0 + gold_count;

        score = hp + (gold_count << 2);
        if (play_sec < 240) score += 6;
        else if (play_sec < 420) score += 3;
        draw_text(6, 18, "RACCOON RANK:");
        if (score >= 16)      draw_text(6, 19, "TRASH PANDA SUPREME");
        else if (score >= 11) draw_text(6, 19, "DUMPSTER DUKE");
        else if (score >= 6)  draw_text(6, 19, "ALLEY APPRENTICE");
        else                  draw_text(6, 19, "SOGGY BUT TRIUMPHANT");
        draw_text(10, 23, "PRESS START");
        ppu_on();
        break;
    }
    }
}

/* ------------------------------------------------------------------ */

static void check_door(void) {
    unsigned char dest;
    if (coll_at(pixx + 8, pixy + 12) != COLL_DOOR) return;
    dest = rooms[cur_room].door_dest;
    audio_sfx(SFX_DOOR);
    if (dest >= ROOM_COUNT) {
        enter_state(ST_VICTORY);
    } else {
        cur_room = dest;
        load_room();
    }
}

static void game_frame(void) {
    if (pad_new & PAD_START) {
        game_state = ST_PAUSE;
        vbuf_text(13, 2, "PAUSED");
        return;
    }
    if (++sec_frames == 60) {
        sec_frames = 0;
        ++play_sec;
    }
    player_frame();
    entities_frame();
    boss_frame();
    check_door();
    if (hp == 0) {
        enter_state(ST_DEATH);
        return;
    }
    if (victory_pending) {
        enter_state(ST_VICTORY);
        return;
    }
    hud_update();
    player_draw();
    entities_draw();
    boss_draw();
}

static void pause_frame(void) {
    if (pad_new & PAD_START) {
        game_state = ST_GAME;
        vbuf_text(13, 2, "      ");
    }
    player_draw();
    entities_draw();
}

/* ------------------------------------------------------------------ */

void main(void) {
    load_chr();
    audio_init();
    enter_state(ST_TITLE);

    while (1) {
        /* ---- vblank window: commit graphics ---- */
        wait_vblank();
        /* If the last logic frame overran, wait_vblank returns mid-render;
           OAM DMA or PPUDATA writes now would land at whatever address the
           PPU is fetching from (= random nametable corruption). Commit only
           when PPUSTATUS confirms we are really inside vblank; otherwise
           the queue simply flushes next frame. */
        if (PPUSTATUS & 0x80) {
            ppu_update();        /* OAM DMA first (must finish in vblank) */
            vbuf_flush();
        }
        PPUSTATUS;               /* PPUADDR use above corrupts scroll: reset */
        PPUCTRL = PPUCTRL_GAME;
        PPUSCROLL = 0;
        PPUSCROLL = 0;

        /* ---- input ---- */
        pad_prev = pad;
        pad = pad_poll();
        pad_new = pad & (pad ^ pad_prev);

        /* ---- logic + sprite build ---- */
        oam_idx = 0;
        switch (game_state) {
        case ST_TITLE:
            if (pad_new & PAD_START) {
                new_game();
                enter_state(ST_GAME);
            }
            break;
        case ST_GAME:
            game_frame();
            break;
        case ST_PAUSE:
            pause_frame();
            break;
        case ST_DEATH:
            if (pad_new & PAD_START) {
                hp = MAX_HP;
                enter_state(ST_GAME);
            }
            break;
        case ST_VICTORY:
            if (pad_new & PAD_START) enter_state(ST_TITLE);
            break;
        }
        hide_rest_of_oam();
        audio_frame();
        ++frame_cnt;
    }
}
