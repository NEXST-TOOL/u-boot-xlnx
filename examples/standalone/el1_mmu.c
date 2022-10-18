#include <el1_mmu.h>
#include <exports.h>

static struct ptdesc *aarch64_mmu_alloc_table(struct mmu_ctx *mmu)
{
	unsigned int nr_entries = 1 << (mmu->grainsize - 3);
	struct ptdesc *ret = mmu->pt.pos;

	if (mmu->pt.pos + nr_entries >= mmu->pt.end) {
		printf("PT OOM\n");
		return NULL;
	}

	mmu->pt.pos += nr_entries;
	memset(ret, 0, nr_entries * sizeof *ret);
	printf("alloc table: %p nr_entries=%u\n", ret, nr_entries);
	return ret;
}

bool aarch64_mmu_map(struct mmu_ctx *mmu,
			uintptr_t va, uintptr_t pa,
			uintptr_t len, unsigned int prot,
			unsigned int attridx,
			int el, bool secure)
{
	uintptr_t masks[] = {
		[PAGEKIND_TINY_4K]  = ((1 << 12) - 1),
		[PAGEKIND_MID_16K] = ((1 << 14) - 1),
		[PAGEKIND_HUGE_64K] = ((1 << 16) - 1),
	};
	uintptr_t sizes[3][4] = {
		[PAGEKIND_TINY_4K]  = {
			0, MAPSIZE_1G, MAPSIZE_2M, MAPSIZE_4K,
		},
	};
	uintptr_t mask = masks[mmu->pagekind];
	struct ptdesc *next = mmu->pt.root;
	struct ptdesc *pte;
	unsigned int level = 0;

	assert((va & mask) == 0);
	assert((pa & mask) == 0);
	assert((len & mask) == 0);

	printf("\nmap S%d %lx -> %lx\n", mmu->stage, va, pa);
	for (level = mmu->startlevel; level < 4; level++) {
		pte = aarch64_ptdesc(mmu, next, level, va);

		printf("\tL%d size=%lx len=%lx\n", level, sizes[mmu->pagekind][level], len);
		if (sizes[mmu->pagekind][level] == len) {
			/* Level 3 descriptors have a different encoding.  */
			bool is_table = level == 3;
			if (mmu->stage == 1) {
				unsigned int ap = prot & PROT_WRITE ? 0 : 2;
				if (el <= 1)
					ap |= 1;
				//unsigned int ap = prot & PROT_WRITE ? 2 : 0;
				//ap |= prot & PROT_READ ? 1 : 0;

				pte[0] = (struct ptdesc) {
					.block_s1 = {
						.is_valid = 1,
						.is_table = is_table,
						.af = prot ? 1 : 0,
						.ap = ap,
						.attrindex = attridx,
						.addr = aarch64_block_addr(mmu, level, pa),
						.xn = prot & PROT_EXEC ? 0 : 1,
					},
			        };
				printf("\tblock[L%d][%p] -> %lx ap=%x\n",
					level, pte, pte->block_s1.addr << 12, ap);

			} else {
				unsigned int ap = prot & PROT_WRITE ? 2 : 0;
				ap |= prot & PROT_READ ? 1 : 0;

				assert(mmu->stage == 2);
				pte[0] = (struct ptdesc) {
					.block_s2 = {
						.is_valid = 1,
						.is_table = is_table,
						.af = 1,
						.s2ap = ap,
						.memattr = 0,
						.addr = aarch64_block_addr(mmu, level, pa),
					},
			        };
				printf("\tblock[L%d][%p] -> %lx s2ap=%x\n",
					level, pte, pte->block_s2.addr << 12, ap);

			}
			break;
		} else {
			assert(level < 3);
			if (pte->sw.swdef & PTE_ALLOCATED) {
				next = (struct ptdesc *) (uintptr_t) (pte->table.addr << 2);
				continue;
			} else {
				next = aarch64_mmu_alloc_table(mmu);
				if (!next)
					return false;
			}

			pte[0] = (struct ptdesc) {
				.table = {
					.is_valid = 1,
					.is_table = 1,
					.addr = ((uintptr_t) next) >> 2,
				},
		        };
			pte->sw.swdef |= PTE_ALLOCATED;
			next = (struct ptdesc *) (uintptr_t) (pte->table.addr << 2);
			printf("\ttable[L%d][%p] -> %lx (%lx)\n",
				level, pte, pte->table.addr << 2, pte->table.addr);
		}
	}
	dmb();
	return true;
}

static uint64_t aarch64_mmu_table_size(struct mmu_ctx *mmu, unsigned int level)
{
	unsigned int stride = mmu->grainsize - 3;

	int bits = mmu->virtbits - mmu->grainsize  - (3 - level) * stride;
	if (bits <= 0)
		return 0;
	return (1 << bits) * 3;
}

void aarch64_mmu_pt_zap(struct mmu_ctx *mmu, unsigned int el)
{
	/* Free all page tables, realloc the root.  */
	//memset(mmu->pt.root, 0, (char *)mmu->pt.end - (char *)mmu->pt.root);
	mmu->pt.pos = mmu->pt.root;
	mmu->pt.root = aarch64_mmu_alloc_table(mmu);
	aarch64_mmu_tlb_flush(mmu, el);
}

