/*
 * S/390 virtual CPU header
 *
 *  Copyright (c) 2009 Ulrich Hecht
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * Contributions after 2012-10-29 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 *
 * You should have received a copy of the GNU (Lesser) General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CPU_S390X_H
#define CPU_S390X_H

#include "config.h"
#include "qemu-common.h"

#define TARGET_LONG_BITS 64

#define ELF_MACHINE_UNAME "S390X"

#define CPUArchState struct CPUS390XState

#include "exec/cpu-defs.h"
#define TARGET_PAGE_BITS 12

#define TARGET_PHYS_ADDR_SPACE_BITS 64
#define TARGET_VIRT_ADDR_SPACE_BITS 64

#include "exec/cpu-all.h"

#include "fpu/softfloat.h"

#define NB_MMU_MODES 3
#define TARGET_INSN_START_EXTRA_WORDS 1

#define MMU_MODE0_SUFFIX _primary
#define MMU_MODE1_SUFFIX _secondary
#define MMU_MODE2_SUFFIX _home

#define MMU_USER_IDX 0

#define MAX_EXT_QUEUE 16
#define MAX_IO_QUEUE 16
#define MAX_MCHK_QUEUE 16

#define PSW_MCHK_MASK 0x0004000000000000
#define PSW_IO_MASK 0x0200000000000000

typedef struct PSW {
    uint64_t mask;
    uint64_t addr;
} PSW;

typedef struct ExtQueue {
    uint32_t code;
    uint32_t param;
    uint32_t param64;
} ExtQueue;

typedef struct IOIntQueue {
    uint16_t id;
    uint16_t nr;
    uint32_t parm;
    uint32_t word;
} IOIntQueue;

typedef struct MchkQueue {
    uint16_t type;
} MchkQueue;

typedef struct CPUS390XState {
    uint64_t regs[16];     /* GP registers */
    /*
     * The floating point registers are part of the vector registers.
     * vregs[0][0] -> vregs[15][0] are 16 floating point registers
     */
    CPU_DoubleU vregs[32][2];  /* vector registers */
    uint32_t aregs[16];    /* access registers */

    uint32_t fpc;          /* floating-point control register */
    uint32_t cc_op;

    float_status fpu_status; /* passed to softfloat lib */

    /* The low part of a 128-bit return, or remainder of a divide.  */
    uint64_t retxl;

    PSW psw;

    uint64_t cc_src;
    uint64_t cc_dst;
    uint64_t cc_vr;

    uint64_t __excp_addr;
    uint64_t psa;

    uint32_t int_pgm_code;
    uint32_t int_pgm_ilen;

    uint32_t int_svc_code;
    uint32_t int_svc_ilen;

    uint64_t per_address;
    uint16_t per_perc_atmid;

    uint64_t cregs[16]; /* control registers */

    ExtQueue ext_queue[MAX_EXT_QUEUE];
    IOIntQueue io_queue[MAX_IO_QUEUE][8];
    MchkQueue mchk_queue[MAX_MCHK_QUEUE];

    int pending_int;
    int ext_index;
    int io_index[8];
    int mchk_index;

    uint64_t ckc;
    uint64_t cputm;
    uint32_t todpr;

    uint64_t pfault_token;
    uint64_t pfault_compare;
    uint64_t pfault_select;

    uint64_t gbea;
    uint64_t pp;

    CPU_COMMON

    /* reset does memset(0) up to here */

    uint32_t cpu_num;
    uint32_t machine_type;

    uint64_t tod_offset;
    uint64_t tod_basetime;
    QEMUTimer *tod_timer;

    QEMUTimer *cpu_timer;

    /*
     * The cpu state represents the logical state of a cpu. In contrast to other
     * architectures, there is a difference between a halt and a stop on s390.
     * If all cpus are either stopped (including check stop) or in the disabled
     * wait state, the vm can be shut down.
     */
#define CPU_STATE_UNINITIALIZED        0x00
#define CPU_STATE_STOPPED              0x01
#define CPU_STATE_CHECK_STOP           0x02
#define CPU_STATE_OPERATING            0x03
#define CPU_STATE_LOAD                 0x04
    uint8_t cpu_state;

    /* currently processed sigp order */
    uint8_t sigp_order;

} CPUS390XState;

static inline CPU_DoubleU *get_freg(CPUS390XState *cs, int nr)
{
    return &cs->vregs[nr][0];
}

#include "cpu-qom.h"
#include <sysemu/kvm.h>

/* distinguish between 24 bit and 31 bit addressing */
#define HIGH_ORDER_BIT 0x80000000

/* Interrupt Codes */
/* Program Interrupts */
#define PGM_OPERATION                   0x0001
#define PGM_PRIVILEGED                  0x0002
#define PGM_EXECUTE                     0x0003
#define PGM_PROTECTION                  0x0004
#define PGM_ADDRESSING                  0x0005
#define PGM_SPECIFICATION               0x0006
#define PGM_DATA                        0x0007
#define PGM_FIXPT_OVERFLOW              0x0008
#define PGM_FIXPT_DIVIDE                0x0009
#define PGM_DEC_OVERFLOW                0x000a
#define PGM_DEC_DIVIDE                  0x000b
#define PGM_HFP_EXP_OVERFLOW            0x000c
#define PGM_HFP_EXP_UNDERFLOW           0x000d
#define PGM_HFP_SIGNIFICANCE            0x000e
#define PGM_HFP_DIVIDE                  0x000f
#define PGM_SEGMENT_TRANS               0x0010
#define PGM_PAGE_TRANS                  0x0011
#define PGM_TRANS_SPEC                  0x0012
#define PGM_SPECIAL_OP                  0x0013
#define PGM_OPERAND                     0x0015
#define PGM_TRACE_TABLE                 0x0016
#define PGM_SPACE_SWITCH                0x001c
#define PGM_HFP_SQRT                    0x001d
#define PGM_PC_TRANS_SPEC               0x001f
#define PGM_AFX_TRANS                   0x0020
#define PGM_ASX_TRANS                   0x0021
#define PGM_LX_TRANS                    0x0022
#define PGM_EX_TRANS                    0x0023
#define PGM_PRIM_AUTH                   0x0024
#define PGM_SEC_AUTH                    0x0025
#define PGM_ALET_SPEC                   0x0028
#define PGM_ALEN_SPEC                   0x0029
#define PGM_ALE_SEQ                     0x002a
#define PGM_ASTE_VALID                  0x002b
#define PGM_ASTE_SEQ                    0x002c
#define PGM_EXT_AUTH                    0x002d
#define PGM_STACK_FULL                  0x0030
#define PGM_STACK_EMPTY                 0x0031
#define PGM_STACK_SPEC                  0x0032
#define PGM_STACK_TYPE                  0x0033
#define PGM_STACK_OP                    0x0034
#define PGM_ASCE_TYPE                   0x0038
#define PGM_REG_FIRST_TRANS             0x0039
#define PGM_REG_SEC_TRANS               0x003a
#define PGM_REG_THIRD_TRANS             0x003b
#define PGM_MONITOR                     0x0040
#define PGM_PER                         0x0080
#define PGM_CRYPTO                      0x0119

