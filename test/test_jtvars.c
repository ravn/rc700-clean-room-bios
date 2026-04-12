#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jtvars.h"
#include "config.h"

static void test_jtvars_from_defaults(void) {
    jtvars_t jt;
    jtvars_init(&jt, confi_defaults);

    /* ADRMOD: from CONFI XY flag, default = 0 (XY mode) */
    assert(jt.adrmod == 0);

    /* WR5A: from SIO-A config byte 6 (offset 0x0E), value 0x60, mask 0x60 = 0x60 */
    assert(jt.wr5a == 0x60);  /* 8-bit TX */

    /* WR5B: from SIO-B config byte 8 (offset 0x19), value 0x60, mask 0x60 = 0x60 */
    assert(jt.wr5b == 0x60);  /* 8-bit TX */

    /* Machine type = 0 (RC702) */
    assert(jt.mtype == 0);

    /* Drive A = 0x08 (8" DD), Drive B = 0x08, Drive C = 0x20 (HD) */
    assert(jt.fd0[0] == 0x08);
    assert(jt.fd0[1] == 0x08);
    assert(jt.fd0[2] == 0x20);
    assert(jt.fd0[3] == 0xFF);  /* not present */

    /* Terminator */
    assert(jt.fd0_term == 0xFF);

    /* Boot device = floppy */
    assert(jt.bootd == 0x00);

    printf("  JTVARS from defaults: verified\n");
}

static void test_jtvars_size(void) {
    assert(sizeof(jtvars_t) == JTVARS_SIZE);
    printf("  JTVARS size: %d bytes (expected %d)\n",
           (int)sizeof(jtvars_t), JTVARS_SIZE);
}

static void test_jtvars_offsets(void) {
    jtvars_t jt;
    byte *base = (byte *)&jt;

    /* Verify field offsets match the spec */
    assert((byte *)&jt.adrmod - base == JT_ADRMOD);
    assert((byte *)&jt.wr5a - base == JT_WR5A);
    assert((byte *)&jt.wr5b - base == JT_WR5B);
    assert((byte *)&jt.mtype - base == JT_MTYPE);
    assert((byte *)&jt.fd0[0] - base == JT_FD0);
    assert((byte *)&jt.fd0_term - base == JT_FD0_TERM);
    assert((byte *)&jt.bootd - base == JT_BOOTD);

    printf("  JTVARS field offsets: verified\n");
}

static void test_bios_addresses(void) {
    /* Verify derived addresses */
    /* BIOS_BASE is currently 0xC000 (temporary, will be 0xDA00 when optimized) */
    assert(JTVARS_ADDR == BIOS_BASE + JTVARS_OFFSET);
    assert(BIOS_JP_SIZE == 51);  /* 17 * 3 */
    assert(BIOS_EXT_ADDR == BIOS_BASE + BIOS_EXT_OFFSET);

    printf("  BIOS addresses: BASE=0x%04X JTVARS=0x%04X EXT=0x%04X\n",
           BIOS_BASE, JTVARS_ADDR, BIOS_EXT_ADDR);
}

int main(void) {
    test_jtvars_from_defaults();
    test_jtvars_size();
    test_jtvars_offsets();
    test_bios_addresses();
    printf("All jtvars tests passed.\n");
    return 0;
}
