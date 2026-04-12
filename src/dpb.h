#ifndef DPB_H
#define DPB_H

#include <stdint.h>

/* CP/M 2.2 Disk Parameter Block */
typedef struct {
    uint16_t spt;   /* logical sectors per track (128-byte records) */
    uint8_t  bsh;   /* block shift factor */
    uint8_t  blm;   /* block mask */
    uint8_t  exm;   /* extent mask */
    uint16_t dsm;   /* total blocks - 1 */
    uint16_t drm;   /* directory entries - 1 */
    uint8_t  al0;   /* directory allocation bitmap byte 0 */
    uint8_t  al1;   /* directory allocation bitmap byte 1 */
    uint16_t cks;   /* check vector size */
    uint16_t off;   /* reserved tracks */
} dpb_t;

/* Floppy System Parameters — bridges DPB with physical format */
typedef struct {
    const dpb_t    *dpb;
    uint8_t         records_per_block;
    uint16_t        cpm_spt;
    uint8_t         sector_mask;
    uint8_t         sector_shift;
    const uint8_t  *tran_table;
    uint8_t         tran_size;
    uint8_t         data_length;   /* physical sector size - 1 (127, 255, 511 low byte) */
} fspa_t;

/* Floppy Disk Format Descriptor — physical format parameters for FDC */
typedef struct {
    uint8_t  phys_spt;     /* physical sectors per track (both sides) */
    uint16_t dma_count;    /* DMA byte count per sector */
    uint8_t  mf;           /* MF flag: 0x00=FM, 0x40=MFM */
    uint8_t  n;            /* sector size code (0=128, 1=256, 2=512) */
    uint8_t  eot;          /* end of track (sectors per side) */
    uint8_t  gap;          /* gap length for read/write */
    uint8_t  tracks;       /* tracks per side */
} fdf_t;

/* Format indices */
#define FMT_8SS_FM     0   /* 8" SS FM 128 B/S */
#define FMT_8DD_MFM    1   /* 8" DD MFM 512 B/S */
#define FMT_5DD_MFM    2   /* 5.25" DD MFM 512 B/S */
#define FMT_8DD_256    3   /* 8" DD MFM 256 B/S */
#define FMT_COUNT      4

/* Convert format code (0, 8, 16, 24) to index (0..3) */
uint8_t dpb_format_index(uint8_t format_code);

/* Global tables */
extern const dpb_t  dpb_table[FMT_COUNT];
extern const fspa_t fspa_table[FMT_COUNT];
extern const fdf_t  fdf_table[FMT_COUNT];

#endif
