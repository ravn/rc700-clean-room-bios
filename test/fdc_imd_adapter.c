#include "fdc_imd_adapter.h"
#include <string.h>

/* Command parameter counts: [cmd*2] = input bytes, [cmd*2+1] = result bytes */
static const byte cmd_params[] = {
    1,1, 1,1, 9,7, 3,0, 2,1, 9,7, 9,7, 2,0,  /* 0-7 */
    1,2, 9,7, 2,7, 1,1, 9,7, 6,7, 1,1, 3,0,  /* 8-15 */
};

#define CMD_SPECIFY       3
#define CMD_WRITE_DATA    5
#define CMD_READ_DATA     6
#define CMD_RECALIBRATE   7
#define CMD_SENSE_INT     8
#define CMD_READ_ID      10
#define CMD_SEEK         15

#define MSR_RQM   0x80
#define MSR_DIO   0x40
#define MSR_CB    0x10

#define ST0_SE    0x20
#define ST0_EC    0x10
#define ST0_NR    0x08
#define ST0_AT    0x40
#define ST0_IC    0x80

#define ST1_ND    0x04

static void update_result(fdc_imd_t *fi, int drive, int head,
                          int cyl, int sector, int n) {
    fi->result[0] = fi->st0 | (byte)((head << 2) | (drive & 3));
    fi->result[1] = fi->st1;
    fi->result[2] = fi->st2;
    fi->result[3] = (byte)cyl;
    fi->result[4] = (byte)head;
    fi->result[5] = (byte)sector;
    fi->result[6] = (byte)n;
}

static void execute_command(fdc_imd_t *fi) {
    int cmd = fi->command[0] & 0x1F;
    int head = (fi->command[1] >> 2) & 1;
    int drive = fi->command[1] & 0x03;
    byte prev_st0 = fi->st0;

    fi->st0 &= 0x07;
    fi->st1 = 0;
    fi->st2 = 0;
    fi->interrupt_flag = 0;

    switch (cmd) {
    case CMD_SPECIFY:
        break;

    case CMD_READ_DATA: {
        int C = fi->command[2];
        int H = fi->command[3];
        int R = fi->command[4];
        int N = fi->command[5];
        imd_disk_t *disk = fi->disk[drive];

        if (!disk) {
            fi->st0 |= ST0_AT | ST0_NR;
            update_result(fi, drive, H, C, R, N);
            break;
        }

        byte *src = imd_get_sector(disk, C, H, R);
        int sec_size = imd_get_sector_size(disk, C, H);

        if (src && fi->dma_addr && fi->dma_count >= sec_size) {
            memcpy(fi->dma_addr, src, (size_t)sec_size);
            fi->dma_addr += sec_size;
            fi->dma_count -= sec_size;
        } else if (!src) {
            fi->st0 |= ST0_AT;
            fi->st1 |= ST1_ND;
        }
        update_result(fi, drive, H, C, R, N);
        fi->interrupt_flag = 1;
        break;
    }

    case CMD_WRITE_DATA: {
        int C = fi->command[2];
        int H = fi->command[3];
        int R = fi->command[4];
        int N = fi->command[5];
        imd_disk_t *disk = fi->disk[drive];

        if (!disk) {
            fi->st0 |= ST0_AT | ST0_NR;
            update_result(fi, drive, H, C, R, N);
            break;
        }

        byte *dst = imd_get_sector(disk, C, H, R);
        int sec_size = imd_get_sector_size(disk, C, H);

        if (dst && fi->dma_addr && fi->dma_count >= sec_size) {
            memcpy(dst, fi->dma_addr, (size_t)sec_size);
            fi->dma_addr += sec_size;
            fi->dma_count -= sec_size;
        } else if (!dst) {
            fi->st0 |= ST0_AT;
            fi->st1 |= ST1_ND;
        }
        update_result(fi, drive, H, C, R, N);
        fi->interrupt_flag = 1;
        break;
    }

    case CMD_RECALIBRATE:
        fi->cylinder[drive] = 0;
        fi->st0 = ST0_SE | (byte)((head << 2) | (drive & 3));
        if (!fi->disk[drive]) fi->st0 |= ST0_EC;
        fi->interrupt_flag = 1;
        break;

    case CMD_SENSE_INT:
        fi->result[0] = prev_st0;
        fi->result[1] = fi->cylinder[prev_st0 & 0x03];
        break;

    case CMD_READ_ID: {
        imd_disk_t *disk = fi->disk[drive];
        int N = 0;
        if (disk) {
            int ss = imd_get_sector_size(disk, fi->cylinder[drive], head);
            while (ss > 128) { ss >>= 1; N++; }
        }
        update_result(fi, drive, head, fi->cylinder[drive], 1, N);
        fi->interrupt_flag = 1;
        break;
    }

    case CMD_SEEK:
        fi->cylinder[drive] = fi->command[2];
        fi->st0 = ST0_SE | (byte)((head << 2) | (drive & 3));
        fi->interrupt_flag = 1;
        break;

    default:
        fi->result[0] = ST0_IC;
        break;
    }

    if (fi->res_index < fi->res_size)
        fi->msr |= MSR_DIO;
    else
        fi->msr &= (byte)~(MSR_DIO | MSR_CB);
}

void fdc_imd_init(fdc_imd_t *fi) {
    memset(fi, 0, sizeof(*fi));
    fi->msr = MSR_RQM;
    fi->st0 = ST0_SE;
}

void fdc_imd_mount(fdc_imd_t *fi, int drive, imd_disk_t *imd) {
    fi->disk[drive] = imd;
}

void fdc_imd_set_dma(fdc_imd_t *fi, byte *addr, int count) {
    fi->dma_addr = addr;
    fi->dma_count = count;
}

byte fdc_imd_read_msr(fdc_imd_t *fi) {
    return fi->msr;
}

byte fdc_imd_read_data(fdc_imd_t *fi) {
    if (fi->res_index < fi->res_size) {
        byte val = fi->result[fi->res_index++];
        if (fi->res_index >= fi->res_size)
            fi->msr &= (byte)~(MSR_DIO | MSR_CB);
        return val;
    }
    return 0;
}

void fdc_imd_write_data(fdc_imd_t *fi, byte data) {
    if (fi->msr & MSR_CB) {
        if (fi->cmd_index < FDC_IMD_MAX_CMD)
            fi->command[fi->cmd_index++] = data;
    } else {
        fi->command[0] = data;
        int cmd = data & 0x1F;
        fi->cmd_index = 1;
        fi->res_index = 0;
        if (cmd < 16) {
            fi->cmd_size = cmd_params[cmd * 2];
            fi->res_size = cmd_params[cmd * 2 + 1];
        } else {
            fi->cmd_size = 1;
            fi->res_size = 1;
        }
        if (fi->cmd_index < fi->cmd_size) {
            fi->msr |= MSR_CB;
            fi->msr &= (byte)~MSR_DIO;
        }
    }

    if (fi->cmd_index >= fi->cmd_size)
        execute_command(fi);
}

int fdc_imd_check_interrupt(fdc_imd_t *fi) {
    int flag = fi->interrupt_flag;
    fi->interrupt_flag = 0;
    return flag;
}
