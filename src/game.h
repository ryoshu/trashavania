/* game.h -- shared state and helpers for Trashavania */
#ifndef GAME_H
#define GAME_H

#include "nes.h"
#include "assets.h"
#include "audio.h"

/* ------------------------------------------------------------------ */
/* Game states */
enum {
    ST_TITLE,
    ST_GAME,
    ST_PAUSE,
    ST_DEATH,
    ST_VICTORY
};

/* ------------------------------------------------------------------ */
/* Fast-access globals (zero page). Every consumer of these must see the
   zpsym pragma, hence declaration here. All are defined in main.c. */
extern unsigned char frame_cnt;
extern unsigned char oam_idx;
extern unsigned char pad, pad_prev, pad_new;
extern unsigned char game_state;
extern unsigned char cur_room;
extern unsigned int px, py;     /* player position, 8.8 fixed point */
extern int pvy;                 /* player vertical velocity, 8.8 */
extern unsigned char pixx, pixy;/* player integer pixel position (top-left) */
extern unsigned char on_ground, facing_left, crouching;
extern unsigned char jump_buf, coyote;
extern unsigned char hp, snacks, weapon;
extern unsigned char inv_timer, attack_timer, knock_timer, knock_dir;
extern unsigned char anim_timer, anim_frame;
#pragma zpsym ("frame_cnt")
#pragma zpsym ("oam_idx")
#pragma zpsym ("pad")
#pragma zpsym ("pad_prev")
#pragma zpsym ("pad_new")
#pragma zpsym ("game_state")
#pragma zpsym ("cur_room")
#pragma zpsym ("px")
#pragma zpsym ("py")
#pragma zpsym ("pvy")
#pragma zpsym ("pixx")
#pragma zpsym ("pixy")
#pragma zpsym ("on_ground")
#pragma zpsym ("facing_left")
#pragma zpsym ("crouching")
#pragma zpsym ("jump_buf")
#pragma zpsym ("coyote")
#pragma zpsym ("hp")
#pragma zpsym ("snacks")
#pragma zpsym ("weapon")
#pragma zpsym ("inv_timer")
#pragma zpsym ("attack_timer")
#pragma zpsym ("knock_timer")
#pragma zpsym ("knock_dir")
#pragma zpsym ("anim_timer")
#pragma zpsym ("anim_frame")

/* Current room map pointer (ROM), set on room load */
extern const unsigned char *room_map;

/* ------------------------------------------------------------------ */
/* Tuning */
#define GRAVITY        0x0040   /* 0.25 px/frame^2 */
#define MAX_FALL       0x0400   /* 4 px/frame */
#define JUMP_VEL       (-0x0430)
#define WALK_SPEED     0x0140   /* 1.25 px/frame */
#define KNOCK_SPEED    0x0180
#define JUMP_BUF_LEN   5
#define COYOTE_LEN     6
#define INV_LEN        60
#define ATTACK_LEN     14
#define KNOCK_LEN      12
#define MAX_HP         8

#define PLAYER_W       16
#define PLAYER_H       24
/* hitbox inside the 16x24 sprite box */
#define HB_X0          3
#define HB_X1          12
#define HB_Y0          4
#define HB_Y0_CROUCH   12
#define HB_Y1          23

/* ------------------------------------------------------------------ */
/* render.c */
void ppu_off(void);
void ppu_on(void);
void clear_nametable(void);
void draw_room(unsigned char room);
void draw_text(unsigned char tx, unsigned char ty, const char *s);
void vbuf_text(unsigned char tx, unsigned char ty, const char *s);
void vbuf_tile(unsigned char tx, unsigned char ty, unsigned char tile);
void vbuf_run(unsigned char tx, unsigned char ty,
              const unsigned char *tiles, unsigned char len);
void vbuf_flush(void);
void vbuf_reset(void);
void load_chr(void);
void draw_meta(unsigned char x, unsigned char y, unsigned char tile,
               unsigned char w, unsigned char h, unsigned char attr);
void hide_rest_of_oam(void);

/* player.c */
void player_init(unsigned char mtx, unsigned char mty);
void player_frame(void);
void player_draw(void);
void player_hurt(unsigned char from_left, unsigned char dmg);
unsigned char mt_at(unsigned char x, unsigned char y);
unsigned char coll_at(unsigned char x, unsigned char y);

/* entities.c */
void entities_reset(void);
void clear_enemies(void);
void spawn_room_entities(const unsigned char *sp);
unsigned char spawn_enemy(unsigned char type, unsigned char x, unsigned char y);
unsigned char spawn_pickup(unsigned char type, unsigned char x, unsigned char y);
void spawn_effect(unsigned char x, unsigned char y);
void entities_frame(void);
void entities_draw(void);
void weapon_fire(void);
void spawn_garbage(unsigned char x, unsigned char y);
unsigned char attack_hits_box(unsigned char x, unsigned char y,
                              unsigned char w, unsigned char h);
extern unsigned char gold_count;
extern unsigned char hud_dirty;

/* boss.c */
void boss_init(void);
void boss_frame(void);
void boss_draw(void);
extern unsigned char boss_active, boss_hp, boss_defeated;
extern unsigned char e_type[], e_x[], e_y[], e_hp[], e_state[], e_timer[],
                     e_dir[], e_var[], e_sub[], e_flash[];

/* main.c */
void enter_state(unsigned char st);
extern unsigned char victory_pending;

#endif