/* External Interrupts */
#define EXT_INTERRUPT_KEY               0x0040
#define EXT_CLOCK_COMP                  0x1004
#define EXT_CPU_TIMER                   0x1005
#define EXT_MALFUNCTION                 0x1200
#define EXT_EMERGENCY                   0x1201
#define EXT_EXTERNAL_CALL               0x1202
#define EXT_ETR                         0x1406
#define EXT_SERVICE                     0x2401
#define EXT_VIRTIO                      0x2603

/* PSW defines */
#undef PSW_MASK_PER
#undef PSW_MASK_DAT
#undef PSW_MASK_IO
#undef PSW_MASK_EXT
#undef PSW_MASK_KEY
#undef PSW_SHIFT_KEY
#undef PSW_MASK_MCHECK
#undef PSW_MASK_WAIT
#undef PSW_MASK_PSTATE
#undef PSW_MASK_ASC
#undef PSW_MASK_CC
#undef PSW_MASK_PM
#undef PSW_MASK_64
#undef PSW_MASK_32
#undef PSW_MASK_ESA_ADDR

#define PSW_MASK_PER            0x4000000000000000ULL
#define PSW_MASK_DAT            0x0400000000000000ULL
#define PSW_MASK_IO             0x0200000000000000ULL
#define PSW_MASK_EXT            0x0100000000000000ULL
#define PSW_MASK_KEY            0x00F0000000000000ULL
#define PSW_SHIFT_KEY           56
#define PSW_MASK_MCHECK         0x0004000000000000ULL
#define PSW_MASK_WAIT           0x0002000000000000ULL
#define PSW_MASK_PSTATE         0x0001000000000000ULL
#define PSW_MASK_ASC            0x0000C00000000000ULL
#define PSW_MASK_CC             0x0000300000000000ULL
#define PSW_MASK_PM             0x00000F0000000000ULL
#define PSW_MASK_64             0x0000000100000000ULL
#define PSW_MASK_32             0x0000000080000000ULL
#define PSW_MASK_ESA_ADDR       0x000000007fffffffULL

#undef PSW_ASC_PRIMARY
#undef PSW_ASC_ACCREG
#undef PSW_ASC_SECONDARY
#undef PSW_ASC_HOME

#define PSW_ASC_PRIMARY         0x0000000000000000ULL
#define PSW_ASC_ACCREG          0x0000400000000000ULL
#define PSW_ASC_SECONDARY       0x0000800000000000ULL
#define PSW_ASC_HOME            0x0000C00000000000ULL

/* tb flags */

#define FLAG_MASK_PER           (PSW_MASK_PER    >> 32)
#define FLAG_MASK_DAT           (PSW_MASK_DAT    >> 32)
#define FLAG_MASK_IO            (PSW_MASK_IO     >> 32)
#define FLAG_MASK_EXT           (PSW_MASK_EXT    >> 32)
#define FLAG_MASK_KEY           (PSW_MASK_KEY    >> 32)
#define FLAG_MASK_MCHECK        (PSW_MASK_MCHECK >> 32)
#define FLAG_MASK_WAIT          (PSW_MASK_WAIT   >> 32)
#define FLAG_MASK_PSTATE        (PSW_MASK_PSTATE >> 32)
#define FLAG_MASK_ASC           (PSW_MASK_ASC    >> 32)
#define FLAG_MASK_CC            (PSW_MASK_CC     >> 32)
#define FLAG_MASK_PM            (PSW_MASK_PM     >> 32)
#define FLAG_MASK_64            (PSW_MASK_64     >> 32)
#define FLAG_MASK_32            0x00001000

/* Control register 0 bits */
#define CR0_LOWPROT             0x0000000010000000ULL
#define CR0_EDAT                0x0000000000800000ULL

/* MMU */
#define MMU_PRIMARY_IDX         0
#define MMU_SECONDARY_IDX       1
#define MMU_HOME_IDX            2

static inline int cpu_mmu_index (CPUS390XState *env, bool ifetch)
{
    switch (env->psw.mask & PSW_MASK_ASC) {
    case PSW_ASC_PRIMARY:
        return MMU_PRIMARY_IDX;
    case PSW_ASC_SECONDARY:
        return MMU_SECONDARY_IDX;
    case PSW_ASC_HOME:
        return MMU_HOME_IDX;
    case PSW_ASC_ACCREG:
        /* Fallthrough: access register mode is not yet supported */
    default:
        abort();
    }
}

static inline uint64_t cpu_mmu_idx_to_asc(int mmu_idx)
{
    switch (mmu_idx) {
    case MMU_PRIMARY_IDX:
        return PSW_ASC_PRIMARY;
    case MMU_SECONDARY_IDX:
        return PSW_ASC_SECONDARY;
    case MMU_HOME_IDX:
        return PSW_ASC_HOME;
    default:
        abort();
    }
}

static inline void cpu_get_tb_cpu_state(CPUS390XState* env, target_ulong *pc,
                                        target_ulong *cs_base, int *flags)
{
    *pc = env->psw.addr;
    *cs_base = 0;
    *flags = ((env->psw.mask >> 32) & ~FLAG_MASK_CC) |
             ((env->psw.mask & PSW_MASK_32) ? FLAG_MASK_32 : 0);
}

/* While the PoO talks about ILC (a number between 1-3) what is actually
   stored in LowCore is shifted left one bit (an even between 2-6).  As
   this is the actual length of the insn and therefore more useful, that
   is what we want to pass around and manipulate.  To make sure that we
   have applied this distinction universally, rename the "ILC" to "ILEN".  */
static inline int get_ilen(uint8_t opc)
{
    switch (opc >> 6) {
    case 0:
        return 2;
    case 1:
    case 2:
        return 4;
    default:
        return 6;
    }
}

/* PER bits from control register 9 */
#define PER_CR9_EVENT_BRANCH           0x80000000
#define PER_CR9_EVENT_IFETCH           0x40000000
#define PER_CR9_EVENT_STORE            0x20000000
#define PER_CR9_EVENT_STORE_REAL       0x08000000
#define PER_CR9_EVENT_NULLIFICATION    0x01000000
#define PER_CR9_CONTROL_BRANCH_ADDRESS 0x00800000
#define PER_CR9_CONTROL_ALTERATION     0x00200000

/* PER bits from the PER CODE/ATMID/AI in lowcore */
#define PER_CODE_EVENT_BRANCH          0x8000
#define PER_CODE_EVENT_IFETCH          0x4000
#define PER_CODE_EVENT_STORE           0x2000
#define PER_CODE_EVENT_STORE_REAL      0x0800
#define PER_CODE_EVENT_NULLIFICATION   0x0100

/* Compute the ATMID field that is stored in the per_perc_atmid lowcore
   entry when a PER exception is triggered.  */
