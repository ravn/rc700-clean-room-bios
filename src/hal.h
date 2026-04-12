#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/* Port I/O */
void hal_out(uint8_t port, uint8_t value);
uint8_t hal_in(uint8_t port);

/* Interrupt control */
void hal_di(void);
void hal_ei(void);

#endif
