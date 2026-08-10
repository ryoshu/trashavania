#include "nes.h"

void wait_vblank(void) {
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
