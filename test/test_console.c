#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "console.h"

static void test_init(void) {
    console_t con;
    console_init(&con);
    assert(con.curx == 0);
    assert(con.cursy == 0);
    assert(con.cury == 0);
    assert(con.xflg == XY_STATE_NORMAL);
    /* Display should be all spaces */
    for (int i = 0; i < SCREEN_SIZE; i++)
        assert(con.display[i] == 0x20);
}

static void test_printable(void) {
    console_t con;
    console_init(&con);
    console_putchar(&con, 'A');
    assert(con.display[0] == 'A');
    assert(con.curx == 1);
    assert(con.cursy == 0);

    console_putchar(&con, 'B');
    assert(con.display[1] == 'B');
    assert(con.curx == 2);
}

static void test_cursor_movement(void) {
    console_t con;
    console_init(&con);

    /* Move right */
    console_cursor_right(&con);
    assert(con.curx == 1);

    /* Move left */
    console_cursor_left(&con);
    assert(con.curx == 0);

    /* Left at 0,0 wraps to 79,24 */
    console_cursor_left(&con);
    assert(con.curx == 79);
    assert(con.cursy == 24);

    /* Cursor up from row 24 */
    console_cursor_up(&con);
    assert(con.cursy == 23);

    /* Cursor up from row 0 wraps to row 24 */
    console_home(&con);
    console_cursor_up(&con);
    assert(con.cursy == 24);
}

static void test_line_wrap(void) {
    console_t con;
    console_init(&con);

    /* Move to column 79 */
    for (int i = 0; i < 79; i++)
        console_cursor_right(&con);
    assert(con.curx == 79);
    assert(con.cursy == 0);

    /* One more right wraps to next row */
    console_cursor_right(&con);
    assert(con.curx == 0);
    assert(con.cursy == 1);
}

static void test_scroll(void) {
    console_t con;
    console_init(&con);

    /* Put identifying chars on each row */
    for (int r = 0; r < SCREEN_ROWS; r++)
        con.display[r * SCREEN_COLS] = (byte)('A' + r);

    /* Move to last row, then linefeed to trigger scroll */
    con.cursy = SCREEN_ROWS - 1;
    con.cury = (word)(SCREEN_ROWS - 1) * SCREEN_COLS;
    console_linefeed(&con);

    /* Row 0 should now have what was row 1 */
    assert(con.display[0] == 'B');
    /* Last row should be spaces */
    assert(con.display[(SCREEN_ROWS - 1) * SCREEN_COLS] == 0x20);
    /* Cursor stays on last row */
    assert(con.cursy == SCREEN_ROWS - 1);
}

static void test_clear_screen(void) {
    console_t con;
    console_init(&con);
    console_putchar(&con, 'X');
    console_putchar(&con, 'Y');

    console_putchar(&con, 0x0C);  /* FF = clear screen */
    assert(con.curx == 0);
    assert(con.cursy == 0);
    assert(con.display[0] == 0x20);
    assert(con.display[1] == 0x20);
}

static void test_carriage_return_linefeed(void) {
    console_t con;
    console_init(&con);
    console_putchar(&con, 'H');
    console_putchar(&con, 'i');
    assert(con.curx == 2);

    console_putchar(&con, 0x0D);  /* CR */
    assert(con.curx == 0);
    assert(con.cursy == 0);

    console_putchar(&con, 0x0A);  /* LF */
    assert(con.curx == 0);
    assert(con.cursy == 1);
}

static void test_tab(void) {
    console_t con;
    console_init(&con);
    console_putchar(&con, 0x09);  /* TAB */
    assert(con.curx == 4);
    console_putchar(&con, 0x09);
    assert(con.curx == 8);
}

static void test_erase_to_eol(void) {
    console_t con;
    console_init(&con);

    /* Write some chars */
    for (int i = 0; i < 10; i++)
        console_putchar(&con, (byte)('0' + i));

    /* Move to col 5, erase to EOL */
    con.curx = 5;
    con.cury = 0;
    console_putchar(&con, 0x1E);  /* RS = erase to EOL */

    /* Cols 0-4 preserved, 5+ erased */
    assert(con.display[0] == '0');
    assert(con.display[4] == '4');
    assert(con.display[5] == 0x20);
    assert(con.display[79] == 0x20);
}

static void test_erase_to_eos(void) {
    console_t con;
    console_init(&con);

    con.display[0] = 'A';
    con.display[SCREEN_SIZE - 1] = 'Z';

    /* Erase from position (0,1) to end */
    con.curx = 0;
    con.cursy = 1;
    con.cury = SCREEN_COLS;
    console_putchar(&con, 0x1F);  /* US = erase to EOS */

    assert(con.display[0] == 'A');  /* row 0 preserved */
    assert(con.display[SCREEN_COLS] == 0x20);  /* row 1 erased */
    assert(con.display[SCREEN_SIZE - 1] == 0x20);
}

