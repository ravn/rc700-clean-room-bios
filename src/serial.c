#include "serial.h"
#include "hal.h"

/* PIO port for keyboard */
#define PIO_A_DATA  0x10

void serial_ch_init(serial_ch_t *ch, byte ctrl_port, byte data_port) {
    ringbuf_init(&ch->rx_ring, ch->rx_storage, (byte)SIO_MASK);
    ch->tx_ready = 0x00;
    ch->wr5_base = 0x60;  /* default: 8-bit TX */
    ch->rr0_cached = 0;
    ch->ctrl_port = ctrl_port;
    ch->data_port = data_port;
}

void keyboard_init(keyboard_t *kb) {
    ringbuf_init(&kb->ring, kb->storage, KB_MASK);
    kb->status = 0x00;
}

/*
 * ISR entry points
 */

void serial_rx_isr(serial_ch_t *ch) {
    byte data = hal_in(ch->data_port);
    ringbuf_put(&ch->rx_ring, data);
}

void serial_tx_isr(serial_ch_t *ch) {
    /* Reset TX interrupt pending: write 0x28 to control port */
    hal_out(ch->ctrl_port, 0x28);
    ch->tx_ready = 0xFF;
}

void serial_ext_isr(serial_ch_t *ch) {
    /* Read and cache RR0 (already selected by default) */
    ch->rr0_cached = hal_in(ch->ctrl_port);
    /* Reset external/status interrupt: write 0x10 */
    hal_out(ch->ctrl_port, 0x10);
}

void serial_special_isr(serial_ch_t *ch) {
    /* Select RR1: write 0x01 to control port, then read */
    hal_out(ch->ctrl_port, 0x01);
    (void)hal_in(ch->ctrl_port);  /* read and discard RR1 */
    /* Reset error: write 0x30 */
    hal_out(ch->ctrl_port, 0x30);
}

void keyboard_isr(keyboard_t *kb) {
    byte key = hal_in(PIO_A_DATA);
    if (!ringbuf_full(&kb->ring)) {
        ringbuf_put(&kb->ring, key);
        kb->status = 0xFF;
    }
    /* If full, silently discard per spec */
}

/*
 * Main-line functions
 */

void serial_send(serial_ch_t *ch, byte value) {
    /* Wait for TX ready (set by TX ISR) */
    /* For SIO-B console output, Section 9.7 specifies a 255-iteration
     * timeout to avoid hanging on disconnected serial cable */
    byte timeout = 255;
    while (ch->tx_ready == 0x00 && timeout > 0)
        timeout--;

    if (ch->tx_ready == 0x00)
        return;  /* timeout — skip output */

    hal_di();
    ch->tx_ready = 0x00;

    /* Write WR5: select register 5 */
    hal_out(ch->ctrl_port, 0x05);
    /* Write WR5 value: base + DTR + TX enable + RTS */
    hal_out(ch->ctrl_port, ch->wr5_base + WR5_DTR_RTS_TX);

    /* Write WR1: select register 1 */
    hal_out(ch->ctrl_port, 0x01);
    /* Write WR1 value: interrupts enabled */
    hal_out(ch->ctrl_port, 0x1B);

    /* Write data */
    hal_out(ch->data_port, value);

    hal_ei();
}

int serial_rx_ready(const serial_ch_t *ch) {
    return ringbuf_has_data(&ch->rx_ring);
}

byte serial_receive(serial_ch_t *ch) {
    return ringbuf_get(&ch->rx_ring);
}

void serial_a_check_rts(serial_ch_t *ch) {
    if (ringbuf_count(&ch->rx_ring) >= SIO_A_HIGHWATER) {
        /* Deassert RTS */
        hal_out(ch->ctrl_port, 0x05);
        hal_out(ch->ctrl_port, ch->wr5_base + WR5_DTR_TX);
    }
}

void serial_a_reassert_rts(serial_ch_t *ch) {
    /* Reassert RTS only when buffer is completely empty */
    if (!ringbuf_has_data(&ch->rx_ring)) {
        hal_out(ch->ctrl_port, 0x05);
        hal_out(ch->ctrl_port, ch->wr5_base + WR5_DTR_RTS_TX);
    }
}
