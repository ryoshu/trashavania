/* boss.c -- Count Dumpula, the immortal lord of Castle Refuse.
   The dumpster body is background tiles (room 4 art); sprites are only the
   flying lid, the glowing eyes, and effects. Attack cycle per the brief:
   lid slam -> garbage toss -> trash bat summon -> weak point open. */
#include "game.h"

#define BOSS_MAX_HP 12

/* weak point: the glowing opening at the dumpster's front-bottom */
#define WEAK_X 144
#define WEAK_Y 192
#define WEAK_W 16
#define WEAK_H 16

/* lid rest position on top of the dumpster */
#define LID_REST_X 152
#define LID_REST_Y 136

unsigned char boss_active, boss_hp, boss_defeated;
static unsigned char phase, timer, toss_count, accel;
static unsigned char lid_x, lid_y, lid_airborne;

enum {
    BP_IDLE1, BP_TELE, BP_RISE, BP_TRACK, BP_DROP, BP_HOLD, BP_RETURN,
    BP_IDLE2, BP_TOSS, BP_IDLE3, BP_SUMMON, BP_OPEN, BP_DYING, BP_DONE
};

void boss_init(void) {
    boss_active = 1;
    boss_defeated = 0;
    boss_hp = BOSS_MAX_HP;
    phase = BP_IDLE1;
    timer = 90;
    accel = 0;
    lid_x = LID_REST_X;
    lid_y = LID_REST_Y;
    lid_airborne = 0;
}

static void next_phase(unsigned char p, unsigned char t) {
    phase = p;
    /* below half health the whole cycle runs faster */
    timer = accel ? (t - (t >> 2)) : t;
}

void boss_hurt(unsigned char dmg) {
    audio_sfx(SFX_BOSS_HURT);
    if (boss_hp > dmg) {
        boss_hp -= dmg;
        if (boss_hp <= BOSS_MAX_HP / 2) accel = 1;
        return;
    }
    boss_hp = 0;
    boss_defeated = 1;
    phase = BP_DYING;
    timer = 96;
    lid_x = LID_REST_X;
    lid_y = LID_REST_Y;
    lid_airborne = 0;
}

void boss_frame(void) {
    unsigned char t;

    if (!boss_active) return;

    if (timer) --timer;
    switch (phase) {
    case BP_IDLE1:
        if (!timer) next_phase(BP_TELE, 40);
        break;
    case BP_TELE:
        /* lid rattles in place: telegraph */
        lid_x = LID_REST_X + ((frame_cnt & 2) ? 1 : 0);
        lid_y = LID_REST_Y - ((frame_cnt & 4) ? 2 : 0);
        if (!timer) {
            lid_airborne = 1;
            next_phase(BP_RISE, 30);
        }
        break;
    case BP_RISE:
        if (lid_y > 72) lid_y -= 2;
        if (!timer) next_phase(BP_TRACK, 50);
        break;
    case BP_TRACK:
        /* hover toward the player's x */
        t = pixx;
        if (t > 224) t = 224;
        if (lid_x < t) { ++lid_x; if (lid_x < t) ++lid_x; }
        else if (lid_x > t) { --lid_x; if (lid_x > t) --lid_x; }
        if (!timer) next_phase(BP_DROP, 0);
        break;
    case BP_DROP:
        lid_y += 6;
        if (lid_y >= 192) {
            lid_y = 192;
            audio_sfx(SFX_SLAM);
            next_phase(BP_HOLD, 45);
        }
        break;
    case BP_HOLD:
        if (!timer) next_phase(BP_RETURN, 0);
        break;
    case BP_RETURN:
        /* fly home */
        if (lid_y > LID_REST_Y) lid_y -= 2;
        if (lid_x < LID_REST_X) ++lid_x;
        else if (lid_x > LID_REST_X) --lid_x;
        if (lid_y <= LID_REST_Y && lid_x == LID_REST_X) {
            lid_y = LID_REST_Y;
            lid_airborne = 0;
            next_phase(BP_IDLE2, 40);
        }
        break;
    case BP_IDLE2:
        if (!timer) {
            toss_count = accel ? 4 : 3;
            next_phase(BP_TOSS, 10);
        }
        break;
    case BP_TOSS:
        if (!timer) {
            spawn_garbage(160, 136);
            audio_sfx(SFX_THROW);
            if (--toss_count) {
                timer = accel ? 14 : 20;
            } else {
                next_phase(BP_IDLE3, 40);
            }
        }
        break;
    case BP_IDLE3:
        if (!timer) next_phase(BP_SUMMON, 20);
        break;
    case BP_SUMMON:
        if (!timer) {
            spawn_enemy(6 /* ET_BOSSBAT */, 200, 64);
            if (accel) spawn_enemy(6, 200, 96);
            next_phase(BP_OPEN, 150);
        }
        break;
    case BP_OPEN:
        /* weak point vulnerable; handled below */
        if (!timer) next_phase(BP_IDLE1, 60);
        break;
    case BP_DYING:
        if (frame_cnt & 4) {
            spawn_effect(150 + (frame_cnt & 63), 150 + (frame_cnt & 31));
        }
        if (!timer) {
            phase = BP_DONE;
            spawn_pickup(SP_GOLD, 120, 160);
        }
        break;
    case BP_DONE:
        return;
    }

    if (boss_defeated) return;

    /* lid crush damage while dropping or grounded */
    if ((phase == BP_DROP || phase == BP_HOLD) && lid_y >= pixy) {
        if (pixx + HB_X1 >= lid_x && lid_x + 31 >= pixx + HB_X0) {
            player_hurt((lid_x + 16 < pixx) ? 1 : 0, 1);
        }
    }

    /* weak point takes hits only while open */
    if (phase == BP_OPEN) {
        if (attack_hits_box(WEAK_X, WEAK_Y, WEAK_W, WEAK_H)) {
            boss_hurt(1);
        }
    }
}

void boss_draw(void) {
    unsigned char gy;

    if (!boss_active || phase >= BP_DYING) {
        if (phase == BP_DYING) return;      /* body flickers via effects */
        if (phase == BP_DONE) return;
        return;
    }

    /* the lid (32x8 = 4x1 tiles) */
    draw_meta(lid_x, lid_y, SPR_LID, 4, 1, 0x02);

    /* glowing eyes above the opening; blink occasionally */
    if ((frame_cnt & 63) > 8) {
        draw_meta(168, 152, SPR_EYE, 1, 1, 0x02);
        draw_meta(184, 152, SPR_EYE, 1, 1, 0x02);
    }

    /* weak point glow flare while open */
    if (phase == BP_OPEN) {
        gy = 192 + ((frame_cnt & 8) ? 1 : 0);
        draw_meta(WEAK_X + 4, gy, SPR_EYE, 1, 1, (frame_cnt & 4) ? 0x03 : 0x02);
    }
}
