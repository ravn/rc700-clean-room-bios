#ifndef SECTOR_H
#define SECTOR_H

#include "types.h"

/*
 * Sector translation tables.
 *
 * Each table maps CP/M logical sector numbers (0-based index)
 * to physical sector numbers (1-based values).
 */

#define TRAN0_SIZE   26  /* 8" SS FM 128 B/S, skew 6 */
#define TRAN8_SIZE   15  /* 8" DD MFM 512 B/S, skew 4 */
#define TRAN16_SIZE   9  /* 5.25" DD MFM 512 B/S, skew 2 */
#define TRAN24_SIZE  26  /* 8" DD MFM 256 B/S, identity */

extern const byte tran0[TRAN0_SIZE];
extern const byte tran8[TRAN8_SIZE];
extern const byte tran16[TRAN16_SIZE];
extern const byte tran24[TRAN24_SIZE];

/* Translate logical sector to physical sector using a translation table.
 * logical is 0-based index into the table.
 * Returns the 1-based physical sector number, or 0 on error. */
byte sector_translate(const byte *table, byte table_size, byte logical);

#endif