static inline uint8_t get_per_atmid(CPUS390XState *env)
{
    return ((env->psw.mask & PSW_MASK_64) ?      (1 << 7) : 0) |
           (                                     (1 << 6)    ) |
           ((env->psw.mask & PSW_MASK_32) ?      (1 << 5) : 0) |
           ((env->psw.mask & PSW_MASK_DAT)?      (1 << 4) : 0) |
           ((env->psw.mask & PSW_ASC_SECONDARY)? (1 << 3) : 0) |
           ((env->psw.mask & PSW_ASC_ACCREG)?    (1 << 2) : 0);
}

/* Check if an address is within the PER starting address and the PER
   ending address.  The address range might loop.  */
static inline bool get_per_in_range(CPUS390XState *env, uint64_t addr)
{
    if (env->cregs[10] <= env->cregs[11]) {
        return env->cregs[10] <= addr && addr <= env->cregs[11];
    } else {
        return env->cregs[10] <= addr || addr <= env->cregs[11];
    }
}

#ifndef CONFIG_USER_ONLY
/* In several cases of runtime exceptions, we havn't recorded the true
   instruction length.  Use these codes when raising exceptions in order
   to re-compute the length by examining the insn in memory.  */
#define ILEN_LATER       0x20
#define ILEN_LATER_INC   0x21
void trigger_pgm_exception(CPUS390XState *env, uint32_t code, uint32_t ilen);
#endif

S390CPU *cpu_s390x_init(const char *cpu_model);
void s390x_translate_init(void);
int cpu_s390x_exec(CPUState *cpu);

/* you can call this signal handler from your SIGBUS and SIGSEGV
   signal handlers to inform the virtual CPU of exceptions. non zero
   is returned if the signal was handled by the virtual CPU.  */
int cpu_s390x_signal_handler(int host_signum, void *pinfo,
                           void *puc);
int s390_cpu_handle_mmu_fault(CPUState *cpu, vaddr address, int rw,
                              int mmu_idx);

#include "ioinst.h"


#ifndef CONFIG_USER_ONLY
void do_restart_interrupt(CPUS390XState *env);

static inline hwaddr decode_basedisp_s(CPUS390XState *env, uint32_t ipb,
                                       uint8_t *ar)
{
    hwaddr addr = 0;
    uint8_t reg;

    reg = ipb >> 28;
    if (reg > 0) {
        addr = env->regs[reg];
    }
    addr += (ipb >> 16) & 0xfff;
    if (ar) {
        *ar = reg;
    }

    return addr;
}

/* Base/displacement are at the same locations. */
#define decode_basedisp_rs decode_basedisp_s

/* helper functions for run_on_cpu() */
static inline void s390_do_cpu_reset(void *arg)
{
    CPUState *cs = arg;
    S390CPUClass *scc = S390_CPU_GET_CLASS(cs);

    scc->cpu_reset(cs);
}
static inline void s390_do_cpu_full_reset(void *arg)
{
    CPUState *cs = arg;

    cpu_reset(cs);
}

void s390x_tod_timer(void *opaque);
void s390x_cpu_timer(void *opaque);

int s390_virtio_hypercall(CPUS390XState *env);
void s390_virtio_irq(int config_change, uint64_t token);

#ifdef CONFIG_KVM
void kvm_s390_virtio_irq(int config_change, uint64_t token);
void kvm_s390_service_interrupt(uint32_t parm);
void kvm_s390_vcpu_interrupt(S390CPU *cpu, struct kvm_s390_irq *irq);
void kvm_s390_floating_interrupt(struct kvm_s390_irq *irq);
int kvm_s390_inject_flic(struct kvm_s390_irq *irq);
void kvm_s390_access_exception(S390CPU *cpu, uint16_t code, uint64_t te_code);
int kvm_s390_mem_op(S390CPU *cpu, vaddr addr, uint8_t ar, void *hostbuf,
                    int len, bool is_write);
int kvm_s390_get_clock(uint8_t *tod_high, uint64_t *tod_clock);
int kvm_s390_set_clock(uint8_t *tod_high, uint64_t *tod_clock);
#else
static inline void kvm_s390_virtio_irq(int config_change, uint64_t token)
{
}
static inline void kvm_s390_service_interrupt(uint32_t parm)
{
}
static inline int kvm_s390_get_clock(uint8_t *tod_high, uint64_t *tod_low)
{
    return -ENOSYS;
}
static inline int kvm_s390_set_clock(uint8_t *tod_high, uint64_t *tod_low)
{
    return -ENOSYS;
}
static inline int kvm_s390_mem_op(S390CPU *cpu, vaddr addr, uint8_t ar,
                                  void *hostbuf, int len, bool is_write)
{
    return -ENOSYS;
}
static inline void kvm_s390_access_exception(S390CPU *cpu, uint16_t code,
                                             uint64_t te_code)
{
}
#endif

static inline int s390_get_clock(uint8_t *tod_high, uint64_t *tod_low)
{
    if (kvm_enabled()) {
        return kvm_s390_get_clock(tod_high, tod_low);
    }
    /* Fixme TCG */
    *tod_high = 0;
    *tod_low = 0;
    return 0;
}

static inline int s390_set_clock(uint8_t *tod_high, uint64_t *tod_low)
{
    if (kvm_enabled()) {
        return kvm_s390_set_clock(tod_high, tod_low);
    }
    /* Fixme TCG */
    return 0;
}

S390CPU *s390_cpu_addr2state(uint16_t cpu_addr);
unsigned int s390_cpu_halt(S390CPU *cpu);
void s390_cpu_unhalt(S390CPU *cpu);
unsigned int s390_cpu_set_state(uint8_t cpu_state, S390CPU *cpu);
static inline uint8_t s390_cpu_get_state(S390CPU *cpu)
{
    return cpu->env.cpu_state;
}

void gtod_save(QEMUFile *f, void *opaque);
int gtod_load(QEMUFile *f, void *opaque, int version_id);

/* service interrupts are floating therefore we must not pass an cpustate */
void s390_sclp_extint(uint32_t parm);

#else
static inline unsigned int s390_cpu_halt(S390CPU *cpu)
{
    return 0;
}

static inline void s390_cpu_unhalt(S390CPU *cpu)
{
}

static inline unsigned int s390_cpu_set_state(uint8_t cpu_state, S390CPU *cpu)
{
    return 0;
}
#endif
void cpu_lock(void);
void cpu_unlock(void);

typedef struct SubchDev SubchDev;

#ifndef CONFIG_USER_ONLY
extern void subsystem_reset(void);
SubchDev *css_find_subch(uint8_t m, uint8_t cssid, uint8_t ssid,
                         uint16_t schid);
