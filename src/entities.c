/* entities.c -- enemy/pickup/projectile pools and all combat interaction.
   Fixed-size pools per the brief (section 16); struct-of-arrays layout
   because cc65 indexes parallel byte arrays much better than structs. */
#include "game.h"

/* runtime enemy types */
enum {
    ET_NONE, ET_BAT, ET_CAT, ET_GNOME, ET_BAG, ET_CANDLE,
    ET_BOSSBAT              /* bat summoned by the boss: no snack drops */
};

/* pools */
#define NE 6
#define NP 4                /* pickups */
#define NPP 3               /* player projectiles */
#define NEP 3               /* enemy projectiles */
#define NFX 2

unsigned char e_type[NE], e_x[NE], e_y[NE], e_hp[NE], e_state[NE],
              e_timer[NE], e_dir[NE], e_var[NE], e_sub[NE], e_flash[NE];
static unsigned char p_type[NP], p_x[NP], p_y[NP];
static unsigned char pp_type[NPP];              /* 0 none 1 cap 2 tomato 3 splat */
static unsigned int pp_x[NPP], pp_y[NPP];
static int pp_vx[NPP], pp_vy[NPP];
static unsigned char pp_timer[NPP];
static unsigned char ep_on[NEP], ep_kind[NEP];   /* kind: 0 shard, 1 garbage */
static unsigned int ep_x[NEP], ep_y[NEP];
static int ep_vx[NEP], ep_vy[NEP];
static unsigned char fx_on[NFX], fx_x[NFX], fx_y[NFX], fx_timer[NFX];

unsigned char gold_count;
unsigned char hud_dirty;
static unsigned char rng;
static unsigned char swipe_x0, swipe_y0, swipe_active;

/* cat aggro / bat flight tuning */
#define CAT_SIGHT 72
#define BAT_AMP_TABLE                                                   \
    0, 2, 5, 7, 8, 10, 11, 12, 12, 12, 11, 10, 8, 7, 5, 2,             \
    0, -2, -5, -7, -8, -10, -11, -12, -12, -12, -11, -10, -8, -7, -5, -2
static const signed char bat_wave[32] = { BAT_AMP_TABLE };

static unsigned char rand8(void) {
    rng ^= rng << 3;
    rng ^= rng >> 5;
    rng ^= frame_cnt;
    if (!rng) rng = 0xA5;
    return rng;
}

/* ------------------------------------------------------------------ */

void clear_enemies(void) {
    unsigned char i;
    for (i = 0; i < NE; ++i) {
        if (e_type[i]) {
            spawn_effect(e_x[i] + 4, e_y[i] + 4);
            e_type[i] = 0;
        }
    }
}

void entities_reset(void) {
    unsigned char i;
    for (i = 0; i < NE; ++i) e_type[i] = 0;
    for (i = 0; i < NP; ++i) p_type[i] = 0;
    for (i = 0; i < NPP; ++i) pp_type[i] = 0;
    for (i = 0; i < NEP; ++i) ep_on[i] = 0;
    for (i = 0; i < NFX; ++i) fx_on[i] = 0;
    if (!rng) rng = 0xA5;
}

unsigned char spawn_pickup(unsigned char type, unsigned char x, unsigned char y) {
    unsigned char i;
    for (i = 0; i < NP; ++i) {
        if (!p_type[i]) {
            p_type[i] = type;
            p_x[i] = x;
            p_y[i] = y;
            return 1;
        }
    }
    return 0;
}

void spawn_effect(unsigned char x, unsigned char y) {
    unsigned char i;
    for (i = 0; i < NFX; ++i) {
        if (!fx_on[i]) {
            fx_on[i] = 1;
            fx_x[i] = x;
            fx_y[i] = y;
            fx_timer[i] = 14;
            return;
        }
    }
}

unsigned char spawn_enemy(unsigned char type, unsigned char x, unsigned char y) {
    unsigned char i;
    for (i = 0; i < NE; ++i) {
        if (!e_type[i]) {
            e_type[i] = type;
            e_x[i] = x;
            e_y[i] = y;
            e_state[i] = 0;
            e_timer[i] = 0;
            e_dir[i] = 0;
            e_sub[i] = 0;
            e_flash[i] = 0;
            e_var[i] = y;               /* bats: base flight line */
            switch (type) {
            case ET_CAT: e_hp[i] = 2; break;
            case ET_GNOME: e_hp[i] = 2; break;
            default: e_hp[i] = 1; break;
            }
            return i;
        }
    }
    return 0xFF;
}

