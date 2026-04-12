#include "fdc_sim.h"
#include <string.h>

/* Command parameter counts: [cmd*2] = input bytes, [cmd*2+1] = result bytes */
static const byte cmd_params[] = {
    1,1, 1,1, 9,7, 3,0, 2,1, 9,7, 9,7, 2,0,  /* 0-7 */
    1,2, 9,7, 2,7, 1,1, 9,7, 6,7, 1,1, 3,0,  /* 8-15 */
};

/* FDC commands */
#define CMD_SPECIFY           3
#define CMD_SENSE_DRIVE       4
#define CMD_WRITE_DATA        5
#define CMD_READ_DATA         6
#define CMD_RECALIBRATE       7
#define CMD_SENSE_INT         8
#define CMD_READ_ID          10
#define CMD_FORMAT_TRACK     13
#define CMD_SEEK             15

/* Get sector data pointer in the flat image */
static byte *sector_ptr(fdc_disk_t *disk, int cyl, int head, int sector_1based) {
    if (!disk->present || !disk->data) return NULL;
    if (cyl >= disk->tracks || head >= disk->heads) return NULL;
    if (sector_1based < 1 || sector_1based > disk->sectors) return NULL;
    int offset = ((cyl * disk->heads + head) * disk->sectors + (sector_1based - 1))
                 * disk->sector_size;
    return &disk->data[offset];
}

static void update_result(fdc_sim_t *fdc, int drive, int head,
                          int cyl, int sector, int n) {
    fdc->result[0] = fdc->st0 | (byte)((head << 2) | (drive & 3));
    fdc->result[1] = fdc->st1;
    fdc->result[2] = fdc->st2;
    fdc->result[3] = (byte)cyl;
    fdc->result[4] = (byte)head;
    fdc->result[5] = (byte)sector;
    fdc->result[6] = (byte)n;
}

static void execute_command(fdc_sim_t *fdc) {
    int cmd = fdc->command[0] & 0x1F;
    int head = (fdc->command[1] >> 2) & 1;
    int drive = fdc->command[1] & 0x03;
    byte prev_st0 = fdc->st0;

    fdc->st0 &= 0x07;  /* preserve drive/head bits, clear IC */
    fdc->st1 = 0;
    fdc->st2 = 0;
    fdc->interrupt_flag = 0;

    switch (cmd) {
    case CMD_SPECIFY:
        /* No result bytes, just accept parameters */
        break;

    case CMD_SENSE_DRIVE: {
        byte st3 = (byte)((head << 2) | (drive & 3));
        if (fdc->disk[drive].present) {
            st3 |= 0x20;  /* ready */
            if (fdc->cylinder[drive] == 0) st3 |= 0x10;  /* track 0 */
        }
        fdc->result[0] = st3;
        break;
    }

    case CMD_READ_DATA: {
        int C = fdc->command[2];
        int H = fdc->command[3];
        int R = fdc->command[4];
        int N = fdc->command[5];
        fdc_disk_t *disk = &fdc->disk[drive];

        if (!disk->present) {
            fdc->st0 |= FDC_ST0_AT | FDC_ST0_NR;
            update_result(fdc, drive, H, C, R, N);
            break;
        }

        int sec_size = N > 0 ? (128 << N) : fdc->command[8];
        byte *src = sector_ptr(disk, C, H, R);
        if (src && fdc->dma_addr && fdc->dma_count >= sec_size) {
            memcpy(fdc->dma_addr, src, (size_t)sec_size);
            fdc->dma_addr += sec_size;
            fdc->dma_count -= sec_size;
        } else if (!src) {
            fdc->st0 |= FDC_ST0_AT;
            fdc->st1 |= FDC_ST1_ND;
        }
        update_result(fdc, drive, H, C, R, N);
        fdc->interrupt_flag = 1;
        break;
    }

    case CMD_WRITE_DATA: {
        int C = fdc->command[2];
        int H = fdc->command[3];
        int R = fdc->command[4];
        int N = fdc->command[5];
        fdc_disk_t *disk = &fdc->disk[drive];

        if (!disk->present) {
            fdc->st0 |= FDC_ST0_AT | FDC_ST0_NR;
            update_result(fdc, drive, H, C, R, N);
            break;
        }

        int sec_size = N > 0 ? (128 << N) : fdc->command[8];
        byte *dst = sector_ptr(disk, C, H, R);
        if (dst && fdc->dma_addr && fdc->dma_count >= sec_size) {
            memcpy(dst, fdc->dma_addr, (size_t)sec_size);
            fdc->dma_addr += sec_size;
            fdc->dma_count -= sec_size;
        } else if (!dst) {
            fdc->st0 |= FDC_ST0_AT;
            fdc->st1 |= FDC_ST1_ND;
        }
        update_result(fdc, drive, H, C, R, N);
        fdc->interrupt_flag = 1;
        break;
    }

    case CMD_RECALIBRATE:
        fdc->cylinder[drive] = 0;
        fdc->st0 = FDC_ST0_SE | (byte)((head << 2) | (drive & 3));
        if (!fdc->disk[drive].present)
            fdc->st0 |= FDC_ST0_EC;
        fdc->interrupt_flag = 1;
        break;

    case CMD_SENSE_INT:
        fdc->result[0] = prev_st0;
        fdc->result[1] = fdc->cylinder[prev_st0 & 0x03];
        break;

    case CMD_READ_ID: {
        fdc_disk_t *disk = &fdc->disk[drive];
        int N = 0;
        if (disk->present) {
            int ss = disk->sector_size;
            while (ss > 128) { ss >>= 1; N++; }
        }
        update_result(fdc, drive, head, fdc->cylinder[drive], 1, N);
        fdc->interrupt_flag = 1;
        break;
    }

    case CMD_SEEK:
        fdc->cylinder[drive] = fdc->command[2];
        fdc->st0 = FDC_ST0_SE | (byte)((head << 2) | (drive & 3));
        fdc->interrupt_flag = 1;
        break;

    default:
        fdc->result[0] = FDC_ST0_IC;
        break;
    }

    /* Update MSR: set DIO if results available */
    if (fdc->res_index < fdc->res_size)
        fdc->msr |= FDC_MSR_DIO;
    else
        fdc->msr &= (byte)~(FDC_MSR_DIO | FDC_MSR_CB);
}

