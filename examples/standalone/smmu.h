#ifndef _SMMU_H
#define _SMMU_H

#include <common.h>
#include <asm/io.h>
#include <asm/armv8/mmu.h>
#include "reg-smmu.h"

#define R_SMMU500_SMMU_GATS1PR	(0x110)
#define R_SMMU500_SMMU_GATS1PW	(0x118)
#define R_SMMU500_SMMU_GATS12PR	(0x130)
#define R_SMMU500_SMMU_GATS12PW	(0x138)
#define R_SMMU500_SMMU_GPAR	(0x180)
#define R_SMMU500_SMMU_GATSR	(0x188)

#define smmu_writel(addr, val) writel(val, addr)

#define ARM_SMMU_MAX_NUM_STREAM     48
#define ARM_SMMU_MAX_NUM_CONTEXT    16

#define ARM_SMMU_TOTAL_NUM_TEE      8

#define IDX_REG_OFFSET(idx)   (idx <<  2)
#define IDX_PAGE_OFFSET(idx)  (idx << 12)

#define ARM_SMMU_SMR(idx)   R_SMMU500_SMMU_SMR0 + IDX_REG_OFFSET(idx) 
#define ARM_SMMU_S2CR(idx)  R_SMMU500_SMMU_S2CR0 + IDX_REG_OFFSET(idx) 

#define ARM_SMMU_CBAR(idx)  R_SMMU500_SMMU_CBAR0 + IDX_REG_OFFSET(idx) 
#define ARM_SMMU_CBA2R(idx) R_SMMU500_SMMU_CBA2R0 + IDX_REG_OFFSET(idx) 

#define ARM_SMMU_CB_TCR(idx)   R_SMMU500_SMMU_CB0_TCR_LPAE + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_TCR2(idx)  R_SMMU500_SMMU_CB0_TCR2 + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_TTBR0(idx) R_SMMU500_SMMU_CB0_TTBR0_LOW + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_TTBR1(idx) R_SMMU500_SMMU_CB0_TTBR1_LOW + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_SCTLR(idx) R_SMMU500_SMMU_CB0_SCTLR + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_MAIR0(idx) R_SMMU500_SMMU_CB0_PRRR_MAIR0 + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_MAIR1(idx) R_SMMU500_SMMU_CB0_NMRR_MAIR1 + IDX_PAGE_OFFSET(idx)
#define ARM_SMMU_CB_FSR(idx)   R_SMMU500_SMMU_CB0_FSR + IDX_PAGE_OFFSET(idx)

#define FSR_IGN   (SMMU500_SMMU_CB0_FSR_AFF_MASK | \
		   SMMU500_SMMU_CB0_FSR_ASF_MASK | \
		   SMMU500_SMMU_CB0_FSR_TLBMCF_MASK | \
		   SMMU500_SMMU_CB0_FSR_TLBLKF_MASK)

#define FSR_FAULT (SMMU500_SMMU_CB0_FSR_MULTI_MASK | \
		   SMMU500_SMMU_CB0_FSR_SS_MASK | \
		   SMMU500_SMMU_CB0_FSR_UUT_MASK | \
		   SMMU500_SMMU_CB0_FSR_EF_MASK | \
		   SMMU500_SMMU_CB0_FSR_PF_MASK | \
		   SMMU500_SMMU_CB0_FSR_TF_MASK | \
		   FSR_IGN) 

#define QCOM_DUMMY_VAL -1

#define STAGE_2_TCR_RES1   (1 << 31)

#define CBAR_S1_BPSHCFG_NSH    3
#define CBAR_S1_MEMATTR_WB     0xF

enum arm_smmu_s2cr_type {
	S2CR_TYPE_TRANS,
	S2CR_TYPE_BYPASS,
	S2CR_TYPE_FAULT,
};

enum arm_smmu_s2cr_privcfg {
	S2CR_PRIVCFG_DEFAULT,
	S2CR_PRIVCFG_DIPAN,
	S2CR_PRIVCFG_UNPRIV,
	S2CR_PRIVCFG_PRIV,
};

struct arm_smmu_s2cr {
	enum arm_smmu_s2cr_type      type;
	enum arm_smmu_s2cr_privcfg   privcfg;
	u8                           cbndx;
	u32                          count;
};

struct arm_smmu_smr {
	u16    mask;
	u16    id;
	u8     valid;
};

struct arm_smmu_cb {
	u64    ttbr[2];
	u32    tcr[2];
	u32    mair[2];
	u8     configured;
};

enum arm_smmu_cbar_type {
	CBAR_TYPE_S2_TRANS,
	CBAR_TYPE_S1_TRANS_S2_BYPASS,
	CBAR_TYPE_S1_TRANS_S2_FAULT,
	CBAR_TYPE_S1_TRANS_S2_TRANS,
};

struct tee_info {
	u32 tee_id;
	/* stream_id & mask in the format of SME */
	u32 sm_id;
	u32 sme_index;
};

/* We should really be registering this kind of drivers some where
 * but got no time now, so we'll just hack it into a global.  */
struct arm_smmu {
	volatile void *base; // = (void *)0xFD800000;

	unsigned int num_mapping_groups;
	unsigned int num_context_banks;

	unsigned int num_tee;

	struct arm_smmu_s2cr s2crs[ARM_SMMU_MAX_NUM_STREAM];
	struct arm_smmu_smr  smrs[ARM_SMMU_MAX_NUM_STREAM];

	struct arm_smmu_cb  cbs[ARM_SMMU_MAX_NUM_CONTEXT];
	
	struct tee_info     tee_entry[ARM_SMMU_TOTAL_NUM_TEE];
	u8                  tee_entry_bitmap[ARM_SMMU_TOTAL_NUM_TEE];

	//unsigned int cb[2];
	//struct mmu_ctx *ctx[2];
};

static inline uint32_t bit_mask32(unsigned int bitlen)
{
	return (1ULL << bitlen) - 1;
}

static inline uint32_t bit_field32(uint32_t val,
				unsigned int bit, unsigned int len)
{
	uint32_t r = val >> bit;
	return r & bit_mask32(len);
}

#define reg32_field(v, RG, RN, RF)				\
	bit_field32(v,						\
		RG ## _ ## RN ## _ ## RF ## _ ## SHIFT,		\
		RG ## _ ## RN ## _ ## RF ## _ ## WIDTH)

bool smmu_map(bool s1, bool s2, bool write, uintptr_t va, uintptr_t *pa);
#if 0
void smmu_init_ctx(struct mmu_ctx *s1_mmu, struct mmu_ctx *s2_mmu,
                        unsigned int s1_cb, unsigned int s2_cb,
                        unsigned int el, bool enable);
#endif
void smmu_init_ctx();

#endif
