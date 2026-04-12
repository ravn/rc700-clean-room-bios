#include <assert.h>
#include <stdio.h>
#include "sector.h"

static void test_tran0_values(void) {
    /* 8" SS FM, skew 6: verify all 26 entries from spec */
    const uint8_t expected[] = {
         1,  7, 13, 19, 25,  5, 11, 17, 23,  3,  9, 15,
        21,  2,  8, 14, 20, 26,  6, 12, 18, 24,  4, 10, 16, 22
    };
    for (int i = 0; i < TRAN0_SIZE; i++)
        assert(tran0[i] == expected[i]);
}

static void test_tran8_values(void) {
    /* 8" DD MFM 512 B/S, skew 4: verify all 15 entries */
    const uint8_t expected[] = {
         1,  5,  9, 13,  2,  6, 10, 14,  3,  7, 11, 15,  4,  8, 12
    };
    for (int i = 0; i < TRAN8_SIZE; i++)
        assert(tran8[i] == expected[i]);
}

static void test_tran16_values(void) {
    /* 5.25" DD MFM 512 B/S, skew 2: verify all 9 entries */
    const uint8_t expected[] = { 1, 3, 5, 7, 9, 2, 4, 6, 8 };
    for (int i = 0; i < TRAN16_SIZE; i++)
        assert(tran16[i] == expected[i]);
}

static void test_tran24_identity(void) {
    /* 8" DD MFM 256 B/S, identity: 1..26 */
    for (int i = 0; i < TRAN24_SIZE; i++)
        assert(tran24[i] == (uint8_t)(i + 1));
}

static void test_translate_function(void) {
    /* Valid lookups */
    assert(sector_translate(tran0, TRAN0_SIZE, 0) == 1);
    assert(sector_translate(tran0, TRAN0_SIZE, 1) == 7);
    assert(sector_translate(tran0, TRAN0_SIZE, 25) == 22);

    assert(sector_translate(tran8, TRAN8_SIZE, 0) == 1);
    assert(sector_translate(tran8, TRAN8_SIZE, 14) == 12);

    /* Out of bounds returns 0 */
    assert(sector_translate(tran0, TRAN0_SIZE, 26) == 0);
    assert(sector_translate(tran8, TRAN8_SIZE, 15) == 0);
    assert(sector_translate(tran16, TRAN16_SIZE, 9) == 0);
    assert(sector_translate(tran24, TRAN24_SIZE, 26) == 0);
}

static void test_tables_are_permutations(void) {
    /* Every table must contain each value 1..N exactly once */
    uint8_t seen[27] = {0};

    for (int i = 0; i < TRAN0_SIZE; i++) {
        assert(tran0[i] >= 1 && tran0[i] <= 26);
        assert(seen[tran0[i]] == 0);
        seen[tran0[i]] = 1;
    }

    uint8_t seen8[16] = {0};
    for (int i = 0; i < TRAN8_SIZE; i++) {
        assert(tran8[i] >= 1 && tran8[i] <= 15);
        assert(seen8[tran8[i]] == 0);
        seen8[tran8[i]] = 1;
    }

    uint8_t seen16[10] = {0};
    for (int i = 0; i < TRAN16_SIZE; i++) {
        assert(tran16[i] >= 1 && tran16[i] <= 9);
        assert(seen16[tran16[i]] == 0);
        seen16[tran16[i]] = 1;
    }
}

int main(void) {
    test_tran0_values();
    test_tran8_values();
    test_tran16_values();
    test_tran24_identity();
    test_translate_function();
    test_tables_are_permutations();
    printf("All sector tests passed.\n");
    return 0;
}
