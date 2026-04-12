#ifndef HWINIT_H
#define HWINIT_H

#include "types.h"

/*
 * Hardware initialization — Section 7.
 *
 * Programs all I/O devices in the required order:
 *   1. Interrupt vector table (IM2)
 *   2. PIO
 *   3. CTC
 *   4. SIO
 *   5. DMA
 *   6. FDC (SPECIFY)
 *   7. 8275 Display controller
 *
 * Uses the CONFI configuration block for actual register values.
 * On native test builds, all I/O goes through the HAL mock.
 */

/* I/O port addresses */
#define PIO_A_CTRL  0x12
#define PIO_B_CTRL  0x13

#define CTC_CH0     0x0C
#define CTC_CH1     0x0D
#define CTC_CH2     0x0E
#define CTC_CH3     0x0F

#define SIO_A_CTRL_PORT  0x0A
#define SIO_B_CTRL_PORT  0x0B

#define DMA_CMD_PORT     0xF8
#define DMA_MODE_PORT_HW 0xFB

#define CRT_DATA    0x00
#define CRT_CMD     0x01

/* Initialize all hardware from the CONFI block */
void hw_init_all(const byte *confi);

/* Individual init routines (called by hw_init_all, exposed for testing) */
void hw_init_pio(void);
void hw_init_ctc(const byte *confi);
void hw_init_sio(const byte *confi);
void hw_init_dma(const byte *confi);
void hw_init_fdc(const byte *confi);
void hw_init_display(const byte *confi);

#endif
