/* DirectHW_arch.hpp - architecture abstraction for the DirectHW kext
 *
 * Copyright © 2008-2010 coresystems GmbH <info@coresystems.de>
 * arm64 support Copyright © 2026
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.
 *
 * ---------------------------------------------------------------------------
 * This header isolates every architecture-specific hardware primitive used by
 * DirectHW so that DirectHW.cpp itself stays architecture neutral.
 *
 * DirectHW was designed for x86, where the following instructions exist:
 *   - IN / OUT       : access to the separate I/O port address space
 *   - RDMSR / WRMSR  : dynamic (register number in ECX) model-specific regs
 *   - CPUID          : CPU feature identification
 *
 * None of these have a direct equivalent on AArch64:
 *   - There is NO I/O port address space at all. We reinterpret a "port"
 *     access as a physical MMIO access (IOMappedRead/Write). This is a
 *     deliberate semantic change: callers must pass a physical address, not
 *     an x86 port number.
 *   - MRS / MSR encode the system register in the instruction opcode, so an
 *     arbitrary register number cannot be selected at run time. We expose a
 *     *curated* table of well-known system registers, addressed by the same
 *     msrcmd_t.index field the x86 path uses for the MSR number.
 *   - CPU identification is done by reading MIDR_EL1 / ID_AA64*_EL1.
 * ---------------------------------------------------------------------------
 */

#ifndef __DIRECTHW_ARCH_HPP
#define __DIRECTHW_ARCH_HPP

#include <stdint.h>
#include <IOKit/IOLib.h>	/* IOMappedRead / IOMappedWrite accessors */

#if defined(__x86_64__) || defined(__i386__)
#define DIRECTHW_ARCH_X86 1
#elif defined(__arm64__) || defined(__aarch64__)
#define DIRECTHW_ARCH_ARM64 1
#else
#error "DirectHW: unsupported target architecture"
#endif

/*
 * arm64 system-register selectors.
 *
 * These values are placed in msrcmd_t.index (the x86 "MSR number" field) to
 * pick which AArch64 system register the curated table reads or writes. Keep
 * this list in sync with the copy in include/DirectHW.h.
 */
enum {
	DHW_ARM_MIDR_EL1 = 0,		/* Main ID Register                 (RO) */
	DHW_ARM_MPIDR_EL1,		/* Multiprocessor Affinity          (RO) */
	DHW_ARM_REVIDR_EL1,		/* Revision ID                      (RO) */
	DHW_ARM_ID_AA64PFR0_EL1,	/* Processor Feature 0              (RO) */
	DHW_ARM_ID_AA64PFR1_EL1,	/* Processor Feature 1              (RO) */
	DHW_ARM_ID_AA64ISAR0_EL1,	/* Instruction Set Attribute 0      (RO) */
	DHW_ARM_ID_AA64ISAR1_EL1,	/* Instruction Set Attribute 1      (RO) */
	DHW_ARM_ID_AA64MMFR0_EL1,	/* Memory Model Feature 0           (RO) */
	DHW_ARM_ID_AA64MMFR1_EL1,	/* Memory Model Feature 1           (RO) */
	DHW_ARM_ID_AA64DFR0_EL1,	/* Debug Feature 0                  (RO) */
	DHW_ARM_SCTLR_EL1,		/* System Control                   (RO) */
	DHW_ARM_TCR_EL1,		/* Translation Control              (RO) */
	DHW_ARM_TTBR0_EL1,		/* Translation Table Base 0         (RO) */
	DHW_ARM_TTBR1_EL1,		/* Translation Table Base 1         (RO) */
	DHW_ARM_MAIR_EL1,		/* Memory Attribute Indirection     (RO) */
	DHW_ARM_CNTFRQ_EL0,		/* Counter-timer Frequency          (RO) */
	DHW_ARM_CNTPCT_EL0,		/* Counter-timer Physical Count     (RO) */
	DHW_ARM_PMCR_EL0,		/* Performance Monitors Control     (RW) */
	DHW_ARM_PMCCNTR_EL0,		/* Cycle Counter                    (RW) */
	DHW_ARM_NUM_SYSREGS
};

