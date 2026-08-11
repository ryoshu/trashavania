/* player.c -- Jimothy: movement, jumping, crouching, collision, damage,
   animation, and OAM drawing. Positions are 8.8 fixed point; the metatile
   grid is 16x16 px so pixel>>4 indexes the room map directly. */
#include "game.h"

unsigned char mt_at(unsigned char x, unsigned char y) {
    return room_map[(y & 0xF0) + (x >> 4)];
}

unsigned char coll_at(unsigned char x, unsigned char y) {
    return mt_coll[room_map[(y & 0xF0) + (x >> 4)]];
}

void player_init(unsigned char mtx, unsigned char mty) {
    /* feet standing on top of metatile row mty */
    pixx = mtx << 4;
    pixy = (mty << 4) - PLAYER_H;
    px = ((unsigned int)pixx) << 8;
    py = ((unsigned int)pixy) << 8;
    pvy = 0;
    on_ground = 1;
    facing_left = 0;
    crouching = 0;
    jump_buf = 0;
    coyote = 0;
    inv_timer = 0;
    attack_timer = 0;
    knock_timer = 0;
    anim_timer = 0;
    anim_frame = 0;
}

void player_hurt(unsigned char from_left, unsigned char dmg) {
    if (inv_timer || game_state != ST_GAME) return;
    if (hp > dmg) hp -= dmg; else hp = 0;
    inv_timer = INV_LEN;
    knock_timer = KNOCK_LEN;
    knock_dir = from_left;   /* pushed right if hit from left */
    pvy = -0x0200;
    on_ground = 0;
    audio_sfx(SFX_HURT);
    /* hud + death handled by main.c */
}

/* ------------------------------------------------------------------ */

static void move_horizontal(void) {
    unsigned int nx;
    unsigned char step, top, cl;

    if (knock_timer) {
        nx = knock_dir ? px + KNOCK_SPEED : px - KNOCK_SPEED;
    } else if (attack_timer && on_ground) {
        return;              /* grounded swipe roots Jimothy briefly */
    } else if (pad & PAD_LEFT) {
        if (crouching && on_ground) return;
        facing_left = 1;
        nx = px - WALK_SPEED;
    } else if (pad & PAD_RIGHT) {
        if (crouching && on_ground) return;
        facing_left = 0;
        nx = px + WALK_SPEED;
    } else {
        return;
    }

    if (nx < 0x0200u || nx > 0xF800u) nx = 0x0200u;   /* left clamp incl. wrap */
    else if (nx > 0xEE00u) nx = 0xEE00u;              /* right clamp (238px) */
    step = (unsigned char)(nx >> 8);
    top = crouching ? HB_Y0_CROUCH : HB_Y0;

    if ((unsigned char)(nx >> 8) > pixx) {          /* moving right */
        unsigned char edge = step + HB_X1;
        cl = coll_at(edge, pixy + top);
        if (cl != COLL_SOLID) cl = coll_at(edge, pixy + 14);
        if (cl != COLL_SOLID) cl = coll_at(edge, pixy + HB_Y1);
        if (cl == COLL_SOLID) {
            step = ((edge & 0xF0) - HB_X1) - 1;
            nx = ((unsigned int)step) << 8;
        }
    } else if ((unsigned char)(nx >> 8) < pixx) {   /* moving left */
        unsigned char edge = step + HB_X0;
        cl = coll_at(edge, pixy + top);
        if (cl != COLL_SOLID) cl = coll_at(edge, pixy + 14);
        if (cl != COLL_SOLID) cl = coll_at(edge, pixy + HB_Y1);
        if (cl == COLL_SOLID) {
            step = ((edge & 0xF0) + 16 - HB_X0);
            nx = ((unsigned int)step) << 8;
        }
    }
    px = nx;
    pixx = (unsigned char)(px >> 8);
}

