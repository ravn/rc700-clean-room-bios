#include <assert.h>
#include <stdio.h>
#include "iobyte.h"

static void test_field_extraction(void) {
    /* 0x97 = 10_01_01_11 = LST:LPT, PUN:CRT, RDR:CRT, CON:UC1 */
    assert(iobyte_con_mode(0x97) == CON_UC1);
    assert(iobyte_rdr_mode(0x97) == RDR_CRT);
    assert(iobyte_pun_mode(0x97) == PUN_CRT);
    assert(iobyte_lst_mode(0x97) == LST_LPT);

    /* 0x95 = 10_01_01_01 = LST:LPT, PUN:CRT, RDR:CRT, CON:CRT */
    assert(iobyte_con_mode(0x95) == CON_CRT);
    assert(iobyte_rdr_mode(0x95) == RDR_CRT);
    assert(iobyte_pun_mode(0x95) == PUN_CRT);
    assert(iobyte_lst_mode(0x95) == LST_LPT);

    /* 0x00 = all TTY */
    assert(iobyte_con_mode(0x00) == CON_TTY);
    assert(iobyte_rdr_mode(0x00) == RDR_TTY);
    assert(iobyte_pun_mode(0x00) == PUN_TTY);
    assert(iobyte_lst_mode(0x00) == LST_TTY);

    /* 0xFF = all UC1/UL1 */
    assert(iobyte_con_mode(0xFF) == CON_UC1);
    assert(iobyte_rdr_mode(0xFF) == RDR_UC1);
    assert(iobyte_pun_mode(0xFF) == PUN_UC1);
    assert(iobyte_lst_mode(0xFF) == LST_UL1);
}

static void test_keyboard_allowed(void) {
    /* Keyboard allowed when CON field bit 0 is set: CRT(1) and UC1(3) */
    assert(!iobyte_keyboard_allowed(0x00));  /* CON:TTY(0) */
    assert( iobyte_keyboard_allowed(0x01));  /* CON:CRT(1) */
    assert(!iobyte_keyboard_allowed(0x02));  /* CON:BAT(2) */
    assert( iobyte_keyboard_allowed(0x03));  /* CON:UC1(3) */

    /* With preset values */
    assert( iobyte_keyboard_allowed(IOBYTE_JOINED));  /* UC1 */
    assert( iobyte_keyboard_allowed(IOBYTE_LOCAL));   /* CRT */
}

static void test_serial_allowed(void) {
    /* Serial allowed when CON field != CRT(1): TTY(0), BAT(2), UC1(3) */
    assert( iobyte_serial_allowed(0x00));  /* CON:TTY(0) */
    assert(!iobyte_serial_allowed(0x01));  /* CON:CRT(1) */
    assert( iobyte_serial_allowed(0x02));  /* CON:BAT(2) */
    assert( iobyte_serial_allowed(0x03));  /* CON:UC1(3) */

    /* With preset values */
    assert( iobyte_serial_allowed(IOBYTE_JOINED));  /* UC1 */
    assert(!iobyte_serial_allowed(IOBYTE_LOCAL));   /* CRT */
}

static void test_preset_values(void) {
    /* Verify the preset constants match the spec */
    assert(IOBYTE_JOINED == 0x97);
    assert(IOBYTE_LOCAL  == 0x95);

    /* Only difference between joined and local is CON field */
    assert(iobyte_lst_mode(IOBYTE_JOINED) == iobyte_lst_mode(IOBYTE_LOCAL));
    assert(iobyte_pun_mode(IOBYTE_JOINED) == iobyte_pun_mode(IOBYTE_LOCAL));
    assert(iobyte_rdr_mode(IOBYTE_JOINED) == iobyte_rdr_mode(IOBYTE_LOCAL));
    assert(iobyte_con_mode(IOBYTE_JOINED) != iobyte_con_mode(IOBYTE_LOCAL));
}

int main(void) {
    test_field_extraction();
    test_keyboard_allowed();
    test_serial_allowed();
    test_preset_values();
    printf("All iobyte tests passed.\n");
    return 0;
}