static void test_xy_escape_xy_mode(void) {
    console_t con;
    console_init(&con);
    con.adrmod = 0;  /* XY mode: first=col, second=row */

    console_putchar(&con, 0x06);  /* ACK = enter XY mode */
    assert(con.xflg == XY_STATE_BYTE1);

    console_putchar(&con, 32 + 10);  /* column 10 */
    assert(con.xflg == XY_STATE_BYTE2);

    console_putchar(&con, 32 + 5);   /* row 5 */
    assert(con.xflg == XY_STATE_NORMAL);
    assert(con.curx == 10);
    assert(con.cursy == 5);
    assert(con.cury == 5 * SCREEN_COLS);
}

static void test_xy_escape_yx_mode(void) {
    console_t con;
    console_init(&con);
    con.adrmod = 1;  /* YX mode: first=row, second=col */

    console_putchar(&con, 0x06);
    console_putchar(&con, 32 + 12);  /* row 12 */
    console_putchar(&con, 32 + 40);  /* col 40 */

    assert(con.curx == 40);
    assert(con.cursy == 12);
}

static void test_xy_wrap(void) {
    console_t con;
    console_init(&con);
    con.adrmod = 0;

    console_putchar(&con, 0x06);
    console_putchar(&con, 32 + 85);  /* col 85 >= 80, should wrap to 5 */
    console_putchar(&con, 32 + 27);  /* row 27 >= 25, should wrap to 2 */

    assert(con.curx == 5);
    assert(con.cursy == 2);
}

static void test_insert_line(void) {
    console_t con;
    console_init(&con);

    /* Put 'A' on row 0, 'B' on row 1, 'C' on row 2 */
    con.display[0 * SCREEN_COLS] = 'A';
    con.display[1 * SCREEN_COLS] = 'B';
    con.display[2 * SCREEN_COLS] = 'C';

    /* Insert line at row 1 */
    con.cursy = 1;
    con.cury = SCREEN_COLS;
    console_insert_line(&con);

    assert(con.display[0 * SCREEN_COLS] == 'A');  /* row 0 unchanged */
    assert(con.display[1 * SCREEN_COLS] == 0x20); /* row 1 cleared */
    assert(con.display[2 * SCREEN_COLS] == 'B');  /* old row 1 → row 2 */
    assert(con.display[3 * SCREEN_COLS] == 'C');  /* old row 2 → row 3 */
}

static void test_delete_line(void) {
    console_t con;
    console_init(&con);

    con.display[0 * SCREEN_COLS] = 'A';
    con.display[1 * SCREEN_COLS] = 'B';
    con.display[2 * SCREEN_COLS] = 'C';

    /* Delete line at row 0 */
    con.cursy = 0;
    con.cury = 0;
    console_delete_line(&con);

    assert(con.display[0 * SCREEN_COLS] == 'B');  /* old row 1 → row 0 */
    assert(con.display[1 * SCREEN_COLS] == 'C');  /* old row 2 → row 1 */
    assert(con.display[(SCREEN_ROWS - 1) * SCREEN_COLS] == 0x20);  /* last row cleared */
}

static void test_background_mode(void) {
    console_t con;
    console_init(&con);

    /* Enter background mode */
    console_putchar(&con, 0x13);  /* DC3 = enter background */
    assert(con.bg_flag == BG_BACKGROUND);

    /* Write some chars — should set bgmap bits */
    console_putchar(&con, 'X');
    console_putchar(&con, 'Y');

    /* Enter foreground mode */
    console_putchar(&con, 0x14);  /* DC4 = enter foreground */
    assert(con.bg_flag == BG_FOREGROUND);

    /* Write foreground char */
    console_putchar(&con, 'Z');

    /* Clear foreground — X and Y should survive, Z should be erased */
    console_putchar(&con, 0x15);  /* NAK = clear foreground */
    assert(con.display[0] == 'X');  /* background preserved */
    assert(con.display[1] == 'Y');  /* background preserved */
    assert(con.display[2] == 0x20); /* foreground erased */
}

static void test_home(void) {
    console_t con;
    console_init(&con);
    con.curx = 40;
    con.cursy = 12;
    con.cury = 12 * SCREEN_COLS;

    console_putchar(&con, 0x1D);  /* GS = home */
    assert(con.curx == 0);
    assert(con.cursy == 0);
}

static void test_ignored_control_codes(void) {
    console_t con;
    console_init(&con);
    /* Codes not in dispatch table should be silently ignored */
    console_putchar(&con, 0x00);
    console_putchar(&con, 0x03);
    console_putchar(&con, 0x04);
    console_putchar(&con, 0x0B);
    console_putchar(&con, 0x10);
    assert(con.curx == 0);
    assert(con.cursy == 0);
}

int main(void) {
    test_init();
    test_printable();
    test_cursor_movement();
    test_line_wrap();
    test_scroll();
    test_clear_screen();
    test_carriage_return_linefeed();
    test_tab();
    test_erase_to_eol();
    test_erase_to_eos();
    test_xy_escape_xy_mode();
    test_xy_escape_yx_mode();
    test_xy_wrap();
    test_insert_line();
    test_delete_line();
    test_background_mode();
    test_home();
    test_ignored_control_codes();
    printf("All console tests passed.\n");
    return 0;
}
