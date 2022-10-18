#ifndef CHECK_MMU_H
#define CHECK_MMU_H

#include <common.h>
#include "el1_mmu.h"

struct test_map {
	uintptr_t va;
	uintptr_t ipa;
	uintptr_t pa;
	uint64_t size;
	uint32_t prot;
	uint32_t prot2;
};

struct tester_map {
	struct mmu_ctx mmu[2];
}; 

enum stages {
	S1,
	S12
};

#define PHYSMASK  ((1ULL << 40) - 1)

#define A64_AT(m, va) asm volatile("at " m ", %0\n" : : "r" (va) : "memory")
#define PAR_ADDR_MASK    ((1ULL << 48) - 1)
#define PAR_OFFSET_MASK  ((1ULL << 12) - 1)

void direct_map_test();

#endif

