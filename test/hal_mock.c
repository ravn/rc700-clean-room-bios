#include "hal.h"
#include "hal_mock.h"
#include <string.h>

static byte port_in[256];
static byte port_out[256];

void hal_mock_reset(void) {
    memset(port_in, 0, sizeof(port_in));
    memset(port_out, 0, sizeof(port_out));
}

void hal_mock_set_port(byte port, byte value) {
    port_in[port] = value;
}

byte hal_mock_get_port(byte port) {
    return port_out[port];
}

void hal_out(byte port, byte value) {
    port_out[port] = value;
}

byte hal_in(byte port) {
    return port_in[port];
}

void hal_di(void) {}
void hal_ei(void) {}