namespace dhw {

/* ------------------------------------------------------------------ */
/* Port I/O (x86) / MMIO (arm64)                                       */
/* ------------------------------------------------------------------ */

/*
 * Read a 1/2/4-byte value. On x86 `addr` is an I/O port; on arm64 it is a
 * physical MMIO address. Returns true on success, false for an invalid width.
 */
static inline bool port_read(uint64_t addr, uint32_t width, uint32_t *out)
{
#if DIRECTHW_ARCH_X86
	uint16_t port = (uint16_t)addr;
	switch (width) {
	case 1: { uint8_t  d; __asm__ volatile("inb %w1, %b0" : "=a"(d) : "Nd"(port)); *out = d; return true; }
	case 2: { uint16_t d; __asm__ volatile("inw %w1, %w0" : "=a"(d) : "Nd"(port)); *out = d; return true; }
	case 4: { uint32_t d; __asm__ volatile("inl %w1, %0"  : "=a"(d) : "Nd"(port)); *out = d; return true; }
	}
	return false;
#else /* arm64: physical MMIO */
	/*
	 * Device memory does not tolerate unaligned access: an access that
	 * straddles the natural boundary of its width raises an alignment fault
	 * and panics the kernel. Require natural alignment and reject anything
	 * that is not a 1/2/4-byte access.
	 */
	if (width != 1 && width != 2 && width != 4)
		return false;
	if ((addr & (uint64_t)(width - 1)) != 0)
		return false;
	switch (width) {
	case 1: *out = IOMappedRead8((IOPhysicalAddress)addr);  return true;
	case 2: *out = IOMappedRead16((IOPhysicalAddress)addr); return true;
	case 4: *out = IOMappedRead32((IOPhysicalAddress)addr); return true;
	}
	return false;
#endif
}

static inline bool port_write(uint64_t addr, uint32_t width, uint32_t data)
{
#if DIRECTHW_ARCH_X86
	uint16_t port = (uint16_t)addr;
	switch (width) {
	case 1: __asm__ volatile("outb %b0, %w1" : : "a"((uint8_t)data),  "Nd"(port)); return true;
	case 2: __asm__ volatile("outw %w0, %w1" : : "a"((uint16_t)data), "Nd"(port)); return true;
	case 4: __asm__ volatile("outl %0, %w1"  : : "a"(data),           "Nd"(port)); return true;
	}
	return false;
#else /* arm64: physical MMIO */
	/* See port_read(): unaligned Device-memory access panics the kernel. */
	if (width != 1 && width != 2 && width != 4)
		return false;
	if ((addr & (uint64_t)(width - 1)) != 0)
		return false;
	switch (width) {
	case 1: IOMappedWrite8((IOPhysicalAddress)addr,  (UInt8)data);  return true;
	case 2: IOMappedWrite16((IOPhysicalAddress)addr, (UInt16)data); return true;
	case 4: IOMappedWrite32((IOPhysicalAddress)addr, (UInt32)data); return true;
	}
	return false;
#endif
}

/* ------------------------------------------------------------------ */
/* CPU identification: x86 CPUID / arm64 MIDR + ID_AA64* registers     */
/* ------------------------------------------------------------------ */

#if DIRECTHW_ARCH_ARM64
/* Defined below; used by cpu_identify() to reach the curated sysreg table. */
static inline bool sysreg_read(uint32_t index, uint32_t *lo, uint32_t *hi);
#endif

/*
 * Fill out[0..3] with CPU identification words.
 *  x86  : classic CPUID(op1 in EAX, op2 in ECX) -> {eax,ebx,ecx,edx}.
 *  arm64: op1 selects a register group. 0 => {MIDR_EL1, MPIDR_EL1};
 *         any other value is treated as a DHW_ARM_* selector and the chosen
 *         64-bit register is split lo/hi into out[0]/out[1] (out[2..3] = 0).
 *         An unknown selector yields all-zero output.
 */
static inline void cpu_identify(uint32_t op1, uint32_t op2, uint32_t out[4])
{
#if DIRECTHW_ARCH_X86
	__asm__ volatile("cpuid"
		: "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
		: "a"(op1), "c"(op2));
#else
	(void)op2;
	out[0] = out[1] = out[2] = out[3] = 0;

	if (op1 == 0) {
		/* Default identity: MIDR_EL1 (out[0:1]) + MPIDR_EL1 (out[2:3]). */
		uint64_t midr, mpidr;
		__asm__ volatile("mrs %0, MIDR_EL1"  : "=r"(midr));
		__asm__ volatile("mrs %0, MPIDR_EL1" : "=r"(mpidr));
		out[0] = (uint32_t)midr;
		out[1] = (uint32_t)(midr >> 32);
		out[2] = (uint32_t)mpidr;
		out[3] = (uint32_t)(mpidr >> 32);
		return;
	}

	/* Non-zero op1: treat as a DHW_ARM_* selector into the curated table. */
	uint32_t lo = 0, hi = 0;
	if (sysreg_read(op1, &lo, &hi)) {
		out[0] = lo;
		out[1] = hi;
	}
#endif
}

/* ------------------------------------------------------------------ */
/* MSR (x86) / system register (arm64) access                         */
/* ------------------------------------------------------------------ */

/*
 * Read a model-specific / system register.
 *  x86  : RDMSR of `index`.
 *  arm64: `index` is a DHW_ARM_* selector into the curated table.
 * Returns true on success. On arm64, an unknown selector returns false.
 */
static inline bool sysreg_read(uint32_t index, uint32_t *lo, uint32_t *hi)
{
#if DIRECTHW_ARCH_X86
	uint32_t l, h;
	__asm__ volatile("rdmsr" : "=a"(l), "=d"(h) : "c"(index));
	*lo = l; *hi = h;
	return true;
#else
	uint64_t v;
#define DHW_MRS(sel, reg) case sel: __asm__ volatile("mrs %0, " reg : "=r"(v)); break
	switch (index) {
	DHW_MRS(DHW_ARM_MIDR_EL1,       "MIDR_EL1");
	DHW_MRS(DHW_ARM_MPIDR_EL1,      "MPIDR_EL1");
	DHW_MRS(DHW_ARM_REVIDR_EL1,     "REVIDR_EL1");
	DHW_MRS(DHW_ARM_ID_AA64PFR0_EL1,  "ID_AA64PFR0_EL1");
	DHW_MRS(DHW_ARM_ID_AA64PFR1_EL1,  "ID_AA64PFR1_EL1");
	DHW_MRS(DHW_ARM_ID_AA64ISAR0_EL1, "ID_AA64ISAR0_EL1");
	DHW_MRS(DHW_ARM_ID_AA64ISAR1_EL1, "ID_AA64ISAR1_EL1");
	DHW_MRS(DHW_ARM_ID_AA64MMFR0_EL1, "ID_AA64MMFR0_EL1");
	DHW_MRS(DHW_ARM_ID_AA64MMFR1_EL1, "ID_AA64MMFR1_EL1");
	DHW_MRS(DHW_ARM_ID_AA64DFR0_EL1,  "ID_AA64DFR0_EL1");
	DHW_MRS(DHW_ARM_SCTLR_EL1,      "SCTLR_EL1");
	DHW_MRS(DHW_ARM_TCR_EL1,        "TCR_EL1");
	DHW_MRS(DHW_ARM_TTBR0_EL1,      "TTBR0_EL1");
	DHW_MRS(DHW_ARM_TTBR1_EL1,      "TTBR1_EL1");
	DHW_MRS(DHW_ARM_MAIR_EL1,       "MAIR_EL1");
	DHW_MRS(DHW_ARM_CNTFRQ_EL0,     "CNTFRQ_EL0");
	DHW_MRS(DHW_ARM_CNTPCT_EL0,     "CNTPCT_EL0");
	DHW_MRS(DHW_ARM_PMCR_EL0,       "PMCR_EL0");
	DHW_MRS(DHW_ARM_PMCCNTR_EL0,    "PMCCNTR_EL0");
	default:
		return false;
	}
#undef DHW_MRS
	*lo = (uint32_t)v;
	*hi = (uint32_t)(v >> 32);
	return true;
#endif
}

/*
 * Write a model-specific / system register.
 *  x86  : WRMSR of `index`.
 *  arm64: only the small writable subset of the curated table is accepted;
 *         everything else (read-only ID registers) returns false.
 */
static inline bool sysreg_write(uint32_t index, uint32_t lo, uint32_t hi)
{
#if DIRECTHW_ARCH_X86
	__asm__ volatile("wrmsr" : : "c"(index), "a"(lo), "d"(hi));
	return true;
#else
	uint64_t v = ((uint64_t)hi << 32) | lo;
#define DHW_MSR(sel, reg) case sel: __asm__ volatile("msr " reg ", %0" : : "r"(v)); break
	switch (index) {
	DHW_MSR(DHW_ARM_PMCR_EL0,    "PMCR_EL0");
	DHW_MSR(DHW_ARM_PMCCNTR_EL0, "PMCCNTR_EL0");
	default:
		return false;	/* read-only or unknown selector */
	}
#undef DHW_MSR
	return true;
#endif
}

/* ------------------------------------------------------------------ */
/* Control / system register debug dump                                */
/* ------------------------------------------------------------------ */

/* Log a few key control registers at driver start (debug aid only). */
static inline void dump_control_regs(void)
{
#if DIRECTHW_ARCH_X86
	uint64_t cr0 = 0, cr2 = 0, cr3 = 0;
	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	IOLog("DirectHW: cr0 = 0x%016llx\n", cr0);
	IOLog("DirectHW: cr2 = 0x%016llx\n", cr2);
	IOLog("DirectHW: cr3 = 0x%016llx\n", cr3);
#else
	uint64_t sctlr = 0, ttbr0 = 0, ttbr1 = 0;
	__asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr));
	__asm__ volatile("mrs %0, TTBR0_EL1" : "=r"(ttbr0));
	__asm__ volatile("mrs %0, TTBR1_EL1" : "=r"(ttbr1));
	IOLog("DirectHW: SCTLR_EL1 = 0x%016llx\n", sctlr);
	IOLog("DirectHW: TTBR0_EL1 = 0x%016llx\n", ttbr0);
	IOLog("DirectHW: TTBR1_EL1 = 0x%016llx\n", ttbr1);
