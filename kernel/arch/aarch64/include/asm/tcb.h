#ifndef __ASM_TCB_H__
#define __ASM_TCB_H__

#include <minos/types.h>

struct aarch64_regs {
	uint64_t pc;		// elr_el2
	uint64_t pstate;	// spsr_el2
	uint64_t sp;		// sp_el0
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t lr;
}__packed;

#define PT_REG_R0	3

typedef struct aarch64_regs gp_regs;

struct fpsimd_context {
	uint64_t regs[64] __align(16);
	uint32_t fpsr;
	uint32_t fpcr;
#ifdef CONFIG_VIRT
	uint32_t fpexc32_el2;
	uint32_t padding0;
#endif
};

struct cpu_context {
	uint64_t tpidr_el0;
	uint64_t tpidrro_el0;
	uint64_t ttbr_el0;

	struct fpsimd_context fpsimd_state;

	/*
	 * for cpu which has no VHE, the user task will run
	 * in EL0 and the kernel will run in EL2, use VMID to
	 * isolate the task
	 */
#if defined(CONFIG_VIRT) && !defined(CONFIG_ARM_VHE)
	uint64_t hcr_el2;
	uint64_t sctlr_el1;
	uint64_t cpacr_el1;
	uint64_t mdscr_el1;
	uint64_t cntvoff_el2;
	uint64_t cntkctl_el1;
	uint64_t cntv_ctl_el0;
	uint64_t vmpidr_el2;
	uint64_t vpidr_el2;
#endif
};

#endif
