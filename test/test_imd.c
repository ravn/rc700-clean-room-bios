#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "imd.h"

/* Path to the test disk image — set by CMake */
#ifndef TEST_IMAGE
#define TEST_IMAGE "disks/SW1711-I8.imd"
#endif

static imd_disk_t disk;

static void test_load(void) {
    int rc = imd_load(&disk, TEST_IMAGE);
    assert(rc == 0);
    assert(disk.max_cylinder == 76);
    assert(disk.num_heads == 2);
    printf("  Loaded: %d track records, %d cylinders, %d heads\n",
           disk.num_tracks, disk.max_cylinder + 1, disk.num_heads);
}

static void test_track0_head0_geometry(void) {
    /* Track 0, head 0: FM, 26 sectors × 128 bytes */
    assert(imd_get_num_sectors(&disk, 0, 0) == 26);
    assert(imd_get_sector_size(&disk, 0, 0) == 128);
    assert(disk.tracks[0][0].mode == 0);  /* FM 500kbps */
}

static void test_track0_head1_geometry(void) {
    /* Track 0, head 1: MFM, 26 sectors × 256 bytes */
    assert(imd_get_num_sectors(&disk, 0, 1) == 26);
    assert(imd_get_sector_size(&disk, 0, 1) == 256);
    assert(disk.tracks[0][1].mode == 3);  /* MFM 500kbps */
}

static void test_data_tracks_geometry(void) {
    /* Tracks 1-76: MFM, 15 sectors × 512 bytes per side */
    for (int cyl = 1; cyl <= 76; cyl++) {
        for (int head = 0; head < 2; head++) {
            assert(imd_get_num_sectors(&disk, cyl, head) == 15);
            assert(imd_get_sector_size(&disk, cyl, head) == 512);
        }
    }
}

static void test_boot_sector(void) {
    /* Track 0, head 0, sector 1: boot sector */
    byte *sec1 = imd_get_sector(&disk, 0, 0, 1);
    assert(sec1 != NULL);

    /* Per spec Section 16.1: bytes 8-13 = machine identifier */
    /* Actual image has ' RC702' (space-prefixed) */
    assert(memcmp(&sec1[8], " RC702", 6) == 0);
    printf("  Boot sector machine ID: '%.6s'\n", &sec1[8]);
}

static void test_confi_block(void) {
    /* Track 0, head 0, sector 2: CONFI configuration block */
    byte *sec2 = imd_get_sector(&disk, 0, 0, 2);
    assert(sec2 != NULL);

    /* CTC Ch.0 mode should be 0x47 (timer, prescaler /16) */
    printf("  CONFI CTC Ch.0: mode=0x%02X count=0x%02X\n", sec2[0], sec2[1]);

    /* Drive table at offset 0x2F: drive A format code */
    printf("  CONFI drive A format: 0x%02X\n", sec2[0x2F]);
    printf("  CONFI drive B format: 0x%02X\n", sec2[0x30]);
    printf("  CONFI boot device: 0x%02X\n", sec2[0x47]);
}

static void test_sector_access(void) {
    /* All 26 sectors on track 0 head 0 should be readable */
    for (int s = 1; s <= 26; s++) {
        byte *data = imd_get_sector(&disk, 0, 0, s);
        assert(data != NULL);
    }

    /* All 15 sectors on track 1 head 0 should be readable */
    for (int s = 1; s <= 15; s++) {
        byte *data = imd_get_sector(&disk, 1, 0, s);
        assert(data != NULL);
    }

    /* Nonexistent sector should return NULL */
    assert(imd_get_sector(&disk, 0, 0, 27) == NULL);
    assert(imd_get_sector(&disk, 0, 0, 0) == NULL);
}

int main(void) {
    test_load();
    test_track0_head0_geometry();
    test_track0_head1_geometry();
    test_data_tracks_geometry();
    test_boot_sector();
    test_confi_block();
    test_sector_access();
    printf("All imd tests passed.\n");
    return 0;
}
