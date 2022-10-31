#ifndef EL1_MMU_H
#define EL1_MMU_H

#include <common.h>
#include <asm/armv8/mmu.h>

static struct mm_region check_map = 
{
	.virt = 0x140000000UL,
	.phys = 0x180000000UL,
	.size = 0x70000000UL,
	.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
		 PTE_BLOCK_INNER_SHARE
};

#endif
