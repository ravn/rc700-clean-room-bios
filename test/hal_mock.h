#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include "types.h"

/* Reset all mock state */
void hal_mock_reset(void);

/* Pre-set a port value that hal_in() will return */
void hal_mock_set_port(byte port, byte value);

/* Read what was last written to a port via hal_out() */
byte hal_mock_get_port(byte port);

#endif
