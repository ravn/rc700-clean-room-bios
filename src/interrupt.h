#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "types.h"

/*
 * Z80 Interrupt Mode 2 vector table and ISR dispatch.
 * Per RC702_BIOS_SPECIFICATION.md Section 4.
 *
 * 18 entries at 0xF600 (page-aligned), 2 bytes each = 36 bytes.
 * I register = 0xF6, IM 2.
 */

#define IVT_BASE     0xF600
#define IVT_PAGE     0xF6
#define IVT_ENTRIES  18

/* Vector table indices */
#define IVT_CTC_CH0      0   /* baud rate — dummy */
#define IVT_CTC_CH1      1   /* baud rate — dummy */
#define IVT_CTC_CH2      2   /* display refresh + tick */
#define IVT_CTC_CH3      3   /* floppy complete */
#define IVT_CTC2_CH0     4   /* hard disk complete */
#define IVT_CTC2_CH1     5   /* dummy */
#define IVT_CTC2_CH2     6   /* dummy */
#define IVT_CTC2_CH3     7   /* dummy */
#define IVT_SIO_B_TX     8   /* console TX empty */
#define IVT_SIO_B_EXT    9   /* console external status */
#define IVT_SIO_B_RX    10   /* console data received */
#define IVT_SIO_B_SPEC  11   /* console special/error */
#define IVT_SIO_A_TX    12   /* printer TX empty */
#define IVT_SIO_A_EXT   13   /* printer external status */
#define IVT_SIO_A_RX    14   /* printer data received */
#define IVT_SIO_A_SPEC  15   /* printer special/error */
#define IVT_PIO_A       16   /* keyboard */
#define IVT_PIO_B       17   /* parallel port */

/* Display refresh ports */
#define CRT_STATUS_PORT  0x01
#define CRT_DATA_PORT    0x00
#define CRT_CMD_PORT     0x01
#define MOTOR_PORT       0x14

/* Timer/counter addresses in WorkArea (Section 18) */
#define TIMER1_ADDR   0xFFDF  /* exit timer countdown */
#define TIMER2_ADDR   0xFFE1  /* motor-off timer */
#define DELCNT_ADDR   0xFFE3  /* delay counter */
#define WARMJP_ADDR   0xFFE5  /* exit callback address */
#define RTC_ADDR      0xFFFC  /* 32-bit real-time clock */

/* Floppy ISR state */
typedef struct {
    byte complete;            /* set to 0xFF when FDC operation done */
    byte st0;                 /* saved ST0 from result phase */
    byte st1;                 /* saved ST1 */
} fdc_isr_state_t;

/* Display ISR state */
typedef struct {
    byte *display_buf;        /* pointer to display buffer (for DMA programming) */
    byte  cursor_dirty;       /* set when cursor position changed */
    byte  curx;               /* current cursor column */
    byte  cursy;              /* current cursor row */
    word  timer1;             /* exit timer (20ms ticks) */
    word  timer2;             /* motor-off timer */
    word  delcnt;             /* delay counter */
    word  warmjp;             /* exit callback address */
    uint32_t rtc;             /* 32-bit real-time clock counter */
} display_isr_state_t;

extern fdc_isr_state_t fdc_isr_state;
extern display_isr_state_t display_isr_state;

/* ISR handlers — called from the vector table dispatch */
void isr_display_refresh(void);   /* CTC Ch.2 — 50 Hz */
void isr_floppy_complete(void);
/* isr_dummy defined in crt0.asm (EI+RETI), not in C */

/* Reprogram DMA channels 2+3 for display refresh */
void isr_reprogram_display_dma(void);

/* Update cursor on 8275 */
void isr_update_cursor(void);

/* Tick timers (called by display ISR) */
void isr_tick_timers(void);

#endif
