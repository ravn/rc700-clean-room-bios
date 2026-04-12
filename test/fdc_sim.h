#ifndef FDC_SIM_H
#define FDC_SIM_H

#include "types.h"

/*
 * uPD765 FDC simulator for the native test harness.
 *
 * Hooks into hal_mock port I/O on ports 0x04 (MSR) and 0x05 (data).
 * Backs onto a flat byte array as the disk image.
 * Simulates DMA by copying directly to/from a memory buffer.
 *
 * Based on the rc700 emulator's fdc.c by Michael Ringgaard.
 */

#define FDC_MSR_PORT    0x04
#define FDC_DATA_PORT   0x05

#define FDC_MAX_DRIVES  4
#define FDC_MAX_CMD     9
#define FDC_MAX_RESULT  7

/* Disk geometry for simulation */
#define FDC_MAX_TRACKS  77
#define FDC_MAX_HEADS   2
#define FDC_MAX_SECTORS 26
#define FDC_MAX_SECSIZE 512

/* MSR bits */
#define FDC_MSR_RQM     0x80  /* Request for master */
#define FDC_MSR_DIO     0x40  /* Data direction: 1=FDC->CPU, 0=CPU->FDC */
#define FDC_MSR_EXM     0x20  /* Execution mode */
#define FDC_MSR_CB      0x10  /* Controller busy */

/* ST0 bits */
#define FDC_ST0_SE      0x20  /* Seek end */
#define FDC_ST0_EC      0x10  /* Equipment check */
#define FDC_ST0_NR      0x08  /* Not ready */
#define FDC_ST0_NT      0x00  /* Normal termination */
#define FDC_ST0_AT      0x40  /* Abnormal termination */
#define FDC_ST0_IC      0x80  /* Invalid command */

/* ST1 bits */
#define FDC_ST1_MA      0x01  /* Missing address mark */
#define FDC_ST1_NW      0x02  /* Not writable */
#define FDC_ST1_ND      0x04  /* No data */
#define FDC_ST1_EN      0x80  /* End of cylinder */

/* Simulated disk image */
typedef struct {
    byte *data;          /* flat sector data */
    int   tracks;
    int   heads;
    int   sectors;       /* sectors per side */
    int   sector_size;   /* bytes per sector */
    int   present;       /* drive present flag */
} fdc_disk_t;

/* FDC simulator state */
typedef struct {
    byte msr;
    byte st0;
    byte st1;
    byte st2;

    byte cylinder[FDC_MAX_DRIVES];
    fdc_disk_t disk[FDC_MAX_DRIVES];

    byte command[FDC_MAX_CMD];
    byte result[FDC_MAX_RESULT];
    int  cmd_index;
    int  cmd_size;
    int  res_index;
    int  res_size;

    /* Simulated DMA buffer and transfer */
    byte *dma_addr;      /* points into test memory */
    int   dma_count;     /* remaining byte count */
    int   interrupt_flag; /* set after seek/read/write */
} fdc_sim_t;

/* Initialize the FDC simulator */
void fdc_sim_init(fdc_sim_t *fdc);

/* Mount a disk image (flat byte array) on a drive */
void fdc_sim_mount(fdc_sim_t *fdc, int drive, byte *image_data,
                   int tracks, int heads, int sectors, int sector_size);

/* Unmount a drive */
void fdc_sim_unmount(fdc_sim_t *fdc, int drive);

/* Set the DMA buffer for transfers */
void fdc_sim_set_dma(fdc_sim_t *fdc, byte *addr, int count);

/* Port I/O — call from hal_mock or directly in tests */
byte fdc_sim_read_msr(fdc_sim_t *fdc);
byte fdc_sim_read_data(fdc_sim_t *fdc);
void fdc_sim_write_data(fdc_sim_t *fdc, byte data);

/* Check and clear the interrupt flag */
int fdc_sim_check_interrupt(fdc_sim_t *fdc);

#endif
