#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

static void test_confi_defaults_size(void) {
    assert(sizeof(confi_defaults) == 128);
}

static void test_ctc_defaults(void) {
    const uint8_t *c = confi_defaults;

    /* CTC Ch.0: timer mode 0x47, count 1 (38400 baud for SIO-A) */
    assert(confi_get_ctc_mode(c, 0) == 0x47);
    assert(confi_get_ctc_count(c, 0) == 0x01);

    /* CTC Ch.1: timer mode 0x47, count 1 (38400 baud for SIO-B) */
    assert(confi_get_ctc_mode(c, 1) == 0x47);
    assert(confi_get_ctc_count(c, 1) == 0x01);

    /* CTC Ch.2: counter mode 0xD7, count 1 (CRT refresh) */
    assert(confi_get_ctc_mode(c, 2) == 0xD7);
    assert(confi_get_ctc_count(c, 2) == 0x01);

    /* CTC Ch.3: counter mode 0xD7, count 1 (FDC interrupt) */
    assert(confi_get_ctc_mode(c, 3) == 0xD7);
    assert(confi_get_ctc_count(c, 3) == 0x01);

    /* Out of range */
    assert(confi_get_ctc_mode(c, 4) == 0);
    assert(confi_get_ctc_count(c, 4) == 0);
}

static void test_sio_a_defaults(void) {
    const uint8_t *sio_a = &confi_defaults[CONFI_SIO_A_START];
    assert(sio_a[0] == 0x18);  /* WR0: channel reset */
    assert(sio_a[1] == 0x04);  /* WR0: select WR4 */
    assert(sio_a[2] == 0x44);  /* WR4: x16 clock, 1 stop, no parity */
    assert(sio_a[3] == 0x03);  /* WR0: select WR3 */
    assert(sio_a[4] == 0xE1);  /* WR3: 8-bit RX, auto enables, RX enable */
    assert(sio_a[5] == 0x05);  /* WR0: select WR5 */
    assert(sio_a[6] == 0x60);  /* WR5: 8-bit TX, TX disabled */
    assert(sio_a[7] == 0x01);  /* WR0: select WR1 */
    assert(sio_a[8] == 0x1B);  /* WR1: RX/TX/ext int enabled */
}

static void test_sio_b_defaults(void) {
    const uint8_t *sio_b = &confi_defaults[CONFI_SIO_B_START];
    assert(sio_b[0] == 0x18);   /* WR0: channel reset */
    assert(sio_b[1] == 0x02);   /* WR0: select WR2 */
    assert(sio_b[2] == 0x10);   /* WR2: interrupt vector base */
    assert(sio_b[3] == 0x04);   /* WR0: select WR4 */
    assert(sio_b[4] == 0x44);   /* WR4: x16, 1 stop, no parity */
    assert(sio_b[5] == 0x03);   /* WR0: select WR3 */
    assert(sio_b[6] == 0xE1);   /* WR3: 8-bit RX, auto enables */
    assert(sio_b[7] == 0x05);   /* WR0: select WR5 */
    assert(sio_b[8] == 0x60);   /* WR5: 8-bit TX, TX disabled */
    assert(sio_b[9] == 0x01);   /* WR0: select WR1 */
    assert(sio_b[10] == 0x1F);  /* WR1: RX/TX/ext/status/parity */
}

static void test_crt_params(void) {
    assert(confi_defaults[CONFI_CRT_PAR_START + 0] == 0x4F);  /* 80 chars */
    assert(confi_defaults[CONFI_CRT_PAR_START + 1] == 0x98);  /* 25 rows */
    assert(confi_defaults[CONFI_CRT_PAR_START + 2] == 0x7A);  /* retrace */
    assert(confi_defaults[CONFI_CRT_PAR_START + 3] == 0x6D);  /* cursor */
}

static void test_user_settings(void) {
    const uint8_t *c = confi_defaults;
    assert(confi_get_cursor(c) == 0x00);
    assert(c[CONFI_CONV_TABLE] == 0x00);
    assert(c[CONFI_BAUD_A_INDEX] == 0x06);
    assert(c[CONFI_BAUD_B_INDEX] == 0x06);
    assert(confi_get_xy_flag(c) == 0x00);
    assert(confi_get_motor_timer(c) == 250);
}

