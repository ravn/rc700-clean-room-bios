#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "interrupt.h"
#include "hal.h"

/* Port write capture */
#define MAX_WRITES 256
static struct { byte port; byte value; } writes[MAX_WRITES];
static int write_count;

/* Port read capture — returns predefined values */
static byte read_values[256];  /* indexed by port */

void hal_out(byte port, byte value) {
    if (write_count < MAX_WRITES) {
        writes[write_count].port = port;
        writes[write_count].value = value;
        write_count++;
    }
}

byte hal_in(byte port) {
    return read_values[port];
}

void hal_di(void) {}
void hal_ei(void) {}

static void reset(void) {
    write_count = 0;
    memset(read_values, 0, sizeof(read_values));
    memset(&fdc_isr_state, 0, sizeof(fdc_isr_state));
    memset(&display_isr_state, 0, sizeof(display_isr_state));
}

static void test_display_dma_reprogram(void) {
    reset();
    /* Set display buffer to 0xF800 (standard Z80 address) */
    display_isr_state.display_buf = (byte *)0xF800;
    isr_reprogram_display_dma();

    /* 8 port writes: mask ch2, mask ch3, clear flip, ch2 addr lo+hi, ch2 count lo+hi, unmask ch2 */
    assert(write_count == 8);

    int w = 0;
    assert(writes[w].port == 0xFA && writes[w].value == 0x06); w++;  /* mask ch.2 */
    assert(writes[w].port == 0xFA && writes[w].value == 0x07); w++;  /* mask ch.3 */
    assert(writes[w].port == 0xFC && writes[w].value == 0x00); w++;  /* clear flip-flop */
    assert(writes[w].port == 0xF4 && writes[w].value == 0x00); w++;  /* ch.2 addr low = 0x00 */
    assert(writes[w].port == 0xF4 && writes[w].value == 0xF8); w++;  /* ch.2 addr high = 0xF8 */
    assert(writes[w].port == 0xF5 && writes[w].value == 0xCF); w++;  /* ch.2 count low */
    assert(writes[w].port == 0xF5 && writes[w].value == 0x07); w++;  /* ch.2 count high */
    assert(writes[w].port == 0xFA && writes[w].value == 0x02); w++;  /* unmask ch.2 */
    printf("  Display DMA reprogram: %d writes verified\n", w);
}

static void test_cursor_update(void) {
    reset();
    display_isr_state.cursor_dirty = 1;
    display_isr_state.curx = 40;
    display_isr_state.cursy = 12;

    isr_update_cursor();

    assert(write_count == 3);
    assert(writes[0].port == 0x01 && writes[0].value == 0x80);  /* load cursor cmd */
    assert(writes[1].port == 0x00 && writes[1].value == 40);    /* curx */
    assert(writes[2].port == 0x00 && writes[2].value == 12);    /* cursy */
    assert(display_isr_state.cursor_dirty == 0);
    printf("  Cursor update: 3 writes verified\n");
}

static void test_cursor_not_dirty(void) {
    reset();
    display_isr_state.cursor_dirty = 0;
    isr_update_cursor();
    assert(write_count == 0);  /* no writes when not dirty */
    printf("  Cursor not dirty: no writes (correct)\n");
}

static void test_rtc_increment(void) {
    reset();
    display_isr_state.rtc = 0;
    isr_tick_timers();
    assert(display_isr_state.rtc == 1);
    isr_tick_timers();
    assert(display_isr_state.rtc == 2);

    /* Test 32-bit wrap */
    display_isr_state.rtc = 0xFFFFFFFF;
    isr_tick_timers();
    assert(display_isr_state.rtc == 0);
    printf("  RTC increment: verified (including 32-bit wrap)\n");
}

static void test_timer1_countdown(void) {
    reset();
    display_isr_state.timer1 = 3;
    isr_tick_timers();
    assert(display_isr_state.timer1 == 2);
    isr_tick_timers();
    assert(display_isr_state.timer1 == 1);
    isr_tick_timers();
    assert(display_isr_state.timer1 == 0);
    /* Should not go negative */
    isr_tick_timers();
    assert(display_isr_state.timer1 == 0);
    printf("  TIMER1 countdown: verified\n");
}

static void test_timer2_motor_off(void) {
    reset();
    display_isr_state.timer2 = 2;

    isr_tick_timers();
    assert(display_isr_state.timer2 == 1);
    assert(write_count == 0);  /* motor still running */

    isr_tick_timers();
    assert(display_isr_state.timer2 == 0);
    /* Should have written motor off */
    assert(write_count == 1);
    assert(writes[0].port == 0x14 && writes[0].value == 0x00);
    printf("  TIMER2 motor-off: verified\n");
}

static void test_delcnt(void) {
    reset();
    display_isr_state.delcnt = 50;  /* 1 second at 50 Hz */
    for (int i = 0; i < 50; i++)
        isr_tick_timers();
    assert(display_isr_state.delcnt == 0);
    /* Should not underflow */
    isr_tick_timers();
    assert(display_isr_state.delcnt == 0);
    printf("  DELCNT: verified (50 ticks to zero)\n");
}

static void test_full_display_isr(void) {
    reset();
    /* Set up 8275 status read to return 0 */
    read_values[0x01] = 0x00;
    display_isr_state.display_buf = (byte *)0xF800;
    display_isr_state.cursor_dirty = 1;
    display_isr_state.curx = 5;
    display_isr_state.cursy = 10;
    display_isr_state.rtc = 100;

    isr_display_refresh();

    /* Verify sequence:
     * 1. DMA reprogram (11 writes)
     * 2. Cursor update (3 writes)
     * 3. CTC Ch.2 reprogram (2 writes)
     * Total: 16 writes */
    /* 8 DMA + 3 cursor + 2 CTC = 13 writes */
    assert(write_count == 13);

    /* Verify CTC reprogram at the end */
    assert(writes[11].port == 0x0E && writes[11].value == 0xD7);
    assert(writes[12].port == 0x0E && writes[12].value == 0x01);

    /* RTC should have incremented */
    assert(display_isr_state.rtc == 101);
    printf("  Full display ISR: %d writes, RTC incremented\n", write_count);
}

static void test_floppy_isr_sets_flag(void) {
    reset();
    /* Minimal ISR: just sets complete flag */
    fdc_isr_state.complete = 0;
    isr_floppy_complete();
    assert(fdc_isr_state.complete == 0xFF);
    printf("  Floppy ISR (sets complete flag): verified\n");
}

int main(void) {
    test_display_dma_reprogram();
    test_cursor_update();
    test_cursor_not_dirty();
    test_rtc_increment();
    test_timer1_countdown();
    test_timer2_motor_off();
    test_delcnt();
    test_full_display_isr();
    test_floppy_isr_sets_flag();
    printf("All interrupt tests passed.\n");
    return 0;
}
