#ifndef FDC_IMD_ADAPTER_H
#define FDC_IMD_ADAPTER_H

#include "types.h"
#include "imd.h"

/*
 * uPD765 FDC simulator backed by IMD disk images.
 * Handles per-track variable formats (FM/MFM, different sector sizes).
 * Provides port I/O interface matching the real FDC (ports 0x04/0x05).
 */

#define FDC_IMD_MAX_DRIVES  4
#define FDC_IMD_MAX_CMD     9
#define FDC_IMD_MAX_RESULT  7

typedef struct {
    byte msr;
    byte st0;
    byte st1;
    byte st2;

    byte cylinder[FDC_IMD_MAX_DRIVES];
    imd_disk_t *disk[FDC_IMD_MAX_DRIVES];

    byte command[FDC_IMD_MAX_CMD];
    byte result[FDC_IMD_MAX_RESULT];
    int  cmd_index;
    int  cmd_size;
    int  res_index;
    int  res_size;

    /* DMA simulation */
    byte *dma_addr;
    int   dma_count;
    int   interrupt_flag;
} fdc_imd_t;

void fdc_imd_init(fdc_imd_t *fi);
void fdc_imd_mount(fdc_imd_t *fi, int drive, imd_disk_t *imd);
void fdc_imd_set_dma(fdc_imd_t *fi, byte *addr, int count);

byte fdc_imd_read_msr(fdc_imd_t *fi);
byte fdc_imd_read_data(fdc_imd_t *fi);
void fdc_imd_write_data(fdc_imd_t *fi, byte data);

int fdc_imd_check_interrupt(fdc_imd_t *fi);

#endif