static void move_vertical(void) {
    unsigned int ny;
    unsigned char feet, prev_feet, cl, cr, c;

    pvy += GRAVITY;
    if (pvy > MAX_FALL) pvy = MAX_FALL;
    ny = py + (unsigned int)pvy;
    if (ny > 0xF800u) ny = 0;            /* wrapped past top of screen */
    prev_feet = pixy + HB_Y1;

    if (pvy > 0) {                       /* falling */
        feet = (unsigned char)(ny >> 8) + HB_Y1 + 1;
        cl = coll_at(pixx + HB_X0, feet);
        cr = coll_at(pixx + HB_X1, feet);
        c = (cl > cr) ? cl : cr;
        if (c == COLL_SOLID ||
            ((cl == COLL_PLATFORM || cr == COLL_PLATFORM) &&
             prev_feet < (unsigned char)(feet & 0xF0))) {
            /* land: feet on tile top */
            pixy = (feet & 0xF0) - PLAYER_H;
            py = ((unsigned int)pixy) << 8;
            pvy = 0;
            if (!on_ground) anim_frame = 0;
            on_ground = 1;
            coyote = COYOTE_LEN;
            return;
        }
        on_ground = 0;
    } else if (pvy < 0) {                /* rising: bump head */
        unsigned char head = (unsigned char)(ny >> 8) + HB_Y0;
        if (coll_at(pixx + HB_X0, head) == COLL_SOLID ||
            coll_at(pixx + HB_X1, head) == COLL_SOLID) {
            pixy = (head | 0x0F) + 1 - HB_Y0;
            py = ((unsigned int)pixy) << 8;
            pvy = 0;
            return;
        }
        on_ground = 0;
    }
    py = ny;
    pixy = (unsigned char)(py >> 8);
}

void player_frame(void) {
    /* timers */
    if (inv_timer) --inv_timer;
    if (attack_timer) --attack_timer;
    if (knock_timer) --knock_timer;
    if (coyote) --coyote;
    if (jump_buf) --jump_buf;

    /* crouch */
    crouching = (on_ground && (pad & PAD_DOWN) && !knock_timer) ? 1 : 0;

    /* jump: buffered input + coyote time */
    if (pad_new & PAD_A) jump_buf = JUMP_BUF_LEN;
    if (jump_buf && (on_ground || coyote) && !crouching && !knock_timer) {
        pvy = JUMP_VEL;
        on_ground = 0;
        coyote = 0;
        jump_buf = 0;
        audio_sfx(SFX_JUMP);
    }
    /* variable jump height: release A while rising cuts velocity */
    if (!(pad & PAD_A) && pvy < -0x0100) pvy = -0x0100;

    /* swipe */
    if ((pad_new & PAD_B) && !(pad & PAD_UP) && !attack_timer) {
        attack_timer = ATTACK_LEN;
        audio_sfx(SFX_SWIPE);
    }

    move_horizontal();
    move_vertical();

    /* standing in broken glass or other hazard tiles */
    if (coll_at(pixx + 8, pixy + HB_Y1) == COLL_HAZARD ||
        coll_at(pixx + 8, pixy + 16) == COLL_HAZARD) {
        player_hurt(facing_left ? 0 : 1, 1);    /* knocked back the way we came */
    }
}

/* ------------------------------------------------------------------ */

void player_draw(void) {
    unsigned char tile, attr, h;

    /* blink while invincible */
    if (inv_timer && (frame_cnt & 2)) return;

    attr = facing_left ? 0x40 : 0x00;   /* palette 0, hflip if left */
    h = 3;

    if (knock_timer) {
        tile = SPR_JIM_HURT;
    } else if (crouching) {
        tile = SPR_JIM_CROUCH;
        h = 2;
    } else if (attack_timer > ATTACK_LEN / 3) {
        tile = SPR_JIM_SWIPE;
    } else if (!on_ground) {
        tile = SPR_JIM_JUMP;
    } else if ((pad & (PAD_LEFT | PAD_RIGHT)) && !crouching) {
        /* 2-frame walk at ~7.5 fps */
        tile = (frame_cnt & 8) ? SPR_JIM_WALK1 : SPR_JIM_WALK2;
    } else {
        tile = SPR_JIM_IDLE;
    }

    if (h == 2) {
        draw_meta(pixx, pixy + 8, tile, 2, 2, attr);
    } else {
        draw_meta(pixx, pixy, tile, 2, 3, attr);
    }

    /* claw slash effect at the fist during the active swipe window */
    if (tile == SPR_JIM_SWIPE) {
        if (facing_left) {
            draw_meta(pixx - 8, pixy + 10, SPR_SLASH, 1, 1, 0x42);
        } else {
            draw_meta(pixx + 16, pixy + 10, SPR_SLASH, 1, 1, 0x02);
        }
    }
}
