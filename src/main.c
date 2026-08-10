#include "nes.h"

static const unsigned char palette[32] = {
    0x0F, 0x01, 0x11, 0x21,   /* bg 0: black, dark blue, med blue, light blue  (Garbage Grove sky) */
    0x0F, 0x00, 0x10, 0x30,   /* bg 1: black, grays                            (masonry) */
    0x0F, 0x18, 0x28, 0x38,   /* bg 2: black, sickly greens */
    0x0F, 0x04, 0x14, 0x24,   /* bg 3: black, purples */
    0x0F, 0x17, 0x27, 0x30,   /* spr 0: Jimothy - black, browns, white mask */
    0x0F, 0x16, 0x26, 0x36,   /* spr 1: reserved (enemies) */
    0x0F, 0x01, 0x11, 0x21,   /* spr 2: reserved */
    0x0F, 0x19, 0x29, 0x39    /* spr 3: reserved (pickups) */
};

/* Tile 1: a solid 8x8 block used for both the floor strip and the Jimothy
   placeholder sprite until real CHR art exists. */
static const unsigned char tile1[16] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* plane 0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* plane 1 -> color index 1 */
};

#define PLAYER_START_X 120
#define PLAYER_START_Y 120

static unsigned char player_x = PLAYER_START_X;
static unsigned char player_y = PLAYER_START_Y;

static void draw_floor(void) {
    unsigned char col;
    ppu_set_addr(0x2360); /* nametable row 27 (near bottom of screen) */
    for (col = 0; col < 32; ++col) {
        PPUDATA = 1;
    }
}

int main(void) {
    ppu_write(0x0010, tile1, 16);   /* load tile 1 into CHR RAM */
    ppu_write(0x3F00, palette, 32);
    draw_floor();

    oam[0] = PLAYER_START_Y;
    oam[1] = 1;    /* tile index */
    oam[2] = 0;    /* attributes: palette 0, no flip */
    oam[3] = PLAYER_START_X;

    PPUCTRL = PPUCTRL_NMI_ENABLE;
    PPUMASK = PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR |
              PPUMASK_SHOW_BG_LC | PPUMASK_SHOW_SPR_LC;

    while (1) {
        unsigned char pad;

        wait_vblank();
        ppu_update();

        pad = pad_poll();
        if (pad & PAD_LEFT)  { if (player_x > 8)   player_x--; }
        if (pad & PAD_RIGHT) { if (player_x < 240) player_x++; }
        if (pad & PAD_UP)    { if (player_y > 8)   player_y--; }
        if (pad & PAD_DOWN)  { if (player_y < 200) player_y++; }

        oam[0] = player_y;
        oam[3] = player_x;
    }
}
