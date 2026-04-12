#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hwinit.h"
#include "config.h"
#include "hal.h"

/*
 * Capture all port writes in order so we can verify the exact
 * initialization sequence matches the spec.
 */
#define MAX_WRITES 256

static struct {
    byte port;
    byte value;
} writes[MAX_WRITES];
static int write_count;

static struct {
    byte port;
    byte value;
} reads[MAX_WRITES];
static int read_count;
static int read_index;

void hal_out(byte port, byte value) {
    if (write_count < MAX_WRITES) {
        writes[write_count].port = port;
        writes[write_count].value = value;
        write_count++;
    }
}

byte hal_in(byte port) {
    (void)port;
    if (read_index < read_count)
        return reads[read_index++].value;
    return 0x80;  /* RQM set, DIO clear — FDC ready for commands */
}

void hal_di(void) {}
void hal_ei(void) {}

static void reset_capture(void) {
    write_count = 0;
    read_count = 0;
    read_index = 0;
}

static void expect_write(int index, byte port, byte value, const char *desc) {
    if (index >= write_count) {
        printf("  FAIL: expected write #%d (%s) but only %d writes recorded\n",
               index, desc, write_count);
        assert(0);
    }
    if (writes[index].port != port || writes[index].value != value) {
        printf("  FAIL: write #%d (%s): expected port=0x%02X val=0x%02X, "
               "got port=0x%02X val=0x%02X\n",
               index, desc, port, value,
               writes[index].port, writes[index].value);
        assert(0);
    }
}

static void test_pio_init(void) {
    reset_capture();
    hw_init_pio();

    assert(write_count == 6);
    /* Section 7.2: vector before mode, then enable interrupts */
    expect_write(0, 0x12, 0x20, "PIO-A vector");
    expect_write(1, 0x13, 0x22, "PIO-B vector");
    expect_write(2, 0x12, 0x4F, "PIO-A input mode");
    expect_write(3, 0x13, 0x0F, "PIO-B output mode");
    expect_write(4, 0x12, 0x83, "PIO-A enable int");
    expect_write(5, 0x13, 0x83, "PIO-B enable int");
    printf("  PIO init: 6 writes verified\n");
}

static void test_ctc_init(void) {
    reset_capture();
    hw_init_ctc(confi_defaults);

    assert(write_count == 9);
    /* Section 7.3: vector base first, then mode+count for each channel */
    expect_write(0, 0x0C, 0x00, "CTC vector base");
    expect_write(1, 0x0C, 0x47, "CTC Ch.0 mode");
    expect_write(2, 0x0C, 0x01, "CTC Ch.0 count");
    expect_write(3, 0x0D, 0x47, "CTC Ch.1 mode");
    expect_write(4, 0x0D, 0x01, "CTC Ch.1 count");
    expect_write(5, 0x0E, 0xD7, "CTC Ch.2 mode");
    expect_write(6, 0x0E, 0x01, "CTC Ch.2 count");
    expect_write(7, 0x0F, 0xD7, "CTC Ch.3 mode");
    expect_write(8, 0x0F, 0x01, "CTC Ch.3 count");
    printf("  CTC init: 9 writes verified\n");
}

