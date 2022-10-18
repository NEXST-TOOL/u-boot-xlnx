#include "check_mmu.h"

static u32 *_pt_s2_start = (void *)(0x50000000);
static struct tester_map tmap;

#if 0
static bool map_smmu(enum stages stages,
			bool write, uintptr_t va, uintptr_t *pa)
{
	return smmu_map(true, stages == S12, write, va, pa);
}
#endif

static bool map_cpu(unsigned int el, enum stages stages,
			bool write, uintptr_t va, uintptr_t *pa)
{
	uint64_t phy_addr;
	bool err;

	switch (stages) {
		case S1:
			if (write)
				A64_AT("S1E1W", va);
			else
				A64_AT("S1E1R", va);
			break;
		case S12:
			if (write)
				A64_AT("S12E1W", va);
			else
				A64_AT("S12E1R", va);
			break;
		default:
			break;
	}

	isb();

        asm volatile("mrs %0, par_el1" : "=r" (phy_addr) : : "memory");
	dmb();
	err = phy_addr & 1;

	printf("%s: el1 phy_addr = %llx\n", __func__, phy_addr);

	*pa = phy_addr & PAR_ADDR_MASK; 
	*pa &= ~PAR_OFFSET_MASK;

	asm volatile("mrs %0, esr_el1" : "=r" (phy_addr));
	dmb();

	printf("%s: el1 esr = %llx\n", __func__, phy_addr);

	return err;
}

static void check_map(struct mmu_ctx *mmu, struct test_map *map, uint32_t el)
{
	uintptr_t pa, smmu_pa;
	bool err, smmu_err;

	/* checking MMU and SMMU page table walks */

	//Stage-1 read
	err = map_cpu(el, S1, false, map->va, &pa);
	//smmu_err = map_smmu(S1, 0, map->va, &smmu_pa);
	//assert(err == smmu_err);
	printf("ipa=%lx pa=%lx smmu_pa=%lx\n", map->ipa, pa, smmu_pa);

	//Stage-1 & Stage-2 read 
	err = map_cpu(el, S12, false, map->va, &pa);
	//smmu_err = map_smmu(S12, 0, map->va, &smmu_pa);
	//assert(err == smmu_err);
	printf("ipa=%lx pa=%lx smmu_pa=%lx\n", map->ipa, pa, smmu_pa);

	//Stage-1 write 
	err = map_cpu(el, S1, true, map->va, &pa);
	//smmu_err = map_smmu(S1, true, map->va, &smmu_pa);
	//assert(err == smmu_err);
	printf("ipa=%lx pa=%lx smmu_pa=%lx\n", map->ipa, pa, smmu_pa);

	//Stage-1 & Stage-2 write 
	err = map_cpu(el, S12, true, map->va, &pa);
	//smmu_err = map_smmu(S12, true, map->va, &smmu_pa);
	//assert(err == smmu_err);
	printf("ipa=%lx pa=%lx smmu_pa=%lx\n", map->ipa, pa, smmu_pa);
}

#if 0
static void prep_smmu(uint32_t el)
{
	smmu_init_ctx(&tmap.mmu[0], &tmap.mmu[1], 0, 1, el, true);
	printf("%s: SMMU S12MMU enabled\n", __func__);
}
#endif

static void prep_mmu(struct test_map *map, uint32_t el)
{
	uintptr_t pt_root;

	assert(el == 1);

	//FIXME: page table base address
	//Stage-1 page table 
	aarch64_mmu_pt_setup(&tmap.mmu[0], PAGEKIND_TINY_4K, 1,
			40, 40,
			_pt_s2_start,
			(void *)_pt_s2_start + 0x40000);

	//Stage-2 page table 
	aarch64_mmu_pt_setup(&tmap.mmu[1], PAGEKIND_TINY_4K, 2,
			39, 40,
			(void *)_pt_s2_start + 0x40000,
			(void *)_pt_s2_start + 0x80000);

	//setup address mapping in pagetables of each stage
	aarch64_mmu_map(&tmap.mmu[0], map->va, map->ipa, map->size, map->prot,
			MAIR_IDX_MEM, el, MAP_NONSECURE);
	aarch64_mmu_map(&tmap.mmu[1], map->ipa, map->pa, map->size, map->prot2,
			MAIR_IDX_MEM, el, MAP_NONSECURE);

	pt_root = (uintptr_t) tmap.mmu[0].pt.root;
	//mapping pagetables of each stage for page table walks in MMU
	aarch64_mmu_map(&tmap.mmu[1], pt_root, pt_root,
			MAPSIZE_2M, PROT_READ, MAIR_IDX_MEM, el, MAP_NONSECURE);

	//flush cache
	flush_dcache_all();
	asm volatile("dmb sy" : : : "memory");
	//flush TLB of EL=2
	asm volatile("tlbi alle2" : : : "memory");
	asm volatile("dsb sy" : : : "memory");
	isb();

	aarch64_mmu_setup(&tmap.mmu[1], el, true);
	aarch64_mmu_setup(&tmap.mmu[0], el, true);

	printf("S12 page tables setup\n");

	//FIXME: Setup the SMMU
	//prep_smmu(el);
}

#if 0
void teardown_map()
{
	struct tester_map *tmap = user;
	aarch64_mmu_setup(&tmap.mmu[0], 1, false);
	aarch64_mmu_setup(&tmap.mmu[1], 1, false);
	printf("S2MMU disabled\n");
}
#endif

void direct_map_test()
{
	struct test_map map = {
		.va = 0x41ULL << 30,
		.ipa = 0x41ULL << 30,
		.pa = 0x42ULL << 30,
		.size = MAPSIZE_1G,
		.prot = PROT_RW,
		.prot2 = PROT_RW,
	};

	uint32_t el = 1;

	//prepare temporary EL=1 pagetable mapped in both MMU and SMMU
	prep_mmu(&map, el);

	printf("va=%lx ipa=%lx pa=%lx prot=%x.%x size=%llx\n",
		map.va, map.ipa, map.pa,
		map.prot, map.prot2, map.size);

	//fill pagetable with direct mapping and 
	//check page table walks in both MMU and SMMU
	check_map(&tmap.mmu[0], &map, el);
}

