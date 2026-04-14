#include <stddef.h>
#include "interrupt.h"
#include "hal.h"

fdc_isr_state_t fdc_isr_state;
display_isr_state_t display_isr_state;

/* ---- Display Refresh ISR (Section 4.1, 50 Hz) ---- */

void isr_reprogram_display_dma(void) {
    /* Only use DMA channel 2 for the full 2000-byte display transfer.
     * Channel 3 stays masked (not used until scrolling is implemented). */
    byte addr_lo = (byte)(word)(size_t)display_isr_state.display_buf;
    byte addr_hi = (byte)((word)(size_t)display_isr_state.display_buf >> 8);

    hal_out(0xFA, 0x06);  /* mask ch.2 */
    hal_out(0xFA, 0x07);  /* mask ch.3 */
    hal_out(0xFC, 0x00);  /* clear byte pointer flip-flop */

    hal_out(0xF4, addr_lo);   /* ch.2 addr low */
    hal_out(0xF4, addr_hi);   /* ch.2 addr high */
    hal_out(0xF5, 0xCF);      /* ch.2 count low (1999 & 0xFF) */
    hal_out(0xF5, 0x07);      /* ch.2 count high (1999 >> 8) */

    hal_out(0xFA, 0x02);  /* unmask ch.2 */
}

void isr_update_cursor(void) {
    if (!display_isr_state.cursor_dirty)
        return;
    display_isr_state.cursor_dirty = 0;

    hal_out(CRT_CMD_PORT, 0x80);  /* Load Cursor command */
    hal_out(CRT_DATA_PORT, display_isr_state.curx);
    hal_out(CRT_DATA_PORT, display_isr_state.cursy);
}

void isr_tick_timers(void) {
    /* Increment 32-bit RTC */
    display_isr_state.rtc++;

    /* Decrement TIMER1 (exit timer) */
    if (display_isr_state.timer1 > 0) {
        display_isr_state.timer1--;
        if (display_isr_state.timer1 == 0) {
            /* Call exit callback — on Z80 this would be:
             *   LD HL,(WARMJP); JP (HL)
             * On native we just note it fired. */
        }
    }

    /* Decrement TIMER2 (motor-off timer) */
    if (display_isr_state.timer2 > 0) {
        display_isr_state.timer2--;
        if (display_isr_state.timer2 == 0) {
            hal_out(MOTOR_PORT, 0x00);  /* motor off */
        }
    }

    /* Decrement DELCNT (delay counter) */
    if (display_isr_state.delcnt > 0)
        display_isr_state.delcnt--;
}

void isr_display_refresh(void) {
    /* 1. Acknowledge interrupt by reading 8275 status */
    (void)hal_in(CRT_STATUS_PORT);

    /* 2. Reprogram DMA for next frame */
    isr_reprogram_display_dma();

    /* 3. Update cursor if changed */
    isr_update_cursor();

    /* 4. Reprogram CTC Ch.2 for next interrupt */
    hal_out(0x0E, 0xD7);  /* counter mode, interrupt enabled */
    hal_out(0x0E, 0x01);  /* count 1 */

    /* 5-6. Tick timers and RTC */
    isr_tick_timers();
}

/* ---- Floppy Complete ISR (Section 4.2) ---- */

/* Use __sfr for FDC ports — generates IN A,(n) / OUT (n),A
 * which doesn't involve the B register. */
#ifdef __SDCC
__sfr __at(0x04) isr_fdc_msr;
__sfr __at(0x05) isr_fdc_data;
__sfr __at(0x0F) isr_ctc_ch3;
#define ISR_FDC_MSR   isr_fdc_msr
#define ISR_FDC_DATA  isr_fdc_data
#else
#define ISR_FDC_MSR   hal_in(0x04)
#define ISR_FDC_DATA  hal_in(0x05)
#endif

#ifdef __SDCC
void isr_floppy_complete(void) __interrupt {
#else
void isr_floppy_complete(void) {
#endif
    /* Check MSR bit 4 (CB — Controller Busy).
     * CB=1 → result phase (READ/WRITE): read 7 result bytes.
     * CB=0 → seek complete: issue SENSE INTERRUPT STATUS. */
    byte msr = ISR_FDC_MSR;
    if (msr & 0x10) {
        /* Result phase: read all 7 result bytes */
        fdc_isr_state.st0 = ISR_FDC_DATA;
        fdc_isr_state.st1 = ISR_FDC_DATA;
        for (int i = 0; i < 5; i++)
            (void)ISR_FDC_DATA;
    } else {
        /* Seek/recalibrate complete: SENSE INTERRUPT STATUS */
#ifdef __SDCC
        isr_fdc_data = 0x08;  /* send SENSE INT command */
#else
        hal_out(0x05, 0x08);
#endif
        fdc_isr_state.st0 = ISR_FDC_DATA;
        (void)ISR_FDC_DATA;  /* PCN */
    }

    /* Re-arm CTC Ch.3 for next FDC operation */
#ifdef __SDCC
    isr_ctc_ch3 = 0xD7;
    isr_ctc_ch3 = 0x01;
#else
    hal_out(0x0F, 0xD7);
    hal_out(0x0F, 0x01);
#endif

    /* Signal completion LAST — mainline polls this */
    fdc_isr_state.complete = 0xFF;
}

/* ---- Dummy ISR ---- */

void isr_dummy(void) {
    /* Do nothing — used for baud rate CTC channels and unused vectors */
}
