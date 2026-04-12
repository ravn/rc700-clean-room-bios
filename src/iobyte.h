#ifndef IOBYTE_H
#define IOBYTE_H

#include <stdint.h>

/*
 * IOBYTE bit field extraction (CP/M 2.2 Table 6-4).
 *
 * Bit layout:
 *   7:6 = LST:  5:4 = PUN:  3:2 = RDR:  1:0 = CON:
 *
 * Each returns 0..3.
 */
uint8_t iobyte_con_mode(uint8_t iobyte);
uint8_t iobyte_rdr_mode(uint8_t iobyte);
uint8_t iobyte_pun_mode(uint8_t iobyte);
uint8_t iobyte_lst_mode(uint8_t iobyte);

/* CON: mode constants */
#define CON_TTY  0  /* SIO-B serial (+ CRT display fallthrough) */
#define CON_CRT  1  /* Keyboard + CRT only */
#define CON_BAT  2  /* Batch (RDR:/LST:) */
#define CON_UC1  3  /* Joined: keyboard OR SIO-B, output to both */

/* LST: mode constants */
#define LST_TTY  0  /* SIO-B serial */
#define LST_CRT  1  /* CRT display */
#define LST_LPT  2  /* SIO-A printer */
#define LST_UL1  3  /* Parked */

/* PUN: mode constants */
#define PUN_TTY  0  /* SIO-A */
#define PUN_CRT  1  /* SIO-A (same as TTY) */
#define PUN_BAT  2  /* SIO-B */
#define PUN_UC1  3  /* Parked */

/* RDR: mode constants */
#define RDR_TTY  0  /* SIO-A (PTR, RTS flow control) */
#define RDR_CRT  1  /* SIO-A (PTR, same as TTY) */
#define RDR_BAT  2  /* SIO-B (UR1) */
#define RDR_UC1  3  /* Parked (returns EOF = 0x1A) */

/* Preset IOBYTE values */
#define IOBYTE_JOINED  0x97  /* CON:UC1, RDR:PTR, PUN:PTP, LST:LPT */
#define IOBYTE_LOCAL   0x95  /* CON:CRT, RDR:PTR, PUN:PTP, LST:LPT */

/* CON: input source queries */
int iobyte_keyboard_allowed(uint8_t iobyte);  /* true for CRT(1), UC1(3) */
int iobyte_serial_allowed(uint8_t iobyte);    /* true for TTY(0), BAT(2), UC1(3) */

#endif