static void test_drive_table(void) {
    const uint8_t *c = confi_defaults;
    assert(confi_get_drive_format(c, 0) == 0x08);  /* Drive A: 8" DD */
    assert(confi_get_drive_format(c, 1) == 0x08);  /* Drive B: 8" DD */
    assert(confi_get_drive_format(c, 2) == 0x20);  /* Drive C: HD */
    assert(confi_get_drive_format(c, 3) == 0xFF);  /* Drive D: not present */
    assert(confi_get_drive_format(c, 15) == 0xFF); /* Drive P: not present */
    assert(confi_get_drive_format(c, 16) == DRIVE_NOT_PRESENT); /* out of range */
}

static void test_boot_device(void) {
    const uint8_t *c = confi_defaults;
    assert(confi_get_boot_device(c) == BOOT_FLOPPY);
}

static void test_reserved_zero(void) {
    /* Bytes 0x48..0x7F must be zero */
    for (int i = 0x48; i < CONFI_SIZE; i++)
        assert(confi_defaults[i] == 0);
}

static void test_baud_rate_calc(void) {
    /* 614400 / (1 * 16) = 38400 */
    assert(baud_rate_calc(1, 0x44) == 38400);
    /* 614400 / (2 * 16) = 19200 */
    assert(baud_rate_calc(2, 0x44) == 19200);
    /* 614400 / (4 * 16) = 9600 */
    assert(baud_rate_calc(4, 0x44) == 9600);
    /* 614400 / (32 * 16) = 1200 */
    assert(baud_rate_calc(32, 0x44) == 1200);
    /* 614400 / (64 * 16) = 600 */
    assert(baud_rate_calc(64, 0x44) == 600);

    /* x64 clock mode */
    /* 614400 / (32 * 64) = 300 */
    assert(baud_rate_calc(32, 0xC4) == 300);
    /* 614400 / (64 * 64) = 150 */
    assert(baud_rate_calc(64, 0xC4) == 150);
    /* 614400 / (128 * 64) = 75 */
    assert(baud_rate_calc(128, 0xC4) == 75);
    /* 614400 / (192 * 64) = 50 */
    assert(baud_rate_calc(192, 0xC4) == 50);

    /* 110 baud: 614400 / (87 * 64) = 110.3... truncated to 110 */
    assert(baud_rate_calc(87, 0xC4) == 110);

    /* Invalid WR4 */
    assert(baud_rate_calc(1, 0x00) == 0);
}

static void test_baud_table(void) {
    /* Verify table entries match the formula */
    for (int i = 0; i < BAUD_TABLE_SIZE; i++) {
        const baud_entry_t *e = &baud_table[i];
        uint32_t computed = baud_rate_calc(e->ctc_count, e->wr4_value);
        assert(computed == e->baud_rate);
    }

    /* Verify table is in descending baud rate order */
    for (int i = 1; i < BAUD_TABLE_SIZE; i++)
        assert(baud_table[i].baud_rate < baud_table[i - 1].baud_rate);
}

static void test_baud_lookup(void) {
    assert(baud_lookup(0)->baud_rate == 38400);
    assert(baud_lookup(6)->baud_rate == 600);
    assert(baud_lookup(11)->baud_rate == 50);
    assert(baud_lookup(12) == NULL);
    assert(baud_lookup(255) == NULL);
}

static void test_baud_index_matches_confi(void) {
    /* Default baud index in CONFI is 6 for both channels */
    /* Index 6 should be 600 baud */
    const baud_entry_t *e = baud_lookup(confi_defaults[CONFI_BAUD_A_INDEX]);
    assert(e != NULL);
    assert(e->baud_rate == 600);

    /* But default CTC count is 1 → 38400 baud.
     * The baud index in CONFI is a display value for the setup menu,
     * not the actual CTC programming. The CTC count is what matters. */
    assert(baud_rate_calc(confi_defaults[CONFI_CTC_CH0_COUNT], 0x44) == 38400);
    assert(baud_rate_calc(confi_defaults[CONFI_CTC_CH1_COUNT], 0x44) == 38400);
}

int main(void) {
    test_confi_defaults_size();
    test_ctc_defaults();
    test_sio_a_defaults();
    test_sio_b_defaults();
    test_crt_params();
    test_user_settings();
    test_drive_table();
    test_boot_device();
    test_reserved_zero();
    test_baud_rate_calc();
    test_baud_table();
    test_baud_lookup();
    test_baud_index_matches_confi();
    printf("All config tests passed.\n");
    return 0;
}
