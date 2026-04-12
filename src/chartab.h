#ifndef CHARTAB_H
#define CHARTAB_H

#include <stdint.h>

/*
 * Character conversion tables.
 * Per RC702_BIOS_SPECIFICATION.md Section 15.
 *
 * OUTCON: 128 bytes, maps 0x00-0x7F before display output.
 * INCONV: 256 bytes, maps raw input 0x00-0xFF to internal codes.
 *
 * Tables are loaded from Track 0 sectors 3-5 at cold boot.
 * Identity mapping means no conversion (ASCII pass-through).
 */

#define OUTCON_SIZE  128
#define INCONV_SIZE  256

/* Character conversion state */
typedef struct {
    uint8_t outcon[OUTCON_SIZE];   /* output conversion table */
    uint8_t inconv[INCONV_SIZE];   /* input conversion table */
} chartab_t;

/* Initialize both tables to identity mapping */
void chartab_init_identity(chartab_t *ct);

/* Load tables from raw data (as read from Track 0 sectors 3-5) */
void chartab_load(chartab_t *ct, const uint8_t *data, uint16_t size);

/* Convert a character for output (0x00-0x7F only) */
uint8_t chartab_output(const chartab_t *ct, uint8_t ch);

/* Convert a character from input (full 0x00-0xFF range) */
uint8_t chartab_input(const chartab_t *ct, uint8_t ch);

#endif