bool css_subch_visible(SubchDev *sch);
void css_conditional_io_interrupt(SubchDev *sch);
int css_do_stsch(SubchDev *sch, SCHIB *schib);
bool css_schid_final(int m, uint8_t cssid, uint8_t ssid, uint16_t schid);
int css_do_msch(SubchDev *sch, const SCHIB *schib);
int css_do_xsch(SubchDev *sch);
int css_do_csch(SubchDev *sch);
int css_do_hsch(SubchDev *sch);
int css_do_ssch(SubchDev *sch, ORB *orb);
int css_do_tsch_get_irb(SubchDev *sch, IRB *irb, int *irb_len);
void css_do_tsch_update_subch(SubchDev *sch);
int css_do_stcrw(CRW *crw);
void css_undo_stcrw(CRW *crw);
int css_do_tpi(IOIntCode *int_code, int lowcore);
int css_collect_chp_desc(int m, uint8_t cssid, uint8_t f_chpid, uint8_t l_chpid,
                         int rfmt, void *buf);
void css_do_schm(uint8_t mbk, int update, int dct, uint64_t mbo);
int css_enable_mcsse(void);
int css_enable_mss(void);
int css_do_rsch(SubchDev *sch);
int css_do_rchp(uint8_t cssid, uint8_t chpid);
bool css_present(uint8_t cssid);
#endif

#define cpu_init(model) CPU(cpu_s390x_init(model))
#define cpu_exec cpu_s390x_exec
#define cpu_signal_handler cpu_s390x_signal_handler

void s390_cpu_list(FILE *f, fprintf_function cpu_fprintf);
#define cpu_list s390_cpu_list

#include "exec/exec-all.h"

#define EXCP_EXT 1 /* external interrupt */
#define EXCP_SVC 2 /* supervisor call (syscall) */
#define EXCP_PGM 3 /* program interruption */
#define EXCP_IO  7 /* I/O interrupt */
#define EXCP_MCHK 8 /* machine check */

#define INTERRUPT_EXT        (1 << 0)
#define INTERRUPT_TOD        (1 << 1)
#define INTERRUPT_CPUTIMER   (1 << 2)
#define INTERRUPT_IO         (1 << 3)
#define INTERRUPT_MCHK       (1 << 4)

/* Program Status Word.  */
#define S390_PSWM_REGNUM 0
#define S390_PSWA_REGNUM 1
/* General Purpose Registers.  */
#define S390_R0_REGNUM 2
#define S390_R1_REGNUM 3
#define S390_R2_REGNUM 4
#define S390_R3_REGNUM 5
#define S390_R4_REGNUM 6
#define S390_R5_REGNUM 7
#define S390_R6_REGNUM 8
#define S390_R7_REGNUM 9
#define S390_R8_REGNUM 10
#define S390_R9_REGNUM 11
#define S390_R10_REGNUM 12
#define S390_R11_REGNUM 13
#define S390_R12_REGNUM 14
#define S390_R13_REGNUM 15
#define S390_R14_REGNUM 16
#define S390_R15_REGNUM 17
/* Total Core Registers. */
#define S390_NUM_CORE_REGS 18

/* CC optimization */

enum cc_op {
    CC_OP_CONST0 = 0,           /* CC is 0 */
    CC_OP_CONST1,               /* CC is 1 */
    CC_OP_CONST2,               /* CC is 2 */
    CC_OP_CONST3,               /* CC is 3 */

    CC_OP_DYNAMIC,              /* CC calculation defined by env->cc_op */
    CC_OP_STATIC,               /* CC value is env->cc_op */

    CC_OP_NZ,                   /* env->cc_dst != 0 */
    CC_OP_LTGT_32,              /* signed less/greater than (32bit) */
    CC_OP_LTGT_64,              /* signed less/greater than (64bit) */
    CC_OP_LTUGTU_32,            /* unsigned less/greater than (32bit) */
    CC_OP_LTUGTU_64,            /* unsigned less/greater than (64bit) */
    CC_OP_LTGT0_32,             /* signed less/greater than 0 (32bit) */
    CC_OP_LTGT0_64,             /* signed less/greater than 0 (64bit) */

    CC_OP_ADD_64,               /* overflow on add (64bit) */
    CC_OP_ADDU_64,              /* overflow on unsigned add (64bit) */
    CC_OP_ADDC_64,              /* overflow on unsigned add-carry (64bit) */
    CC_OP_SUB_64,               /* overflow on subtraction (64bit) */
    CC_OP_SUBU_64,              /* overflow on unsigned subtraction (64bit) */
    CC_OP_SUBB_64,              /* overflow on unsigned sub-borrow (64bit) */
    CC_OP_ABS_64,               /* sign eval on abs (64bit) */
    CC_OP_NABS_64,              /* sign eval on nabs (64bit) */

    CC_OP_ADD_32,               /* overflow on add (32bit) */
    CC_OP_ADDU_32,              /* overflow on unsigned add (32bit) */
    CC_OP_ADDC_32,              /* overflow on unsigned add-carry (32bit) */
    CC_OP_SUB_32,               /* overflow on subtraction (32bit) */
    CC_OP_SUBU_32,              /* overflow on unsigned subtraction (32bit) */
    CC_OP_SUBB_32,              /* overflow on unsigned sub-borrow (32bit) */
    CC_OP_ABS_32,               /* sign eval on abs (64bit) */
    CC_OP_NABS_32,              /* sign eval on nabs (64bit) */

    CC_OP_COMP_32,              /* complement */
    CC_OP_COMP_64,              /* complement */

    CC_OP_TM_32,                /* test under mask (32bit) */
    CC_OP_TM_64,                /* test under mask (64bit) */

    CC_OP_NZ_F32,               /* FP dst != 0 (32bit) */
    CC_OP_NZ_F64,               /* FP dst != 0 (64bit) */
    CC_OP_NZ_F128,              /* FP dst != 0 (128bit) */

    CC_OP_ICM,                  /* insert characters under mask */
    CC_OP_SLA_32,               /* Calculate shift left signed (32bit) */
    CC_OP_SLA_64,               /* Calculate shift left signed (64bit) */
    CC_OP_FLOGR,                /* find leftmost one */
    CC_OP_MAX
};

