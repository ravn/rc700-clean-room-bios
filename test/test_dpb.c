#include <assert.h>
#include <stdio.h>
#include "dpb.h"
#include "sector.h"

static void test_format_index(void) {
    assert(dpb_format_index(0)  == 0);  /* 8" SS FM */
    assert(dpb_format_index(8)  == 1);  /* 8" DD MFM 512 */
    assert(dpb_format_index(16) == 2);  /* 5.25" DD MFM 512 */
    assert(dpb_format_index(24) == 3);  /* 8" DD MFM 256 */
}

static void test_dpb_values(void) {
    /* DPB 0: 8" SS FM 128 B/S */
    const dpb_t *d = &dpb_table[FMT_8SS_FM];
    assert(d->spt == 26);
    assert(d->bsh == 3);
    assert(d->blm == 7);
    assert(d->exm == 0);
    assert(d->dsm == 242);
    assert(d->drm == 63);
    assert(d->al0 == 0xC0);
    assert(d->al1 == 0x00);
    assert(d->cks == 16);
    assert(d->off == 2);

    /* DPB 1: 8" DD MFM 512 B/S */
    d = &dpb_table[FMT_8DD_MFM];
    assert(d->spt == 120);
    assert(d->bsh == 4);
    assert(d->blm == 15);
    assert(d->exm == 0);
    assert(d->dsm == 449);
    assert(d->drm == 127);
    assert(d->al0 == 0xC0);
    assert(d->al1 == 0x00);
    assert(d->cks == 32);
    assert(d->off == 2);

    /* DPB 2: 5.25" DD MFM 512 B/S */
    d = &dpb_table[FMT_5DD_MFM];
    assert(d->spt == 26);
    assert(d->bsh == 3);
    assert(d->blm == 7);
    assert(d->exm == 0);
    assert(d->dsm == 242);
    assert(d->drm == 63);
    assert(d->cks == 16);
    assert(d->off == 0);

    /* DPB 3: 8" DD MFM 256 B/S */
    d = &dpb_table[FMT_8DD_256];
    assert(d->spt == 104);
    assert(d->bsh == 4);
    assert(d->blm == 15);
    assert(d->exm == 0);
    assert(d->dsm == 471);
    assert(d->drm == 127);
    assert(d->cks == 32);
    assert(d->off == 0);
}

static void test_dpb_bsh_blm_consistency(void) {
    /* BLM must equal (1 << BSH) - 1 for all formats */
    for (int i = 0; i < FMT_COUNT; i++) {
        const dpb_t *d = &dpb_table[i];
        assert(d->blm == (1 << d->bsh) - 1);
    }
}

static void test_fspa_values(void) {
    /* FSPA 0: 8" SS FM */
    const fspa_t *f = &fspa_table[FMT_8SS_FM];
    assert(f->dpb == &dpb_table[FMT_8SS_FM]);
    assert(f->records_per_block == 8);
    assert(f->cpm_spt == 26);
    assert(f->sector_mask == 0);
    assert(f->sector_shift == 1);
    assert(f->tran_table == tran0);
    assert(f->data_length == 128);

    /* FSPA 1: 8" DD MFM 512 B/S */
    f = &fspa_table[FMT_8DD_MFM];
    assert(f->records_per_block == 16);
    assert(f->cpm_spt == 120);
    assert(f->sector_mask == 3);
    assert(f->sector_shift == 3);
    assert(f->tran_table == tran8);
    assert(f->data_length == 255);

    /* FSPA 2: 5.25" uses tran24 (identity), not tran16 */
    f = &fspa_table[FMT_5DD_MFM];
    assert(f->tran_table == tran24);
    assert(f->sector_mask == 0);
    assert(f->sector_shift == 1);

    /* FSPA 3: 8" DD MFM 256 B/S uses tran24 */
    f = &fspa_table[FMT_8DD_256];
    assert(f->tran_table == tran24);
    assert(f->sector_mask == 1);
    assert(f->sector_shift == 2);
    assert(f->cpm_spt == 104);
}

static void test_fdf_values(void) {
    /* FDF 0: 8" SS FM */
    const fdf_t *f = &fdf_table[FMT_8SS_FM];
    assert(f->phys_spt == 26);
    assert(f->dma_count == 127);
    assert(f->mf == 0x00);
    assert(f->n == 0);
    assert(f->eot == 26);
    assert(f->gap == 7);
    assert(f->tracks == 77);

    /* FDF 1: 8" DD MFM 512 B/S */
    f = &fdf_table[FMT_8DD_MFM];
    assert(f->phys_spt == 30);
    assert(f->dma_count == 511);
    assert(f->mf == 0x40);
    assert(f->n == 2);
    assert(f->eot == 15);
    assert(f->gap == 27);
    assert(f->tracks == 77);

    /* FDF 3: 8" DD MFM 256 B/S */
    f = &fdf_table[FMT_8DD_256];
    assert(f->phys_spt == 52);
    assert(f->dma_count == 255);
    assert(f->mf == 0x40);
    assert(f->n == 1);
    assert(f->eot == 26);
    assert(f->gap == 14);
    assert(f->tracks == 77);
}

static void test_fspa_dpb_spt_match(void) {
    /* FSPA cpm_spt must match the DPB spt for each format */
    for (int i = 0; i < FMT_COUNT; i++)
        assert(fspa_table[i].cpm_spt == fspa_table[i].dpb->spt);
}

int main(void) {
    test_format_index();
    test_dpb_values();
    test_dpb_bsh_blm_consistency();
    test_fspa_values();
    test_fdf_values();
    test_fspa_dpb_spt_match();
    printf("All dpb tests passed.\n");
    return 0;
}
