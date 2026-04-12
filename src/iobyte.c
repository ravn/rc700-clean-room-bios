#include "iobyte.h"

uint8_t iobyte_con_mode(uint8_t iobyte) {
    return iobyte & 0x03;
}

uint8_t iobyte_rdr_mode(uint8_t iobyte) {
    return (iobyte >> 2) & 0x03;
}

uint8_t iobyte_pun_mode(uint8_t iobyte) {
    return (iobyte >> 4) & 0x03;
}

uint8_t iobyte_lst_mode(uint8_t iobyte) {
    return (iobyte >> 6) & 0x03;
}

int iobyte_keyboard_allowed(uint8_t iobyte) {
    return (iobyte_con_mode(iobyte) & 1) != 0;
}

int iobyte_serial_allowed(uint8_t iobyte) {
    return iobyte_con_mode(iobyte) != CON_CRT;
}