static const char *cc_names[] = {
    [CC_OP_CONST0]    = "CC_OP_CONST0",
    [CC_OP_CONST1]    = "CC_OP_CONST1",
    [CC_OP_CONST2]    = "CC_OP_CONST2",
    [CC_OP_CONST3]    = "CC_OP_CONST3",
    [CC_OP_DYNAMIC]   = "CC_OP_DYNAMIC",
    [CC_OP_STATIC]    = "CC_OP_STATIC",
    [CC_OP_NZ]        = "CC_OP_NZ",
    [CC_OP_LTGT_32]   = "CC_OP_LTGT_32",
    [CC_OP_LTGT_64]   = "CC_OP_LTGT_64",
    [CC_OP_LTUGTU_32] = "CC_OP_LTUGTU_32",
    [CC_OP_LTUGTU_64] = "CC_OP_LTUGTU_64",
    [CC_OP_LTGT0_32]  = "CC_OP_LTGT0_32",
    [CC_OP_LTGT0_64]  = "CC_OP_LTGT0_64",
    [CC_OP_ADD_64]    = "CC_OP_ADD_64",
    [CC_OP_ADDU_64]   = "CC_OP_ADDU_64",
    [CC_OP_ADDC_64]   = "CC_OP_ADDC_64",
    [CC_OP_SUB_64]    = "CC_OP_SUB_64",
    [CC_OP_SUBU_64]   = "CC_OP_SUBU_64",
    [CC_OP_SUBB_64]   = "CC_OP_SUBB_64",
    [CC_OP_ABS_64]    = "CC_OP_ABS_64",
    [CC_OP_NABS_64]   = "CC_OP_NABS_64",
    [CC_OP_ADD_32]    = "CC_OP_ADD_32",
    [CC_OP_ADDU_32]   = "CC_OP_ADDU_32",
    [CC_OP_ADDC_32]   = "CC_OP_ADDC_32",
    [CC_OP_SUB_32]    = "CC_OP_SUB_32",
    [CC_OP_SUBU_32]   = "CC_OP_SUBU_32",
    [CC_OP_SUBB_32]   = "CC_OP_SUBB_32",
    [CC_OP_ABS_32]    = "CC_OP_ABS_32",
    [CC_OP_NABS_32]   = "CC_OP_NABS_32",
    [CC_OP_COMP_32]   = "CC_OP_COMP_32",
    [CC_OP_COMP_64]   = "CC_OP_COMP_64",
    [CC_OP_TM_32]     = "CC_OP_TM_32",
    [CC_OP_TM_64]     = "CC_OP_TM_64",
    [CC_OP_NZ_F32]    = "CC_OP_NZ_F32",
    [CC_OP_NZ_F64]    = "CC_OP_NZ_F64",
    [CC_OP_NZ_F128]   = "CC_OP_NZ_F128",
    [CC_OP_ICM]       = "CC_OP_ICM",
    [CC_OP_SLA_32]    = "CC_OP_SLA_32",
    [CC_OP_SLA_64]    = "CC_OP_SLA_64",
    [CC_OP_FLOGR]     = "CC_OP_FLOGR",
};

static inline const char *cc_name(int cc_op)
{
    return cc_names[cc_op];
}

static inline void setcc(S390CPU *cpu, uint64_t cc)
{
    CPUS390XState *env = &cpu->env;

    env->psw.mask &= ~(3ull << 44);
    env->psw.mask |= (cc & 3) << 44;
    env->cc_op = cc;
}

typedef struct LowCore
{
    /* prefix area: defined by architecture */
    uint32_t        ccw1[2];                  /* 0x000 */
    uint32_t        ccw2[4];                  /* 0x008 */
    uint8_t         pad1[0x80-0x18];          /* 0x018 */
    uint32_t        ext_params;               /* 0x080 */
    uint16_t        cpu_addr;                 /* 0x084 */
    uint16_t        ext_int_code;             /* 0x086 */
    uint16_t        svc_ilen;                 /* 0x088 */
    uint16_t        svc_code;                 /* 0x08a */
    uint16_t        pgm_ilen;                 /* 0x08c */
    uint16_t        pgm_code;                 /* 0x08e */
    uint32_t        data_exc_code;            /* 0x090 */
    uint16_t        mon_class_num;            /* 0x094 */
    uint16_t        per_perc_atmid;           /* 0x096 */
    uint64_t        per_address;              /* 0x098 */
    uint8_t         exc_access_id;            /* 0x0a0 */
    uint8_t         per_access_id;            /* 0x0a1 */
    uint8_t         op_access_id;             /* 0x0a2 */
    uint8_t         ar_access_id;             /* 0x0a3 */
    uint8_t         pad2[0xA8-0xA4];          /* 0x0a4 */
    uint64_t        trans_exc_code;           /* 0x0a8 */
    uint64_t        monitor_code;             /* 0x0b0 */
    uint16_t        subchannel_id;            /* 0x0b8 */
    uint16_t        subchannel_nr;            /* 0x0ba */
    uint32_t        io_int_parm;              /* 0x0bc */
    uint32_t        io_int_word;              /* 0x0c0 */
    uint8_t         pad3[0xc8-0xc4];          /* 0x0c4 */
    uint32_t        stfl_fac_list;            /* 0x0c8 */
    uint8_t         pad4[0xe8-0xcc];          /* 0x0cc */
    uint32_t        mcck_interruption_code[2]; /* 0x0e8 */
    uint8_t         pad5[0xf4-0xf0];          /* 0x0f0 */
    uint32_t        external_damage_code;     /* 0x0f4 */
    uint64_t        failing_storage_address;  /* 0x0f8 */
    uint8_t         pad6[0x110-0x100];        /* 0x100 */
    uint64_t        per_breaking_event_addr;  /* 0x110 */
    uint8_t         pad7[0x120-0x118];        /* 0x118 */
    PSW             restart_old_psw;          /* 0x120 */
    PSW             external_old_psw;         /* 0x130 */
    PSW             svc_old_psw;              /* 0x140 */
    PSW             program_old_psw;          /* 0x150 */
    PSW             mcck_old_psw;             /* 0x160 */
    PSW             io_old_psw;               /* 0x170 */
    uint8_t         pad8[0x1a0-0x180];        /* 0x180 */
    PSW             restart_new_psw;          /* 0x1a0 */
    PSW             external_new_psw;         /* 0x1b0 */
    PSW             svc_new_psw;              /* 0x1c0 */
    PSW             program_new_psw;          /* 0x1d0 */
    PSW             mcck_new_psw;             /* 0x1e0 */
    PSW             io_new_psw;               /* 0x1f0 */
    PSW             return_psw;               /* 0x200 */
    uint8_t         irb[64];                  /* 0x210 */
    uint64_t        sync_enter_timer;         /* 0x250 */
    uint64_t        async_enter_timer;        /* 0x258 */
    uint64_t        exit_timer;               /* 0x260 */
    uint64_t        last_update_timer;        /* 0x268 */
    uint64_t        user_timer;               /* 0x270 */
    uint64_t        system_timer;             /* 0x278 */
    uint64_t        last_update_clock;        /* 0x280 */
    uint64_t        steal_clock;              /* 0x288 */
    PSW             return_mcck_psw;          /* 0x290 */
    uint8_t         pad9[0xc00-0x2a0];        /* 0x2a0 */
    /* System info area */
    uint64_t        save_area[16];            /* 0xc00 */
    uint8_t         pad10[0xd40-0xc80];       /* 0xc80 */
    uint64_t        kernel_stack;             /* 0xd40 */
    uint64_t        thread_info;              /* 0xd48 */
    uint64_t        async_stack;              /* 0xd50 */
    uint64_t        kernel_asce;              /* 0xd58 */
    uint64_t        user_asce;                /* 0xd60 */
    uint64_t        panic_stack;              /* 0xd68 */
    uint64_t        user_exec_asce;           /* 0xd70 */
    uint8_t         pad11[0xdc0-0xd78];       /* 0xd78 */

    /* SMP info area: defined by DJB */
    uint64_t        clock_comparator;         /* 0xdc0 */
    uint64_t        ext_call_fast;            /* 0xdc8 */
    uint64_t        percpu_offset;            /* 0xdd0 */
    uint64_t        current_task;             /* 0xdd8 */
    uint32_t        softirq_pending;          /* 0xde0 */
    uint32_t        pad_0x0de4;               /* 0xde4 */
    uint64_t        int_clock;                /* 0xde8 */
    uint8_t         pad12[0xe00-0xdf0];       /* 0xdf0 */

    /* 0xe00 is used as indicator for dump tools */
    /* whether the kernel died with panic() or not */
    uint32_t        panic_magic;              /* 0xe00 */

    uint8_t         pad13[0x11b8-0xe04];      /* 0xe04 */

    /* 64 bit extparam used for pfault, diag 250 etc  */
    uint64_t        ext_params2;               /* 0x11B8 */

    uint8_t         pad14[0x1200-0x11C0];      /* 0x11C0 */

    /* System info area */

    uint64_t        floating_pt_save_area[16]; /* 0x1200 */
    uint64_t        gpregs_save_area[16];      /* 0x1280 */
    uint32_t        st_status_fixed_logout[4]; /* 0x1300 */
    uint8_t         pad15[0x1318-0x1310];      /* 0x1310 */
    uint32_t        prefixreg_save_area;       /* 0x1318 */
    uint32_t        fpt_creg_save_area;        /* 0x131c */
    uint8_t         pad16[0x1324-0x1320];      /* 0x1320 */
    uint32_t        tod_progreg_save_area;     /* 0x1324 */
    uint32_t        cpu_timer_save_area[2];    /* 0x1328 */
    uint32_t        clock_comp_save_area[2];   /* 0x1330 */
    uint8_t         pad17[0x1340-0x1338];      /* 0x1338 */
    uint32_t        access_regs_save_area[16]; /* 0x1340 */
    uint64_t        cregs_save_area[16];       /* 0x1380 */

    /* align to the top of the prefix area */

    uint8_t         pad18[0x2000-0x1400];      /* 0x1400 */
} QEMU_PACKED LowCore;

