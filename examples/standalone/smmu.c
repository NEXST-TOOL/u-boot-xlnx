#include "smmu.h"

static struct arm_smmu smmu;

static unsigned int dec_bussize(unsigned int v)
{
	unsigned int map[8] = {
		[0] = 32,
		[1] = 36,
		[2] = 40,
		[3] = 42,
		[4] = 44,
		[5] = 48,
	};

	v &= 7;
	return map[v];
}

#if 0
static unsigned int cb_offset(unsigned int cb)
{
	return cb * (R_SMMU500_SMMU_CB1_SCTLR - R_SMMU500_SMMU_CB0_SCTLR);
}

bool smmu_map(bool s1, bool s2, bool write, uintptr_t va, uintptr_t *pa)
{
	struct smmu *m = &smmu;
	uintptr_t regs[] = {
		[0] = R_SMMU500_SMMU_GATS1PR,
		[1] = R_SMMU500_SMMU_GATS12PR
	};
	uint64_t addr;
	uint64_t v = va;
	uint64_t par;
	bool err;

	assert(s1);
	addr = m->base + regs[s2];
	addr += write * 8;

	v |= m->cb[0];

	reg64_write(addr, v);
	dmb();
	reg32_waitformask(m->base + R_SMMU500_SMMU_GATSR, 1, 0);
	par = reg64_read(m->base + R_SMMU500_SMMU_GPAR);
	err = par & 1;
	printf("smmu: par %llx\n", par);
	if (!err) {
		*pa = par & ~((1ULL << 12) - 1);
		*pa &= ((1ULL << 48) - 1);
	}
#if 1
	printf("smmu: S%s%s err=%d %lx -> %lx\n",
		s1 ? "1" : "",
		s2 ? "2" : "",
		err, va, *pa);
#endif
	return err;
}

void smmu_setup_tcr(struct smmu *m)
{
	unsigned int i;

	for (i = 0; i < 2; i++) {
		uint64_t tcr;

		tcr = m->ctx[i]->tcr;
		tcr |= SMMU500_SMMU_CB0_TCR_LPAE_EAE_MASK;
		smmu_writel(m->base + cb_offset(m->cb[i]) + R_SMMU500_SMMU_CB0_TCR_LPAE,
				tcr);
		smmu_writel(m->base + cb_offset(m->cb[i]) + R_SMMU500_SMMU_CB0_TCR2,
				tcr >> 32);
	}
	dmb();
}

void smmu_setup_mair(struct smmu *m)
{
	/* MAIR is only used for stage1.  */
	smmu_writel(m->base + cb_offset(m->cb[0]) + R_SMMU500_SMMU_CB0_PRRR_MAIR0,
		m->ctx[0]->mair);
	dmb();
}
#endif

