/*
 * DirectHW.h - userspace part for DirectHW
 *
 * Copyright © 2008-2017 coresystems GmbH <info@coresystems.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __DIRECTHW_H
#define __DIRECTHW_H

#include <stdint.h>

int iopl(int unused);

unsigned char inb(unsigned short addr);
unsigned short inw(unsigned short addr);
unsigned int inl(unsigned short addr);

void outb(unsigned char val, unsigned short addr);
void outw(unsigned short val, unsigned short addr);
void outl(unsigned int val, unsigned short addr);

int readmem32(uint64_t addr, uint32_t* data);

void *map_physical(uint64_t phys_addr, size_t len);
void unmap_physical(void *virt_addr, size_t len);

typedef struct { uint32_t hi, lo; } msr_t;
msr_t rdmsr(int addr);
int wrmsr(int addr, msr_t msr);

/*
 * On arm64 there is no dynamic MSR instruction: MRS/MSR encode the system
 * register in the opcode, so the kext exposes a fixed set of registers. Pass
 * one of these selectors as the `addr` argument to rdmsr()/wrmsr() when running
 * on Apple Silicon. Keep this list in sync with kext/DirectHW_arch.hpp.
 *
 * Likewise, arm64 has no I/O port space: inb/outb & friends address physical
 * MMIO locations, not x86 I/O ports.
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
	DHW_ARM_CTR_EL0,		/* Cache Type Register              (RO) */
	DHW_ARM_DCZID_EL0,		/* Data Cache Zero ID               (RO) */
	DHW_ARM_CNTVCT_EL0,		/* Counter-timer Virtual Count      (RO) */
	DHW_ARM_NUM_SYSREGS
};
int logical_cpu_select(int cpu);
int rdcpuid(uint32_t eax, uint32_t ecx, uint32_t cpudata[4]);
int darwin_ioread(int pos, unsigned char * buf, int len);

#define INVALID_MSR_LO 0x63744857
#define INVALID_MSR_HI 0x44697265

#endif