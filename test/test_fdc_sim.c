#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fdc_sim.h"

/* Small test disk: 2 tracks, 1 head, 4 sectors of 512 bytes = 4096 bytes */
#define TEST_TRACKS   2
#define TEST_HEADS    1
#define TEST_SECTORS  4
#define TEST_SECSIZE  512
#define TEST_DISKSIZE (TEST_TRACKS * TEST_HEADS * TEST_SECTORS * TEST_SECSIZE)

static byte disk_image[TEST_DISKSIZE];
static byte dma_buf[512];

/* Helper: send a command byte sequence to the FDC */
static void send_cmd(fdc_sim_t *fdc, const byte *cmd, int len) {
    for (int i = 0; i < len; i++)
        fdc_sim_write_data(fdc, cmd[i]);
}

/* Helper: read result bytes from the FDC */
static int read_results(fdc_sim_t *fdc, byte *buf, int max) {
    int n = 0;
    while (n < max && (fdc_sim_read_msr(fdc) & FDC_MSR_DIO)) {
        buf[n++] = fdc_sim_read_data(fdc);
    }
    return n;
}

static void test_init(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);
    assert((fdc_sim_read_msr(&fdc) & FDC_MSR_RQM) != 0);
    assert((fdc_sim_read_msr(&fdc) & FDC_MSR_CB) == 0);
}

static void test_specify(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    /* SPECIFY: 3 bytes, 0 result */
    byte cmd[] = { 0x03, 0xDF, 0x28 };
    send_cmd(&fdc, cmd, 3);

    /* No result bytes expected, MSR should be idle */
    assert((fdc_sim_read_msr(&fdc) & FDC_MSR_DIO) == 0);
    assert((fdc_sim_read_msr(&fdc) & FDC_MSR_CB) == 0);
}

static void test_recalibrate_and_sense_int(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    memset(disk_image, 0, TEST_DISKSIZE);
    fdc_sim_mount(&fdc, 0, disk_image, TEST_TRACKS, TEST_HEADS,
                  TEST_SECTORS, TEST_SECSIZE);

    /* Seek to track 5 first */
    byte seek_cmd[] = { 0x0F, 0x00, 0x05 };
    send_cmd(&fdc, seek_cmd, 3);
    assert(fdc_sim_check_interrupt(&fdc));

    /* Sense interrupt after seek */
    byte sense[] = { 0x08 };
    send_cmd(&fdc, sense, 1);
    byte res[2];
    int n = read_results(&fdc, res, 2);
    assert(n == 2);
    assert(res[1] == 5);  /* PCN = 5 */

    /* Recalibrate */
    byte recal[] = { 0x07, 0x00 };
    send_cmd(&fdc, recal, 2);
    assert(fdc_sim_check_interrupt(&fdc));

    /* Sense interrupt after recalibrate */
    send_cmd(&fdc, sense, 1);
    n = read_results(&fdc, res, 2);
    assert(n == 2);
    assert((res[0] & FDC_ST0_SE) != 0);  /* seek end */
    assert(res[1] == 0);  /* PCN = 0 */
}

static void test_read_data(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    /* Fill sector at C=0, H=0, S=1 with pattern */
    memset(disk_image, 0, TEST_DISKSIZE);
    for (int i = 0; i < TEST_SECSIZE; i++)
        disk_image[i] = (byte)(i & 0xFF);  /* sector 1 at offset 0 */

    fdc_sim_mount(&fdc, 0, disk_image, TEST_TRACKS, TEST_HEADS,
                  TEST_SECTORS, TEST_SECSIZE);

    /* Set up DMA buffer */
    memset(dma_buf, 0, sizeof(dma_buf));
    fdc_sim_set_dma(&fdc, dma_buf, TEST_SECSIZE);

    /* READ DATA: cmd=0x46 (MFM+read), drive 0, C=0, H=0, R=1, N=2, EOT=4, GPL=27, DTL=0xFF */
    byte cmd[] = { 0x46, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x1B, 0xFF };
    send_cmd(&fdc, cmd, 9);

    /* Read 7 result bytes */
    byte res[7];
    int n = read_results(&fdc, res, 7);
    assert(n == 7);

    /* ST0 should be normal termination */
    assert((res[0] & 0xC0) == FDC_ST0_NT);

    /* Verify DMA buffer got the sector data */
    for (int i = 0; i < TEST_SECSIZE; i++)
        assert(dma_buf[i] == (byte)(i & 0xFF));

    assert(fdc_sim_check_interrupt(&fdc));
}

