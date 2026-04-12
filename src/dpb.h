#ifndef DPB_H
#define DPB_H

#include "types.h"

/* CP/M 2.2 Disk Parameter Block */
typedef struct {
    word spt;   /* logical sectors per track (128-byte records) */
    byte  bsh;   /* block shift factor */
    byte  blm;   /* block mask */
    byte  exm;   /* extent mask */
    word dsm;   /* total blocks - 1 */
    word drm;   /* directory entries - 1 */
    byte  al0;   /* directory allocation bitmap byte 0 */
    byte  al1;   /* directory allocation bitmap byte 1 */
    word cks;   /* check vector size */
    word off;   /* reserved tracks */
} dpb_t;

/* Floppy System Parameters — bridges DPB with physical format */
typedef struct {
    const dpb_t    *dpb;
    byte         records_per_block;
    word        cpm_spt;
    byte         sector_mask;
    byte         sector_shift;
    const byte  *tran_table;
    byte         tran_size;
    byte         data_length;   /* physical sector size - 1 (127, 255, 511 low byte) */
} fspa_t;

/* Floppy Disk Format Descriptor — physical format parameters for FDC */
typedef struct {
    byte  phys_spt;     /* physical sectors per track (both sides) */
    word dma_count;    /* DMA byte count per sector */
    byte  mf;           /* MF flag: 0x00=FM, 0x40=MFM */
    byte  n;            /* sector size code (0=128, 1=256, 2=512) */
    byte  eot;          /* end of track (sectors per side) */
    byte  gap;          /* gap length for read/write */
    byte  tracks;       /* tracks per side */
} fdf_t;

/* Format indices */
#define FMT_8SS_FM     0   /* 8" SS FM 128 B/S */
#define FMT_8DD_MFM    1   /* 8" DD MFM 512 B/S */
#define FMT_5DD_MFM    2   /* 5.25" DD MFM 512 B/S */
#define FMT_8DD_256    3   /* 8" DD MFM 256 B/S */
#define FMT_COUNT      4

/* Convert format code (0, 8, 16, 24) to index (0..3) */
byte dpb_format_index(byte format_code);

/* Global tables */
extern const dpb_t  dpb_table[FMT_COUNT];
extern const fspa_t fspa_table[FMT_COUNT];
extern const fdf_t  fdf_table[FMT_COUNT];

#endif