static void test_sio_init(void) {
    reset_capture();
    hw_init_sio(confi_defaults);

    /* 9 bytes to SIO-A + 11 bytes to SIO-B + 4 status clear writes = 24 writes
     * Plus 4 reads (RR0-A, RR1-A, RR0-B, RR1-B) */
    int w = 0;

    /* SIO-A: 9 bytes per Section 7.4 */
    expect_write(w++, 0x0A, 0x18, "SIO-A WR0 reset");
    expect_write(w++, 0x0A, 0x04, "SIO-A select WR4");
    expect_write(w++, 0x0A, 0x44, "SIO-A WR4 x16,1stop,noparity");
    expect_write(w++, 0x0A, 0x03, "SIO-A select WR3");
    expect_write(w++, 0x0A, 0xE1, "SIO-A WR3 8bit,auto,RX enable");
    expect_write(w++, 0x0A, 0x05, "SIO-A select WR5");
    expect_write(w++, 0x0A, 0x60, "SIO-A WR5 8bit TX disabled");
    expect_write(w++, 0x0A, 0x01, "SIO-A select WR1");
    expect_write(w++, 0x0A, 0x1B, "SIO-A WR1 all ints enabled");

    /* SIO-B: 11 bytes */
    expect_write(w++, 0x0B, 0x18, "SIO-B WR0 reset");
    expect_write(w++, 0x0B, 0x02, "SIO-B select WR2");
    expect_write(w++, 0x0B, 0x10, "SIO-B WR2 vector base 0x10");
    expect_write(w++, 0x0B, 0x04, "SIO-B select WR4");
    expect_write(w++, 0x0B, 0x44, "SIO-B WR4 x16,1stop,noparity");
    expect_write(w++, 0x0B, 0x03, "SIO-B select WR3");
    expect_write(w++, 0x0B, 0xE1, "SIO-B WR3 8bit,auto,RX enable");
    expect_write(w++, 0x0B, 0x05, "SIO-B select WR5");
    expect_write(w++, 0x0B, 0x60, "SIO-B WR5 8bit TX disabled");
    expect_write(w++, 0x0B, 0x01, "SIO-B select WR1");
    expect_write(w++, 0x0B, 0x1F, "SIO-B WR1 all+status+parity");

    /* Status clear: select RR1-A, select RR1-B (reads are hal_in, not captured here) */
    expect_write(w++, 0x0A, 0x01, "select RR1-A");
    expect_write(w++, 0x0B, 0x01, "select RR1-B");

    assert(write_count == w);
    printf("  SIO init: %d writes verified\n", w);
}

static void test_dma_init(void) {
    reset_capture();
    hw_init_dma(confi_defaults);

    assert(write_count == 4);
    expect_write(0, 0xF8, 0x20, "DMA master clear");
    expect_write(1, 0xFB, 0x48, "DMA Ch.0 mode");
    expect_write(2, 0xFB, 0x4A, "DMA Ch.2 mode");
    expect_write(3, 0xFB, 0x4B, "DMA Ch.3 mode");
    printf("  DMA init: 4 writes verified\n");
}

static void test_fdc_init(void) {
    reset_capture();
    hw_init_fdc(confi_defaults);

    /* SPECIFY: 3 bytes (command + 2 params) */
    assert(write_count == 3);
    expect_write(0, 0x05, 0x03, "FDC SPECIFY command");
    expect_write(1, 0x05, 0xDF, "FDC SRT/HUT");
    expect_write(2, 0x05, 0x28, "FDC HLT/DMA");
    printf("  FDC init: 3 writes verified\n");
}

static void test_display_init(void) {
    reset_capture();
    hw_init_display(confi_defaults);

    int w = 0;
    expect_write(w++, 0x01, 0x00, "8275 reset");
    expect_write(w++, 0x00, 0x4F, "8275 PAR1 80 chars");
    expect_write(w++, 0x00, 0x98, "8275 PAR2 25 rows");
    expect_write(w++, 0x00, 0x7A, "8275 PAR3 retrace");
    expect_write(w++, 0x00, 0x6D, "8275 PAR4 cursor");
    expect_write(w++, 0x01, 0x80, "8275 load cursor");
    expect_write(w++, 0x00, 0x00, "8275 cursor X=0");
    expect_write(w++, 0x00, 0x00, "8275 cursor Y=0");
    expect_write(w++, 0x01, 0xE0, "8275 preset counters");
    expect_write(w++, 0x01, 0x23, "8275 start display");
    assert(write_count == w);
    printf("  Display init: %d writes verified\n", w);
}

static void test_full_init_sequence(void) {
    reset_capture();
    hw_init_all(confi_defaults);

    /* Total: PIO(6) + CTC(9) + SIO(22) + DMA(4) + FDC(3) + Display(10) = 54 */
    printf("  Full init: %d total writes\n", write_count);
    assert(write_count == 54);
}

int main(void) {
    test_pio_init();
    test_ctc_init();
    test_sio_init();
    test_dma_init();
    test_fdc_init();
    test_display_init();
    test_full_init_sequence();
    printf("All hwinit tests passed.\n");
    return 0;
}
