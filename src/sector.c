#include "sector.h"

const byte tran0[TRAN0_SIZE] = {
     1,  7, 13, 19, 25,  5, 11, 17, 23,  3,  9, 15,
    21,  2,  8, 14, 20, 26,  6, 12, 18, 24,  4, 10, 16, 22
};

const byte tran8[TRAN8_SIZE] = {
     1,  5,  9, 13,  2,  6, 10, 14,  3,  7, 11, 15,  4,  8, 12
};

const byte tran16[TRAN16_SIZE] = {
     1,  3,  5,  7,  9,  2,  4,  6,  8
};

const byte tran24[TRAN24_SIZE] = {
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26
};

byte sector_translate(const byte *table, byte table_size, byte logical) {
    if (logical >= table_size)
        return 0;
    return table[logical];
}