uintptr_t aarch64_mmu_pt_setup(struct mmu_ctx *mmu,
			enum pagekind pagekind,
			unsigned int stage,
			unsigned int virtbits,
			unsigned int physbits,
			void *start, void *end)
{
	uintptr_t pagemap[] = {
		[PAGEKIND_TINY_4K]  = PAGESIZE_4K,
		[PAGEKIND_MID_16K]  = PAGESIZE_16K,
		[PAGEKIND_HUGE_64K] = PAGESIZE_64K,
	};
	uintptr_t pagebits_map[] = {
		[PAGEKIND_TINY_4K]  = 12,
		[PAGEKIND_MID_16K]  = 14,
		[PAGEKIND_HUGE_64K] = 16,
	};

	unsigned int i;
	uintptr_t pos = (uintptr_t) start;
	uint32_t sctlr;

	mmu->stage = stage;
	mmu->pagekind = pagekind;
	mmu->pagesize = pagemap[pagekind];
	mmu->physbits = physbits;
	mmu->virtbits = virtbits;
	mmu->grainsize = pagebits_map[mmu->pagekind];

	mmu->startlevel = 0;
	mmu->pt.pos = start;
	mmu->pt.end = end;
	for (i = 0; i < 4; i++) {
		uint64_t size = aarch64_mmu_table_size(mmu, i);
		if (size == 0) {
			mmu->startlevel = i + 1;
			continue;
		}
		pos += size;
		printf("%s: i=%d, size = 0x%llx\n", __func__, i, size);
	}
	mmu->pt.root = aarch64_mmu_alloc_table(mmu);

	printf("%s: pt root: %p\n", __func__, mmu->pt.root);

	printf("PT: S%d startlevel=%d vs=%u ps=%u end=%lx\n",
		stage, mmu->startlevel, virtbits, physbits, pos);

	//disable cache & MMU of EL=1
	asm volatile("mrs %0, sctlr_el1" : "=r" (sctlr) : : "memory");
	sctlr &= ~(CR_M | CR_I | CR_C);
	dmb();
	asm volatile("msr sctlr_el1, %0" : : "r" (sctlr) : "memory");
	isb();

	//flush TLB of EL=1
	asm volatile("tlbi vmalle1" : : : "memory");
	asm volatile("dsb sy" : : : "memory");
	isb();

	return 0;
}

void aarch64_mmu_setup(struct mmu_ctx *mmu, unsigned int el, bool enable)
{
	//uint32_t tmp;
	
	if (el > 1) return;

	printf("%s: el=%d stage=%d enable=%d\n", __func__, el, mmu->stage, enable);
#if 0
	if (!enable) {
		if (el < 2 && mmu->stage == 2) {
			aarch64_msr("vttbr_el2", 0);
			aarch64_mrs(tmp, "hcr_el2");
			tmp &= ~HCR_VM;
			dmb();
			aarch64_msr("hcr_el2", tmp);
			ibarrier();
		}
		switch (el) {
		case 1:
			aarch64_mrs(sctlr, "sctlr_el1");
			sctlr &= ~SCTLR_M;
			dmb();
			aarch64_msr("sctlr_el1", sctlr);
			ibarrier();
			break;
		default:
			aarch64_mrs(sctlr, "sctlr_el3");
			sctlr &= ~SCTLR_M;
			/* Make sure all outstanding mem transactions are done.  */
			dmb();
			aarch64_msr("sctlr_el3", sctlr);
			ibarrier();
		};
		aarch64_mmu_tlb_flush(mmu, el);
		return;
	}
#endif
	if (enable) {
		uintptr_t ttbr = (uintptr_t) mmu->pt.root;
		uint64_t tcr, mair, sctlr;
		uint64_t hcr;
		
		//setup stage-2 page table base address 
		if (mmu->stage == 2) {

			//setup base address of stage-2 table to vttbr_el2
			asm volatile("msr vttbr_el2, %0" : : "r" (ttbr) : "memory");
			isb();

			//enable virtualization
			asm volatile("mrs %0, hcr_el2" : "=r" (hcr) : : "memory");
			hcr |= HCR_VM;
			dmb();
			asm volatile("msr hcr_el2, %0" : : "r" (hcr) : "memory");
			isb();

			asm volatile("mrs %0, vttbr_el2" : "=r" (ttbr) : : "memory");
			asm volatile("mrs %0, hcr_el2" : "=r" (hcr) : : "memory");
			printf("%s: vttbr = %llx, hcr = %llx\n", __func__, ttbr, hcr);

			return;
		}
			
		//set TTBR, TCR and MAIR of EL=1
		asm volatile("msr ttbr0_el1, %0" : : "r" (ttbr) : "memory");
		dmb();
		tcr = get_tcr(el, NULL, NULL);
		asm volatile("msr tcr_el1, %0" : : "r" (tcr) : "memory");
		dmb();
		mair = MEMORY_ATTRIBUTES;
		asm volatile("msr mair_el1, %0" : : "r" (mair) : "memory");
		isb();

		//set sctlr register of EL=1 (enable MMU, I-$ and D-$)
		asm volatile("mrs %0, sctlr_el1" : "=r" (sctlr) : : "memory");
		sctlr |= CR_M | CR_I | CR_C;
		dmb();
		asm volatile("msr sctlr_el1, %0" : : "r" (sctlr) : "memory");
		isb();

		//fense: ensure all CRS operations are effective
		asm volatile("ic iallu" : : : "memory");
		asm volatile("dsb sy" : : : "memory");
		asm volatile("isb sy" : : : "memory");
			
		asm volatile("mrs %0, ttbr0_el1" : "=r" (ttbr) : : "memory");
		asm volatile("mrs %0, tcr_el1" : "=r" (tcr) : : "memory");
		asm volatile("mrs %0, mair_el1" : "=r" (mair) : : "memory");
		asm volatile("mrs %0, sctlr_el1" : "=r" (sctlr) : : "memory");
		printf("%s: ttbr0_el1 = %llx, tcr_el1 = %llx, \
				mair_el1 = %llx, sctlr_el1 = %llx\n", __func__, ttbr, tcr, mair, sctlr);
	}
}
