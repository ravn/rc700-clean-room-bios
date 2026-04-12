#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "chartab.h"

static void test_identity_init(void) {
    chartab_t ct;
    chartab_init_identity(&ct);

    /* Output table: identity for 0x00-0x7F */
    for (int i = 0; i < OUTCON_SIZE; i++)
        assert(ct.outcon[i] == (byte)i);

    /* Input table: identity for 0x00-0xFF */
    for (int i = 0; i < INCONV_SIZE; i++)
        assert(ct.inconv[i] == (byte)i);
}

static void test_output_conversion(void) {
    chartab_t ct;
    chartab_init_identity(&ct);

    /* Identity: ASCII passes through */
    assert(chartab_output(&ct, 'A') == 'A');
    assert(chartab_output(&ct, ' ') == ' ');
    assert(chartab_output(&ct, 0x7F) == 0x7F);

    /* Modify table: simulate Danish '@' -> some other code */
    ct.outcon[0x40] = 0x5B;
    assert(chartab_output(&ct, 0x40) == 0x5B);

    /* High bit is masked off (only 0x00-0x7F range) */
    assert(chartab_output(&ct, 0xC0) == ct.outcon[0x40]);
}

static void test_input_conversion(void) {
    chartab_t ct;
    chartab_init_identity(&ct);

    assert(chartab_input(&ct, 'A') == 'A');
    assert(chartab_input(&ct, 0xFF) == 0xFF);

    /* Modify table */
    ct.inconv[0x80] = 0x41;
    assert(chartab_input(&ct, 0x80) == 0x41);
}

static void test_load(void) {
    chartab_t ct;
    chartab_init_identity(&ct);

    /* Simulate loading from Track 0 sectors 3-5 (384 bytes) */
    byte data[384];
    /* Fill OUTCON portion with reversed values */
    for (int i = 0; i < 128; i++)
        data[i] = (byte)(127 - i);
    /* Fill INCONV portion with complemented values */
    for (int i = 0; i < 256; i++)
        data[128 + i] = (byte)(~i);

    chartab_load(&ct, data, 384);

    assert(ct.outcon[0] == 127);
    assert(ct.outcon[127] == 0);
    assert(ct.inconv[0] == 0xFF);
    assert(ct.inconv[0xFF] == 0x00);
}

static void test_partial_load(void) {
    chartab_t ct;
    chartab_init_identity(&ct);

    /* Load only OUTCON (128 bytes), INCONV stays identity */
    byte data[128];
    memset(data, 0x42, 128);
    chartab_load(&ct, data, 128);

    assert(ct.outcon[0] == 0x42);
    assert(ct.inconv[0] == 0x00);  /* unchanged identity */
    assert(ct.inconv[0x42] == 0x42);
}

int main(void) {
    test_identity_init();
    test_output_conversion();
    test_input_conversion();
    test_load();
    test_partial_load();
    printf("All chartab tests passed.\n");
    return 0;
}
