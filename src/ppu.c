#include "nes.h"

void wait_vblank(void) {
    /* Wait for a FRESH vblank edge. If the flag is already set we are an
       unknown distance into (or past) this vblank; committing now can run
       out of window and spill writes into rendering, where the PPU address
       register is live (observed: HUD bytes for $2022 landing in the
       pattern table at $1022, eating the 'B' glyph). Discard the stale
       vblank and sync to the next NMI instead -- costs one frame only on
       overrun frames. */
    nmi_flag = 0;
    while (!nmi_flag) { }
    nmi_flag = 0;
}

void ppu_update(void) {
    OAMADDR = 0;
    OAMDMA = 0x02; /* DMA source page = oam[] (see nes-uxrom.cfg OAM_BUF @ $0200) */
}

void ppu_set_addr(unsigned int addr) {
    PPUSTATUS; /* reset the address latch */
    PPUADDR = (unsigned char)(addr >> 8);
    PPUADDR = (unsigned char)(addr & 0xFF);
}

void ppu_write(unsigned int addr, const unsigned char *data, unsigned int len) {
    unsigned int i;
    ppu_set_addr(addr);
    for (i = 0; i < len; ++i) {
        PPUDATA = data[i];
    }
}