static void smmu_probe(struct arm_smmu *smmu)
{
	uint32_t r;

	r = readl(smmu->base + R_SMMU500_SMMU_SIDR7);

	printf("SMMU-500: v%d.%d at %llx %llx\n",
		reg32_field(r, SMMU500, SMMU_SIDR7, MAJOR),
		reg32_field(r, SMMU500, SMMU_SIDR7, MINOR), smmu->base, smmu->base + R_SMMU500_SMMU_SIDR0);

	r = readl(smmu->base + R_SMMU500_SMMU_SIDR0);
	printf("SMMU: cttw=%d btm=%d sec=%d stages=%s.%s str-match=%d ato-ns=%d nrsidb=%d extids=%d nrsmrg=%d\n",
		reg32_field(r, SMMU500, SMMU_SIDR0, CTTW),
		reg32_field(r, SMMU500, SMMU_SIDR0, BTM),
		reg32_field(r, SMMU500, SMMU_SIDR0, SES),
		reg32_field(r, SMMU500, SMMU_SIDR0, S1TS) ? "S1" : "",
		reg32_field(r, SMMU500, SMMU_SIDR0, S2TS) ? "S2" : "",
		reg32_field(r, SMMU500, SMMU_SIDR0, SMS),
		reg32_field(r, SMMU500, SMMU_SIDR0, ATOSNS),
		reg32_field(r, SMMU500, SMMU_SIDR0, NUMSIDB),
		r & (1 << 8),
		reg32_field(r, SMMU500, SMMU_SIDR0, NUMSMRG)
		);

	//set the number of mapping groups
	smmu->num_mapping_groups = reg32_field(r, SMMU500, SMMU_SIDR0, NUMSMRG);

	r = readl(smmu->base + R_SMMU500_SMMU_SIDR1);
	printf("SMMU: pagesize=%d nrpgidx=%d nrs2cb=%d smcd=%d nrcb=%d\n",
		reg32_field(r, SMMU500, SMMU_SIDR1, PAGESIZE),
		reg32_field(r, SMMU500, SMMU_SIDR1, NUMPAGENDXB),
		reg32_field(r, SMMU500, SMMU_SIDR1, NUMS2CB),
		reg32_field(r, SMMU500, SMMU_SIDR1, SMCD),
		reg32_field(r, SMMU500, SMMU_SIDR1, NUMCB));

	//set the number of context banks
	smmu->num_context_banks = reg32_field(r, SMMU500, SMMU_SIDR1, NUMCB);

	r = readl(smmu->base + R_SMMU500_SMMU_SIDR2);
	printf("SMMU: r=%x 64k=%d 16k=%d 4k=%d ubs=%d oas=%d ias=%d\n", r,
		reg32_field(r, SMMU500, SMMU_SIDR2, PTFSV8_64KB),
		reg32_field(r, SMMU500, SMMU_SIDR2, PTFSV8_16KB),
		reg32_field(r, SMMU500, SMMU_SIDR2, TFSV8_4KB),
		dec_bussize(reg32_field(r, SMMU500, SMMU_SIDR2, UBS)),
		dec_bussize(reg32_field(r, SMMU500, SMMU_SIDR2, OAS)),
		dec_bussize(reg32_field(r, SMMU500, SMMU_SIDR2, IAS))
		);

	printf("SMMU: idr[]=%x, %x, %x\n",
			readl(smmu->base + R_SMMU500_SMMU_SIDR0),
			readl(smmu->base + R_SMMU500_SMMU_SIDR1),
			readl(smmu->base + R_SMMU500_SMMU_SIDR2));
}

static void arm_smmu_address_translation(struct arm_smmu *smmu, u64 vaddr)
{
}

static void arm_smmu_write_context_bank(struct arm_smmu *smmu, int idx, int stage)
{
	u64 reg;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];

	/* Unassigned context banks only need disabling */
	if (!cb->configured) {
		smmu_writel(smmu->base + ARM_SMMU_CB_SCTLR(idx), 0);
	
		reg = readl(smmu->base + ARM_SMMU_CB_SCTLR(idx));
		printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_SCTLR(idx), reg);

		return;
	}

	/* CBA2R */
	/*we assume ARM_SMMU_CTX_FMT_AARCH64*/
	reg = SMMU500_SMMU_CBA2R0_VA64_MASK;
	smmu_writel(smmu->base + ARM_SMMU_CBA2R(idx), reg);
	
	reg = readl(smmu->base + ARM_SMMU_CBA2R(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CBA2R(idx), reg);

	/* CBAR */
	if (stage == 1)
		reg = (CBAR_TYPE_S1_TRANS_S2_BYPASS << SMMU500_SMMU_CBAR0_TYPE_SHIFT);
	else
		reg = (CBAR_TYPE_S2_TRANS << SMMU500_SMMU_CBAR0_TYPE_SHIFT);

	if (stage == 1)
		reg |= (CBAR_S1_BPSHCFG_NSH << SMMU500_SMMU_CBAR0_BPSHCFG_CBNDX_1_0_SHIFT) |
		       (CBAR_S1_MEMATTR_WB << SMMU500_SMMU_CBAR0_MEMATTR_CBNDX_7_4_SHIFT);

	smmu_writel(smmu->base + ARM_SMMU_CBAR(idx), reg);

	reg = readl(smmu->base + ARM_SMMU_CBAR(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CBAR(idx), reg);

	/*
	 * TCR
	 * We must write this before the TTBRs, since it determines the
	 * access behaviour of some fields (in particular, ASID[15:8]).
	 */
	if (stage == 1)
	{
		smmu_writel(smmu->base + ARM_SMMU_CB_TCR2(idx), cb->tcr[1]);
	
		reg = readl(smmu->base + ARM_SMMU_CB_TCR2(idx));
		printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_TCR2(idx), reg);
	}

	smmu_writel(smmu->base + ARM_SMMU_CB_TCR(idx), cb->tcr[0]);

	reg = readl(smmu->base + ARM_SMMU_CB_TCR(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_TCR(idx), reg);

	/* TTBRs */
	writeq(cb->ttbr[0], smmu->base + ARM_SMMU_CB_TTBR0(idx));

	reg = readq(smmu->base + ARM_SMMU_CB_TTBR0(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_TTBR0(idx), reg);

	if (stage == 1)
	{
		writeq(cb->ttbr[1], smmu->base + ARM_SMMU_CB_TTBR1(idx));
	
		reg = readq(smmu->base + ARM_SMMU_CB_TTBR1(idx));
		printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_TTBR1(idx), reg);
	}

	/* MAIR (stage-1 only) */
	if (stage == 1)
	{
		smmu_writel(smmu->base + ARM_SMMU_CB_MAIR0(idx), cb->mair[0]);
	
		reg = readl(smmu->base + ARM_SMMU_CB_MAIR0(idx));
		printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_MAIR0(idx), reg);

		smmu_writel(smmu->base + ARM_SMMU_CB_MAIR1(idx), cb->mair[1]);
	
		reg = readl(smmu->base + ARM_SMMU_CB_MAIR1(idx));
		printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_MAIR1(idx), reg);
	}

	/* SCTLR */
	reg = SMMU500_SMMU_CB0_SCTLR_M_MASK |
	      SMMU500_SMMU_CB0_SCTLR_TRE_MASK |
	      SMMU500_SMMU_CB0_SCTLR_AFE_MASK |
	      SMMU500_SMMU_CB0_SCTLR_CFRE_MASK |
	      SMMU500_SMMU_CB0_SCTLR_CFIE_MASK;

	if (stage == 1)
		reg |= SMMU500_SMMU_CB0_SCTLR_ASIDPNE_MASK;

	smmu_writel(smmu->base + ARM_SMMU_CB_SCTLR(idx), reg);

	reg = readl(smmu->base + ARM_SMMU_CB_SCTLR(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_CB_SCTLR(idx), reg);
}

