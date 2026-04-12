#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"
#include "ringbuf.h"

/*
 * Serial I/O driver — SIO-A (data/printer) and SIO-B (console).
 * Per RC702_BIOS_SPECIFICATION.md Sections 9.1-9.7.
 *
 * Each channel has:
 * - A 256-byte receive ring buffer (filled by ISR)
 * - A TX ready flag (set by TX ISR, cleared when sending)
 * - A WR5 base value (bits/char mask, stored from config)
 * - Cached RR0 (external status)
 */

/* SIO port addresses */
#define SIO_A_DATA  0x08
#define SIO_A_CTRL  0x0A
#define SIO_B_DATA  0x09
#define SIO_B_CTRL  0x0B

/* WR5 signal construction (Section 9.5) */
#define WR5_DTR_RTS_TX  0x8A  /* DTR + TX enable + RTS */
#define WR5_DTR_TX      0x88  /* DTR + TX enable (no RTS) */
#define WR5_RELEASE     0x00  /* all off */

/* Serial channel state */
typedef struct {
    ringbuf_t rx_ring;            /* receive ring buffer */
    byte      rx_storage[SIO_BUFSZ]; /* ring buffer storage */
    byte      tx_ready;           /* 0x00=busy, 0xFF=ready */
    byte      wr5_base;           /* WR5 bits/char mask (from config) */
    byte      rr0_cached;         /* cached RR0 (external status) */
    byte      ctrl_port;          /* SIO control port address */
    byte      data_port;          /* SIO data port address */
} serial_ch_t;

/* Keyboard state */
typedef struct {
    ringbuf_t ring;
    byte      storage[KB_BUFSZ];
    byte      status;             /* 0xFF=data available, 0x00=empty */
} keyboard_t;

/* Initialize serial channel */
void serial_ch_init(serial_ch_t *ch, byte ctrl_port, byte data_port);

/* Initialize keyboard */
void keyboard_init(keyboard_t *kb);

/*
 * ISR entry points — called from interrupt handlers.
 * These must be called with interrupts disabled.
 */

/* RX ISR: store received byte in ring buffer */
void serial_rx_isr(serial_ch_t *ch);

/* TX ISR: reset TX interrupt, set ready flag */
void serial_tx_isr(serial_ch_t *ch);

/* External status ISR: cache RR0, reset condition */
void serial_ext_isr(serial_ch_t *ch);

/* Special condition ISR: read RR1, reset error */
void serial_special_isr(serial_ch_t *ch);

/* Keyboard ISR: read PIO and store in ring buffer */
void keyboard_isr(keyboard_t *kb);

/*
 * Main-line functions — called from BIOS entry points.
 */

/* Send a byte on SIO channel (waits for TX ready, timeout for SIO-B) */
void serial_send(serial_ch_t *ch, byte value);

/* Check if receive data is available */
int serial_rx_ready(const serial_ch_t *ch);

/* Read from receive ring buffer (blocks until data available) */
byte serial_receive(serial_ch_t *ch);

/* Check SIO-A RTS flow control — deassert RTS if high-water reached */
void serial_a_check_rts(serial_ch_t *ch);

/* Reassert RTS on SIO-A when buffer drained */
void serial_a_reassert_rts(serial_ch_t *ch);

#endif