void spawn_room_entities(const unsigned char *sp) {
    unsigned char t, x, y, i;
    entities_reset();
    while (*sp != 0xFF) {
        t = sp[0];
        x = sp[1] << 4;
        y = sp[2] << 4;
        sp += 3;
        switch (t) {
        case SP_BAT: spawn_enemy(ET_BAT, x, y); break;
        case SP_CAT: spawn_enemy(ET_CAT, x, y); break;
        case SP_GNOME: spawn_enemy(ET_GNOME, x, y); break;
        case SP_BAG:
            i = spawn_enemy(ET_BAG, x, y);
            if (i != 0xFF) e_var[i] = SP_BIGSNACK;   /* bags hold big snacks */
            break;
        case SP_CANDLE:
            i = spawn_enemy(ET_CANDLE, x, y);
            if (i != 0xFF) e_var[i] = SP_SNACK;
            break;
        default: spawn_pickup(t, x + 4, y + 8); break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* rectangle overlap: boxes given as x,y,w,h with small sizes; unsigned
   subtraction trick keeps it cheap */
static unsigned char boxes_hit(unsigned char ax, unsigned char ay,
                               unsigned char aw, unsigned char ah,
                               unsigned char bx, unsigned char by,
                               unsigned char bw, unsigned char bh) {
    if ((unsigned char)(bx - ax) < aw || (unsigned char)(ax - bx) < bw) {
        if ((unsigned char)(by - ay) < ah || (unsigned char)(ay - by) < bh) {
            return 1;
        }
    }
    return 0;
}

static void kill_enemy(unsigned char i) {
    unsigned char r, drop = 0;
    spawn_effect(e_x[i] + 4, e_y[i] + 4);
    switch (e_type[i]) {
    case ET_BAG:
    case ET_CANDLE:
        drop = e_var[i];                /* containers: preset contents */
        break;
    case ET_BAT:
        r = rand8();
        if (r < 160) drop = SP_SNACK;   /* bats drop snacks frequently */
        break;
    case ET_CAT:
        r = rand8();
        if (r < 64) drop = SP_BURRITO;
        else if (r < 160) drop = SP_SNACK;
        break;
    case ET_GNOME:
        r = rand8();
        if (r < 128) drop = SP_SNACK;
        break;
    }
    if (drop) spawn_pickup(drop, e_x[i] + 4, e_y[i] + 4);
    e_type[i] = 0;
    audio_sfx(SFX_ENEMY_DIE);
}

static void damage_enemy(unsigned char i, unsigned char dmg) {
    if (e_flash[i]) return;             /* brief mercy so splat doesn't melt */
    if (e_hp[i] > dmg) {
        e_hp[i] -= dmg;
        e_flash[i] = 14;    /* > swipe active window: one hit per swing */
        audio_sfx(SFX_HIT);
    } else {
        kill_enemy(i);
    }
}

/* ------------------------------------------------------------------ */
/* Enemy behaviors */

static void bat_frame(unsigned char i) {
    unsigned char idx;
    /* horizontal: 0.75 px/frame toward current direction, turn at edges */
    e_sub[i] += 0xC0;
    if (e_sub[i] < 0xC0) {              /* carry */
        if (e_dir[i]) {
            if (e_x[i] <= 4) e_dir[i] = 0; else --e_x[i];
        } else {
            if (e_x[i] >= 236) e_dir[i] = 1; else ++e_x[i];
        }
    }
    idx = ((frame_cnt >> 2) + (i << 2)) & 31;
    e_y[i] = e_var[i] + bat_wave[idx];
}

static void cat_frame(unsigned char i) {
    unsigned char nx, feet, dy;
    if (e_state[i] == 2) {              /* stunned after charge */
        if (--e_timer[i] == 0) e_state[i] = 0;
        return;
    }
    if (e_state[i] == 0) {
        /* pace at 0.5 px/frame */
        if (frame_cnt & 1) return;
        /* spot the player: same-ish height, in front, within range */
        dy = (unsigned char)(pixy + 8 - e_y[i]);
        if (dy < 24 || dy > 232) {
            unsigned char dx = pixx - e_x[i];
            if (e_dir[i] == 0 && dx < CAT_SIGHT) {          /* facing right */
                e_state[i] = 1;
            } else if (e_dir[i] == 1 &&
                       (unsigned char)(e_x[i] - pixx) < CAT_SIGHT) {
                e_state[i] = 1;
            }
        }
    }
    /* move 1px (pace, every other frame) or 2px (charge, every frame) */
    nx = e_x[i];
    if (e_dir[i]) {
        nx -= (e_state[i] == 1) ? 2 : 1;
    } else {
        nx += (e_state[i] == 1) ? 2 : 1;
    }
    /* turn (or end charge) at walls and platform edges */
    feet = e_y[i] + 16;
    if (nx < 2 || nx > 238 ||
        coll_at(e_dir[i] ? nx : nx + 15, e_y[i] + 8) == COLL_SOLID ||
        coll_at(e_dir[i] ? nx : nx + 15, feet) == COLL_NONE) {
        if (e_state[i] == 1) {
            e_state[i] = 2;             /* charge ends in a stun */
            e_timer[i] = 40;
        } else {
            e_dir[i] ^= 1;
        }
        return;
    }
    e_x[i] = nx;
}

static void spawn_shard(unsigned char x, unsigned char y, unsigned char left) {
    unsigned char i;
    for (i = 0; i < NEP; ++i) {
        if (!ep_on[i]) {
            ep_on[i] = 1;
            ep_kind[i] = 0;
            ep_x[i] = ((unsigned int)x) << 8;
            ep_y[i] = ((unsigned int)y) << 8;
            ep_vx[i] = left ? -0x0100 : 0x0100;
            ep_vy[i] = -0x0280;
            return;
        }
    }
}

/* boss garbage toss: arcs left with randomized reach */
void spawn_garbage(unsigned char x, unsigned char y) {
    unsigned char i;
    for (i = 0; i < NEP; ++i) {
        if (!ep_on[i]) {
            ep_on[i] = 1;
            ep_kind[i] = 1;
            ep_x[i] = ((unsigned int)x) << 8;
            ep_y[i] = ((unsigned int)y) << 8;
            ep_vx[i] = -0x00C0 - ((rand8() & 0x7F) << 1);
            ep_vy[i] = -0x0380;
            return;
        }
    }
}

/* does the player's current attack (swipe or projectile) hit this box?
   Consumes a projectile on hit. Used by the boss weak point. */
unsigned char attack_hits_box(unsigned char x, unsigned char y,
                              unsigned char w, unsigned char h) {
    unsigned char j;
    if (swipe_active &&
        boxes_hit(swipe_x0, swipe_y0, 12, 12, x, y, w, h)) {
        return 1;
    }
    for (j = 0; j < NPP; ++j) {
        if (!pp_type[j] || pp_type[j] == 3) continue;
        if (boxes_hit((unsigned char)(pp_x[j] >> 8),
                      (unsigned char)(pp_y[j] >> 8), 8, 8, x, y, w, h)) {
            pp_type[j] = 0;
            return 1;
        }
    }
    return 0;
}

static void gnome_frame(unsigned char i) {
    ++e_timer[i];
    e_dir[i] = (pixx < e_x[i]) ? 1 : 0;         /* face the player */
    if (e_timer[i] == 100) {
        e_state[i] = 1;                          /* wind up (throw pose) */
    } else if (e_timer[i] == 116) {
        spawn_shard(e_x[i] + 4, e_y[i], e_dir[i]);
        audio_sfx(SFX_THROW);
    } else if (e_timer[i] >= 128) {
        e_timer[i] = 0;
        e_state[i] = 0;
    }
}

/* ------------------------------------------------------------------ */

void weapon_fire(void) {
    unsigned char i, cost;
    if (!weapon || !snacks) return;
    cost = (weapon == 2) ? 2 : 1;
    if (snacks < cost) return;
    for (i = 0; i < NPP; ++i) {
        if (!pp_type[i]) break;
    }
    if (i == NPP || pp_type[i]) return;
    snacks -= cost;
    hud_dirty = 1;
    pp_type[i] = weapon;
    pp_y[i] = ((unsigned int)(pixy + 8)) << 8;
    if (facing_left) {
        pp_x[i] = ((unsigned int)(pixx)) << 8;
        pp_vx[i] = (weapon == 2) ? -0x0140 : -0x0300;
    } else {
        pp_x[i] = ((unsigned int)(pixx + 12)) << 8;
        pp_vx[i] = (weapon == 2) ? 0x0140 : 0x0300;
    }
    pp_vy[i] = (weapon == 2) ? -0x0200 : 0;
    pp_timer[i] = 0;
    audio_sfx(SFX_THROW);
}

static void player_projectile_frame(unsigned char i) {
    unsigned char x, y;
    if (pp_type[i] == 3) {              /* splat damage zone on the floor */
        if (--pp_timer[i] == 0) pp_type[i] = 0;
        return;
    }
    pp_x[i] += (unsigned int)pp_vx[i];
    if (pp_type[i] == 2) {
        pp_vy[i] += 0x20;
        pp_y[i] += (unsigned int)pp_vy[i];
    }
    x = (unsigned char)(pp_x[i] >> 8);
    y = (unsigned char)(pp_y[i] >> 8);
    if (x < 2 || x > 246 || y > 230) {
        pp_type[i] = 0;
        return;
    }
    if (coll_at(x + 3, y + 3) == COLL_SOLID) {
        if (pp_type[i] == 2) {          /* tomato -> splat zone */
            pp_type[i] = 3;
            pp_timer[i] = 60;
            pp_y[i] = (unsigned int)((y & 0xF0) - 5) << 8;
            audio_sfx(SFX_SPLAT);
        } else {
            pp_type[i] = 0;
        }
    }
}

static void enemy_projectile_frame(unsigned char i) {
    unsigned char x, y;
    ep_x[i] += (unsigned int)ep_vx[i];
    ep_vy[i] += 0x20;
    ep_y[i] += (unsigned int)ep_vy[i];
    x = (unsigned char)(ep_x[i] >> 8);
    y = (unsigned char)(ep_y[i] >> 8);
    if (x < 2 || x > 246 || y > 230 ||
        (ep_vy[i] > 0 && coll_at(x + 3, y + 6) == COLL_SOLID)) {
        ep_on[i] = 0;       /* off screen or splatted on the floor */
        return;
    }
    if (boxes_hit(x, y, 6, 6, pixx + HB_X0,
                  pixy + (crouching ? HB_Y0_CROUCH : HB_Y0),
                  HB_X1 - HB_X0 + 1, HB_Y1 - (crouching ? HB_Y0_CROUCH : HB_Y0) + 1)) {
        player_hurt((x < pixx) ? 1 : 0, 1);
        ep_on[i] = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Swipe: active early in the attack; also deflects enemy projectiles */

static void compute_swipe(void) {
    swipe_active = 0;
    if (attack_timer > ATTACK_LEN / 3) {
        swipe_active = 1;
        swipe_y0 = pixy + 8;
        swipe_x0 = facing_left ? (unsigned char)(pixx - 9) : pixx + 13;
    }
}

/* ------------------------------------------------------------------ */

static void collect_pickup(unsigned char i) {
    unsigned char t = p_type[i];
    p_type[i] = 0;
    audio_sfx(SFX_PICKUP);
    switch (t) {
    case SP_SNACK:
        if (snacks < 99) ++snacks;
        break;
    case SP_BIGSNACK:
        snacks = (snacks <= 94) ? snacks + 5 : 99;
        break;
    case SP_BURRITO:
        hp += 3;
        if (hp > MAX_HP) hp = MAX_HP;
        break;
    case SP_GOLD:
        ++gold_count;
        if (boss_defeated) victory_pending = 1;
        break;
    case SP_WEAPON_CAP:
        weapon = 1;
        snacks = (snacks <= 96) ? snacks + 3 : 99;
        break;
    case SP_WEAPON_TOMATO:
        weapon = 2;
        snacks = (snacks <= 96) ? snacks + 3 : 99;
        break;
    default:                            /* mystery leftovers */
        if (rand8() & 1) {
            hp += 2;
            if (hp > MAX_HP) hp = MAX_HP;
        } else {
            snacks = (snacks <= 96) ? snacks + 3 : 99;
        }
        break;
    }
    hud_dirty = 1;
}

static void pickup_frame(unsigned char i) {
    /* fall until resting on something solid */
    unsigned char c = coll_at(p_x[i] + 4, p_y[i] + 8);
    if (c != COLL_SOLID && c != COLL_PLATFORM) {
        if (p_y[i] < 216) p_y[i] += 2;
    }
    if (boxes_hit(p_x[i], p_y[i], 8, 8,
                  pixx + HB_X0, pixy + HB_Y0, HB_X1 - HB_X0 + 1,
                  HB_Y1 - HB_Y0 + 1)) {
        collect_pickup(i);
    }
}

/* ------------------------------------------------------------------ */

void entities_frame(void) {
    unsigned char i, j, ex, ey, harmless;

    compute_swipe();
    if ((pad_new & PAD_B) && (pad & PAD_UP)) weapon_fire();

    for (i = 0; i < NE; ++i) {
        if (!e_type[i]) continue;
        if (e_flash[i]) --e_flash[i];
        switch (e_type[i]) {
        case ET_BAT:
        case ET_BOSSBAT: bat_frame(i); break;
        case ET_CAT: cat_frame(i); break;
        case ET_GNOME: gnome_frame(i); break;
        }

        ex = e_x[i];
        ey = e_y[i];
        harmless = (e_type[i] == ET_BAG || e_type[i] == ET_CANDLE);

        /* swipe vs enemy */
        if (swipe_active &&
            boxes_hit(swipe_x0, swipe_y0, 12, 12, ex, ey, 16, 16)) {
            damage_enemy(i, 1);
            if (!e_type[i]) continue;
        }
        /* player projectiles vs enemy */
        for (j = 0; j < NPP; ++j) {
            if (!pp_type[j]) continue;
            if (boxes_hit((unsigned char)(pp_x[j] >> 8),
                          (unsigned char)(pp_y[j] >> 8),
                          (pp_type[j] == 3) ? 16 : 8,
                          (pp_type[j] == 3) ? 8 : 8,
                          ex, ey, 16, 16)) {
                if (pp_type[j] == 3) {
                    damage_enemy(i, 1);         /* splat zone: uses flash timer */
                } else {
                    pp_type[j] = 0;
                    damage_enemy(i, 1);
                }
                if (!e_type[i]) break;
            }
        }
        if (!e_type[i]) continue;

        /* enemy vs player contact */
        if (!harmless &&
            boxes_hit(ex + 2, ey + 2, 12, 12,
                      pixx + HB_X0,
                      pixy + (crouching ? HB_Y0_CROUCH : HB_Y0),
                      HB_X1 - HB_X0 + 1,
                      HB_Y1 - (crouching ? HB_Y0_CROUCH : HB_Y0) + 1)) {
            player_hurt((ex < pixx) ? 1 : 0, 1);
        }
    }

    /* swipe vs enemy projectiles (knock them out of the air) */
    if (swipe_active) {
        for (j = 0; j < NEP; ++j) {
            if (ep_on[j] &&
                boxes_hit(swipe_x0, swipe_y0, 12, 12,
                          (unsigned char)(ep_x[j] >> 8),
                          (unsigned char)(ep_y[j] >> 8), 6, 6)) {
                ep_on[j] = 0;
                spawn_effect((unsigned char)(ep_x[j] >> 8),
                             (unsigned char)(ep_y[j] >> 8));
            }
        }
    }

    for (i = 0; i < NPP; ++i) if (pp_type[i]) player_projectile_frame(i);
    for (i = 0; i < NEP; ++i) if (ep_on[i]) enemy_projectile_frame(i);
    for (i = 0; i < NP; ++i) if (p_type[i]) pickup_frame(i);
    for (i = 0; i < NFX; ++i) {
        if (fx_on[i] && --fx_timer[i] == 0) fx_on[i] = 0;
    }
}

/* ------------------------------------------------------------------ */

void entities_draw(void) {
    unsigned char i, tile, flip;

    for (i = 0; i < NE; ++i) {
        if (!e_type[i]) continue;
        if (e_flash[i] & 2) continue;   /* hit flash */
        flip = e_dir[i] ? 0x40 : 0x00;
        switch (e_type[i]) {
        case ET_BAT:
        case ET_BOSSBAT:
            tile = (frame_cnt & 8) ? SPR_BAT1 : SPR_BAT2;
            draw_meta(e_x[i], e_y[i], tile, 2, 2, 0x01 | flip);
            break;
        case ET_CAT:
            tile = ((frame_cnt & 4) && e_state[i] != 2) ? SPR_CAT1 : SPR_CAT2;
            /* cat art faces right when flipped bit clear? art faces right */
            draw_meta(e_x[i], e_y[i], tile, 2, 2, 0x01 | (e_dir[i] ? 0x40 : 0));
            break;
        case ET_GNOME:
            tile = e_state[i] ? SPR_GNOME2 : SPR_GNOME1;
            draw_meta(e_x[i], e_y[i], tile, 2, 2, 0x03 | (e_dir[i] ? 0x40 : 0));
            break;
        case ET_BAG:
            draw_meta(e_x[i], e_y[i], SPR_BAG, 2, 2, 0x01);
            break;
        case ET_CANDLE:
            draw_meta(e_x[i], e_y[i], SPR_CANDLE, 1, 2, 0x02);
            break;
        }
    }

    for (i = 0; i < NPP; ++i) {
        if (!pp_type[i]) continue;
        switch (pp_type[i]) {
        case 1:
            draw_meta((unsigned char)(pp_x[i] >> 8),
                      (unsigned char)(pp_y[i] >> 8), SPR_CAP, 1, 1, 0x02);
            break;
        case 2:
            draw_meta((unsigned char)(pp_x[i] >> 8),
                      (unsigned char)(pp_y[i] >> 8), SPR_TOMATO, 1, 1, 0x03);
            break;
        case 3:
            if (frame_cnt & 2) {
                draw_meta((unsigned char)(pp_x[i] >> 8),
                          (unsigned char)(pp_y[i] >> 8), SPR_SPLAT, 2, 1, 0x03);
            }
            break;
        }
    }
    for (i = 0; i < NEP; ++i) {
        if (ep_on[i]) {
            draw_meta((unsigned char)(ep_x[i] >> 8),
                      (unsigned char)(ep_y[i] >> 8),
                      ep_kind[i] ? SPR_GARB : SPR_SHARD, 1, 1,
                      ep_kind[i] ? 0x02 : 0x01);
        }
    }
    for (i = 0; i < NP; ++i) {
        if (!p_type[i]) continue;
        switch (p_type[i]) {
        case SP_SNACK:
            draw_meta(p_x[i], p_y[i], SPR_SNACK, 1, 1, 0x03); break;
        case SP_BIGSNACK:
            draw_meta(p_x[i], p_y[i] - 8, SPR_BIGSNACK, 2, 2, 0x03); break;
        case SP_BURRITO:
            draw_meta(p_x[i], p_y[i], SPR_BURRITO, 1, 1, 0x03); break;
        case SP_GOLD:
            draw_meta(p_x[i], p_y[i] - 8, SPR_GOLD, 2, 2, 0x03); break;
        case SP_WEAPON_CAP:
            draw_meta(p_x[i], p_y[i], SPR_CAP, 1, 1, 0x02); break;
        case SP_WEAPON_TOMATO:
            draw_meta(p_x[i], p_y[i], SPR_TOMATO, 1, 1, 0x03); break;
        default:
            draw_meta(p_x[i], p_y[i], SPR_MYSTERY, 1, 1, 0x03); break;
        }
    }
    for (i = 0; i < NFX; ++i) {
        if (fx_on[i]) {
            draw_meta(fx_x[i], fx_y[i],
                      (fx_timer[i] > 7) ? SPR_POOF1 : SPR_POOF2, 1, 1, 0x02);
        }
    }
}
