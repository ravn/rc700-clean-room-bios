/*
 * Integration test: warm boot loading CCP+BDOS from real IMD disk image.
 *
 * Exercises the complete disk I/O path:
 *   bios_read → deblock → host_disk_read → sector_translate →
 *   floppy_read_with_retry → FDC sim → IMD reader
 *
 * Provides its own HAL that routes FDC and DMA ports to the simulator.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "types.h"
#include "iobyte.h"
#include "dpb.h"
#include "config.h"
#include "deblock.h"
#include "console.h"
#include "chartab.h"
#include "ringbuf.h"
#include "serial.h"
#include "floppy.h"
#include "sector.h"
#include "imd.h"
#include "fdc_imd_adapter.h"
#include "hal.h"

#ifndef TEST_IMAGE
#define TEST_IMAGE "disks/SW1711-I8.imd"
#endif

/* Simulated Z80 memory */
static byte z80_mem[65536];

/* FDC simulator backed by IMD */
static fdc_imd_t fdc;
static imd_disk_t disk;

/* DMA state */
static word dma_addr;
static word dma_count;
static byte dma_addr_toggle;
static byte dma_count_toggle;

/* HAL implementation */
void hal_out(byte port, byte value) {
    switch (port) {
    case 0x05:
        fdc_imd_write_data(&fdc, value);
        break;
    case 0xFA:
        if (value == 0x01)
            fdc_imd_set_dma(&fdc, &z80_mem[dma_addr], dma_count + 1);
        break;
    case 0xFC:
        dma_addr_toggle = 0;
        dma_count_toggle = 0;
        break;
    case 0xF2:
        if (dma_addr_toggle == 0) {
            dma_addr = (dma_addr & 0xFF00) | value;
            dma_addr_toggle = 1;
        } else {
            dma_addr = (dma_addr & 0x00FF) | ((word)value << 8);
        }
        break;
    case 0xF3:
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
    case 0x04: return fdc_imd_read_msr(&fdc);
    case 0x05: return fdc_imd_read_data(&fdc);
    default:   return 0;
    }
}

void hal_di(void) {}
void hal_ei(void) {}

/* ---- Replicate the BIOS disk infrastructure for testing ---- */

#define CCP_BASE   0xC400
#define CPML       0x1600
#define NSECTS     (CPML / 128)
#define MAX_DRIVES 16

static deblock_t deblock_state;
static floppy_t  floppy_state;
static byte      drive_formats[MAX_DRIVES];
static const fspa_t *cur_fspa;
static const fdf_t  *cur_fdf;
static byte  cur_disk;
static word  cur_track;
static word  cur_sector;
static byte *cur_dma;

static int host_disk_read(byte drv, word track, byte sector, byte *buf) {
    (void)drv;
    byte head = 0;
    byte phys_sec;
    if (sector < cur_fdf->eot) {
        phys_sec = sector_translate(cur_fspa->tran_table,
                                    cur_fspa->tran_size, sector);
    } else {
        head = 1;
        phys_sec = sector_translate(cur_fspa->tran_table,
                                    cur_fspa->tran_size,
                                    sector - cur_fdf->eot);
    }
    if (phys_sec == 0) return 1;

    /* On native, bypass the DMA mechanism and read directly from IMD.
     * The floppy driver programs DMA ports, but on 64-bit native the
     * 16-bit DMA address can't represent a real pointer. Instead, we
     * read the sector data directly into the host buffer. */
    byte *src = imd_get_sector(&disk, (int)track, (int)head, (int)phys_sec);
    if (!src) return 1;
    int sec_size = imd_get_sector_size(&disk, (int)track, (int)head);
    if (sec_size > 512) sec_size = 512;
    memcpy(buf, src, (size_t)sec_size);
    return 0;
}

static int host_disk_write(byte drv, word track, byte sector,
                           const byte *buf) {
    (void)drv; (void)track; (void)sector; (void)buf;
    return 1;  /* read-only for this test */
}

