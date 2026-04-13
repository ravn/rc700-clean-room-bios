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
        ; sdcccall(1): port in A, return in A.
        ; B must be 0 — IN A,(C) uses B:C as 16-bit I/O address.
        ld c, a         ; C = port
        ld b, #0        ; B = 0 (high byte of port address)
        in a, (c)       ; A = IN(0x00:port)
        ld l, a         ; L = A — covers both return conventions
    __endasm;
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
