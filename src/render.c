/* render.c -- PPU-facing drawing: room rendering, text, OAM metasprites,
   and the small buffered-VRAM-write queue used for HUD updates while
   rendering is on (flushed during vblank). */
#include "game.h"

#define PPUCTRL_GAME (PPUCTRL_NMI_ENABLE | PPUCTRL_BG_PT_1000)

const unsigned char *room_map;

/* small VRAM write queue: [hi, lo, len, bytes...]*, terminated by len byte 0 */
#define VBUF_SIZE 96
static unsigned char vbuf[VBUF_SIZE];
static unsigned char vbuf_len;

void vbuf_reset(void) { vbuf_len = 0; }

static unsigned char *vbuf_entry(unsigned int addr, unsigned char len) {
    unsigned char *p;
    if (vbuf_len + 3u + len >= VBUF_SIZE) return 0;
    p = vbuf + vbuf_len;
    p[0] = (unsigned char)(addr >> 8);
    p[1] = (unsigned char)addr;
    p[2] = len;
    vbuf_len += 3 + len;
    return p + 3;
}

void vbuf_flush(void) {
    unsigned char i = 0, len;
    while (i < vbuf_len) {
        PPUSTATUS;
        PPUADDR = vbuf[i];
        PPUADDR = vbuf[i + 1];
        len = vbuf[i + 2];
        i += 3;
        while (len--) {
            PPUDATA = vbuf[i++];
        }
    }
    vbuf_len = 0;
}

/* ------------------------------------------------------------------ */

static unsigned char nmi_on;    /* BSS: 0 at boot */

void ppu_off(void) {
    if (nmi_on) wait_vblank();  /* no NMI yet at boot: don't deadlock */
    PPUMASK = 0;
    PPUCTRL = 0;                /* NMI off while we do long VRAM writes */
    nmi_on = 0;
}

void ppu_on(void) {
    PPUSTATUS;              /* reset latch */
    PPUCTRL = PPUCTRL_GAME;
    nmi_on = 1;
    wait_vblank();
    PPUSCROLL = 0;
    PPUSCROLL = 0;
    PPUMASK = PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR |
              PPUMASK_SHOW_BG_LC | PPUMASK_SHOW_SPR_LC;
}

void load_chr(void) {
    ppu_write(0x0000, chr_sprites, CHR_SPRITES_LEN);
    ppu_write(0x1000, chr_bg, CHR_BG_LEN);
    ppu_write(0x3F00, game_palette, 32);
}

void clear_nametable(void) {
    unsigned int i;
    ppu_set_addr(0x2000);
    for (i = 0; i < 0x400; ++i) PPUDATA = 0;
}

/* text screens: attribute palette 2 everywhere -> white font */
void text_screen_palette(void) {
    unsigned char i;
    ppu_set_addr(0x23C0);
    for (i = 0; i < 64; ++i) PPUDATA = 0xAA;
}

/* Restore the standard palette RAM (the title screen retints BG palette 0
   for the portrait). Rendering must be off. */
void load_palette(void) {
    ppu_write(0x3F00, game_palette, 32);
}

/* Draw a full room: 15 metatile rows -> 30 tile rows + 64 attribute bytes.
   Rendering must be off. Also latches the room's map pointer for collision. */
void draw_room(unsigned char room) {
    unsigned char mx, my;
    const unsigned char *map = rooms[room].map;
    const unsigned char *attr = rooms[room].attr;
    unsigned char mt;

    room_map = map;
    ppu_set_addr(0x2000);
    for (my = 0; my < 15; ++my) {
        const unsigned char *row = map + my * 16;
        for (mx = 0; mx < 16; ++mx) {
            mt = row[mx];
            PPUDATA = mt_tl[mt];
            PPUDATA = mt_tr[mt];
        }
        for (mx = 0; mx < 16; ++mx) {
            mt = row[mx];
            PPUDATA = mt_bl[mt];
            PPUDATA = mt_br[mt];
        }
    }
    ppu_set_addr(0x23C0);
    for (mx = 0; mx < 64; ++mx) PPUDATA = attr[mx];
}

/* ------------------------------------------------------------------ */
/* Text */

static unsigned char font_tile(char c) {
    if (c >= 'A' && c <= 'Z') return FONT_A + (c - 'A');
    if (c >= 'a' && c <= 'z') return FONT_A + (c - 'a');
    if (c >= '0' && c <= '9') return FONT_0 + (c - '0');
    switch (c) {
        case '.': return FONT_PERIOD;
        case '!': return FONT_BANG;
        case '-': return FONT_DASH;
        case '\'': return FONT_APOS;
        case ',': return FONT_COMMA;
        case ':': return FONT_COLON;
    }
    return 0; /* space and anything unknown */
}

/* direct write; rendering must be off */
void draw_text(unsigned char tx, unsigned char ty, const char *s) {
    ppu_set_addr(0x2000 + ty * 32 + tx);
    while (*s) PPUDATA = font_tile(*s++);
}

/* buffered write; safe while rendering (flushed next vblank) */
void vbuf_text(unsigned char tx, unsigned char ty, const char *s) {
    unsigned char n = 0, *p;
    const char *t = s;
    while (*t++) ++n;
    p = vbuf_entry(0x2000 + ty * 32 + tx, n);
    if (!p) return;
    while (*s) *p++ = font_tile(*s++);
}

void vbuf_tile(unsigned char tx, unsigned char ty, unsigned char tile) {
    unsigned char *p = vbuf_entry(0x2000 + ty * 32 + tx, 1);
    if (p) *p = tile;
}

/* one entry covering a horizontal run of tiles -- far cheaper to flush
   than per-tile entries (vblank budget!) */
void vbuf_run(unsigned char tx, unsigned char ty,
              const unsigned char *tiles, unsigned char len) {
    unsigned char *p = vbuf_entry(0x2000 + ty * 32 + tx, len);
    if (!p) return;
    while (len--) *p++ = *tiles++;
}

/* ------------------------------------------------------------------ */
/* OAM */

void draw_meta(unsigned char x, unsigned char y, unsigned char tile,
               unsigned char w, unsigned char h, unsigned char attr) {
    unsigned char r, c, o, sx;
    o = oam_idx;
    for (r = 0; r < h; ++r) {
        for (c = 0; c < w; ++c) {
            if (o >= 252) break;
            oam[o] = y + (r << 3) - 1;
            oam[o + 1] = tile++;
            oam[o + 2] = attr;
            sx = (attr & 0x40) ? (w - 1 - c) : c;
            oam[o + 3] = x + (sx << 3);
            o += 4;
        }
    }
    oam_idx = o;
}

void hide_rest_of_oam(void) {
    unsigned char o;
    for (o = oam_idx; ; o += 4) {   /* oam_idx is always a multiple of 4, <= 252 */
        oam[o] = 0xF8;
        if (o == 252) break;
    }
}