void fdc_sim_init(fdc_sim_t *fdc) {
    memset(fdc, 0, sizeof(*fdc));
    fdc->msr = FDC_MSR_RQM;
    fdc->st0 = FDC_ST0_SE;
}

void fdc_sim_mount(fdc_sim_t *fdc, int drive, byte *image_data,
                   int tracks, int heads, int sectors, int sector_size) {
    fdc->disk[drive].data = image_data;
    fdc->disk[drive].tracks = tracks;
    fdc->disk[drive].heads = heads;
    fdc->disk[drive].sectors = sectors;
    fdc->disk[drive].sector_size = sector_size;
    fdc->disk[drive].present = 1;
}

void fdc_sim_unmount(fdc_sim_t *fdc, int drive) {
    memset(&fdc->disk[drive], 0, sizeof(fdc_disk_t));
}

void fdc_sim_set_dma(fdc_sim_t *fdc, byte *addr, int count) {
    fdc->dma_addr = addr;
    fdc->dma_count = count;
}

byte fdc_sim_read_msr(fdc_sim_t *fdc) {
    return fdc->msr;
}

byte fdc_sim_read_data(fdc_sim_t *fdc) {
    if (fdc->res_index < fdc->res_size) {
        byte val = fdc->result[fdc->res_index++];
        if (fdc->res_index >= fdc->res_size)
            fdc->msr &= (byte)~(FDC_MSR_DIO | FDC_MSR_CB);
        return val;
    }
    return 0;
}

void fdc_sim_write_data(fdc_sim_t *fdc, byte data) {
    if (fdc->msr & FDC_MSR_CB) {
        /* Accumulate command bytes */
        if (fdc->cmd_index < FDC_MAX_CMD)
            fdc->command[fdc->cmd_index++] = data;
    } else {
        /* Start new command */
        fdc->command[0] = data;
        int cmd = data & 0x1F;
        fdc->cmd_index = 1;
        fdc->res_index = 0;
        if (cmd < 16) {
            fdc->cmd_size = cmd_params[cmd * 2];
            fdc->res_size = cmd_params[cmd * 2 + 1];
        } else {
            fdc->cmd_size = 1;
            fdc->res_size = 1;
        }
        if (fdc->cmd_index < fdc->cmd_size) {
            fdc->msr |= FDC_MSR_CB;
            fdc->msr &= (byte)~FDC_MSR_DIO;
        }
    }

    if (fdc->cmd_index >= fdc->cmd_size)
        execute_command(fdc);
}

int fdc_sim_check_interrupt(fdc_sim_t *fdc) {
    int flag = fdc->interrupt_flag;
    fdc->interrupt_flag = 0;
    return flag;
}