static void smmu_set_tcr(struct arm_smmu_cb *smmu_cb, int stage)
{
	u64 reg;

	reg = TCR_ORGN_WBWA | TCR_IRGN_WBWA;

	if (stage == 1)
		reg |= TCR_SHARED_OUTER;
	else
		//for stage-2 translation, EAE (31-bit) is set to 1
		reg |= TCR_SHARED_INNER | STAGE_2_TCR_RES1;

	//we use 4KB page granule
	reg |= TCR_TG0_4K;

	//we use 40-bit physical address (oas) 
	if (stage == 1)
		reg |= (2ULL << (SMMU500_SMMU_CB0_TCR2_PASIZE_SHIFT + 32));
	else
		reg |= (2ULL << SMMU500_SMMU_CB0_TCR_LPAE_T1SZ_2_0_PASIZE_SHIFT);

	//we use 48-bit virtual address (ias)
	reg |= 16ULL;

	if (stage == 1)
		reg |= SMMU500_SMMU_CB0_TCR_LPAE_EPD1_MASK;

	//lookup start level = 0
	if (stage == 2)
		reg |= (0x2 << SMMU500_SMMU_CB0_TCR_LPAE_SL0_0_SHIFT);

	smmu_cb->tcr[0] = (reg & 0xFFFFFFFF);
	
	if (stage == 1)
		smmu_cb->tcr[1] = (reg >> 32);
}

static void smmu_set_ttbr(struct arm_smmu_cb *smmu_cb, unsigned long pg_table, int stage)
{
	smmu_cb->ttbr[0] = (unsigned long long)pg_table;
	if (stage == 1)
		smmu_cb->ttbr[1] = 0;
}

static void smmu_set_mair(struct arm_smmu_cb *smmu_cb)
{
	u64 attr = MEMORY_ATTRIBUTES;
	smmu_cb->mair[0] = (attr & 0xFFFFFFFF);
	smmu_cb->mair[1] = (attr >> 32);
}

