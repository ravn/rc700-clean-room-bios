#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include <stdint.h>

/* Reset all mock state */
void hal_mock_reset(void);

/* Pre-set a port value that hal_in() will return */
void hal_mock_set_port(uint8_t port, uint8_t value);

/* Read what was last written to a port via hal_out() */
uint8_t hal_mock_get_port(uint8_t port);

#endif