/* STSI */
#define STSI_LEVEL_MASK         0x00000000f0000000ULL
#define STSI_LEVEL_CURRENT      0x0000000000000000ULL
#define STSI_LEVEL_1            0x0000000010000000ULL
#define STSI_LEVEL_2            0x0000000020000000ULL
#define STSI_LEVEL_3            0x0000000030000000ULL
#define STSI_R0_RESERVED_MASK   0x000000000fffff00ULL
#define STSI_R0_SEL1_MASK       0x00000000000000ffULL
#define STSI_R1_RESERVED_MASK   0x00000000ffff0000ULL
#define STSI_R1_SEL2_MASK       0x000000000000ffffULL

/* Basic Machine Configuration */
struct sysib_111 {
    uint32_t res1[8];
    uint8_t  manuf[16];
    uint8_t  type[4];
    uint8_t  res2[12];
    uint8_t  model[16];
    uint8_t  sequence[16];
    uint8_t  plant[4];
    uint8_t  res3[156];
};

/* Basic Machine CPU */
struct sysib_121 {
    uint32_t res1[80];
    uint8_t  sequence[16];
    uint8_t  plant[4];
    uint8_t  res2[2];
    uint16_t cpu_addr;
    uint8_t  res3[152];
};

/* Basic Machine CPUs */
struct sysib_122 {
    uint8_t res1[32];
    uint32_t capability;
    uint16_t total_cpus;
    uint16_t active_cpus;
    uint16_t standby_cpus;
    uint16_t reserved_cpus;
    uint16_t adjustments[2026];
};

/* LPAR CPU */
struct sysib_221 {
    uint32_t res1[80];
    uint8_t  sequence[16];
    uint8_t  plant[4];
    uint16_t cpu_id;
    uint16_t cpu_addr;
    uint8_t  res3[152];
};

/* LPAR CPUs */
struct sysib_222 {
    uint32_t res1[32];
    uint16_t lpar_num;
    uint8_t  res2;
    uint8_t  lcpuc;
    uint16_t total_cpus;
    uint16_t conf_cpus;
    uint16_t standby_cpus;
    uint16_t reserved_cpus;
    uint8_t  name[8];
    uint32_t caf;
    uint8_t  res3[16];
    uint16_t dedicated_cpus;
    uint16_t shared_cpus;
    uint8_t  res4[180];
};

/* VM CPUs */
struct sysib_322 {
    uint8_t  res1[31];
    uint8_t  count;
    struct {
        uint8_t  res2[4];
        uint16_t total_cpus;
        uint16_t conf_cpus;
        uint16_t standby_cpus;
        uint16_t reserved_cpus;
        uint8_t  name[8];
        uint32_t caf;
        uint8_t  cpi[16];
        uint8_t res5[3];
        uint8_t ext_name_encoding;
        uint32_t res3;
        uint8_t uuid[16];
    } vm[8];
    uint8_t res4[1504];
    uint8_t ext_names[8][256];
};

/* MMU defines */
#define _ASCE_ORIGIN            ~0xfffULL /* segment table origin             */
#define _ASCE_SUBSPACE          0x200     /* subspace group control           */
#define _ASCE_PRIVATE_SPACE     0x100     /* private space control            */
#define _ASCE_ALT_EVENT         0x80      /* storage alteration event control */
#define _ASCE_SPACE_SWITCH      0x40      /* space switch event               */
#define _ASCE_REAL_SPACE        0x20      /* real space control               */
#define _ASCE_TYPE_MASK         0x0c      /* asce table type mask             */
#define _ASCE_TYPE_REGION1      0x0c      /* region first table type          */
#define _ASCE_TYPE_REGION2      0x08      /* region second table type         */
#define _ASCE_TYPE_REGION3      0x04      /* region third table type          */
#define _ASCE_TYPE_SEGMENT      0x00      /* segment table type               */
#define _ASCE_TABLE_LENGTH      0x03      /* region table length              */

#define _REGION_ENTRY_ORIGIN    ~0xfffULL /* region/segment table origin      */
#define _REGION_ENTRY_RO        0x200     /* region/segment protection bit    */
#define _REGION_ENTRY_TF        0xc0      /* region/segment table offset      */
#define _REGION_ENTRY_INV       0x20      /* invalid region table entry       */
#define _REGION_ENTRY_TYPE_MASK 0x0c      /* region/segment table type mask   */
#define _REGION_ENTRY_TYPE_R1   0x0c      /* region first table type          */
#define _REGION_ENTRY_TYPE_R2   0x08      /* region second table type         */
#define _REGION_ENTRY_TYPE_R3   0x04      /* region third table type          */
#define _REGION_ENTRY_LENGTH    0x03      /* region third length              */