static void arm_smmu_write_s2cr(struct arm_smmu *smmu, int idx) 
{
	struct arm_smmu_s2cr *s2cr = smmu->s2crs + idx;
	u32 reg = (s2cr->type << SMMU500_SMMU_S2CR0_TYPE_SHIFT) |
		  (s2cr->cbndx << SMMU500_SMMU_S2CR0_CBNDX_VMID_SHIFT) |
		  (s2cr->privcfg << SMMU500_SMMU_S2CR0_PRIVCFG_BSU_SHIFT);

	smmu_writel(smmu->base + ARM_SMMU_S2CR(idx), reg);

	reg = readl(smmu->base + ARM_SMMU_S2CR(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_S2CR(idx), reg);
}

static void arm_smmu_write_smr(struct arm_smmu *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = (smr->id << SMMU500_SMMU_SMR0_ID_SHIFT) | 
		  (smr->mask << SMMU500_SMMU_SMR0_MASK_SHIFT);

	if (smr->valid)
		reg |= SMMU500_SMMU_SMR0_VALID_MASK;

	smmu_writel(smmu->base + ARM_SMMU_SMR(idx), reg);

	reg = readl(smmu->base + ARM_SMMU_SMR(idx));
	printf("%s: reg %p = %llx\n", __func__, smmu->base + ARM_SMMU_SMR(idx), reg);
}

static void arm_smmu_write_sme(struct arm_smmu *smmu, int idx)
{
	arm_smmu_write_s2cr(smmu, idx);
	arm_smmu_write_smr(smmu, idx);
}

static int arm_smmu_find_sme(struct arm_smmu *smmu, u16 id, u16 mask)
{
	struct arm_smmu_smr *smrs = smmu->smrs;
	int i, free_idx = -ENOSPC;

	/* Validating SMRs is... less so */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		if (!smrs[i].valid) {
			/*
			 * Note the first free entry we come across, which
			 * we'll claim in the end if nothing else matches.
			 */
			if (free_idx < 0)
				free_idx = i;
			continue;
		}
		/*
		 * If the new entry is _entirely_ matched by an existing entry,
		 * then reuse that, with the guarantee that there also cannot
		 * be any subsequent conflicting entries. In normal use we'd
		 * expect simply identical entries for this case, but there's
		 * no harm in accommodating the generalisation.
		 */
		if ((mask & smrs[i].mask) == mask &&
		    !((id ^ smrs[i].id) & ~smrs[i].mask))
			return i;
		/*
		 * If the new entry has any other overlap with an existing one,
		 * though, then there always exists at least one stream ID
		 * which would cause a conflict, and we can't allow that risk.
		 */
		if (!((id ^ smrs[i].id) & ~(smrs[i].mask | mask)))
			return -EINVAL;
	}

	return free_idx;
}

static void arm_smmu_alloc_smes(struct arm_smmu *smmu, struct tee_info *current_tee)
{
	u32 dev_id = current_tee->sm_id; 
	u16 sid =  (dev_id & SMMU500_SMMU_SMR0_ID_MASK) >> SMMU500_SMMU_SMR0_ID_SHIFT;
	u16 mask = (dev_id & SMMU500_SMMU_SMR0_MASK_MASK) >> SMMU500_SMMU_SMR0_MASK_SHIFT;
	int ret, idx;

	ret = arm_smmu_find_sme(smmu, sid, mask);
	if (ret < 0)
	{
		printf("%s: no free or available stream-to-context match\n", __func__);
		return;
	}

	idx = ret;
	if (smmu->s2crs[idx].count == 0) {
		smmu->smrs[idx].id = sid;
		smmu->smrs[idx].mask = mask;
		smmu->smrs[idx].valid = 1;
	}
	smmu->s2crs[idx].count++;
	printf("%s: find SMR index %d\n", __func__, idx);
	current_tee->sme_index = idx;
}

static int tee_smmu_register(struct arm_smmu *smmu, u32 tee_id, u32 sm_id)
{
	int i;

	for(i = 0; i < ARM_SMMU_TOTAL_NUM_TEE; i++)
	{
		if (smmu->tee_entry_bitmap[i])
		{
			if (tee_id == smmu->tee_entry[i].tee_id)
			{
				printf("%s: such TEE has been registrated\n", __func__);
				return 0;
			}
			else
				continue;
		}
		// the left most, unregistrated entry can be used for current TEE
		else if (!smmu->tee_entry_bitmap[i])
			break;
	}
	
	if (i == ARM_SMMU_TOTAL_NUM_TEE)
		return -1;

	/* registration of current  */
	smmu->tee_entry[i].tee_id = tee_id;
	smmu->tee_entry[i].sm_id = sm_id;
	smmu->tee_entry_bitmap[i] = 1;
	
	return 0;
}

