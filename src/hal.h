#ifndef HAL_H
#define HAL_H

#include "types.h"

/* Port I/O */
void hal_out(byte port, byte value);
byte hal_in(byte port);

/* Interrupt control */
void hal_di(void);
void hal_ei(void);

#endif
