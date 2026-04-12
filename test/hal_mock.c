#include "hal.h"
#include "hal_mock.h"
#include <string.h>

static uint8_t port_in[256];
static uint8_t port_out[256];

void hal_mock_reset(void) {
    memset(port_in, 0, sizeof(port_in));
    memset(port_out, 0, sizeof(port_out));
}

void hal_mock_set_port(uint8_t port, uint8_t value) {
    port_in[port] = value;
}

uint8_t hal_mock_get_port(uint8_t port) {
    return port_out[port];
}

void hal_out(uint8_t port, uint8_t value) {
    port_out[port] = value;
}

uint8_t hal_in(uint8_t port) {
    return port_in[port];
}

void hal_di(void) {}
void hal_ei(void) {}
