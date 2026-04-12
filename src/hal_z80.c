/*
 * Z80 HAL implementation — real hardware I/O via SDCC extensions.
 * Only compiled for the Z80 target (not native tests).
 */

#include "hal.h"

/* SDCC port I/O intrinsics */
void hal_out(byte port, byte value) {
    (void)port;
    (void)value;
    __asm
        ld a, 4(ix)     ; port
        ld c, a
        ld a, 5(ix)     ; value
        out (c), a
    __endasm;
}

byte hal_in(byte port) {
    (void)port;
    __asm
        ld a, 4(ix)     ; port
        ld c, a
        in l, (c)       ; return value in L
    __endasm;
    return 0;  /* unreachable — silences warning */
}

void hal_di(void) {
    __asm
        di
    __endasm;
}

void hal_ei(void) {
    __asm
        ei
    __endasm;
}
