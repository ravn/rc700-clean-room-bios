#include "chartab.h"
#include <string.h>

void chartab_init_identity(chartab_t *ct) {
    for (int i = 0; i < OUTCON_SIZE; i++)
        ct->outcon[i] = (byte)i;
    for (int i = 0; i < INCONV_SIZE; i++)
        ct->inconv[i] = (byte)i;
}

void chartab_load(chartab_t *ct, const byte *data, word size) {
    /* Sector 3 = 128 bytes OUTCON, sectors 4-5 = 256 bytes INCONV */
    if (size >= OUTCON_SIZE)
        memcpy(ct->outcon, data, OUTCON_SIZE);
    if (size >= OUTCON_SIZE + INCONV_SIZE)
        memcpy(ct->inconv, data + OUTCON_SIZE, INCONV_SIZE);
}

byte chartab_output(const chartab_t *ct, byte ch) {
    return ct->outcon[ch & 0x7F];
}

byte chartab_input(const chartab_t *ct, byte ch) {
    return ct->inconv[ch];
}