#endif
}

/* ------------------------------------------------------------------ */
/* Physical memory read (used by ReadMem)                              */
/* ------------------------------------------------------------------ */

/* Read a 32-bit word from a physical address via the IOKit mapped accessor. */
static inline uint32_t phys_read32(uint64_t phys)
{
	return IOMappedRead32((IOPhysicalAddress)phys);
}

/* ------------------------------------------------------------------ */
/* Run a function on a logical CPU                                     */
/* ------------------------------------------------------------------ */
/*
 * x86 broadcasts with mp_rendezvous (exported via com.apple.kpi.unsupported)
 * and each callback self-filters on cpu_number(), so it runs exactly on the
 * requested core.
 *
 * arm64 has NO exported KPI to pin execution to a specific CPU: mp_rendezvous
 * is x86 only, and cpu_xcall / cpu_broadcast_xcall are private (not exported
 * to third-party kexts). We therefore run the callback on the current CPU and
 * cannot honor a specific core request. For P/E-core-specific registers the
 * result reflects whichever core the thread happened to run on.
 */

} /* namespace dhw */

extern "C" {
#if DIRECTHW_ARCH_X86
/* from osfmk/i386/mp.c */
extern void mp_rendezvous(void (*setup_func)(void *),
			  void (*action_func)(void *),
			  void (*teardown_func)(void *),
			  void *arg);
extern int cpu_number(void);
#endif
}

namespace dhw {

/*
 * True if the caller's per-core helper should proceed. On x86 the broadcast
 * runs on every CPU, so only the requested one proceeds. On arm64 there is no
 * broadcast (we already run on a single, uncontrollable CPU), so always proceed.
 */
static inline bool on_this_core(uint32_t requested)
{
#if DIRECTHW_ARCH_X86
	return (int)requested == cpu_number();
#else
	(void)requested;
	return true;
#endif
}

/* Dispatch `func(arg)` so that it executes on logical CPU `core` (best effort). */
static inline void dispatch_on_core(int core, void (*func)(void *), void *arg)
{
#if DIRECTHW_ARCH_X86
	(void)core;
	mp_rendezvous(NULL, func, NULL, arg);
#else
	(void)core;	/* cannot target a CPU on arm64; run on the current one */
	func(arg);
#endif
}

} /* namespace dhw */

#endif /* __DIRECTHW_ARCH_HPP */
