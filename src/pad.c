#include "nes.h"

unsigned char pad_poll(void) {
    unsigned char i, result = 0;

    JOYPAD1 = 1;
    JOYPAD1 = 0;

    for (i = 0; i < 8; ++i) {
        result <<= 1;
        result |= (JOYPAD1 & 0x01);
    }
    return result;
}