#define _SEGMENT_ENTRY_ORIGIN   ~0x7ffULL /* segment table origin             */
#define _SEGMENT_ENTRY_FC       0x400     /* format control                   */
#define _SEGMENT_ENTRY_RO       0x200     /* page protection bit              */
#define _SEGMENT_ENTRY_INV      0x20      /* invalid segment table entry      */

#define _PAGE_RO        0x200            /* HW read-only bit  */
#define _PAGE_INVALID   0x400            /* HW invalid bit    */
#define _PAGE_RES0      0x800            /* bit must be zero  */

#define SK_C                    (0x1 << 1)
#define SK_R                    (0x1 << 2)
#define SK_F                    (0x1 << 3)
#define SK_ACC_MASK             (0xf << 4)

/* SIGP order codes */
#define SIGP_SENSE             0x01
#define SIGP_EXTERNAL_CALL     0x02
#define SIGP_EMERGENCY         0x03
#define SIGP_START             0x04
#define SIGP_STOP              0x05
#define SIGP_RESTART           0x06
#define SIGP_STOP_STORE_STATUS 0x09
#define SIGP_INITIAL_CPU_RESET 0x0b
#define SIGP_CPU_RESET         0x0c
#define SIGP_SET_PREFIX        0x0d
#define SIGP_STORE_STATUS_ADDR 0x0e
#define SIGP_SET_ARCH          0x12
#define SIGP_STORE_ADTL_STATUS 0x17

/* SIGP condition codes */
#define SIGP_CC_ORDER_CODE_ACCEPTED 0
#define SIGP_CC_STATUS_STORED       1
#define SIGP_CC_BUSY                2
#define SIGP_CC_NOT_OPERATIONAL     3

/* SIGP status bits */
#define SIGP_STAT_EQUIPMENT_CHECK   0x80000000UL
#define SIGP_STAT_INCORRECT_STATE   0x00000200UL
#define SIGP_STAT_INVALID_PARAMETER 0x00000100UL
#define SIGP_STAT_EXT_CALL_PENDING  0x00000080UL
#define SIGP_STAT_STOPPED           0x00000040UL
#define SIGP_STAT_OPERATOR_INTERV   0x00000020UL
#define SIGP_STAT_CHECK_STOP        0x00000010UL
#define SIGP_STAT_INOPERATIVE       0x00000004UL
#define SIGP_STAT_INVALID_ORDER     0x00000002UL
#define SIGP_STAT_RECEIVER_CHECK    0x00000001UL

/* SIGP SET ARCHITECTURE modes */
#define SIGP_MODE_ESA_S390 0
#define SIGP_MODE_Z_ARCH_TRANS_ALL_PSW 1
#define SIGP_MODE_Z_ARCH_TRANS_CUR_PSW 2

void load_psw(CPUS390XState *env, uint64_t mask, uint64_t addr);
int mmu_translate(CPUS390XState *env, target_ulong vaddr, int rw, uint64_t asc,
                  target_ulong *raddr, int *flags, bool exc);
int sclp_service_call(CPUS390XState *env, uint64_t sccb, uint32_t code);
uint32_t calc_cc(CPUS390XState *env, uint32_t cc_op, uint64_t src, uint64_t dst,
                 uint64_t vr);
void s390_cpu_recompute_watchpoints(CPUState *cs);

int s390_cpu_virt_mem_rw(S390CPU *cpu, vaddr laddr, uint8_t ar, void *hostbuf,
                         int len, bool is_write);

#define s390_cpu_virt_mem_read(cpu, laddr, ar, dest, len)    \
        s390_cpu_virt_mem_rw(cpu, laddr, ar, dest, len, false)
#define s390_cpu_virt_mem_write(cpu, laddr, ar, dest, len)       \
        s390_cpu_virt_mem_rw(cpu, laddr, ar, dest, len, true)
#define s390_cpu_virt_mem_check_write(cpu, laddr, ar, len)   \
        s390_cpu_virt_mem_rw(cpu, laddr, ar, NULL, len, true)

/* The value of the TOD clock for 1.1.1970. */
#define TOD_UNIX_EPOCH 0x7d91048bca000000ULL

/* Converts ns to s390's clock format */
static inline uint64_t time2tod(uint64_t ns) {
    return (ns << 9) / 125;
}

/* Converts s390's clock format to ns */
static inline uint64_t tod2time(uint64_t t) {
    return (t * 125) >> 9;
}

