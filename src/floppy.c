#include "floppy.h"
#include "hal.h"
#include "interrupt.h"

/* All FDC/DMA/CTC port I/O uses __sfr to generate IN A,(n) / OUT (n),A.
 * The hal_in/hal_out functions use IN A,(C) / OUT (C),A which put B on
 * the high address bus — this can cause wrong port reads in MAME. */
#ifdef __SDCC
__sfr __at(FDC_MSR_PORT)    fdc_msr;
__sfr __at(FDC_DATA_PORT)   fdc_data;
__sfr __at(DMA_MASK_PORT)   dma_mask;
__sfr __at(DMA_MODE_PORT)   dma_mode;
__sfr __at(DMA_CLEAR_PORT)  dma_clear;
__sfr __at(DMA_ADDR1_PORT)  dma_addr1;
__sfr __at(DMA_COUNT1_PORT) dma_count1;
__sfr __at(0x0C)            ctc_ch0;   /* CTC vector base */
__sfr __at(0x0F)            ctc_ch3;   /* FDC interrupt trigger */
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
    /* Read result bytes until CB (bit 4) clears, max 7.
     * Short delay between read and CB check lets FDC update MSR. */
    byte *results[7] = { st0, st1, st2, c, h, r, n };
    for (int i = 0; i < 7; i++) {
        *results[i] = fdc_read_byte();
        /* Delay: 4 iterations per working BIOS */
        for (volatile int d = 0; d < 4; d++) ;
        /* Check CB — if clear, FDC done with result phase */
        if (!(FDC_MSR_READ() & MSR_CB))
            break;
    }
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

/* Arm CTC Ch.3 and clear completion flag.
 * Must be called before sending FDC command. */
static void fdc_arm_interrupt(void) {
    fdc_isr_state.complete = 0;
#ifdef __SDCC
    /* Do NOT write vector base — writing 0x08 to Ch.0 port may be
     * interpreted as a time constant if Ch.0 expects one.
     * The PROM already set the vector base. */
    ctc_ch3 = 0xD7;   /* counter mode, interrupt enabled, software reset */
    ctc_ch3 = 0x01;   /* count 1 trigger */
#else
    /* native: ISR is called synchronously by test harness */
#endif
}

/* Wait for ISR to set completion flag. */
static void fdc_wait_complete(void) {
    while (fdc_isr_state.complete == 0)
        hal_ei();
}

/* Interrupt-driven seek/recalibrate: arm CTC Ch.3, send command,
 * wait for ISR flag, then mainline does SENSE INTERRUPT. */
void fdc_recalibrate(floppy_t *fl, byte drive) {
    fdc_arm_interrupt();
    fdc_send_byte(FDC_CMD_RECALIBRATE);
    fdc_send_byte(drive & 0x03);
    fdc_wait_complete();
    /* ISR set flag — now do SENSE INTERRUPT in mainline */
    byte st0, pcn;
    fdc_sense_interrupt(&st0, &pcn);
    fl->last_st0 = st0;
    fl->current_track = 0;
}

void fdc_seek(floppy_t *fl, byte drive, byte cylinder) {
    fdc_arm_interrupt();
    fdc_send_byte(FDC_CMD_SEEK);
    fdc_send_byte(drive & 0x03);
    fdc_send_byte(cylinder);
    fdc_wait_complete();
    byte st0, pcn;
    fdc_sense_interrupt(&st0, &pcn);
    fl->last_st0 = st0;
    fl->current_track = cylinder;
}

void dma_setup(word addr, word count, byte mode) {
#ifdef __SDCC
    dma_mask   = 0x05;                /* mask channel 1 */
    dma_mode   = mode;                /* set mode */
    dma_clear  = 0x00;               /* clear byte pointer */
    dma_addr1  = (byte)addr;          /* address low */
    dma_addr1  = (byte)(addr >> 8);   /* address high */
    dma_count1 = (byte)count;         /* count low */
    dma_count1 = (byte)(count >> 8);  /* count high */
    dma_mask   = 0x01;                /* unmask channel 1 */
#else
    hal_out(DMA_MASK_PORT, 0x05);
    hal_out(DMA_MODE_PORT, mode);
    hal_out(DMA_CLEAR_PORT, 0x00);
    hal_out(DMA_ADDR1_PORT, (byte)addr);
    hal_out(DMA_ADDR1_PORT, (byte)(addr >> 8));
    hal_out(DMA_COUNT1_PORT, (byte)count);
    hal_out(DMA_COUNT1_PORT, (byte)(count >> 8));
    hal_out(DMA_MASK_PORT, 0x01);
#endif
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

    fdc_arm_interrupt();
    fdc_send_byte(cmd);
    fdc_send_byte(hd_sel);
    fdc_send_byte(cylinder);
    fdc_send_byte(head);
    fdc_send_byte(sector);
    fdc_send_byte(fdf->n);
    fdc_send_byte(fdf->eot);
    fdc_send_byte(fdf->gap);
    fdc_send_byte((byte)(fdf->n == 0 ? 0x80 : 0xFF));

    /* DMA runs autonomously. ISR sets flag when FDC fires INT. */
    fdc_wait_complete();
    /* Mainline reads result bytes */
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

    fdc_arm_interrupt();
    fdc_send_byte(cmd);
    fdc_send_byte(hd_sel);
    fdc_send_byte(cylinder);
    fdc_send_byte(head);
    fdc_send_byte(sector);
    fdc_send_byte(fdf->n);
    fdc_send_byte(fdf->eot);
    fdc_send_byte(fdf->gap);
    fdc_send_byte((byte)(fdf->n == 0 ? 0x80 : 0xFF));

    fdc_wait_complete();
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
