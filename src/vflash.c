/**
 * vflash.c
 * Authors: Yun-Sheng Chang
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config.h"
#include "vflash.h"
#include "vram.h"
#include "vpage.h"
#include "log.h"
#include "checker.h"
#include "stat.h"

#define VST_UNKNOWN_CONTENT ((uint32_t)-1)

/* emulated DRAM and Flash memory */
static flash_t flash;

/* macro functions */
#define get_page(bank, blk, page) \
        (flash.banks[(bank)].blocks[(blk)].pages[(page)])

/* public interfaces */
/* flash memory APIs */
void vst_read_page(uint32_t const bank, uint32_t const blk, uint32_t const page,
               uint32_t const sect, uint32_t const n_sect, uint64_t const dram_addr)
{
    record(LOG_FLASH, "R: flash(%u, %u, %u) -> mem[0x%lx]\n",
            bank, blk, page, dram_addr);
    inc_flash_read(1);

    assert(bank < VST_NUM_BANKS);
    assert(blk < VST_BLOCKS_PER_BANK);
    assert(page < VST_PAGES_PER_BLOCK);

    flash_page_t *pp = &get_page(bank, blk, page);

    vpage_copy(vram_vpage_map(dram_addr), &pp->vpage, sect, n_sect);
}

void vst_write_page(uint32_t const bank, uint32_t const blk, uint32_t const page,
                uint32_t const sect, uint32_t const n_sect, uint64_t const dram_addr)
{
    record(LOG_FLASH, "W: mem[0x%lx] -> flash(%u, %u, %u)\n",
            dram_addr, bank, blk, page);
    inc_flash_write(1);

    assert(bank < VST_NUM_BANKS);
    assert(blk < VST_BLOCKS_PER_BANK);
    assert(page < VST_PAGES_PER_BLOCK);

    chk_non_seq_write(&flash, bank, blk, page);

    chk_overwrite(&flash, bank, blk, page);

    flash_page_t *pp = &get_page(bank, blk, page);
    pp->is_erased = 0;
    vpage_copy(&pp->vpage, vram_vpage_map(dram_addr), sect, n_sect);
}

void vst_copyback_page(uint32_t const bank, uint32_t const blk_src, uint32_t const page_src,
                   uint32_t const blk_dst, uint32_t const page_dst)
{
    record(LOG_FLASH, "CB: flash(%u, %u, %u) -> flash(%u, %u, %u)\n",
            bank, blk_src, page_src,
            bank, blk_dst, page_dst);
    inc_flash_cb(1);

    assert(bank < VST_NUM_BANKS);
    assert(blk_src < VST_BLOCKS_PER_BANK);
    assert(page_src < VST_PAGES_PER_BLOCK);
    assert(blk_dst < VST_BLOCKS_PER_BANK);
    assert(page_dst < VST_PAGES_PER_BLOCK);

    chk_overwrite(&flash, bank, blk_dst, page_dst);

    flash_page_t *pp_dst, *pp_src;
    pp_dst = &get_page(bank, blk_dst, page_dst);
    pp_src = &get_page(bank, blk_src, page_src);
    pp_dst->is_erased = 0;
    vpage_copy(&pp_dst->vpage, &pp_src->vpage, 0, VST_SECTORS_PER_PAGE);
}

void vst_erase_block(uint32_t const bank, uint32_t const blk)
{
    record(LOG_FLASH, "E: flash(%u, %u)\n", bank, blk);
    inc_flash_erase(1);

    for (uint32_t i = 0; i < VST_PAGES_PER_BLOCK; i++) {
        flash_page_t *pp;
        pp = &get_page(bank, blk, i);
        pp->is_erased = 1;
        vpage_free(&pp->vpage);
    }
}

int open_flash(void)
{
    uint32_t i, j, k;

    for (i = 0; i < VST_NUM_BANKS; i++) {
        for (j = 0; j < VST_BLOCKS_PER_BANK; j++) {
            for (k = 0; k < VST_PAGES_PER_BLOCK; k++) {
                flash_page_t *pp = &get_page(i, j, k);
                pp->is_erased = 1;
                vpage_init(&pp->vpage, NULL);
            }
        }
    }
    record(LOG_FLASH, "Virtual flash initialized\n");
    return 0;
}

void close_flash(void)
{
}