static void test_write_data(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    memset(disk_image, 0, TEST_DISKSIZE);
    fdc_sim_mount(&fdc, 0, disk_image, TEST_TRACKS, TEST_HEADS,
                  TEST_SECTORS, TEST_SECSIZE);

    /* Fill DMA buffer with pattern */
    for (int i = 0; i < TEST_SECSIZE; i++)
        dma_buf[i] = (byte)(0xAA ^ i);
    fdc_sim_set_dma(&fdc, dma_buf, TEST_SECSIZE);

    /* WRITE DATA: cmd=0x45 (MFM+write), drive 0, C=0, H=0, R=2, N=2 */
    byte cmd[] = { 0x45, 0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x1B, 0xFF };
    send_cmd(&fdc, cmd, 9);

    byte res[7];
    int n = read_results(&fdc, res, 7);
    assert(n == 7);
    assert((res[0] & 0xC0) == FDC_ST0_NT);

    /* Verify the disk image got the data at sector 2 (offset = 1 * 512) */
    for (int i = 0; i < TEST_SECSIZE; i++)
        assert(disk_image[TEST_SECSIZE + i] == (byte)(0xAA ^ i));
}

static void test_read_write_roundtrip(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    memset(disk_image, 0, TEST_DISKSIZE);
    fdc_sim_mount(&fdc, 0, disk_image, TEST_TRACKS, TEST_HEADS,
                  TEST_SECTORS, TEST_SECSIZE);

    /* Write a pattern to C=1, H=0, S=3 */
    byte write_data[512];
    for (int i = 0; i < 512; i++)
        write_data[i] = (byte)(i * 3 + 7);

    fdc_sim_set_dma(&fdc, write_data, 512);
    byte wcmd[] = { 0x45, 0x00, 0x01, 0x00, 0x03, 0x02, 0x04, 0x1B, 0xFF };
    send_cmd(&fdc, wcmd, 9);
    byte res[7];
    read_results(&fdc, res, 7);
    assert((res[0] & 0xC0) == FDC_ST0_NT);

    /* Read it back */
    memset(dma_buf, 0, sizeof(dma_buf));
    fdc_sim_set_dma(&fdc, dma_buf, 512);
    byte rcmd[] = { 0x46, 0x00, 0x01, 0x00, 0x03, 0x02, 0x04, 0x1B, 0xFF };
    send_cmd(&fdc, rcmd, 9);
    read_results(&fdc, res, 7);
    assert((res[0] & 0xC0) == FDC_ST0_NT);

    /* Verify roundtrip */
    for (int i = 0; i < 512; i++)
        assert(dma_buf[i] == (byte)(i * 3 + 7));
}

static void test_seek(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    fdc_sim_mount(&fdc, 0, disk_image, TEST_TRACKS, TEST_HEADS,
                  TEST_SECTORS, TEST_SECSIZE);

    /* Seek to track 1 */
    byte cmd[] = { 0x0F, 0x00, 0x01 };
    send_cmd(&fdc, cmd, 3);
    assert(fdc_sim_check_interrupt(&fdc));

    /* Sense interrupt */
    byte sense[] = { 0x08 };
    send_cmd(&fdc, sense, 1);
    byte res[2];
    read_results(&fdc, res, 2);
    assert((res[0] & FDC_ST0_SE) != 0);
    assert(res[1] == 1);  /* PCN = 1 */
}

static void test_no_disk_error(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);
    /* Drive 0 not mounted */

    memset(dma_buf, 0, sizeof(dma_buf));
    fdc_sim_set_dma(&fdc, dma_buf, 512);

    /* Try to read from unmounted drive */
    byte cmd[] = { 0x46, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x1B, 0xFF };
    send_cmd(&fdc, cmd, 9);

    byte res[7];
    read_results(&fdc, res, 7);

    /* ST0 should show abnormal termination + not ready */
    assert((res[0] & FDC_ST0_AT) != 0);
    assert((res[0] & FDC_ST0_NR) != 0);
}

static void test_read_id(void) {
    fdc_sim_t fdc;
    fdc_sim_init(&fdc);

    fdc_sim_mount(&fdc, 0, disk_image, TEST_TRACKS, TEST_HEADS,
                  TEST_SECTORS, TEST_SECSIZE);

    /* READ ID: cmd=0x4A (MFM + read ID) */
    byte cmd[] = { 0x4A, 0x00 };
    send_cmd(&fdc, cmd, 2);

    byte res[7];
    int n = read_results(&fdc, res, 7);
    assert(n == 7);

    /* N should be 2 for 512-byte sectors (128 << 2 = 512) */
    assert(res[6] == 2);
}

int main(void) {
    test_init();
    test_specify();
    test_recalibrate_and_sense_int();
    test_read_data();
    test_write_data();
    test_read_write_roundtrip();
    test_seek();
    test_no_disk_error();
    test_read_id();
    printf("All fdc_sim tests passed.\n");
    return 0;
}
