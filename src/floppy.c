#include "floppy.h"
#include "hal.h"

/* Direct port access via __sfr generates IN A,(n) / OUT (n),A.
 * This is critical — IN A,(C) / OUT (C),A use B:C as 16-bit address
 * and give wrong results when B != 0 (which SDCC doesn't guarantee). */
#ifdef __SDCC
__sfr __at(FDC_MSR_PORT)  fdc_msr;
__sfr __at(FDC_DATA_PORT) fdc_data;
#define FDC_MSR_READ()      fdc_msr
#define FDC_DATA_READ()     fdc_data
#define FDC_DATA_WRITE(v)   (fdc_data = (v))
#else
#define FDC_MSR_READ()      hal_in(FDC_MSR_PORT)
#define FDC_DATA_READ()     hal_in(FDC_DATA_PORT)
#define FDC_DATA_WRITE(v)   hal_out(FDC_DATA_PORT, (v))
#endif

void floppy_init(floppy_t *fl) {
    fl->current_drive = 0xFF;
    fl->current_track = 0;
    fl->last_st0 = 0;
    fl->last_st1 = 0;
    fl->erflag = 0;
}

/* Wait for RQM=1 and DIO=0 (ready to accept command byte) */
static void wait_rqm_write(void) {
    while ((FDC_MSR_READ() & (MSR_RQM | MSR_DIO)) != MSR_RQM)
        hal_ei();
}

/* Wait for RQM=1 and DIO=1 (result byte available) */
static void wait_rqm_read(void) {
    while ((FDC_MSR_READ() & (MSR_RQM | MSR_DIO)) != (MSR_RQM | MSR_DIO))
        hal_ei();
}

void fdc_send_byte(byte val) {
    wait_rqm_write();
    FDC_DATA_WRITE(val);
}

byte fdc_read_byte(void) {
    wait_rqm_read();
    return FDC_DATA_READ();
}

void fdc_read_results(byte *st0, byte *st1, byte *st2,
                      byte *c, byte *h, byte *r, byte *n) {
    *st0 = fdc_read_byte();
    *st1 = fdc_read_byte();
    *st2 = fdc_read_byte();
    *c   = fdc_read_byte();
    *h   = fdc_read_byte();
    *r   = fdc_read_byte();
    *n   = fdc_read_byte();
}

void fdc_specify(byte srt_hut, byte hlt_nd) {
    fdc_send_byte(FDC_CMD_SPECIFY);
    fdc_send_byte(srt_hut);
    fdc_send_byte(hlt_nd);
}

void fdc_sense_interrupt(byte *st0, byte *pcn) {
    fdc_send_byte(FDC_CMD_SENSE_INT);
    *st0 = fdc_read_byte();
    *pcn = fdc_read_byte();
}

/* Wait for FDC to become idle at init.
 * Per working BIOS: poll MSR until bits 4:0 all zero. */
void fdc_wait_idle(void) {
    while (FDC_MSR_READ() & 0x1F)
        hal_ei();
}

/* Polling-based seek/recalibrate: wait for RQM, then SENSE INTERRUPT.
 * After RECALIBRATE/SEEK, the FDC sets D0B. When the seek completes,
 * RQM goes high. SENSE INTERRUPT clears D0B and returns status. */
void fdc_recalibrate(floppy_t *fl, byte drive) {
    fdc_send_byte(FDC_CMD_RECALIBRATE);
    fdc_send_byte(drive & 0x03);
    wait_rqm_write();
    byte st0, pcn;
    fdc_sense_interrupt(&st0, &pcn);
    fl->last_st0 = st0;
    fl->current_track = 0;
}

void fdc_seek(floppy_t *fl, byte drive, byte cylinder) {
    fdc_send_byte(FDC_CMD_SEEK);
    fdc_send_byte(drive & 0x03);
    fdc_send_byte(cylinder);
    wait_rqm_write();
    byte st0, pcn;
    fdc_sense_interrupt(&st0, &pcn);
    fl->last_st0 = st0;
    fl->current_track = cylinder;
}

void dma_setup(word addr, word count, byte mode) {
    hal_out(DMA_MASK_PORT, 0x05);           /* mask channel 1 */
    hal_out(DMA_MODE_PORT, mode);           /* set mode */
    hal_out(DMA_CLEAR_PORT, 0x00);          /* clear byte pointer */
    hal_out(DMA_ADDR1_PORT, (byte)addr);         /* address low */
    hal_out(DMA_ADDR1_PORT, (byte)(addr >> 8));  /* address high */
    hal_out(DMA_COUNT1_PORT, (byte)count);        /* count low */
    hal_out(DMA_COUNT1_PORT, (byte)(count >> 8)); /* count high */
    hal_out(DMA_MASK_PORT, 0x01);           /* unmask channel 1 */
}

