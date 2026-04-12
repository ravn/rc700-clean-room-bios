#ifndef FLOPPY_H
#define FLOPPY_H

#include "types.h"
#include "dpb.h"

/*
 * Floppy disk driver for uPD765 FDC.
 * Per RC702_BIOS_SPECIFICATION.md Sections 10.1-10.8.
 */

/* FDC I/O ports */
#define FDC_MSR_PORT    0x04
#define FDC_DATA_PORT   0x05

/* DMA ports (Am9517A channel 1) */
#define DMA_MASK_PORT   0xFA
#define DMA_MODE_PORT   0xFB
#define DMA_CLEAR_PORT  0xFC
#define DMA_ADDR1_PORT  0xF2
#define DMA_COUNT1_PORT 0xF3

/* DMA modes for channel 1 */
#define DMA_READ_MODE   0x45   /* IO-to-mem, channel 1 */
#define DMA_WRITE_MODE  0x49   /* mem-to-IO, channel 1 */

/* FDC commands */
#define FDC_CMD_READ_FM     0x06
#define FDC_CMD_WRITE_FM    0x05
#define FDC_CMD_READ_MFM    0x46
#define FDC_CMD_WRITE_MFM   0x45
#define FDC_CMD_RECALIBRATE 0x07
#define FDC_CMD_SENSE_INT   0x08
#define FDC_CMD_SEEK        0x0F
#define FDC_CMD_SPECIFY     0x03

/* MSR bits */
#define MSR_RQM   0x80
#define MSR_DIO   0x40
#define MSR_CB    0x10

/* ST0 bits */
#define ST0_IC_MASK  0xC0
#define ST0_NT       0x00
#define ST0_AT       0x40
#define ST0_SE       0x20
#define ST0_EC       0x10

/* Error recovery */
#define FDC_MAX_RETRIES     10
#define FDC_RECAL_THRESHOLD  5

/* Floppy driver state */
typedef struct {
    byte current_drive;
    byte current_track;
    byte last_st0;
    byte last_st1;
    byte erflag;
} floppy_t;

/* Initialize the floppy driver */
void floppy_init(floppy_t *fl);

/* Low-level FDC protocol */
void fdc_send_byte(byte val);
byte fdc_read_byte(void);
void fdc_read_results(byte *st0, byte *st1, byte *st2,
                      byte *c, byte *h, byte *r, byte *n);

/* FDC commands */
void fdc_specify(byte srt_hut, byte hlt_nd);
void fdc_recalibrate(floppy_t *fl, byte drive);
void fdc_seek(floppy_t *fl, byte drive, byte cylinder);
void fdc_sense_interrupt(byte *st0, byte *pcn);

/* DMA setup */
void dma_setup(word addr, word count, byte mode);

/* Read/write a physical sector. Returns 0 on success, 1 on error. */
int floppy_read_sector(floppy_t *fl, byte drive, byte cylinder, byte head,
                       byte sector, const fdf_t *fdf, word dma_addr);
int floppy_write_sector(floppy_t *fl, byte drive, byte cylinder, byte head,
                        byte sector, const fdf_t *fdf, word dma_addr);

/* Read/write with retry and error recovery. Returns 0 on success, 1 on error. */
int floppy_read_with_retry(floppy_t *fl, byte drive, byte cylinder, byte head,
                           byte sector, const fdf_t *fdf, word dma_addr);
int floppy_write_with_retry(floppy_t *fl, byte drive, byte cylinder, byte head,
                            byte sector, const fdf_t *fdf, word dma_addr);

#endif
