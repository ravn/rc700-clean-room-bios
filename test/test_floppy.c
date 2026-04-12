/*
 * Integration test: floppy driver reading from real IMD disk image.
 *
 * The BIOS floppy driver sends FDC commands through the HAL.
 * We intercept hal_in/hal_out with a custom mock that routes
 * FDC ports (0x04/0x05) to the IMD-backed FDC simulator, and
 * DMA ports to track the DMA setup.
 *
 * The BIOS doesn't read track 0 — the PROM loader does that.
 * We test reading data tracks (track 1+) which carry CCP+BDOS.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "floppy.h"
#include "dpb.h"
#include "imd.h"
#include "fdc_imd_adapter.h"
#include "hal.h"

#ifndef TEST_IMAGE
#define TEST_IMAGE "disks/SW1711-I8.imd"
#endif

/* Global state for the test harness */
static fdc_imd_t fdc;
static imd_disk_t disk;

/* Simulated Z80 memory for DMA transfers */
static byte z80_mem[65536];

/* DMA state captured from hal_out calls */
static word dma_addr;
static word dma_count;
static byte dma_addr_toggle;
static byte dma_count_toggle;

/* HAL implementation that routes to the FDC simulator */
void hal_out(byte port, byte value) {
    switch (port) {
    case 0x05:  /* FDC data */
        fdc_imd_write_data(&fdc, value);
        break;
    case 0xFA:  /* DMA mask */
        if (value == 0x01) {
            /* Unmask — set up the DMA buffer in the FDC sim */
            fdc_imd_set_dma(&fdc, &z80_mem[dma_addr], dma_count + 1);
        }
        break;
    case 0xFB:  /* DMA mode */
        break;
    case 0xFC:  /* DMA clear byte pointer */
        dma_addr_toggle = 0;
        dma_count_toggle = 0;
        break;
    case 0xF2:  /* DMA channel 1 address */
        if (dma_addr_toggle == 0) {
            dma_addr = (dma_addr & 0xFF00) | value;
            dma_addr_toggle = 1;
        } else {
            dma_addr = (dma_addr & 0x00FF) | ((word)value << 8);
        }
        break;
    case 0xF3:  /* DMA channel 1 count */
        if (dma_count_toggle == 0) {
            dma_count = (dma_count & 0xFF00) | value;
            dma_count_toggle = 1;
        } else {
            dma_count = (dma_count & 0x00FF) | ((word)value << 8);
        }
        break;
    default:
        break;
    }
}

byte hal_in(byte port) {
    switch (port) {
    case 0x04:  /* FDC MSR */
        return fdc_imd_read_msr(&fdc);
    case 0x05:  /* FDC data */
        return fdc_imd_read_data(&fdc);
    default:
        return 0;
    }
}

void hal_di(void) {}
void hal_ei(void) {}

static void test_read_data_track(void) {
    floppy_t fl;
    floppy_init(&fl);

    /* Data tracks use format 8: 8" DD MFM 512 B/S */
    const fdf_t *fdf = &fdf_table[FMT_8DD_MFM];

    /* Read track 1, head 0, sector 1 — first sector of CCP+BDOS area */
    int rc = floppy_read_sector(&fl, 0, 1, 0, 1, fdf, 0x1000);
    assert(rc == 0);

    /* Verify we got data (not all zeros) */
    int nonzero = 0;
    for (int i = 0; i < 512; i++) {
        if (z80_mem[0x1000 + i] != 0) nonzero++;
    }
    assert(nonzero > 0);
    printf("  Track 1 sector 1: %d non-zero bytes\n", nonzero);
}

static void test_read_multiple_sectors(void) {
    floppy_t fl;
    floppy_init(&fl);

    const fdf_t *fdf = &fdf_table[FMT_8DD_MFM];

    /* Read several sectors from track 1 */
    for (int s = 1; s <= 5; s++) {
        word addr = (word)(0x2000 + (s - 1) * 512);
        int rc = floppy_read_sector(&fl, 0, 1, 0, (byte)s, fdf, addr);
        assert(rc == 0);
    }
    printf("  Read 5 sectors from track 1 head 0: OK\n");
}

static void test_seek_and_read(void) {
    floppy_t fl;
    floppy_init(&fl);

    const fdf_t *fdf = &fdf_table[FMT_8DD_MFM];

    /* Read from track 5 — forces a seek */
    int rc = floppy_read_sector(&fl, 0, 5, 0, 1, fdf, 0x3000);
    assert(rc == 0);
    assert(fl.current_track == 5);
    printf("  Seek to track 5 and read: OK\n");
}

static void test_read_both_heads(void) {
    floppy_t fl;
    floppy_init(&fl);

    const fdf_t *fdf = &fdf_table[FMT_8DD_MFM];

    /* Read from head 0 */
    int rc = floppy_read_sector(&fl, 0, 2, 0, 1, fdf, 0x4000);
    assert(rc == 0);

    /* Read from head 1 */
    rc = floppy_read_sector(&fl, 0, 2, 1, 1, fdf, 0x4200);
    assert(rc == 0);

    /* Data should differ */
    assert(memcmp(&z80_mem[0x4000], &z80_mem[0x4200], 512) != 0);
    printf("  Read track 2 both heads: OK (data differs)\n");
}

static void test_read_with_retry_success(void) {
    floppy_t fl;
    floppy_init(&fl);

    const fdf_t *fdf = &fdf_table[FMT_8DD_MFM];

    /* Normal read should succeed on first try */
    int rc = floppy_read_with_retry(&fl, 0, 3, 0, 1, fdf, 0x5000);
    assert(rc == 0);
    assert(fl.erflag == 0);
    printf("  Read with retry (success): OK\n");
}

static void test_verify_sector_content(void) {
    floppy_t fl;
    floppy_init(&fl);

    const fdf_t *fdf = &fdf_table[FMT_8DD_MFM];

    /* Read sector and compare with IMD data directly */
    int rc = floppy_read_sector(&fl, 0, 1, 0, 1, fdf, 0x6000);
    assert(rc == 0);

    byte *imd_data = imd_get_sector(&disk, 1, 0, 1);
    assert(imd_data != NULL);
    assert(memcmp(&z80_mem[0x6000], imd_data, 512) == 0);
    printf("  Sector content matches IMD source: OK\n");
}

int main(void) {
    /* Load disk image */
    int rc = imd_load(&disk, TEST_IMAGE);
    assert(rc == 0);

    /* Initialize FDC simulator with the disk */
    fdc_imd_init(&fdc);
    fdc_imd_mount(&fdc, 0, &disk);

    /* Clear Z80 memory */
    memset(z80_mem, 0, sizeof(z80_mem));

    test_read_data_track();
    test_read_multiple_sectors();
    test_seek_and_read();
    test_read_both_heads();
    test_read_with_retry_success();
    test_verify_sector_content();

    printf("All floppy tests passed.\n");
    return 0;
}