static inline void cpu_inject_ext(S390CPU *cpu, uint32_t code, uint32_t param,
                                  uint64_t param64)
{
    CPUS390XState *env = &cpu->env;

    if (env->ext_index == MAX_EXT_QUEUE - 1) {
        /* ugh - can't queue anymore. Let's drop. */
        return;
    }

    env->ext_index++;
    assert(env->ext_index < MAX_EXT_QUEUE);

    env->ext_queue[env->ext_index].code = code;
    env->ext_queue[env->ext_index].param = param;
    env->ext_queue[env->ext_index].param64 = param64;

    env->pending_int |= INTERRUPT_EXT;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

static inline void cpu_inject_io(S390CPU *cpu, uint16_t subchannel_id,
                                 uint16_t subchannel_number,
                                 uint32_t io_int_parm, uint32_t io_int_word)
{
    CPUS390XState *env = &cpu->env;
    int isc = IO_INT_WORD_ISC(io_int_word);

    if (env->io_index[isc] == MAX_IO_QUEUE - 1) {
        /* ugh - can't queue anymore. Let's drop. */
        return;
    }

    env->io_index[isc]++;
    assert(env->io_index[isc] < MAX_IO_QUEUE);

    env->io_queue[env->io_index[isc]][isc].id = subchannel_id;
    env->io_queue[env->io_index[isc]][isc].nr = subchannel_number;
    env->io_queue[env->io_index[isc]][isc].parm = io_int_parm;
    env->io_queue[env->io_index[isc]][isc].word = io_int_word;

    env->pending_int |= INTERRUPT_IO;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

static inline void cpu_inject_crw_mchk(S390CPU *cpu)
{
    CPUS390XState *env = &cpu->env;

    if (env->mchk_index == MAX_MCHK_QUEUE - 1) {
        /* ugh - can't queue anymore. Let's drop. */
        return;
    }

    env->mchk_index++;
    assert(env->mchk_index < MAX_MCHK_QUEUE);

    env->mchk_queue[env->mchk_index].type = 1;

    env->pending_int |= INTERRUPT_MCHK;
    cpu_interrupt(CPU(cpu), CPU_INTERRUPT_HARD);
}

/* from s390-virtio-ccw */
#define MEM_SECTION_SIZE             0x10000000UL
#define MAX_AVAIL_SLOTS              32

/* fpu_helper.c */
uint32_t set_cc_nz_f32(float32 v);
uint32_t set_cc_nz_f64(float64 v);
uint32_t set_cc_nz_f128(float128 v);

/* misc_helper.c */
#ifndef CONFIG_USER_ONLY
int handle_diag_288(CPUS390XState *env, uint64_t r1, uint64_t r3);
void handle_diag_308(CPUS390XState *env, uint64_t r1, uint64_t r3);
#endif
void program_interrupt(CPUS390XState *env, uint32_t code, int ilen);
void QEMU_NORETURN runtime_exception(CPUS390XState *env, int excp,
                                     uintptr_t retaddr);

#ifdef CONFIG_KVM
void kvm_s390_io_interrupt(uint16_t subchannel_id,
                           uint16_t subchannel_nr, uint32_t io_int_parm,
                           uint32_t io_int_word);
void kvm_s390_crw_mchk(void);
void kvm_s390_enable_css_support(S390CPU *cpu);
int kvm_s390_assign_subch_ioeventfd(EventNotifier *notifier, uint32_t sch,
                                    int vq, bool assign);
int kvm_s390_cpu_restart(S390CPU *cpu);
int kvm_s390_get_memslot_count(KVMState *s);
void kvm_s390_cmma_reset(void);
int kvm_s390_set_cpu_state(S390CPU *cpu, uint8_t cpu_state);
void kvm_s390_reset_vcpu(S390CPU *cpu);
int kvm_s390_set_mem_limit(KVMState *s, uint64_t new_limit, uint64_t *hw_limit);
void kvm_s390_vcpu_interrupt_pre_save(S390CPU *cpu);
int kvm_s390_vcpu_interrupt_post_load(S390CPU *cpu);
void kvm_s390_crypto_reset(void);
#else
static inline void kvm_s390_io_interrupt(uint16_t subchannel_id,
                                        uint16_t subchannel_nr,
                                        uint32_t io_int_parm,
                                        uint32_t io_int_word)
{
}
static inline void kvm_s390_crw_mchk(void)
{
}
static inline void kvm_s390_enable_css_support(S390CPU *cpu)
{
}
static inline int kvm_s390_assign_subch_ioeventfd(EventNotifier *notifier,
                                                  uint32_t sch, int vq,
                                                  bool assign)
{
    return -ENOSYS;
}
static inline int kvm_s390_cpu_restart(S390CPU *cpu)
{
    return -ENOSYS;
}
static inline void kvm_s390_cmma_reset(void)
{
}
static inline int kvm_s390_get_memslot_count(KVMState *s)
{
  return MAX_AVAIL_SLOTS;
}
static inline int kvm_s390_set_cpu_state(S390CPU *cpu, uint8_t cpu_state)
{
    return -ENOSYS;
}
static inline void kvm_s390_reset_vcpu(S390CPU *cpu)
{
}
static inline int kvm_s390_set_mem_limit(KVMState *s, uint64_t new_limit,
                                         uint64_t *hw_limit)
{
    return 0;
}
static inline void kvm_s390_vcpu_interrupt_pre_save(S390CPU *cpu)
{
}
static inline int kvm_s390_vcpu_interrupt_post_load(S390CPU *cpu)
{
    return 0;
}
static inline void kvm_s390_crypto_reset(void)
{
}
#endif

static inline int s390_set_memory_limit(uint64_t new_limit, uint64_t *hw_limit)
{
    if (kvm_enabled()) {
        return kvm_s390_set_mem_limit(kvm_state, new_limit, hw_limit);
    }
    return 0;
}

static inline void s390_cmma_reset(void)
{
    if (kvm_enabled()) {
        kvm_s390_cmma_reset();
    }
}

static inline int s390_cpu_restart(S390CPU *cpu)
{
    if (kvm_enabled()) {
        return kvm_s390_cpu_restart(cpu);
    }
    return -ENOSYS;
}

static inline int s390_get_memslot_count(KVMState *s)
{
    if (kvm_enabled()) {
        return kvm_s390_get_memslot_count(s);
    } else {
        return MAX_AVAIL_SLOTS;
    }
}

void s390_io_interrupt(uint16_t subchannel_id, uint16_t subchannel_nr,
                       uint32_t io_int_parm, uint32_t io_int_word);
void s390_crw_mchk(void);

static inline int s390_assign_subch_ioeventfd(EventNotifier *notifier,
                                              uint32_t sch_id, int vq,
                                              bool assign)
{
    return kvm_s390_assign_subch_ioeventfd(notifier, sch_id, vq, assign);
}

static inline void s390_crypto_reset(void)
{
    if (kvm_enabled()) {
        kvm_s390_crypto_reset();
    }
}

#ifdef CONFIG_KVM
static inline bool vregs_needed(void *opaque)
{
    if (kvm_enabled()) {
        return kvm_check_extension(kvm_state, KVM_CAP_S390_VECTOR_REGISTERS);
    }
    return 0;
}
#else
static inline bool vregs_needed(void *opaque)
{
    return 0;
}
#endif

/* machine check interruption code */

/* subclasses */
#define MCIC_SC_SD 0x8000000000000000ULL
#define MCIC_SC_PD 0x4000000000000000ULL
#define MCIC_SC_SR 0x2000000000000000ULL
#define MCIC_SC_CD 0x0800000000000000ULL
#define MCIC_SC_ED 0x0400000000000000ULL
#define MCIC_SC_DG 0x0100000000000000ULL
#define MCIC_SC_W  0x0080000000000000ULL
#define MCIC_SC_CP 0x0040000000000000ULL
#define MCIC_SC_SP 0x0020000000000000ULL
#define MCIC_SC_CK 0x0010000000000000ULL

/* subclass modifiers */
#define MCIC_SCM_B  0x0002000000000000ULL
#define MCIC_SCM_DA 0x0000000020000000ULL
#define MCIC_SCM_AP 0x0000000000080000ULL

/* storage errors */
#define MCIC_SE_SE 0x0000800000000000ULL
#define MCIC_SE_SC 0x0000400000000000ULL
#define MCIC_SE_KE 0x0000200000000000ULL
#define MCIC_SE_DS 0x0000100000000000ULL
#define MCIC_SE_IE 0x0000000080000000ULL

/* validity bits */
#define MCIC_VB_WP 0x0000080000000000ULL
#define MCIC_VB_MS 0x0000040000000000ULL
#define MCIC_VB_PM 0x0000020000000000ULL
#define MCIC_VB_IA 0x0000010000000000ULL
#define MCIC_VB_FA 0x0000008000000000ULL
#define MCIC_VB_VR 0x0000004000000000ULL
#define MCIC_VB_EC 0x0000002000000000ULL
#define MCIC_VB_FP 0x0000001000000000ULL
#define MCIC_VB_GR 0x0000000800000000ULL
#define MCIC_VB_CR 0x0000000400000000ULL
#define MCIC_VB_ST 0x0000000100000000ULL
#define MCIC_VB_AR 0x0000000040000000ULL
#define MCIC_VB_PR 0x0000000000200000ULL
#define MCIC_VB_FC 0x0000000000100000ULL
#define MCIC_VB_CT 0x0000000000020000ULL
#define MCIC_VB_CC 0x0000000000010000ULL

#endif
