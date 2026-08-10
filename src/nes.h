#ifndef NES_H
#define NES_H

#define PPUCTRL   (*(volatile unsigned char *)0x2000)
#define PPUMASK   (*(volatile unsigned char *)0x2001)
#define PPUSTATUS (*(volatile unsigned char *)0x2002)
#define OAMADDR   (*(volatile unsigned char *)0x2003)
#define OAMDATA   (*(volatile unsigned char *)0x2004)
#define PPUSCROLL (*(volatile unsigned char *)0x2005)
#define PPUADDR   (*(volatile unsigned char *)0x2006)
#define PPUDATA   (*(volatile unsigned char *)0x2007)
#define OAMDMA    (*(volatile unsigned char *)0x4014)
#define APUFRAME  (*(volatile unsigned char *)0x4017)
#define DMCFREQ   (*(volatile unsigned char *)0x4010)
#define JOYPAD1   (*(volatile unsigned char *)0x4016)
#define JOYPAD2   (*(volatile unsigned char *)0x4017)

/* PPUCTRL bits */
#define PPUCTRL_NMI_ENABLE   0x80
#define PPUCTRL_SPR_8X16     0x20
#define PPUCTRL_BG_PT_1000   0x10
#define PPUCTRL_SPR_PT_1000  0x08

/* PPUMASK bits */
#define PPUMASK_SHOW_SPR     0x10
#define PPUMASK_SHOW_BG      0x08
#define PPUMASK_SHOW_SPR_LC  0x04
#define PPUMASK_SHOW_BG_LC   0x02

/* Controller button bits, as read from JOYPAD1/JOYPAD2 in order A,B,Select,
   Start,Up,Down,Left,Right */
#define PAD_A      0x80
#define PAD_B      0x40
#define PAD_SELECT 0x20
#define PAD_START  0x10
#define PAD_UP     0x08
#define PAD_DOWN   0x04
#define PAD_LEFT   0x02
#define PAD_RIGHT  0x01

/* Set by the NMI handler in crt0.s once per vblank; wait_vblank() clears it. */
extern volatile unsigned char nmi_flag;

/* Page-aligned OAM shadow buffer (see OAM_BUF segment in nes-uxrom.cfg),
   DMA'd to real OAM by ppu_update(). */
extern unsigned char oam[256];

void wait_vblank(void);
void ppu_update(void);
void ppu_set_addr(unsigned int addr);
void ppu_write(unsigned int addr, const unsigned char *data, unsigned int len);

unsigned char pad_poll(void);

#endif
