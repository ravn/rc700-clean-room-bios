#include "dpb.h"
#include "sector.h"

const dpb_t dpb_table[FMT_COUNT] = {
    /* FMT_8SS_FM: 8" SS FM 128 B/S */
    { .spt=26, .bsh=3, .blm=7, .exm=0, .dsm=242, .drm=63,
      .al0=0xC0, .al1=0x00, .cks=16, .off=2 },
    /* FMT_8DD_MFM: 8" DD MFM 512 B/S */
    { .spt=120, .bsh=4, .blm=15, .exm=0, .dsm=449, .drm=127,
      .al0=0xC0, .al1=0x00, .cks=32, .off=2 },
    /* FMT_5DD_MFM: 5.25" DD MFM 512 B/S */
    { .spt=26, .bsh=3, .blm=7, .exm=0, .dsm=242, .drm=63,
      .al0=0xC0, .al1=0x00, .cks=16, .off=0 },
    /* FMT_8DD_256: 8" DD MFM 256 B/S */
    { .spt=104, .bsh=4, .blm=15, .exm=0, .dsm=471, .drm=127,
      .al0=0xC0, .al1=0x00, .cks=32, .off=0 },
};

const fspa_t fspa_table[FMT_COUNT] = {
    /* FMT_8SS_FM */
    { .dpb=&dpb_table[0], .records_per_block=8, .cpm_spt=26,
      .sector_mask=0, .sector_shift=1, .tran_table=tran0,
      .tran_size=TRAN0_SIZE, .data_length=128 },
    /* FMT_8DD_MFM */
    { .dpb=&dpb_table[1], .records_per_block=16, .cpm_spt=120,
      .sector_mask=3, .sector_shift=3, .tran_table=tran8,
      .tran_size=TRAN8_SIZE, .data_length=255 },
    /* FMT_5DD_MFM */
    { .dpb=&dpb_table[2], .records_per_block=8, .cpm_spt=26,
      .sector_mask=0, .sector_shift=1, .tran_table=tran24,
      .tran_size=TRAN24_SIZE, .data_length=128 },
    /* FMT_8DD_256 */
    { .dpb=&dpb_table[3], .records_per_block=8, .cpm_spt=104,
      .sector_mask=1, .sector_shift=2, .tran_table=tran24,
      .tran_size=TRAN24_SIZE, .data_length=255 },
};

const fdf_t fdf_table[FMT_COUNT] = {
    /* FMT_8SS_FM */
    { .phys_spt=26, .dma_count=127, .mf=0x00, .n=0, .eot=26, .gap=7, .tracks=77 },
    /* FMT_8DD_MFM */
    { .phys_spt=30, .dma_count=511, .mf=0x40, .n=2, .eot=15, .gap=27, .tracks=77 },
    /* FMT_5DD_MFM */
    { .phys_spt=26, .dma_count=127, .mf=0x00, .n=0, .eot=26, .gap=7, .tracks=77 },
    /* FMT_8DD_256 */
    { .phys_spt=52, .dma_count=255, .mf=0x40, .n=1, .eot=26, .gap=14, .tracks=77 },
};

byte dpb_format_index(byte format_code) {
    return (format_code >> 3) & 0x03;
}
