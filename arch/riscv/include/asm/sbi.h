/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 Regents of the University of California
 *
 * Taken from Linux arch/riscv/include/asm/sbi.h
 */

#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

#include <linux/types.h>

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI 4
#define SBI_REMOTE_FENCE_I 5
#define SBI_REMOTE_SFENCE_VMA 6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN 8

struct sbi_call_res {
	unsigned long a0;
	unsigned long a1;
};

#if 0
#define SBI_CALL(which, arg0, arg1, arg2, arg3) ({			\
	register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);	\
	register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);	\
	register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);	\
	register uintptr_t a3 asm ("a3") = (uintptr_t)(arg3);	\
	register uintptr_t a7 asm ("a7") = (uintptr_t)(which);	\
	asm volatile ("ecall"					\
		      : "+r" (a0)				\
		      : "r" (a1), "r" (a2), "r" (a3), "r" (a7)		\
		      : "memory");				\
	a0;							\
})

#define SBI_CALL_SPEC_0_2(extid, funcid, outval, arg0, arg1, arg2, arg3) ({			\
	register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);	\
	register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);	\
	register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);	\
	register uintptr_t a3 asm ("a3") = (uintptr_t)(arg2);	\
	register uintptr_t a6 asm ("a6") = (uintptr_t)(funcid);	\
	register uintptr_t a7 asm ("a7") = (uintptr_t)(extid);	\
	asm volatile ("ecall"					\
		      : "+r" (a0), "+r" (a1)				\
		      : "r" (a2), "r" (a3), "r"(a6), "r" (a7)		\
		      : "memory");				\
	*outval = a1; \
	a0;							\
})

/* Lazy implementations until SBI is finalized */
#define SBI_CALL_0(which) SBI_CALL(which, 0, 0, 0)
#define SBI_CALL_1(which, arg0) SBI_CALL(which, arg0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, arg0, arg1, 0)
#endif

#define SBI_CALL_RET(extid, funcid, outval, arg0, arg1, arg2, arg3) ({			\
	register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);	\
	register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);	\
	register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);	\
	register uintptr_t a3 asm ("a3") = (uintptr_t)(arg3);	\
	register uintptr_t a6 asm ("a6") = (uintptr_t)(funcid);	\
	register uintptr_t a7 asm ("a7") = (uintptr_t)(extid);	\
	asm volatile ("ecall"					\
		      : "+r" (a0), "+r" (a1)				\
		      : "r" (a2), "r" (a3), "r"(a6), "r" (a7)		\
		      : "memory");				\
	*(outval) = a1; \
	a0;							\
})

#define SBI_CALL(which, funcid, arg0, arg1, arg2, arg3)	\
		struct sbi_call_res res; \
		res.a0 = SBI_CALL_RET((which), (funcid), &res.a1, (arg0), (arg1), (arg2), (arg3));

#define SBI_CALL_0(which) SBI_CALL(which, 0, 0, 0, 0, 0)
#define SBI_CALL_1(which, arg0) SBI_CALL(which, 0, arg0, 0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, 0, arg0, arg1, 0, 0)

static inline void sbi_console_putchar(int ch)
{
	SBI_CALL_1(SBI_CONSOLE_PUTCHAR, ch);
}

static inline int sbi_console_getchar(void)
{
	SBI_CALL_0(SBI_CONSOLE_GETCHAR);
	return res.a0;
}

static inline void sbi_set_timer(uint64_t stime_value)
{
#if __riscv_xlen == 32
	SBI_CALL_2(SBI_SET_TIMER, stime_value, stime_value >> 32);
#else
	SBI_CALL_1(SBI_SET_TIMER, stime_value);
#endif
}

static inline void sbi_shutdown(void)
{
	SBI_CALL_0(SBI_SHUTDOWN);
}

static inline void sbi_clear_ipi(void)
{
	SBI_CALL_0(SBI_CLEAR_IPI);
}

static inline void sbi_send_ipi(const unsigned long *hart_mask)
{
	SBI_CALL_1(SBI_SEND_IPI, hart_mask);
}

static inline void sbi_remote_fence_i(const unsigned long *hart_mask)
{
	SBI_CALL_1(SBI_REMOTE_FENCE_I, hart_mask);
}

static inline void sbi_remote_sfence_vma(const unsigned long *hart_mask,
					 unsigned long start,
					 unsigned long size)
{
	SBI_CALL_1(SBI_REMOTE_SFENCE_VMA, hart_mask);
}

static inline void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
					      unsigned long start,
					      unsigned long size,
					      unsigned long asid)
{
	SBI_CALL_1(SBI_REMOTE_SFENCE_VMA_ASID, hart_mask);
}

#endif
