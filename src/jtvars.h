#ifndef JTVARS_H
#define JTVARS_H

#include "types.h"

/*
 * JTVARS Runtime ABI — Section 19.
 *
 * 22 bytes at BIOS_BASE + 0x33 (0xDA33 for 56K).
 * External programs (CONFI.COM, FORMAT.COM) depend on these exact offsets.
 *
 * The standard BIOS JP table occupies 0xDA00-0xDA32 (17 entries x 3 bytes = 51).
 * JTVARS immediately follows at 0xDA33.
 * Extended entry points follow at 0xDA49 (BIOS_BASE + 0x49).
 */

/* Temporary: BIOS at 0xC000 to fit in memory.
 * Will move to 0xDA00 when code is optimized to fit. */
#define BIOS_BASE    0xC000
#define JTVARS_OFFSET 0x33
#define JTVARS_ADDR  (BIOS_BASE + JTVARS_OFFSET)  /* 0xDA33 */

/* JTVARS field offsets within the block */
#define JT_ADRMOD    0    /* 1 byte: cursor addressing mode (0=XY, 1=YX) */
#define JT_WR5A      1    /* 1 byte: SIO-A WR5 bits/char mask */
#define JT_WR5B      2    /* 1 byte: SIO-B WR5 bits/char mask */
#define JT_MTYPE     3    /* 1 byte: machine type (0=RC702) */
#define JT_FD0       4    /* 16 bytes: drive format table */
#define JT_FD0_TERM 20    /* 1 byte: terminator (0xFF) */
#define JT_BOOTD    21    /* 1 byte: boot device */

#define JTVARS_SIZE  22

/* JTVARS structure */
typedef struct {
    byte adrmod;            /* cursor addressing mode */
    byte wr5a;              /* SIO-A WR5 bits/char */
    byte wr5b;              /* SIO-B WR5 bits/char */
    byte mtype;             /* machine type */
    byte fd0[16];           /* drive format table */
    byte fd0_term;          /* terminator = 0xFF */
    byte bootd;             /* boot device */
} jtvars_t;

/* Initialize JTVARS from the CONFI configuration block */
void jtvars_init(jtvars_t *jt, const byte *confi);

/* CP/M BIOS jump table: 17 entries (BOOT through SECTRAN) */
#define BIOS_JP_ENTRIES  17
#define BIOS_JP_SIZE     (BIOS_JP_ENTRIES * 3)  /* 51 bytes */

/* Extended entry points at BIOS_BASE + 0x4A */
#define BIOS_EXT_OFFSET  0x4A
#define BIOS_EXT_ADDR    (BIOS_BASE + BIOS_EXT_OFFSET)  /* 0xDA4A */

#endif
