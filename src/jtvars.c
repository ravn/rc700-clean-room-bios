#include "jtvars.h"
#include "config.h"
#include <string.h>

void jtvars_init(jtvars_t *jt, const byte *confi) {
    /* Section 7.8: initialize runtime variables from CONFI block */

    /* Cursor addressing mode from CONFI XY flag */
    jt->adrmod = confi[CONFI_XY_FLAG];

    /* WR5 bits/char mask: extract from SIO config byte 6 (Ch.A) and 8 (Ch.B) */
    jt->wr5a = confi[CONFI_SIO_A_START + 6] & 0x60;
    jt->wr5b = confi[CONFI_SIO_B_START + 8] & 0x60;

    /* Machine type = 0 (RC702) */
    jt->mtype = 0;

    /* Copy drive format table (16 entries) */
    memcpy(jt->fd0, &confi[CONFI_DRIVE_TABLE], 16);

    /* Terminator */
    jt->fd0_term = 0xFF;

    /* Boot device */
    jt->bootd = confi[CONFI_BOOT_DEVICE];
}
