#include "iobyte.h"

byte iobyte_con_mode(byte iobyte) {
    return iobyte & 0x03;
}

byte iobyte_rdr_mode(byte iobyte) {
    return (iobyte >> 2) & 0x03;
}

byte iobyte_pun_mode(byte iobyte) {
    return (iobyte >> 4) & 0x03;
}

byte iobyte_lst_mode(byte iobyte) {
    return (iobyte >> 6) & 0x03;
}

int iobyte_keyboard_allowed(byte iobyte) {
    return (iobyte_con_mode(iobyte) & 1) != 0;
}

int iobyte_serial_allowed(byte iobyte) {
    return iobyte_con_mode(iobyte) != CON_CRT;
}