/* Polling-based READ DATA: send 9 command bytes, DMA runs,
 * then poll for result phase (wait_rqm_read blocks until done). */
int floppy_read_sector(floppy_t *fl, byte drive, byte cylinder, byte head,
                       byte sector, const fdf_t *fdf, word dma_addr) {
    if (fl->current_track != cylinder)
        fdc_seek(fl, drive, cylinder);

    dma_setup(dma_addr, fdf->dma_count, DMA_READ_MODE);

    byte cmd = fdf->mf ? FDC_CMD_READ_MFM : FDC_CMD_READ_FM;
    byte hd_sel = (byte)((head << 2) | (drive & 0x03));

    fdc_send_byte(cmd);
    fdc_send_byte(hd_sel);
    fdc_send_byte(cylinder);
    fdc_send_byte(head);
    fdc_send_byte(sector);
    fdc_send_byte(fdf->n);
    fdc_send_byte(fdf->eot);
    fdc_send_byte(fdf->gap);
    fdc_send_byte((byte)(fdf->n == 0 ? 0x80 : 0xFF));

    /* DMA runs autonomously. Poll for result phase. */
    byte st0, st1, st2, rc, rh, rr, rn;
    fdc_read_results(&st0, &st1, &st2, &rc, &rh, &rr, &rn);

    fl->last_st0 = st0;
    fl->last_st1 = st1;

    return (st0 & ST0_IC_MASK) == ST0_NT ? 0 : 1;
}

int floppy_write_sector(floppy_t *fl, byte drive, byte cylinder, byte head,
                        byte sector, const fdf_t *fdf, word dma_addr) {
    if (fl->current_track != cylinder)
        fdc_seek(fl, drive, cylinder);

    dma_setup(dma_addr, fdf->dma_count, DMA_WRITE_MODE);

    byte cmd = fdf->mf ? FDC_CMD_WRITE_MFM : FDC_CMD_WRITE_FM;
    byte hd_sel = (byte)((head << 2) | (drive & 0x03));

    fdc_send_byte(cmd);
    fdc_send_byte(hd_sel);
    fdc_send_byte(cylinder);
    fdc_send_byte(head);
    fdc_send_byte(sector);
    fdc_send_byte(fdf->n);
    fdc_send_byte(fdf->eot);
    fdc_send_byte(fdf->gap);
    fdc_send_byte((byte)(fdf->n == 0 ? 0x80 : 0xFF));

    byte st0, st1, st2, rc, rh, rr, rn;
    fdc_read_results(&st0, &st1, &st2, &rc, &rh, &rr, &rn);

    fl->last_st0 = st0;
    fl->last_st1 = st1;

    return (st0 & ST0_IC_MASK) == ST0_NT ? 0 : 1;
}

int floppy_read_with_retry(floppy_t *fl, byte drive, byte cylinder, byte head,
                           byte sector, const fdf_t *fdf, word dma_addr) {
    for (int retry = 0; retry < FDC_MAX_RETRIES; retry++) {
        if (retry == FDC_RECAL_THRESHOLD) {
            fdc_recalibrate(fl, drive);
            fdc_seek(fl, drive, cylinder);
        }

        int rc = floppy_read_sector(fl, drive, cylinder, head, sector,
                                    fdf, dma_addr);
        if (rc == 0)
            return 0;

        if (fl->last_st0 & ST0_EC) {
            fl->erflag = 1;
            return 1;
        }
    }

    fl->erflag = 1;
    return 1;
}

int floppy_write_with_retry(floppy_t *fl, byte drive, byte cylinder, byte head,
                            byte sector, const fdf_t *fdf, word dma_addr) {
    for (int retry = 0; retry < FDC_MAX_RETRIES; retry++) {
        if (retry == FDC_RECAL_THRESHOLD) {
            fdc_recalibrate(fl, drive);
            fdc_seek(fl, drive, cylinder);
        }

        int rc = floppy_write_sector(fl, drive, cylinder, head, sector,
                                     fdf, dma_addr);
        if (rc == 0)
            return 0;

        if (fl->last_st0 & ST0_EC) {
            fl->erflag = 1;
            return 1;
        }
    }

    fl->erflag = 1;
    return 1;
}