static void smmu_device_reset(struct arm_smmu *smmu)
{
	int i;
	u32 reg;

	/* clear global FSR */
	reg = readl(smmu->base + R_SMMU500_SMMU_SGFSR);
	smmu_writel(smmu->base + R_SMMU500_SMMU_SGFSR, reg);
	reg = readl(smmu->base + R_SMMU500_SMMU_SGFSR);
	printf("%s: reg %p = %llx\n", __func__, smmu->base + R_SMMU500_SMMU_SGFSR, reg);

	/*
	 * Reset stream mapping groups: Initial values mark all SMRn as
	 * invalid and all S2CRn as bypass unless overridden.
	 */
	for (i = 0; i < smmu->num_mapping_groups; ++i)
		arm_smmu_write_sme(smmu, i);

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		arm_smmu_write_context_bank(smmu, i, 0);
		smmu_writel(smmu->base + ARM_SMMU_CB_FSR(i), FSR_FAULT);
	}

	/* Invalidate the TLB, just in case */
	smmu_writel(smmu->base + R_SMMU500_SMMU_TLBIALLNSNH, QCOM_DUMMY_VAL);

	reg = readl(smmu->base + R_SMMU500_SMMU_SCR0);
	printf("%s: reg %p = %llx\n", __func__, smmu->base + R_SMMU500_SMMU_SCR0, reg);

	/* Enable fault reporting */
	reg |= (SMMU500_SMMU_SCR0_GFRE_MASK | SMMU500_SMMU_SCR0_GFIE_MASK | 
		SMMU500_SMMU_SCR0_GCFGFRE_MASK | SMMU500_SMMU_SCR0_GCFGFIE_MASK);

	/* Disable TLB broadcasting. */
	reg |= (SMMU500_SMMU_SCR0_VMIDPNE_MASK | SMMU500_SMMU_SCR0_PTM_MASK);

	/* Enable client access, handling unmatched streams as appropriate */
	reg &= ~SMMU500_SMMU_SCR0_CLIENTPD_MASK;
	/* Enable transactions bypass */	
	reg &= ~SMMU500_SMMU_SCR0_USFCFG_MASK;

	/* Disable forced broadcasting */
	reg &= ~SMMU500_SMMU_SCR0_FB_MASK;

	/* Don't upgrade barriers */
	reg &= ~(SMMU500_SMMU_SCR0_BSU_MASK);

	/* Push the button */
	//arm_smmu_tlb_sync_global(smmu);
	smmu_writel(smmu->base + R_SMMU500_SMMU_SCR0, reg);

	reg = readl(smmu->base + R_SMMU500_SMMU_SCR0);
	printf("%s: reg %p = %llx\n", __func__, smmu->base + R_SMMU500_SMMU_SCR0, reg);
}

#if 0
void smmu_init_ctx(struct mmu_ctx *s1_mmu, struct mmu_ctx *s2_mmu,
			unsigned int s1_cb, unsigned int s2_cb,
			unsigned int el, bool enable)
