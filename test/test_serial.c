#include <assert.h>
#include <stdio.h>
#include "serial.h"
#include "hal.h"
#include "hal_mock.h"

static void test_serial_init(void) {
    serial_ch_t ch;
    serial_ch_init(&ch, SIO_A_CTRL, SIO_A_DATA);
    assert(ch.tx_ready == 0x00);
    assert(ch.wr5_base == 0x60);
    assert(!ringbuf_has_data(&ch.rx_ring));
}

static void test_keyboard_init(void) {
    keyboard_t kb;
    keyboard_init(&kb);
    assert(kb.status == 0x00);
    assert(!ringbuf_has_data(&kb.ring));
}

static void test_rx_isr(void) {
    serial_ch_t ch;
    hal_mock_reset();
    serial_ch_init(&ch, SIO_B_CTRL, SIO_B_DATA);

    /* Simulate receiving a byte: put 0x41 on the data port */
    hal_mock_set_port(SIO_B_DATA, 0x41);
    serial_rx_isr(&ch);

    assert(serial_rx_ready(&ch));
    assert(serial_receive(&ch) == 0x41);
    assert(!serial_rx_ready(&ch));
}

static void test_tx_isr(void) {
    serial_ch_t ch;
    hal_mock_reset();
    serial_ch_init(&ch, SIO_A_CTRL, SIO_A_DATA);
    ch.tx_ready = 0x00;

    serial_tx_isr(&ch);
    assert(ch.tx_ready == 0xFF);
    /* Should have written 0x28 to control port */
    assert(hal_mock_get_port(SIO_A_CTRL) == 0x28);
}

static void test_ext_isr(void) {
    serial_ch_t ch;
    hal_mock_reset();
    serial_ch_init(&ch, SIO_B_CTRL, SIO_B_DATA);

    /* Simulate RR0 with DCD set */
    hal_mock_set_port(SIO_B_CTRL, 0x08);
    serial_ext_isr(&ch);

    assert(ch.rr0_cached == 0x08);
    /* Should have written 0x10 to reset ext status */
    assert(hal_mock_get_port(SIO_B_CTRL) == 0x10);
}

static void test_keyboard_isr(void) {
    keyboard_t kb;
    hal_mock_reset();
    keyboard_init(&kb);

    /* Simulate key press */
    hal_mock_set_port(0x10, 'A');
    keyboard_isr(&kb);
    assert(kb.status == 0xFF);
    assert(ringbuf_has_data(&kb.ring));
    assert(ringbuf_get(&kb.ring) == 'A');
}

static void test_keyboard_overflow(void) {
    keyboard_t kb;
    hal_mock_reset();
    keyboard_init(&kb);

    /* Fill keyboard buffer */
    for (int i = 0; i < KB_BUFSZ - 1; i++) {
        hal_mock_set_port(0x10, (byte)('A' + i));
        keyboard_isr(&kb);
    }
    assert(ringbuf_full(&kb.ring));

    /* Next key should be silently discarded */
    hal_mock_set_port(0x10, 'Z');
    keyboard_isr(&kb);

    /* Buffer still has original data */
    for (int i = 0; i < KB_BUFSZ - 1; i++)
        assert(ringbuf_get(&kb.ring) == (byte)('A' + i));
}

static void test_rts_flow_control(void) {
    serial_ch_t ch;
    hal_mock_reset();
    serial_ch_init(&ch, SIO_A_CTRL, SIO_A_DATA);

    /* Fill to high-water mark */
    for (int i = 0; i < SIO_A_HIGHWATER; i++)
        ringbuf_put(&ch.rx_ring, (byte)i);

    assert(ringbuf_count(&ch.rx_ring) >= SIO_A_HIGHWATER);

    /* Check RTS — should deassert */
    serial_a_check_rts(&ch);
    /* Last write to control port should be WR5 with no RTS */
    assert(hal_mock_get_port(SIO_A_CTRL) == (ch.wr5_base + WR5_DTR_TX));

    /* Drain the buffer */
    while (ringbuf_has_data(&ch.rx_ring))
        ringbuf_get(&ch.rx_ring);

    /* Reassert RTS */
    hal_mock_reset();
    serial_a_reassert_rts(&ch);
    assert(hal_mock_get_port(SIO_A_CTRL) == (ch.wr5_base + WR5_DTR_RTS_TX));
}

static void test_rts_not_reasserted_if_data(void) {
    serial_ch_t ch;
    hal_mock_reset();
    serial_ch_init(&ch, SIO_A_CTRL, SIO_A_DATA);

    /* Put one byte — buffer not empty */
    ringbuf_put(&ch.rx_ring, 0x42);

    /* Reassert should NOT happen because buffer still has data */
    serial_a_reassert_rts(&ch);
    /* Control port should not have been written */
    assert(hal_mock_get_port(SIO_A_CTRL) == 0x00);
}

int main(void) {
    test_serial_init();
    test_keyboard_init();
    test_rx_isr();
    test_tx_isr();
    test_ext_isr();
    test_keyboard_isr();
    test_keyboard_overflow();
    test_rts_flow_control();
    test_rts_not_reasserted_if_data();
    printf("All serial tests passed.\n");
    return 0;
}
