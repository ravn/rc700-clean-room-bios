#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

/*
 * CONFI Configuration Block — 128 bytes from Track 0, Sector 2.
 * All offsets and defaults per RC702_BIOS_SPECIFICATION.md Section 16.2.
 */

/* CONFI block offsets */
#define CONFI_CTC_CH0_MODE    0x00
#define CONFI_CTC_CH0_COUNT   0x01
#define CONFI_CTC_CH1_MODE    0x02
#define CONFI_CTC_CH1_COUNT   0x03
#define CONFI_CTC_CH2_MODE    0x04
#define CONFI_CTC_CH2_COUNT   0x05
#define CONFI_CTC_CH3_MODE    0x06
#define CONFI_CTC_CH3_COUNT   0x07

#define CONFI_SIO_A_START     0x08  /* 9 bytes */
#define CONFI_SIO_A_SIZE      9
#define CONFI_SIO_B_START     0x11  /* 11 bytes */
#define CONFI_SIO_B_SIZE      11

#define CONFI_DMA_MODE_START  0x1C  /* 4 bytes */
#define CONFI_CRT_PAR_START   0x20  /* 4 bytes */
#define CONFI_FDC_SPEC_START  0x24  /* 4 bytes */

#define CONFI_CURSOR          0x28
#define CONFI_CONV_TABLE      0x29
#define CONFI_BAUD_A_INDEX    0x2A
#define CONFI_BAUD_B_INDEX    0x2B
#define CONFI_XY_FLAG         0x2C
#define CONFI_MOTOR_TIMER     0x2D  /* 2 bytes, little-endian */

#define CONFI_DRIVE_TABLE     0x2F  /* 17 bytes (A..P + terminator) */
#define CONFI_DRIVE_COUNT     16
#define CONFI_DRIVE_TERM      0x3F

#define CONFI_HD_PARTITION    0x40  /* 4 bytes */
#define CONFI_CTC2_START      0x44  /* 3 bytes */
#define CONFI_BOOT_DEVICE     0x47

#define CONFI_SIZE            128

/* Drive format codes */
#define DRIVE_NOT_PRESENT     0xFF

/* Boot device codes */
#define BOOT_FLOPPY           0x00
#define BOOT_HARDDISK         0x01

/*
 * Baud rate table entry.
 * Derives from Section 17: baud = 614400 / (ctc_count * sio_clock_mode)
 */
typedef struct {
    uint32_t baud_rate;
    byte  ctc_count;     /* CTC time constant (1-256, 0 means 256) */
    byte  wr4_value;     /* SIO WR4: 0x44 = x16 clock, 0xC4 = x64 clock */
} baud_entry_t;

#define BAUD_TABLE_SIZE  12

extern const baud_entry_t baud_table[BAUD_TABLE_SIZE];

/* Default CONFI block (128 bytes) */
extern const byte confi_defaults[CONFI_SIZE];

/* Extract fields from a CONFI block */
byte  confi_get_ctc_count(const byte *confi, byte channel);
byte  confi_get_ctc_mode(const byte *confi, byte channel);
byte  confi_get_cursor(const byte *confi);
byte  confi_get_xy_flag(const byte *confi);
word confi_get_motor_timer(const byte *confi);
byte  confi_get_drive_format(const byte *confi, byte drive);
byte  confi_get_boot_device(const byte *confi);

/* Calculate baud rate from CTC count and WR4 value */
uint32_t baud_rate_calc(byte ctc_count, byte wr4);

/* Look up baud entry by index (0..11), returns NULL if out of range */
const baud_entry_t *baud_lookup(byte index);

#endif