#endif
void smmu_init_ctx(unsigned long pg_table)
{
	//struct smmu *m = &smmu;
	//uint32_t r;
	
	int tee_idx, stream_idx, context_bank;
	int stage = 2;
	int i;

	u32 tee_id = 0, stream_id;
	u16 test_strm_id = 1, test_mask = (~test_strm_id) & 0x7FFF;

	struct arm_smmu_cb *smmu_cb;

	stream_id = (test_strm_id << SMMU500_SMMU_SMR0_ID_SHIFT) | 
		    (test_mask << SMMU500_SMMU_SMR0_MASK_SHIFT);
	
	smmu.base = (void *)0xFD800000;

	memset(smmu.s2crs, 0, sizeof(smmu.s2crs));
	memset(smmu.smrs, 0, sizeof(smmu.smrs));

	for(i = 0; i < ARM_SMMU_MAX_NUM_STREAM; i++)
		smmu.s2crs[i].type = S2CR_TYPE_BYPASS; 

	memset(smmu.cbs, 0, sizeof(smmu.cbs));

	memset(smmu.tee_entry, 0, sizeof(smmu.tee_entry));
	memset(smmu.tee_entry_bitmap, 0, sizeof(smmu.tee_entry_bitmap));

	smmu_probe(&smmu);
	smmu_device_reset(&smmu);

	tee_idx = tee_smmu_register(&smmu, tee_id, stream_id);
	if(tee_idx < 0)
	{
		printf("%s: no available entry for new TEE\n", __func__);
		return;
	}

	arm_smmu_alloc_smes(&smmu, &smmu.tee_entry[tee_idx]);
	stream_idx = smmu.tee_entry[tee_idx].sme_index;

	//set a fixed context bank
        context_bank = 3;

	/* initialize stream context */
	smmu.s2crs[stream_idx].type = S2CR_TYPE_TRANS; 
	smmu.s2crs[stream_idx].privcfg = S2CR_PRIVCFG_DEFAULT;
	smmu.s2crs[stream_idx].cbndx = context_bank;

	/* It worked! Now, poke the actual hardware */
	arm_smmu_write_sme(&smmu, stream_idx);

	/* initialize context bank */
	smmu_cb = &smmu.cbs[context_bank];

	/* setup stage 1 or stage 2 translation context */
	smmu_set_tcr(smmu_cb, stage);
	if (stage == 1)
		smmu_set_mair(smmu_cb);
	smmu_set_ttbr(smmu_cb, pg_table, stage);
	smmu_cb->configured = 1;
	arm_smmu_write_context_bank(&smmu, context_bank, stage);

	//checkup address mapping via address translation service

#if 0
	m->cb[0] = s1_cb;
	m->cb[1] = s2_cb;
	m->ctx[0] = s1_mmu;
	m->ctx[1] = s2_mmu;

	aarch64_mmu_compute_tcr(s1_mmu, el);
	aarch64_mmu_compute_tcr(s2_mmu, el);
	aarch64_mmu_compute_mair(s1_mmu, el);

	smmu_setup_mair(m);
	smmu_setup_tcr(m);

	smmu_writel(m->base + R_SMMU500_SMMU_CBA2R0, SMMU500_SMMU_CBA2R0_VA64_MASK);

	r = readl(m->base + R_SMMU500_SMMU_SCR0);
	r &= ~SMMU500_SMMU_SCR0_CLIENTPD_MASK;
	smmu_writel(m->base + R_SMMU500_SMMU_SCR0, r);

	r = readl(m->base + R_SMMU500_SMMU_S2CR0);
	r &= ~SMMU500_SMMU_S2CR0_TYPE_MASK;
	smmu_writel(m->base + R_SMMU500_SMMU_S2CR0, r);

	smmu_writel(m->base + R_SMMU500_SMMU_SCR1, 0x00000101);
	smmu_writel(m->base + R_SMMU500_SMMU_SMR0, 0xFFFE0001);

	if (enable && m->ctx[0]) {
		phys_addr_t cb_base = m->base + cb_offset(s1_cb);

		reg32_write64le(cb_base + R_SMMU500_SMMU_CB0_TTBR0_LOW,
				(uintptr_t) m->ctx[0]->pt.root);
		/* No support for split yet.  */
		reg32_write64le(cb_base + R_SMMU500_SMMU_CB0_TTBR1_LOW,
				(uintptr_t) m->ctx[1]->pt.root);

		smmu_writel(m->base + R_SMMU500_SMMU_CBAR0 + s1_cb * 4,
			s2_cb << 8 |
			3 << SMMU500_SMMU_CBAR0_TYPE_SHIFT);

		dmb();

		r = readl(cb_base + R_SMMU500_SMMU_CB0_SCTLR);
		D(printf("SMMU: SCTLR %x - >%x\n", r,
			r | SMMU500_SMMU_CB0_SCTLR_M_MASK));
		r |= SMMU500_SMMU_CB0_SCTLR_M_MASK;
		smmu_writel(cb_base + R_SMMU500_SMMU_CB0_SCTLR, r);
		dmb();
	}

	if (enable && m->ctx[1]) {
		phys_addr_t cb_base = m->base + cb_offset(s2_cb);

		reg32_write64le(cb_base + R_SMMU500_SMMU_CB0_TTBR0_LOW,
				(uintptr_t) m->ctx[1]->pt.root);
		/* No support for split yet.  */
		reg32_write64le(cb_base + R_SMMU500_SMMU_CB0_TTBR1_LOW,
				(uintptr_t) m->ctx[1]->pt.root);
		r = readl(cb_base + R_SMMU500_SMMU_CB0_SCTLR);
		D(printf("SMMU: SCTLR %x - >%x\n", r,
			r | SMMU500_SMMU_CB0_SCTLR_M_MASK));
		r |= SMMU500_SMMU_CB0_SCTLR_M_MASK;
		smmu_writel(cb_base + R_SMMU500_SMMU_CB0_SCTLR, r);
		dmb();
	}
	smmu_writel(m->base + R_SMMU500_SMMU_STLBIALL, 1);
	smmu_writel(m->base + R_SMMU500_SMMU_TLBIALLNSNH, 1);
	dmb();
	smmu_writel(m->base + R_SMMU500_SMMU_STLBGSYNC, 1);
	dmb();
	reg32_waitformask(m->base + R_SMMU500_SMMU_STLBGSTATUS, 1, 0);
#endif
}

