#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "deblock.h"

/* Mock disk storage: 4 tracks x 16 sectors x 512 bytes */
static uint8_t mock_disk[4][16][512];
static int read_count;
static int write_count;

static int mock_read(uint8_t disk, uint16_t track, uint8_t sector,
                     uint8_t *buf) {
    (void)disk;
    read_count++;
    if (track >= 4 || sector >= 16) return 1;
    memcpy(buf, mock_disk[track][sector], 512);
    return 0;
}

static int mock_write(uint8_t disk, uint16_t track, uint8_t sector,
                      const uint8_t *buf) {
    (void)disk;
    write_count++;
    if (track >= 4 || sector >= 16) return 1;
    memcpy(mock_disk[track][sector], buf, 512);
    return 0;
}

static void reset_mock(void) {
    memset(mock_disk, 0, sizeof(mock_disk));
    read_count = 0;
    write_count = 0;
}

static void test_read_basic(void) {
    deblock_t db;
    uint8_t dma[128];
    reset_mock();
    deblock_init(&db, mock_read, mock_write);

    /* Pre-fill physical sector with known pattern */
    for (int i = 0; i < 512; i++)
        mock_disk[0][0][i] = (uint8_t)i;

    /* Setup: 512-byte physical sectors, mask=3, shift=3 */
    db.secmsk = 3;
    db.secshf = 2;  /* shift=2 for 512-byte: seksec>>2 = host sector */
    db.sekdsk = 0;
    db.sektrk = 0;
    db.seksec = 0;
    db.dmaadr = dma;

    assert(deblock_read(&db) == 0);
    assert(read_count == 1);
    /* First 128 bytes of physical sector */
    for (int i = 0; i < 128; i++)
        assert(dma[i] == (uint8_t)i);

    /* Read second record from same physical sector — no disk read */
    db.seksec = 1;
    assert(deblock_read(&db) == 0);
    assert(read_count == 1);  /* still 1, cached */
    for (int i = 0; i < 128; i++)
        assert(dma[i] == (uint8_t)(i + 128));
}

static void test_write_deferred(void) {
    deblock_t db;
    uint8_t dma[128];
    reset_mock();
    deblock_init(&db, mock_read, mock_write);

    db.secmsk = 3;
    db.secshf = 2;
    db.sekdsk = 0;
    db.sektrk = 0;
    db.seksec = 0;
    db.dmaadr = dma;

    memset(dma, 0xAA, 128);
    assert(deblock_write(&db, WRTYP_DEFERRED) == 0);
    assert(write_count == 0);  /* deferred — not written yet */

    /* Flush should write */
    assert(deblock_flush(&db) == 0);
    assert(write_count == 1);
    /* Verify data on disk */
    for (int i = 0; i < 128; i++)
        assert(mock_disk[0][0][i] == 0xAA);
}

static void test_write_directory_immediate(void) {
    deblock_t db;
    uint8_t dma[128];
    reset_mock();
    deblock_init(&db, mock_read, mock_write);

    db.secmsk = 3;
    db.secshf = 2;
    db.sekdsk = 0;
    db.sektrk = 0;
    db.seksec = 0;
    db.dmaadr = dma;

    memset(dma, 0xBB, 128);
    assert(deblock_write(&db, WRTYP_DIRECTORY) == 0);
    assert(write_count == 1);  /* directory write flushes immediately */
}

static void test_write_unalloc_skips_preread(void) {
    deblock_t db;
    uint8_t dma[128];
    reset_mock();
    deblock_init(&db, mock_read, mock_write);

    db.secmsk = 3;
    db.secshf = 2;
    db.sekdsk = 0;
    db.sektrk = 1;
    db.seksec = 0;
    db.dmaadr = dma;

    memset(dma, 0xCC, 128);
    assert(deblock_write(&db, WRTYP_UNALLOC) == 0);
    assert(read_count == 0);  /* no pre-read for unallocated */

    /* Continue sequential unallocated writes */
    db.seksec = 1;
    memset(dma, 0xDD, 128);
    assert(deblock_write(&db, WRTYP_DEFERRED) == 0);
    assert(read_count == 0);  /* still no pre-read */
}

static void test_sector_change_flushes(void) {
    deblock_t db;
    uint8_t dma[128];
    reset_mock();
    deblock_init(&db, mock_read, mock_write);

    db.secmsk = 3;
    db.secshf = 2;
    db.sekdsk = 0;
    db.sektrk = 0;
    db.seksec = 0;
    db.dmaadr = dma;

    memset(dma, 0xEE, 128);
    assert(deblock_write(&db, WRTYP_DEFERRED) == 0);
    assert(write_count == 0);

    /* Switch to a different physical sector — should flush */
    db.seksec = 4;  /* different host sector (4>>2 = 1 vs 0>>2 = 0) */
    assert(deblock_read(&db) == 0);
    assert(write_count == 1);  /* previous dirty buffer flushed */
}

static void test_256_byte_sectors(void) {
    deblock_t db;
    uint8_t dma[128];
    reset_mock();
    deblock_init(&db, mock_read, mock_write);

    /* 256-byte physical sectors: mask=1, shift=1 */
    db.secmsk = 1;
    db.secshf = 1;

    /* Fill physical sector with pattern */
    for (int i = 0; i < 256; i++)
        mock_disk[0][0][i] = (uint8_t)(i ^ 0x55);

    db.sekdsk = 0;
    db.sektrk = 0;
    db.seksec = 0;
    db.dmaadr = dma;

    assert(deblock_read(&db) == 0);
    for (int i = 0; i < 128; i++)
        assert(dma[i] == (uint8_t)(i ^ 0x55));

    /* Second record */
    db.seksec = 1;
    assert(deblock_read(&db) == 0);
    for (int i = 0; i < 128; i++)
        assert(dma[i] == (uint8_t)((i + 128) ^ 0x55));
}

int main(void) {
    test_read_basic();
    test_write_deferred();
    test_write_directory_immediate();
    test_write_unalloc_skips_preread();
    test_sector_change_flushes();
    test_256_byte_sectors();
    printf("All deblock tests passed.\n");
    return 0;
}
