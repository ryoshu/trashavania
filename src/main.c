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

/* ------------------------------------------------------------------ */

static void new_game(void) {
    cur_room = 0;
    hp = MAX_HP;
    snacks = 0;
    weapon = 0;
}

static void load_room(void) {
    ppu_off();
    clear_nametable();
    draw_room(cur_room);
    player_init(rooms[cur_room].start_x, rooms[cur_room].start_y);
    vbuf_reset();
    ppu_on();
}

void enter_state(unsigned char st) {
    game_state = st;
    switch (st) {
    case ST_TITLE:
        ppu_off();
        clear_nametable();
        draw_text(10, 6, "TRASHAVANIA");
        draw_text(3, 8, "THE ADVENTURES OF JIMOTHY");
        draw_text(4, 13, "IT WAS A MISERABLE NIGHT");
        draw_text(7, 14, "TO HAVE A CURSE.");
        draw_text(10, 20, "PRESS START");
        ppu_on();
        break;
    case ST_GAME:
        load_room();
        break;
    case ST_DEATH:
        ppu_off();
        clear_nametable();
        draw_text(6, 10, "JIMOTHY HAS EXPIRED.");
        draw_text(5, 12, "THE TRASH REMAINS UNCLAIMED.");
        draw_text(8, 18, "PRESS START");
        ppu_on();
        break;
    case ST_VICTORY:
        ppu_off();
        clear_nametable();
        draw_text(4, 8, "THE GOLDEN GARBAGE IS YOURS!");
        draw_text(4, 12, "JIMOTHY HAS NO MASTER.");
        draw_text(4, 13, "JIMOTHY HAS SNACKS.");
        draw_text(8, 20, "PRESS START");
        ppu_on();
        break;
    }
}

/* ------------------------------------------------------------------ */

static void check_door(void) {
    unsigned char dest;
    if (coll_at(pixx + 8, pixy + 12) != COLL_DOOR) return;
    dest = rooms[cur_room].door_dest;
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
    player_frame();
    check_door();
    if (hp == 0) {
        enter_state(ST_DEATH);
        return;
    }
    player_draw();
}

static void pause_frame(void) {
    if (pad_new & PAD_START) {
        game_state = ST_GAME;
        vbuf_text(13, 2, "      ");
    }
    player_draw();
}

/* ------------------------------------------------------------------ */

void main(void) {
    load_chr();
    enter_state(ST_TITLE);

    while (1) {
        /* ---- vblank window: commit graphics ---- */
        wait_vblank();
        ppu_update();            /* OAM DMA first (must finish in vblank) */
        vbuf_flush();
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
        ++frame_cnt;
    }
}
