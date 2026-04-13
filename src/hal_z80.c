/*
 * Z80 HAL implementation — real hardware I/O via SDCC extensions.
 * Only compiled for the Z80 target (not native tests).
 *
 * With sdcccall(1): 1st byte param in A, 2nd byte param in L.
 * Return value in L.
 */

#include "hal.h"

void hal_out(byte port, byte value) {
    (void)port;
    (void)value;
    __asm
        ; sdcccall(1): port in A, value in L
        ld c, a         ; C = port
        ld a, l         ; A = value
        out (c), a
    __endasm;
}

byte hal_in(byte port) {
    (void)port;
    __asm
        ; sdcccall(1): port in A, return in L
        ld c, a         ; C = port
        in l, (c)       ; L = result
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
