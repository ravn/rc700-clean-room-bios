#include "hwinit.h"
#include "hal.h"
#include "config.h"

/* ---- 7.2 PIO Initialization ---- */

void hw_init_pio(void) {
    /* Vector must be written BEFORE mode word (Z80-PIO protocol) */
    hal_out(PIO_A_CTRL, 0x20);  /* Channel A: interrupt vector 0x20 */
    hal_out(PIO_B_CTRL, 0x22);  /* Channel B: interrupt vector 0x22 */
    hal_out(PIO_A_CTRL, 0x4F);  /* Channel A: input mode */
    hal_out(PIO_B_CTRL, 0x0F);  /* Channel B: output mode */
    hal_out(PIO_A_CTRL, 0x83);  /* Channel A: enable interrupts */
    hal_out(PIO_B_CTRL, 0x83);  /* Channel B: enable interrupts */
}

/* ---- 7.3 CTC Initialization ---- */

void hw_init_ctc(const byte *confi) {
    /* Write vector base to Ch.0 first */
    hal_out(CTC_CH0, 0x00);  /* interrupt vector base = 0x00 */

    /* Program each channel: mode byte, then time constant */
    hal_out(CTC_CH0, confi[CONFI_CTC_CH0_MODE]);
    hal_out(CTC_CH0, confi[CONFI_CTC_CH0_COUNT]);
    hal_out(CTC_CH1, confi[CONFI_CTC_CH1_MODE]);
    hal_out(CTC_CH1, confi[CONFI_CTC_CH1_COUNT]);
    hal_out(CTC_CH2, confi[CONFI_CTC_CH2_MODE]);
    hal_out(CTC_CH2, confi[CONFI_CTC_CH2_COUNT]);
    hal_out(CTC_CH3, confi[CONFI_CTC_CH3_MODE]);
    hal_out(CTC_CH3, confi[CONFI_CTC_CH3_COUNT]);
}

/* ---- 7.4 SIO Initialization ---- */

void hw_init_sio(const byte *confi) {
    /* Channel A: 9 bytes from CONFI offset 0x08 */
    const byte *sio_a = &confi[CONFI_SIO_A_START];
    for (int i = 0; i < CONFI_SIO_A_SIZE; i++)
        hal_out(SIO_A_CTRL_PORT, sio_a[i]);

    /* Channel B: 11 bytes from CONFI offset 0x11 */
    const byte *sio_b = &confi[CONFI_SIO_B_START];
    for (int i = 0; i < CONFI_SIO_B_SIZE; i++)
        hal_out(SIO_B_CTRL_PORT, sio_b[i]);

    /* Clear pending conditions by reading status registers */
    (void)hal_in(SIO_A_CTRL_PORT);           /* read RR0-A */
    hal_out(SIO_A_CTRL_PORT, 0x01);          /* select RR1-A */
    (void)hal_in(SIO_A_CTRL_PORT);           /* read RR1-A */
    (void)hal_in(SIO_B_CTRL_PORT);           /* read RR0-B */
    hal_out(SIO_B_CTRL_PORT, 0x01);          /* select RR1-B */
    (void)hal_in(SIO_B_CTRL_PORT);           /* read RR1-B */
}

/* ---- 7.5 DMA Initialization ---- */

void hw_init_dma(const byte *confi) {
    hal_out(DMA_CMD_PORT, 0x20);             /* master clear */
    hal_out(DMA_MODE_PORT_HW, confi[CONFI_DMA_MODE_START + 0]);  /* Ch.0 mode */
    hal_out(DMA_MODE_PORT_HW, confi[CONFI_DMA_MODE_START + 2]);  /* Ch.2 mode */
    hal_out(DMA_MODE_PORT_HW, confi[CONFI_DMA_MODE_START + 3]);  /* Ch.3 mode */
    /* Ch.1 (floppy) not configured here — done per-operation */
}

/* ---- 7.6 FDC Initialization ---- */

void hw_init_fdc(const byte *confi) {
    /* Wait for FDC idle (bits 4:0 = 0) */
    while (hal_in(0x04) & 0x1F)
        ;

    /* Send SPECIFY command: 3 bytes from CONFI offset 0x25 */
    /* Wait for RQM=1, DIO=0 before each byte */
    const byte *fdc_spec = &confi[CONFI_FDC_SPEC_START];
    byte count = fdc_spec[0];  /* byte count (3) */
    for (byte i = 1; i <= count; i++) {
        while ((hal_in(0x04) & 0xC0) != 0x80)
            ;
        hal_out(0x05, fdc_spec[i]);
    }
}

/* ---- 7.7 Display Controller (8275) Initialization ---- */

void hw_init_display(const byte *confi) {
    const byte *crt = &confi[CONFI_CRT_PAR_START];

    /* Reset 8275 */
    hal_out(CRT_CMD, 0x00);

    /* Send 4 parameter bytes */
    hal_out(CRT_DATA, crt[0]);  /* PAR1: 80 chars/row */
    hal_out(CRT_DATA, crt[1]);  /* PAR2: 25 rows */
    hal_out(CRT_DATA, crt[2]);  /* PAR3: retrace timing */
    hal_out(CRT_DATA, crt[3]);  /* PAR4: char height + cursor */

    /* Load cursor to (0,0) */
    hal_out(CRT_CMD, 0x80);     /* load cursor command */
    hal_out(CRT_DATA, 0x00);    /* cursor X */
    hal_out(CRT_DATA, 0x00);    /* cursor Y */

    /* Preset counters and start display */
    hal_out(CRT_CMD, 0xE0);     /* preset counters */
    hal_out(CRT_CMD, 0x23);     /* start display */
}

/* ---- Full hardware init sequence ---- */

void hw_init_all(const byte *confi) {
    /* Section 7.1: IM2 setup is done in assembly (crt0) before calling this.
     * The I register and IM2 instruction must be executed in Z80 asm. */

    hw_init_pio();
    hw_init_ctc(confi);
    hw_init_sio(confi);
    hw_init_dma(confi);
    hw_init_fdc(confi);
    hw_init_display(confi);
}