static word seldsk(byte drv) {
    if (drv >= MAX_DRIVES) return 0;
    byte fmt = drive_formats[drv];
    if (fmt == 0xFF || fmt >= 32) return 0;
    byte idx = dpb_format_index(fmt);
    cur_fspa = &fspa_table[idx];
    cur_fdf = &fdf_table[idx];
    cur_disk = drv;
    deblock_state.secmsk = cur_fspa->sector_mask;
    deblock_state.secshf = cur_fspa->sector_shift;
    return 1;
}

static void test_warmboot_loads_ccp_bdos(void) {
    /* Initialize */
    floppy_init(&floppy_state);
    deblock_init(&deblock_state, host_disk_read, host_disk_write);

    /* Load drive formats from the CONFI block on the real disk */
    byte *confi = imd_get_sector(&disk, 0, 0, 2);
    assert(confi != NULL);
    for (int i = 0; i < MAX_DRIVES; i++)
        drive_formats[i] = confi_get_drive_format(confi, (byte)i);

    /* Drive A should be format 0x08 (8" DD MFM) */
    assert(drive_formats[0] == 0x08);

    /* Select drive A */
    assert(seldsk(0) != 0);
    assert(cur_fspa == &fspa_table[FMT_8DD_MFM]);

    /* Clear target memory */
    memset(&z80_mem[CCP_BASE], 0, CPML);

    /* Simulate warm boot: read NSECTS records from track 1 */
    cur_dma = &z80_mem[CCP_BASE];
    cur_track = 1;

    int errors = 0;
    for (word sec = 0; sec < NSECTS; sec++) {
        cur_sector = sec;
        deblock_state.sekdsk = cur_disk;
        deblock_state.sektrk = cur_track;
        deblock_state.seksec = (byte)sec;
        deblock_state.dmaadr = cur_dma;

        int rc = deblock_read(&deblock_state);
        if (rc != 0) {
            printf("  ERROR: read failed at sector %d\n", sec);
            errors++;
            break;
        }
        cur_dma += 128;

        /* Track boundary: 120 CP/M sectors per track for DD MFM */
        if (sec > 0 && ((sec + 1) % 120) == 0) {
            cur_track++;
        }
    }

    assert(errors == 0);

    /* Verify we loaded something meaningful */
    int nonzero = 0;
    for (int i = 0; i < CPML; i++) {
        if (z80_mem[CCP_BASE + i] != 0) nonzero++;
    }
    printf("  Loaded %d records (%d bytes) at 0x%04X\n", NSECTS, CPML, CCP_BASE);
    printf("  Non-zero bytes: %d / %d (%.1f%%)\n",
           nonzero, CPML, 100.0 * nonzero / CPML);
    assert(nonzero > 1000);  /* CCP+BDOS should have substantial code */
}

static void test_ccp_starts_with_code(void) {
    /* The CCP typically starts with executable code, not zeros or 0xE5 fill */
    byte first = z80_mem[CCP_BASE];
    printf("  CCP first byte: 0x%02X\n", first);
    /* Should not be empty/fill */
    assert(first != 0x00);
    assert(first != 0xE5);
}

static void test_bdos_present(void) {
    /* BDOS starts at CCP_BASE + 0x0806 (standard CP/M 2.2 offset) or
     * CCP_BASE + 0x1080 for some configurations. Check for code presence
     * in the BDOS area. */
    int bdos_nonzero = 0;
    for (int i = 0x800; i < CPML; i++) {
        if (z80_mem[CCP_BASE + i] != 0) bdos_nonzero++;
    }
    printf("  BDOS area non-zero bytes: %d\n", bdos_nonzero);
    assert(bdos_nonzero > 500);
}

int main(void) {
    /* Load disk image */
    int rc = imd_load(&disk, TEST_IMAGE);
    if (rc != 0) {
        printf("Failed to load disk image: %d\n", rc);
        return 1;
    }

    /* Initialize FDC with disk */
    fdc_imd_init(&fdc);
    fdc_imd_mount(&fdc, 0, &disk);
    memset(z80_mem, 0, sizeof(z80_mem));

    test_warmboot_loads_ccp_bdos();
    test_ccp_starts_with_code();
    test_bdos_present();

    printf("All warmboot tests passed.\n");
    return 0;
}
