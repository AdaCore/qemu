/*
 *  MIPS emulation helpers for qemu.
 *
 *  Copyright (c) 2004-2005 Jocelyn Mayer
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"

#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "exec/log.h"
#include "qemu/atomic.h"
#include "qemu/error-report.h"
#include "hw/mips/cpudevs.h"
#include "qapi/qapi-commands-machine-target.h"

enum {
#ifdef TARGET_CHERI
    TLBRET_S = -5,
#else
    TLBRET_XI = -6,
    TLBRET_RI = -5,
#endif /* TARGET_CHERI */
    TLBRET_DIRTY = -4,
    TLBRET_INVALID = -3,
    TLBRET_NOMATCH = -2,
    TLBRET_BADADDR = -1,
    TLBRET_MATCH = 0
};

#if !defined(CONFIG_USER_ONLY)

/* no MMU emulation */
int no_mmu_map_address(CPUMIPSState *env, hwaddr *physical, int *prot,
                       target_ulong address, int rw, int access_type)
{
    *physical = address;
    *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    return TLBRET_MATCH;
}

/* fixed mapping MMU emulation */
int fixed_mmu_map_address(CPUMIPSState *env, hwaddr *physical, int *prot,
                          target_ulong address, int rw, int access_type)
{
    if (address <= (int32_t)0x7FFFFFFFUL) {
        if (!(env->CP0_Status & (1 << CP0St_ERL))) {
            *physical = address + 0x40000000UL;
        } else {
            *physical = address;
        }
    } else if (address <= (int32_t)0xBFFFFFFFUL) {
        *physical = address & 0x1FFFFFFF;
    } else {
        *physical = address;
    }

    *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    return TLBRET_MATCH;
}

/* MIPS32/MIPS64 R4000-style MMU emulation */
int r4k_map_address(CPUMIPSState *env, hwaddr *physical, int *prot,
                    target_ulong address, int rw, int access_type)
{
    uint16_t ASID = env->CP0_EntryHi & env->CP0_EntryHi_ASID_mask;
    int i;

    for (i = 0; i < env->tlb->tlb_in_use; i++) {
        r4k_tlb_t *tlb = &env->tlb->mmu.r4k.tlb[i];
        /* 1k pages are not supported. */
        target_ulong mask = tlb->PageMask | ~(TARGET_PAGE_MASK << 1);
        target_ulong tag = address & ~mask;
        target_ulong VPN = tlb->VPN & ~mask;
#if defined(TARGET_MIPS64)
        tag &= env->SEGMask;
#endif

        /* Check ASID, virtual page number & size */
        if ((tlb->G == 1 || tlb->ASID == ASID) && VPN == tag && !tlb->EHINV) {
            /* TLB match */
            int n = !!(address & mask & ~(mask >> 1));
            /* Check access rights */
            if (!(n ? tlb->V1 : tlb->V0)) {
                return TLBRET_INVALID;
            }
#if defined(TARGET_CHERI)
            if (rw == MMU_DATA_CAP_STORE) {
                /*
                 * If we're trying to do a cap-store, first check for the
                 * dirty/store-permitted bit before looking at the the
                 * store-capability inhibit.
                 */
                if (!(n ? tlb->D1 : tlb->D0)) {
                    return TLBRET_DIRTY;
                }
                if (n ? tlb->S1 : tlb->S0) {
                    return TLBRET_S;
                }
            }
#else
            if (rw == MMU_INST_FETCH && (n ? tlb->XI1 : tlb->XI0)) {
                return TLBRET_XI;
            }
            if (rw == MMU_DATA_LOAD && (n ? tlb->RI1 : tlb->RI0)) {
                return TLBRET_RI;
            }
#endif /* TARGET_CHERI */

            if (( (rw != MMU_DATA_STORE)
#if defined(TARGET_CHERI)
                  && (rw != MMU_DATA_CAP_STORE)
#endif
                ) || (n ? tlb->D1 : tlb->D0)) {

                *physical = tlb->PFN[n] | (address & (mask >> 1));
                *prot = PAGE_READ;
                if (n ? tlb->D1 : tlb->D0) {
                    *prot |= PAGE_WRITE;
                }
#if !defined(TARGET_CHERI)
                if (!(n ? tlb->XI1 : tlb->XI0)) {
#else
                if (true) {
#endif
                    *prot |= PAGE_EXEC;
                }

#if defined(TARGET_CHERI)
                if (n ? tlb->L1 : tlb->L0) {
                    *prot |= PAGE_LC_CLEAR;
                }
#endif

                return TLBRET_MATCH;
            }
            return TLBRET_DIRTY;
        }
    }
    return TLBRET_NOMATCH;
}

static int is_seg_am_mapped(unsigned int am, bool eu, int mmu_idx)
{
    /*
     * Interpret access control mode and mmu_idx.
     *           AdE?     TLB?
     *      AM  K S U E  K S U E
     * UK    0  0 1 1 0  0 - - 0
     * MK    1  0 1 1 0  1 - - !eu
     * MSK   2  0 0 1 0  1 1 - !eu
     * MUSK  3  0 0 0 0  1 1 1 !eu
     * MUSUK 4  0 0 0 0  0 1 1 0
     * USK   5  0 0 1 0  0 0 - 0
     * -     6  - - - -  - - - -
     * UUSK  7  0 0 0 0  0 0 0 0
     */
    int32_t adetlb_mask;

    switch (mmu_idx) {
    case 3: /* ERL */
        /* If EU is set, always unmapped */
        if (eu) {
            return 0;
        }
        /* fall through */
    case MIPS_HFLAG_KM:
        /* Never AdE, TLB mapped if AM={1,2,3} */
        adetlb_mask = 0x70000000;
        goto check_tlb;

    case MIPS_HFLAG_SM:
        /* AdE if AM={0,1}, TLB mapped if AM={2,3,4} */
        adetlb_mask = 0xc0380000;
        goto check_ade;

    case MIPS_HFLAG_UM:
        /* AdE if AM={0,1,2,5}, TLB mapped if AM={3,4} */
        adetlb_mask = 0xe4180000;
        /* fall through */
    check_ade:
        /* does this AM cause AdE in current execution mode */
        if ((adetlb_mask << am) < 0) {
            return TLBRET_BADADDR;
        }
        adetlb_mask <<= 8;
        /* fall through */
    check_tlb:
        /* is this AM mapped in current execution mode */
        return ((adetlb_mask << am) < 0);
    default:
        assert(0);
        return TLBRET_BADADDR;
    };
}

static int get_seg_physical_address(CPUMIPSState *env, hwaddr *physical,
                                    int *prot, target_ulong real_address,
                                    int rw, int access_type, int mmu_idx,
                                    unsigned int am, bool eu,
                                    target_ulong segmask,
                                    hwaddr physical_base)
{
    int mapped = is_seg_am_mapped(am, eu, mmu_idx);

    if (mapped < 0) {
        /* is_seg_am_mapped can report TLBRET_BADADDR */
        return mapped;
    } else if (mapped) {
        /* The segment is TLB mapped */
        return env->tlb->map_address(env, physical, prot, real_address, rw,
                                     access_type);
    } else {
        /* The segment is unmapped */
        *physical = physical_base | (real_address & segmask);
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TLBRET_MATCH;
    }
}

static int get_segctl_physical_address(CPUMIPSState *env, hwaddr *physical,
                                       int *prot, target_ulong real_address,
                                       int rw, int access_type, int mmu_idx,
                                       uint16_t segctl, target_ulong segmask)
{
    unsigned int am = (segctl & CP0SC_AM_MASK) >> CP0SC_AM;
    bool eu = (segctl >> CP0SC_EU) & 1;
    hwaddr pa = ((hwaddr)segctl & CP0SC_PA_MASK) << 20;

    return get_seg_physical_address(env, physical, prot, real_address, rw,
                                    access_type, mmu_idx, am, eu, segmask,
                                    pa & ~(hwaddr)segmask);
}

static int get_physical_address(CPUMIPSState *env, hwaddr *physical,
                                int *prot, target_ulong real_address,
                                int rw, int access_type, int mmu_idx)
{
    /* User mode can only access useg/xuseg */
#if defined(TARGET_MIPS64)
    int user_mode = mmu_idx == MIPS_HFLAG_UM;
    int supervisor_mode = mmu_idx == MIPS_HFLAG_SM;
    int kernel_mode = !user_mode && !supervisor_mode;
    int UX = (env->CP0_Status & (1 << CP0St_UX)) != 0;
    int SX = (env->CP0_Status & (1 << CP0St_SX)) != 0;
    int KX = (env->CP0_Status & (1 << CP0St_KX)) != 0;
#endif
    int ret = TLBRET_MATCH;
    /* effective address (modified for KVM T&E kernel segments) */
    target_ulong address = real_address;

#define USEG_LIMIT      ((target_ulong)(int32_t)0x7FFFFFFFUL)
#define KSEG0_BASE      ((target_ulong)(int32_t)0x80000000UL)
#define KSEG1_BASE      ((target_ulong)(int32_t)0xA0000000UL)
#define KSEG2_BASE      ((target_ulong)(int32_t)0xC0000000UL)
#define KSEG3_BASE      ((target_ulong)(int32_t)0xE0000000UL)

#define KVM_KSEG0_BASE  ((target_ulong)(int32_t)0x40000000UL)
#define KVM_KSEG2_BASE  ((target_ulong)(int32_t)0x60000000UL)

    if (mips_um_ksegs_enabled()) {
        /* KVM T&E adds guest kernel segments in useg */
        if (real_address >= KVM_KSEG0_BASE) {
            if (real_address < KVM_KSEG2_BASE) {
                /* kseg0 */
                address += KSEG0_BASE - KVM_KSEG0_BASE;
            } else if (real_address <= USEG_LIMIT) {
                /* kseg2/3 */
                address += KSEG2_BASE - KVM_KSEG2_BASE;
            }
        }
    }

    if (address <= USEG_LIMIT) {
        /* useg */
        uint16_t segctl;

        if (address >= 0x40000000UL) {
            segctl = env->CP0_SegCtl2;
        } else {
            segctl = env->CP0_SegCtl2 >> 16;
        }
        ret = get_segctl_physical_address(env, physical, prot,
                                          real_address, rw, access_type,
                                          mmu_idx, segctl, 0x3FFFFFFF);
#if defined(TARGET_MIPS64)
    } else if (address < 0x4000000000000000ULL) {
        /* xuseg */
        if (UX && address <= (0x3FFFFFFFFFFFFFFFULL & env->SEGMask)) {
            ret = env->tlb->map_address(env, physical, prot,
                                        real_address, rw, access_type);
        } else {
            ret = TLBRET_BADADDR;
        }
    } else if (address < 0x8000000000000000ULL) {
        /* xsseg */
        if ((supervisor_mode || kernel_mode) &&
            SX && address <= (0x7FFFFFFFFFFFFFFFULL & env->SEGMask)) {
            ret = env->tlb->map_address(env, physical, prot,
                                        real_address, rw, access_type);
        } else {
            ret = TLBRET_BADADDR;
        }
    } else if (address < 0xC000000000000000ULL) {
        /* xkphys */
        if ((address & 0x07FFFFFFFFFFFFFFULL) <= env->PAMask) {
            /* KX/SX/UX bit to check for each xkphys EVA access mode */
            static const uint8_t am_ksux[8] = {
                [CP0SC_AM_UK]    = (1u << CP0St_KX),
                [CP0SC_AM_MK]    = (1u << CP0St_KX),
                [CP0SC_AM_MSK]   = (1u << CP0St_SX),
                [CP0SC_AM_MUSK]  = (1u << CP0St_UX),
                [CP0SC_AM_MUSUK] = (1u << CP0St_UX),
                [CP0SC_AM_USK]   = (1u << CP0St_SX),
                [6]              = (1u << CP0St_KX),
                [CP0SC_AM_UUSK]  = (1u << CP0St_UX),
            };
            unsigned int am = CP0SC_AM_UK;
            unsigned int xr = (env->CP0_SegCtl2 & CP0SC2_XR_MASK) >> CP0SC2_XR;

            if (xr & (1 << ((address >> 59) & 0x7))) {
                am = (env->CP0_SegCtl1 & CP0SC1_XAM_MASK) >> CP0SC1_XAM;
            }
            /* Does CP0_Status.KX/SX/UX permit the access mode (am) */
            if (env->CP0_Status & am_ksux[am]) {
                ret = get_seg_physical_address(env, physical, prot,
                                               real_address, rw, access_type,
                                               mmu_idx, am, false, env->PAMask,
                                               0);
            } else {
                ret = TLBRET_BADADDR;
            }
        } else {
            ret = TLBRET_BADADDR;
        }
    } else if (address < 0xFFFFFFFF80000000ULL) {
        /* xkseg */
        if (kernel_mode && KX &&
            address <= (0xFFFFFFFF7FFFFFFFULL & env->SEGMask)) {
            ret = env->tlb->map_address(env, physical, prot,
                                        real_address, rw, access_type);
        } else {
            ret = TLBRET_BADADDR;
        }
#endif
    /*
     * XXXAR: I am not entirely sure deleting the previous code using  is the correct way to make commit
     * a78cda6e0212ccb16711d0dd30c2bb3480b36415 (40 bits for CHERI) to work with
     * the new get_segctl_physical_address added in commit
     * 480e79aedd322fcfac17052caff21626ea7c78e2. As far as I can see from the
     * BERI hardware reference we should still be using 0x1FFFFFFF as the mask
     * for kseg0 and kseg1
     */
    } else if (address < KSEG1_BASE) {
        /* kseg0 */
        ret = get_segctl_physical_address(env, physical, prot, real_address, rw,
                                          access_type, mmu_idx,
                                          env->CP0_SegCtl1 >> 16, 0x1FFFFFFF);
    } else if (address < KSEG2_BASE) {
        /* kseg1 */
        ret = get_segctl_physical_address(env, physical, prot, real_address, rw,
                                          access_type, mmu_idx,
                                          env->CP0_SegCtl1, 0x1FFFFFFF);
    } else if (address < KSEG3_BASE) {
        /* sseg (kseg2) */
        ret = get_segctl_physical_address(env, physical, prot, real_address, rw,
                                          access_type, mmu_idx,
                                          env->CP0_SegCtl0 >> 16, 0x1FFFFFFF);
    } else {
        /*
         * kseg3
         * XXX: debug segment is not emulated
         */
        ret = get_segctl_physical_address(env, physical, prot, real_address, rw,
                                          access_type, mmu_idx,
                                          env->CP0_SegCtl0, 0x1FFFFFFF);
    }
    return ret;
}

void cpu_mips_tlb_flush(CPUMIPSState *env)
{
    /* Flush qemu's TLB and discard all shadowed entries.  */
    tlb_flush(env_cpu(env));
    env->tlb->tlb_in_use = env->tlb->nb_tlb;
}

/* Called for updates to CP0_Status.  */
void sync_c0_status(CPUMIPSState *env, CPUMIPSState *cpu, int tc)
{
    int32_t tcstatus, *tcst;
    uint32_t v = cpu->CP0_Status;
    uint32_t cu, mx, asid, ksu;
    uint32_t mask = ((1 << CP0TCSt_TCU3)
                       | (1 << CP0TCSt_TCU2)
                       | (1 << CP0TCSt_TCU1)
                       | (1 << CP0TCSt_TCU0)
                       | (1 << CP0TCSt_TMX)
                       | (3 << CP0TCSt_TKSU)
                       | (0xff << CP0TCSt_TASID));

    cu = (v >> CP0St_CU0) & 0xf;
    mx = (v >> CP0St_MX) & 0x1;
    ksu = (v >> CP0St_KSU) & 0x3;
    asid = env->CP0_EntryHi & env->CP0_EntryHi_ASID_mask;

    tcstatus = cu << CP0TCSt_TCU0;
    tcstatus |= mx << CP0TCSt_TMX;
    tcstatus |= ksu << CP0TCSt_TKSU;
    tcstatus |= asid;

    if (tc == cpu->current_tc) {
        tcst = &cpu->active_tc.CP0_TCStatus;
    } else {
        tcst = &cpu->tcs[tc].CP0_TCStatus;
    }

    *tcst &= ~mask;
    *tcst |= tcstatus;
    compute_hflags(cpu);
}

void cpu_mips_store_status(CPUMIPSState *env, target_ulong val)
{
    uint32_t mask = env->CP0_Status_rw_bitmask;
    target_ulong old = env->CP0_Status;

    if (env->insn_flags & ISA_MIPS32R6) {
        bool has_supervisor = extract32(mask, CP0St_KSU, 2) == 0x3;
#if defined(TARGET_MIPS64)
        uint32_t ksux = (1 << CP0St_KX) & val;
        ksux |= (ksux >> 1) & val; /* KX = 0 forces SX to be 0 */
        ksux |= (ksux >> 1) & val; /* SX = 0 forces UX to be 0 */
        val = (val & ~(7 << CP0St_UX)) | ksux;
#endif
        if (has_supervisor && extract32(val, CP0St_KSU, 2) == 0x3) {
            mask &= ~(3 << CP0St_KSU);
        }
        mask &= ~(((1 << CP0St_SR) | (1 << CP0St_NMI)) & val);
    }

    env->CP0_Status = (old & ~mask) | (val & mask);
#if defined(TARGET_MIPS64)
    if ((env->CP0_Status ^ old) & (old & (7 << CP0St_UX))) {
        /* Access to at least one of the 64-bit segments has been disabled */
        tlb_flush(env_cpu(env));
    }
#endif
    if (env->CP0_Config3 & (1 << CP0C3_MT)) {
        sync_c0_status(env, env, env->current_tc);
    } else {
        compute_hflags(env);
    }
}

void cpu_mips_store_cause(CPUMIPSState *env, target_ulong val)
{
    uint32_t mask = 0x00C00300;
    uint32_t old = env->CP0_Cause;
    int i;

    if (env->insn_flags & ISA_MIPS32R2) {
        mask |= 1 << CP0Ca_DC;
    }
    if (env->insn_flags & ISA_MIPS32R6) {
        mask &= ~((1 << CP0Ca_WP) & val);
    }

    env->CP0_Cause = (env->CP0_Cause & ~mask) | (val & mask);

    if ((old ^ env->CP0_Cause) & (1 << CP0Ca_DC)) {
        if (env->CP0_Cause & (1 << CP0Ca_DC)) {
            cpu_mips_stop_count(env);
        } else {
            cpu_mips_start_count(env);
        }
    }

    /* Set/reset software interrupts */
    for (i = 0 ; i < 2 ; i++) {
        if ((old ^ env->CP0_Cause) & (1 << (CP0Ca_IP + i))) {
            cpu_mips_soft_irq(env, i, env->CP0_Cause & (1 << (CP0Ca_IP + i)));
        }
    }
}
#endif

#ifdef TARGET_CHERI
static void raise_mmu_exception(CPUMIPSState *env, target_ulong address,
                                int rw, int tlb_error, int reg)
#else
static void raise_mmu_exception(CPUMIPSState *env, target_ulong address,
                                int rw, int tlb_error)
#endif
{
    CPUState *cs = env_cpu(env);
    int exception = 0, error_code = 0;

    if (rw == MMU_INST_FETCH) {
        error_code |= EXCP_INST_NOTAVAIL;
    }

    switch (tlb_error) {
    default:
    case TLBRET_BADADDR:
        /* Reference to kernel address from user mode or supervisor mode */
        /* Reference to supervisor address from user mode */
        if (rw == MMU_DATA_STORE) {
            exception = EXCP_AdES;
        } else {
            exception = EXCP_AdEL;
        }
        break;
    case TLBRET_NOMATCH:
        /* No TLB match for a mapped address */
        if (rw == MMU_DATA_STORE) {
            exception = EXCP_TLBS;
        } else {
            exception = EXCP_TLBL;
        }
        error_code |= EXCP_TLB_NOMATCH;
        break;
    case TLBRET_INVALID:
        /* TLB match with no valid bit */
        if (rw == MMU_DATA_STORE) {
            exception = EXCP_TLBS;
        } else {
            exception = EXCP_TLBL;
        }
        break;
    case TLBRET_DIRTY:
        /* TLB match but 'D' bit is cleared */
        exception = EXCP_LTLBL;
        break;
#ifdef TARGET_CHERI
    case TLBRET_S:
        /* TLB capability store bit was set, blocking capability store. */
        cpu_mips_store_capcause(env, reg, CP2Ca_TLB_STORE);
        env->active_tc.PC = cap_get_cursor(&env->active_tc.PCC);
        exception = EXCP_C2E;
        break;
#else
    case TLBRET_XI:
        /* Execute-Inhibit Exception */
        if (env->CP0_PageGrain & (1 << CP0PG_IEC)) {
            exception = EXCP_TLBXI;
        } else {
            exception = EXCP_TLBL;
        }
        break;
    case TLBRET_RI:
        /* Read-Inhibit Exception */
        if (env->CP0_PageGrain & (1 << CP0PG_IEC)) {
            exception = EXCP_TLBRI;
        } else {
            exception = EXCP_TLBL;
        }
        break;
#endif /* TARGET_CHERI */
    }
    /* Raise exception */
    if (!(env->hflags & MIPS_HFLAG_DM)) {
        env->CP0_BadVAddr = address;
    }
    env->CP0_Context = (env->CP0_Context & ~0x007fffff) |
                       ((address >> 9) & 0x007ffff0);
    env->CP0_EntryHi = (env->CP0_EntryHi & env->CP0_EntryHi_ASID_mask) |
                       (env->CP0_EntryHi & (1 << CP0EnHi_EHINV)) |
                       (address & (TARGET_PAGE_MASK << 1));
#if defined(TARGET_MIPS64)
    env->CP0_EntryHi &= env->SEGMask;
    env->CP0_XContext =
        (env->CP0_XContext & ((~0ULL) << (env->SEGBITS - 7))) | /* PTEBase */
        (extract64(address, 62, 2) << (env->SEGBITS - 9)) |     /* R       */
        (extract64(address, 13, env->SEGBITS - 13) << 4);       /* BadVPN2 */
#endif
    cs->exception_index = exception;
    env->error_code = error_code;
#ifdef TARGET_CHERI
    if (rw == MMU_INST_FETCH)
        env->statcounters_itlb_miss++;
    else
        env->statcounters_dtlb_miss++;
#endif
}

#if !defined(CONFIG_USER_ONLY)
hwaddr mips_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    MIPSCPU *cpu = MIPS_CPU(cs);
    CPUMIPSState *env = &cpu->env;
    hwaddr phys_addr;
    int prot;

    if (get_physical_address(env, &phys_addr, &prot, addr, 0, ACCESS_INT,
                             cpu_mmu_index(env, false)) != 0) {
        return -1;
    }
    return phys_addr;
}
#endif

#if !defined(CONFIG_USER_ONLY)
#if !defined(TARGET_MIPS64)

/*
 * Perform hardware page table walk
 *
 * Memory accesses are performed using the KERNEL privilege level.
 * Synchronous exceptions detected on memory accesses cause a silent exit
 * from page table walking, resulting in a TLB or XTLB Refill exception.
 *
 * Implementations are not required to support page table walk memory
 * accesses from mapped memory regions. When an unsupported access is
 * attempted, a silent exit is taken, resulting in a TLB or XTLB Refill
 * exception.
 *
 * Note that if an exception is caused by AddressTranslation or LoadMemory
 * functions, the exception is not taken, a silent exit is taken,
 * resulting in a TLB or XTLB Refill exception.
 */

static bool get_pte(CPUMIPSState *env, uint64_t vaddr, int entry_size,
        uint64_t *pte)
{
    if ((vaddr & ((entry_size >> 3) - 1)) != 0) {
        return false;
    }
    if (entry_size == 64) {
        *pte = cpu_ldq_code(env, vaddr);
    } else {
        *pte = cpu_ldl_code(env, vaddr);
    }
    return true;
}

static uint64_t get_tlb_entry_layout(CPUMIPSState *env, uint64_t entry,
        int entry_size, int ptei)
{
    uint64_t result = entry;
    uint64_t rixi;
    if (ptei > entry_size) {
        ptei -= 32;
    }
    result >>= (ptei - 2);
    rixi = result & 3;
    result >>= 2;
    result |= rixi << CP0EnLo_XI;
    return result;
}

static int walk_directory(CPUMIPSState *env, uint64_t *vaddr,
        int directory_index, bool *huge_page, bool *hgpg_directory_hit,
        uint64_t *pw_entrylo0, uint64_t *pw_entrylo1)
{
    int dph = (env->CP0_PWCtl >> CP0PC_DPH) & 0x1;
    int psn = (env->CP0_PWCtl >> CP0PC_PSN) & 0x3F;
    int hugepg = (env->CP0_PWCtl >> CP0PC_HUGEPG) & 0x1;
    int pf_ptew = (env->CP0_PWField >> CP0PF_PTEW) & 0x3F;
    int ptew = (env->CP0_PWSize >> CP0PS_PTEW) & 0x3F;
    int native_shift = (((env->CP0_PWSize >> CP0PS_PS) & 1) == 0) ? 2 : 3;
    int directory_shift = (ptew > 1) ? -1 :
            (hugepg && (ptew == 1)) ? native_shift + 1 : native_shift;
    int leaf_shift = (ptew > 1) ? -1 :
            (ptew == 1) ? native_shift + 1 : native_shift;
    uint32_t direntry_size = 1 << (directory_shift + 3);
    uint32_t leafentry_size = 1 << (leaf_shift + 3);
    uint64_t entry;
    uint64_t paddr;
    int prot;
    uint64_t lsb = 0;
    uint64_t w = 0;

    if (get_physical_address(env, &paddr, &prot, *vaddr, MMU_DATA_LOAD,
                             ACCESS_INT, cpu_mmu_index(env, false)) !=
                             TLBRET_MATCH) {
        /* wrong base address */
        return 0;
    }
    if (!get_pte(env, *vaddr, direntry_size, &entry)) {
        return 0;
    }

    if ((entry & (1 << psn)) && hugepg) {
        *huge_page = true;
        *hgpg_directory_hit = true;
        entry = get_tlb_entry_layout(env, entry, leafentry_size, pf_ptew);
        w = directory_index - 1;
        if (directory_index & 0x1) {
            /* Generate adjacent page from same PTE for odd TLB page */
            lsb = (1 << w) >> 6;
            *pw_entrylo0 = entry & ~lsb; /* even page */
            *pw_entrylo1 = entry | lsb; /* odd page */
        } else if (dph) {
            int oddpagebit = 1 << leaf_shift;
            uint64_t vaddr2 = *vaddr ^ oddpagebit;
            if (*vaddr & oddpagebit) {
                *pw_entrylo1 = entry;
            } else {
                *pw_entrylo0 = entry;
            }
            if (get_physical_address(env, &paddr, &prot, vaddr2, MMU_DATA_LOAD,
                                     ACCESS_INT, cpu_mmu_index(env, false)) !=
                                     TLBRET_MATCH) {
                return 0;
            }
            if (!get_pte(env, vaddr2, leafentry_size, &entry)) {
                return 0;
            }
            entry = get_tlb_entry_layout(env, entry, leafentry_size, pf_ptew);
            if (*vaddr & oddpagebit) {
                *pw_entrylo0 = entry;
            } else {
                *pw_entrylo1 = entry;
            }
        } else {
            return 0;
        }
        return 1;
    } else {
        *vaddr = entry;
        return 2;
    }
}

static bool page_table_walk_refill(CPUMIPSState *env, vaddr address, int rw,
        int mmu_idx)
{
    int gdw = (env->CP0_PWSize >> CP0PS_GDW) & 0x3F;
    int udw = (env->CP0_PWSize >> CP0PS_UDW) & 0x3F;
    int mdw = (env->CP0_PWSize >> CP0PS_MDW) & 0x3F;
    int ptw = (env->CP0_PWSize >> CP0PS_PTW) & 0x3F;
    int ptew = (env->CP0_PWSize >> CP0PS_PTEW) & 0x3F;

    /* Initial values */
    bool huge_page = false;
    bool hgpg_bdhit = false;
    bool hgpg_gdhit = false;
    bool hgpg_udhit = false;
    bool hgpg_mdhit = false;

    int32_t pw_pagemask = 0;
    target_ulong pw_entryhi = 0;
    uint64_t pw_entrylo0 = 0;
    uint64_t pw_entrylo1 = 0;

    /* Native pointer size */
    /*For the 32-bit architectures, this bit is fixed to 0.*/
    int native_shift = (((env->CP0_PWSize >> CP0PS_PS) & 1) == 0) ? 2 : 3;

    /* Indices from PWField */
    int pf_gdw = (env->CP0_PWField >> CP0PF_GDW) & 0x3F;
    int pf_udw = (env->CP0_PWField >> CP0PF_UDW) & 0x3F;
    int pf_mdw = (env->CP0_PWField >> CP0PF_MDW) & 0x3F;
    int pf_ptw = (env->CP0_PWField >> CP0PF_PTW) & 0x3F;
    int pf_ptew = (env->CP0_PWField >> CP0PF_PTEW) & 0x3F;

    /* Indices computed from faulting address */
    int gindex = (address >> pf_gdw) & ((1 << gdw) - 1);
    int uindex = (address >> pf_udw) & ((1 << udw) - 1);
    int mindex = (address >> pf_mdw) & ((1 << mdw) - 1);
    int ptindex = (address >> pf_ptw) & ((1 << ptw) - 1);

    /* Other HTW configs */
    int hugepg = (env->CP0_PWCtl >> CP0PC_HUGEPG) & 0x1;

    /* HTW Shift values (depend on entry size) */
    int directory_shift = (ptew > 1) ? -1 :
            (hugepg && (ptew == 1)) ? native_shift + 1 : native_shift;
    int leaf_shift = (ptew > 1) ? -1 :
            (ptew == 1) ? native_shift + 1 : native_shift;

    /* Offsets into tables */
    int goffset = gindex << directory_shift;
    int uoffset = uindex << directory_shift;
    int moffset = mindex << directory_shift;
    int ptoffset0 = (ptindex >> 1) << (leaf_shift + 1);
    int ptoffset1 = ptoffset0 | (1 << (leaf_shift));

    uint32_t leafentry_size = 1 << (leaf_shift + 3);

    /* Starting address - Page Table Base */
    uint64_t vaddr = env->CP0_PWBase;

    uint64_t dir_entry;
    uint64_t paddr;
    int prot;
    int m;

    if (!(env->CP0_Config3 & (1 << CP0C3_PW))) {
        /* walker is unimplemented */
        return false;
    }
    if (!(env->CP0_PWCtl & (1 << CP0PC_PWEN))) {
        /* walker is disabled */
        return false;
    }
    if (!(gdw > 0 || udw > 0 || mdw > 0)) {
        /* no structure to walk */
        return false;
    }
    if ((directory_shift == -1) || (leaf_shift == -1)) {
        return false;
    }

    /* Global Directory */
    if (gdw > 0) {
        vaddr |= goffset;
        switch (walk_directory(env, &vaddr, pf_gdw, &huge_page, &hgpg_gdhit,
                               &pw_entrylo0, &pw_entrylo1))
        {
        case 0:
            return false;
        case 1:
            goto refill;
        case 2:
        default:
            break;
        }
    }

    /* Upper directory */
    if (udw > 0) {
        vaddr |= uoffset;
        switch (walk_directory(env, &vaddr, pf_udw, &huge_page, &hgpg_udhit,
                               &pw_entrylo0, &pw_entrylo1))
        {
        case 0:
            return false;
        case 1:
            goto refill;
        case 2:
        default:
            break;
        }
    }

    /* Middle directory */
    if (mdw > 0) {
        vaddr |= moffset;
        switch (walk_directory(env, &vaddr, pf_mdw, &huge_page, &hgpg_mdhit,
                               &pw_entrylo0, &pw_entrylo1))
        {
        case 0:
            return false;
        case 1:
            goto refill;
        case 2:
        default:
            break;
        }
    }

    /* Leaf Level Page Table - First half of PTE pair */
    vaddr |= ptoffset0;
    if (get_physical_address(env, &paddr, &prot, vaddr, MMU_DATA_LOAD,
                             ACCESS_INT, cpu_mmu_index(env, false)) !=
                             TLBRET_MATCH) {
        return false;
    }
    if (!get_pte(env, vaddr, leafentry_size, &dir_entry)) {
        return false;
    }
    dir_entry = get_tlb_entry_layout(env, dir_entry, leafentry_size, pf_ptew);
    pw_entrylo0 = dir_entry;

    /* Leaf Level Page Table - Second half of PTE pair */
    vaddr |= ptoffset1;
    if (get_physical_address(env, &paddr, &prot, vaddr, MMU_DATA_LOAD,
                             ACCESS_INT, cpu_mmu_index(env, false)) !=
                             TLBRET_MATCH) {
        return false;
    }
    if (!get_pte(env, vaddr, leafentry_size, &dir_entry)) {
        return false;
    }
    dir_entry = get_tlb_entry_layout(env, dir_entry, leafentry_size, pf_ptew);
    pw_entrylo1 = dir_entry;

refill:

    m = (1 << pf_ptw) - 1;

    if (huge_page) {
        switch (hgpg_bdhit << 3 | hgpg_gdhit << 2 | hgpg_udhit << 1 |
                hgpg_mdhit)
        {
        case 4:
            m = (1 << pf_gdw) - 1;
            if (pf_gdw & 1) {
                m >>= 1;
            }
            break;
        case 2:
            m = (1 << pf_udw) - 1;
            if (pf_udw & 1) {
                m >>= 1;
            }
            break;
        case 1:
            m = (1 << pf_mdw) - 1;
            if (pf_mdw & 1) {
                m >>= 1;
            }
            break;
        }
    }
    pw_pagemask = m >> 12;
    update_pagemask(env, pw_pagemask << 13, &pw_pagemask);
    pw_entryhi = (address & ~0x1fff) | (env->CP0_EntryHi & 0xFF);
    {
        target_ulong tmp_entryhi = env->CP0_EntryHi;
        int32_t tmp_pagemask = env->CP0_PageMask;
        uint64_t tmp_entrylo0 = env->CP0_EntryLo0;
        uint64_t tmp_entrylo1 = env->CP0_EntryLo1;

        env->CP0_EntryHi = pw_entryhi;
        env->CP0_PageMask = pw_pagemask;
        env->CP0_EntryLo0 = pw_entrylo0;
        env->CP0_EntryLo1 = pw_entrylo1;

        /*
         * The hardware page walker inserts a page into the TLB in a manner
         * identical to a TLBWR instruction as executed by the software refill
         * handler.
         */
        r4k_helper_tlbwr(env);

        env->CP0_EntryHi = tmp_entryhi;
        env->CP0_PageMask = tmp_pagemask;
        env->CP0_EntryLo0 = tmp_entrylo0;
        env->CP0_EntryLo1 = tmp_entrylo1;
    }
    return true;
}
#endif
#endif

bool mips_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                       MMUAccessType access_type, int mmu_idx,
                       bool probe, uintptr_t retaddr)
{
    MIPSCPU *cpu = MIPS_CPU(cs);
    CPUMIPSState *env = &cpu->env;
#if !defined(CONFIG_USER_ONLY)
    hwaddr physical;
    int prot;
    int mips_access_type;
#endif
    int ret = TLBRET_BADADDR;

    /* data access */
#if !defined(CONFIG_USER_ONLY)
    /* XXX: put correct access by using cpu_restore_state() correctly */
    mips_access_type = ACCESS_INT;
    ret = get_physical_address(env, &physical, &prot, address,
                               access_type, mips_access_type, mmu_idx);
    switch (ret) {
    case TLBRET_MATCH:
        qemu_log_mask(CPU_LOG_MMU,
                      "%s address=%" VADDR_PRIx " physical " TARGET_FMT_plx
                      " prot %d\n", __func__, address, physical, prot);
        break;
    default:
        qemu_log_mask(CPU_LOG_MMU,
                      "%s address=%" VADDR_PRIx " ret %d\n", __func__, address,
                      ret);
        break;
    }
    if (ret == TLBRET_MATCH) {
        tlb_set_page(cs, address & TARGET_PAGE_MASK,
                     physical & TARGET_PAGE_MASK, prot,
                     mmu_idx, TARGET_PAGE_SIZE);
        return true;
    }
#if !defined(TARGET_MIPS64)
    if ((ret == TLBRET_NOMATCH) && (env->tlb->nb_tlb > 1)) {
        /*
         * Memory reads during hardware page table walking are performed
         * as if they were kernel-mode load instructions.
         */
        int mode = (env->hflags & MIPS_HFLAG_KSU);
        bool ret_walker;
        env->hflags &= ~MIPS_HFLAG_KSU;
        ret_walker = page_table_walk_refill(env, address, access_type, mmu_idx);
        env->hflags |= mode;
        if (ret_walker) {
            ret = get_physical_address(env, &physical, &prot, address,
                                       access_type, mips_access_type, mmu_idx);
            if (ret == TLBRET_MATCH) {
                tlb_set_page(cs, address & TARGET_PAGE_MASK,
                             physical & TARGET_PAGE_MASK, prot,
                             mmu_idx, TARGET_PAGE_SIZE);
                return true;
            }
        }
    }
#endif
    if (probe) {
        return false;
    }
#endif

#ifdef TARGET_CHERI
    raise_mmu_exception(env, address, access_type, ret, 0xff);
#else
    raise_mmu_exception(env, address, access_type, ret);
#endif
    do_raise_exception_err(env, cs->exception_index, env->error_code, retaddr);
}

#ifndef CONFIG_USER_ONLY
hwaddr cpu_mips_translate_address(CPUMIPSState *env, target_ulong address,
                                  int rw)
{
    hwaddr physical;
    int prot;
    int access_type;
    int ret = 0;

    /* data access */
    access_type = ACCESS_INT;
    ret = get_physical_address(env, &physical, &prot, address, rw, access_type,
                               cpu_mmu_index(env, false));
    if (ret != TLBRET_MATCH) {
#ifdef TARGET_CHERI
        raise_mmu_exception(env, address, rw, ret, 0xff);
#else
        raise_mmu_exception(env, address, rw, ret);
#endif
        return -1LL;
    } else {
        return physical;
    }
}

#ifdef TARGET_CHERI
static hwaddr cpu_mips_translate_address_c2(CPUMIPSState *env, target_ulong address, int rw, int reg, int *prot)
{
    hwaddr physical;
    int access_type;
    int ret = 0;

    /* data access */
    access_type = ACCESS_INT;
    ret = get_physical_address(env, &physical, prot,
                               address, rw, access_type,
                               cpu_mmu_index(env, false));
    if (ret != TLBRET_MATCH) {
        raise_mmu_exception(env, address, rw, ret, reg);
        return -1LL;
    } else {
        return physical;
    }
}
#endif /* TARGET_CHERI */

static const char * const excp_names[EXCP_LAST + 1] = {
    [EXCP_RESET] = "reset",
    [EXCP_SRESET] = "soft reset",
    [EXCP_DSS] = "debug single step",
    [EXCP_DINT] = "debug interrupt",
    [EXCP_NMI] = "non-maskable interrupt",
    [EXCP_MCHECK] = "machine check",
    [EXCP_EXT_INTERRUPT] = "interrupt",
    [EXCP_DFWATCH] = "deferred watchpoint",
    [EXCP_DIB] = "debug instruction breakpoint",
    [EXCP_IWATCH] = "instruction fetch watchpoint",
    [EXCP_AdEL] = "address error load",
    [EXCP_AdES] = "address error store",
    [EXCP_TLBF] = "TLB refill",
    [EXCP_IBE] = "instruction bus error",
    [EXCP_DBp] = "debug breakpoint",
    [EXCP_SYSCALL] = "syscall",
    [EXCP_BREAK] = "break",
    [EXCP_CpU] = "coprocessor unusable",
    [EXCP_RI] = "reserved instruction",
    [EXCP_OVERFLOW] = "arithmetic overflow",
    [EXCP_TRAP] = "trap",
    [EXCP_FPE] = "floating point",
    [EXCP_DDBS] = "debug data break store",
    [EXCP_DWATCH] = "data watchpoint",
    [EXCP_LTLBL] = "TLB modify",
    [EXCP_TLBL] = "TLB load",
    [EXCP_TLBS] = "TLB store",
    [EXCP_DBE] = "data bus error",
    [EXCP_DDBL] = "debug data break load",
    [EXCP_THREAD] = "thread",
    [EXCP_MDMX] = "MDMX",
    [EXCP_C2E] = "precise coprocessor 2",
    [EXCP_CACHE] = "cache error",
    [EXCP_TLBXI] = "TLB execute-inhibit",
    [EXCP_TLBRI] = "TLB read-inhibit",
    [EXCP_MSADIS] = "MSA disabled",
    [EXCP_MSAFPE] = "MSA floating point",
};
#endif

target_ulong exception_resume_pc(CPUMIPSState *env)
{
    target_ulong bad_pc;
    target_ulong isa_mode;

    isa_mode = !!(env->hflags & MIPS_HFLAG_M16);
    bad_pc = env->active_tc.PC | isa_mode;
    if (env->hflags & MIPS_HFLAG_BMASK) {
        /*
         * If the exception was raised from a delay slot, come back to
         * the jump.
         */
        bad_pc -= (env->hflags & MIPS_HFLAG_B16 ? 2 : 4);
    }

    return bad_pc;
}

#if !defined(CONFIG_USER_ONLY)
static void set_hflags_for_handler(CPUMIPSState *env)
{
    /* Exception handlers are entered in 32-bit mode.  */
    env->hflags &= ~(MIPS_HFLAG_M16);
    /* ...except that microMIPS lets you choose.  */
    if (env->insn_flags & ASE_MICROMIPS) {
        env->hflags |= (!!(env->CP0_Config3 &
                           (1 << CP0C3_ISA_ON_EXC))
                        << MIPS_HFLAG_M16_SHIFT);
    }
}

static inline void set_badinstr_registers(CPUMIPSState *env)
{
    if (env->insn_flags & ISA_NANOMIPS32) {
        if (env->CP0_Config3 & (1 << CP0C3_BI)) {
            uint32_t instr = (cpu_lduw_code(env, env->active_tc.PC)) << 16;
            if ((instr & 0x10000000) == 0) {
                instr |= cpu_lduw_code(env, env->active_tc.PC + 2);
            }
            env->CP0_BadInstr = instr;

            if ((instr & 0xFC000000) == 0x60000000) {
                instr = cpu_lduw_code(env, env->active_tc.PC + 4) << 16;
                env->CP0_BadInstrX = instr;
            }
        }
        return;
    }

    if (env->hflags & MIPS_HFLAG_M16) {
        /* TODO: add BadInstr support for microMIPS */
        return;
    }
    if (env->CP0_Config3 & (1 << CP0C3_BI)) {
        env->CP0_BadInstr = cpu_ldl_code(env, env->active_tc.PC);
    }
    if ((env->CP0_Config3 & (1 << CP0C3_BP)) &&
        (env->hflags & MIPS_HFLAG_BMASK)) {
        env->CP0_BadInstrP = cpu_ldl_code(env, env->active_tc.PC - 4);
    }
}
#endif

#ifdef CONFIG_MIPS_LOG_INSTR
extern void helper_dump_changed_state(CPUMIPSState *env);
#endif

void mips_cpu_do_interrupt(CPUState *cs)
{
#if !defined(CONFIG_USER_ONLY)
    MIPSCPU *cpu = MIPS_CPU(cs);
    CPUMIPSState *env = &cpu->env;
    bool update_badinstr = 0;
    target_ulong offset;
    int cause = -1;
    const char *name = "";

    if ((qemu_loglevel_mask(CPU_LOG_INT) || qemu_loglevel_mask(CPU_LOG_INSTR))
        && cs->exception_index != EXCP_EXT_INTERRUPT) {
        if (cs->exception_index < 0 || cs->exception_index > EXCP_LAST) {
            name = "unknown";
        } else {
            name = excp_names[cs->exception_index];
        }

        qemu_log("%s enter: PC " TARGET_FMT_lx " EPC " TARGET_FMT_lx
                 " %s exception, (hflags & MIPS_HFLAG_BMASK)=%x\n", __func__, env->active_tc.PC, get_CP0_EPC(env),
                 name, env->hflags & MIPS_HFLAG_BMASK);
#ifdef TARGET_CHERI
        qemu_log("\tPCC=" PRINT_CAP_FMTSTR "\n\tKCC= " PRINT_CAP_FMTSTR "\n\tEPCC=" PRINT_CAP_FMTSTR "\n",
                 PRINT_CAP_ARGS(&env->active_tc.PCC), PRINT_CAP_ARGS(&env->active_tc.CHWR.KCC),
                 PRINT_CAP_ARGS(&env->active_tc.CHWR.EPCC));
#endif
    }
#ifdef CONFIG_MIPS_LOG_INSTR
    if (unlikely(qemu_loglevel_mask(CPU_LOG_INSTR | CPU_LOG_CVTRACE | CPU_LOG_USER_ONLY)
        || env->user_only_tracing_enabled)) {
        helper_dump_changed_state(env);
    }
#endif /* CONFIG_MIPS_LOG_INSTR */
    if (cs->exception_index == EXCP_EXT_INTERRUPT &&
        (env->hflags & MIPS_HFLAG_DM)) {
        cs->exception_index = EXCP_DINT;
    }
    offset = 0x180;
    switch (cs->exception_index) {
    case EXCP_DSS:
        env->CP0_Debug |= 1 << CP0DB_DSS;
        /*
         * Debug single step cannot be raised inside a delay slot and
         * resume will always occur on the next instruction
         * (but we assume the pc has always been updated during
         * code translation).
         */
        env->CP0_DEPC = env->active_tc.PC | !!(env->hflags & MIPS_HFLAG_M16);
        goto enter_debug_mode;
    case EXCP_DINT:
        env->CP0_Debug |= 1 << CP0DB_DINT;
        goto set_DEPC;
    case EXCP_DIB:
        env->CP0_Debug |= 1 << CP0DB_DIB;
        goto set_DEPC;
    case EXCP_DBp:
        env->CP0_Debug |= 1 << CP0DB_DBp;
        /* Setup DExcCode - SDBBP instruction */
        env->CP0_Debug = (env->CP0_Debug & ~(0x1fULL << CP0DB_DEC)) |
                         (9 << CP0DB_DEC);
        goto set_DEPC;
    case EXCP_DDBS:
        env->CP0_Debug |= 1 << CP0DB_DDBS;
        goto set_DEPC;
    case EXCP_DDBL:
        env->CP0_Debug |= 1 << CP0DB_DDBL;
    set_DEPC:
        env->CP0_DEPC = exception_resume_pc(env);
        env->hflags &= ~MIPS_HFLAG_BMASK;
 enter_debug_mode:
        if (env->insn_flags & ISA_MIPS3) {
            env->hflags |= MIPS_HFLAG_64;
            if (!(env->insn_flags & ISA_MIPS64R6) ||
                env->CP0_Status & (1 << CP0St_KX)) {
                env->hflags &= ~MIPS_HFLAG_AWRAP;
            }
        }
        env->hflags |= MIPS_HFLAG_DM | MIPS_HFLAG_CP0;
        env->hflags &= ~(MIPS_HFLAG_KSU);
        /* EJTAG probe trap enable is not implemented... */
        if (!(env->CP0_Status & (1 << CP0St_EXL))) {
            env->CP0_Cause &= ~(1U << CP0Ca_BD);
        }
        env->active_tc.PC = env->exception_base + 0x480;
        set_hflags_for_handler(env);
        break;
    case EXCP_RESET:
        cpu_reset(CPU(cpu));
        break;
    case EXCP_SRESET:
        env->CP0_Status |= (1 << CP0St_SR);
        memset(env->CP0_WatchLo, 0, sizeof(env->CP0_WatchLo));
        goto set_error_EPC;
    case EXCP_NMI:
        env->CP0_Status |= (1 << CP0St_NMI);
 set_error_EPC:
#ifdef TARGET_CHERI
        // qemu_log("%s: ErrorEPC <- " TARGET_FMT_lx "\n", __func__,
        // exception_resume_pc(env));
        // qemu_log("%s: EPCC <- PCC and PCC <- KCC\n", __func__);
        env->active_tc.CHWR.ErrorEPCC = env->active_tc.PCC;
        // Note: set_CP0_ErrorEPC() handles the special cases of sealed/unrep EPCC
#endif /* TARGET_CHERI */
        set_CP0_ErrorEPC(env, exception_resume_pc(env));
        env->hflags &= ~MIPS_HFLAG_BMASK;
        env->CP0_Status |= (1 << CP0St_ERL) | (1 << CP0St_BEV);
        if (env->insn_flags & ISA_MIPS3) {
            env->hflags |= MIPS_HFLAG_64;
            if (!(env->insn_flags & ISA_MIPS64R6) ||
                env->CP0_Status & (1 << CP0St_KX)) {
                env->hflags &= ~MIPS_HFLAG_AWRAP;
            }
        }
        env->hflags |= MIPS_HFLAG_CP0;
        env->hflags &= ~(MIPS_HFLAG_KSU);
        if (!(env->CP0_Status & (1 << CP0St_EXL))) {
            env->CP0_Cause &= ~(1U << CP0Ca_BD);
        }
        env->active_tc.PC = env->exception_base;
        set_hflags_for_handler(env);
        break;
    case EXCP_EXT_INTERRUPT:
        cause = 0;
        if (env->CP0_Cause & (1 << CP0Ca_IV)) {
            uint32_t spacing = (env->CP0_IntCtl >> CP0IntCtl_VS) & 0x1f;

            if ((env->CP0_Status & (1 << CP0St_BEV)) || spacing == 0) {
                offset = 0x200;
            } else {
                uint32_t vector = 0;
                uint32_t pending = (env->CP0_Cause & CP0Ca_IP_mask) >> CP0Ca_IP;

                if (env->CP0_Config3 & (1 << CP0C3_VEIC)) {
                    /*
                     * For VEIC mode, the external interrupt controller feeds
                     * the vector through the CP0Cause IP lines.
                     */
                    vector = pending;
                } else {
                    /*
                     * Vectored Interrupts
                     * Mask with Status.IM7-IM0 to get enabled interrupts.
                     */
                    pending &= (env->CP0_Status >> CP0St_IM) & 0xff;
                    /* Find the highest-priority interrupt. */
                    while (pending >>= 1) {
                        vector++;
                    }
                }
                offset = 0x200 + (vector * (spacing << 5));
            }
        }
        goto set_EPC;
    case EXCP_LTLBL:
        cause = 1;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
        goto set_EPC;
    case EXCP_TLBL:
        cause = 2;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
        if ((env->error_code & EXCP_TLB_NOMATCH) &&
            !(env->CP0_Status & (1 << CP0St_EXL))) {
#if defined(TARGET_MIPS64)
            int R = env->CP0_BadVAddr >> 62;
            int UX = (env->CP0_Status & (1 << CP0St_UX)) != 0;
            int KX = (env->CP0_Status & (1 << CP0St_KX)) != 0;

            if ((R != 0 || UX) && (R != 3 || KX) &&
                (!(env->insn_flags & (INSN_LOONGSON2E | INSN_LOONGSON2F)))) {
                offset = 0x080;
            } else {
#endif
                offset = 0x000;
#if defined(TARGET_MIPS64)
            }
#endif
        }
        goto set_EPC;
    case EXCP_TLBS:
        cause = 3;
        update_badinstr = 1;
        if ((env->error_code & EXCP_TLB_NOMATCH) &&
            !(env->CP0_Status & (1 << CP0St_EXL))) {
#if defined(TARGET_MIPS64)
            int R = env->CP0_BadVAddr >> 62;
            int UX = (env->CP0_Status & (1 << CP0St_UX)) != 0;
            int KX = (env->CP0_Status & (1 << CP0St_KX)) != 0;

            if ((R != 0 || UX) && (R != 3 || KX) &&
                (!(env->insn_flags & (INSN_LOONGSON2E | INSN_LOONGSON2F)))) {
                offset = 0x080;
            } else {
#endif
                offset = 0x000;
#if defined(TARGET_MIPS64)
            }
#endif
        }
        goto set_EPC;
    case EXCP_AdEL:
        cause = 4;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
        goto set_EPC;
    case EXCP_AdES:
        cause = 5;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_IBE:
        update_badinstr = 0;
        cause = 6;
        goto set_EPC;
    case EXCP_DBE:
        cause = 7;
        goto set_EPC;
    case EXCP_SYSCALL:
        cause = 8;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_BREAK:
        cause = 9;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_RI:
        cause = 10;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_CpU:
        cause = 11;
        update_badinstr = 1;
        env->CP0_Cause = (env->CP0_Cause & ~(0x3 << CP0Ca_CE)) |
                         (env->error_code << CP0Ca_CE);
        goto set_EPC;
    case EXCP_OVERFLOW:
        cause = 12;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_TRAP:
        cause = 13;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_MSAFPE:
        cause = 14;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_FPE:
        cause = 15;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_C2E:
        cause = 18;
        update_badinstr = !(env->error_code & EXCP_INST_NOTAVAIL);
#ifdef TARGET_CHERI
        // FIXME: why do we always clear ERL?
        env->CP0_Status &= ~(1 << CP0St_ERL);
        if ((env->CP2_CapCause >> 8) == CP2Ca_CALL ||
                (env->CP2_CapCause >> 8) == CP2Ca_RETURN)
            offset = 0x280;
#endif /* TARGET_CHERI */
        goto set_EPC;
    case EXCP_TLBRI:
        cause = 19;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_TLBXI:
        cause = 20;
        goto set_EPC;
    case EXCP_MSADIS:
        cause = 21;
        update_badinstr = 1;
        goto set_EPC;
    case EXCP_MDMX:
        cause = 22;
        goto set_EPC;
    case EXCP_DWATCH:
        cause = 23;
        /* XXX: TODO: manage deferred watch exceptions */
        goto set_EPC;
    case EXCP_MCHECK:
        cause = 24;
        goto set_EPC;
    case EXCP_THREAD:
        cause = 25;
        goto set_EPC;
    case EXCP_DSPDIS:
        cause = 26;
        goto set_EPC;
    case EXCP_CACHE:
        cause = 30;
        offset = 0x100;
 set_EPC:
        if (!(env->CP0_Status & (1 << CP0St_EXL))) {
#ifdef TARGET_CHERI
            // qemu_log("%s: EPC <- " TARGET_FMT_lx "\n", __func__,
            //  exception_resume_pc(env));
            // qemu_log("%s: EPCC <- PCC and PCC <- KCC\n", __func__);

            env->active_tc.CHWR.EPCC = env->active_tc.PCC;
            // Note: set_CP0_EPC() handles the special cases of sealed/unrep EPCC
#endif /* TARGET_CHERI */
            set_CP0_EPC(env, exception_resume_pc(env));
            if (update_badinstr) {
                set_badinstr_registers(env);
            }
            if (env->hflags & MIPS_HFLAG_BMASK) {
                env->CP0_Cause |= (1U << CP0Ca_BD);
            } else {
                env->CP0_Cause &= ~(1U << CP0Ca_BD);
            }
            env->CP0_Status |= (1 << CP0St_EXL);
            if (env->insn_flags & ISA_MIPS3) {
                env->hflags |= MIPS_HFLAG_64;
                if (!(env->insn_flags & ISA_MIPS64R6) ||
                    env->CP0_Status & (1 << CP0St_KX)) {
                    env->hflags &= ~MIPS_HFLAG_AWRAP;
                }
            }
            env->hflags |= MIPS_HFLAG_CP0;
            env->hflags &= ~(MIPS_HFLAG_KSU);
        }
        env->hflags &= ~MIPS_HFLAG_BMASK;
        if (env->CP0_Status & (1 << CP0St_BEV)) {
            env->active_tc.PC = env->exception_base + 0x200;
        } else if (cause == 30 && !(env->CP0_Config3 & (1 << CP0C3_SC) &&
                                    env->CP0_Config5 & (1 << CP0C5_CV))) {
            /* Force KSeg1 for cache errors */
            env->active_tc.PC = KSEG1_BASE | (env->CP0_EBase & 0x1FFFF000);
        } else {
            env->active_tc.PC = env->CP0_EBase & ~0xfff;
        }

        env->active_tc.PC += offset;
        set_hflags_for_handler(env);
        env->CP0_Cause = (env->CP0_Cause & ~(0x1f << CP0Ca_EC)) |
                         (cause << CP0Ca_EC);
        break;
    default:
        abort();
    }

#ifdef TARGET_CHERI
        /* always set PCC from KCC even with EXL */
        env->active_tc.PCC = env->active_tc.CHWR.KCC;
        // FIXME: KCC must not be sealed
        if (!cap_is_unsealed(&env->active_tc.CHWR.KCC)) {
            error_report("Sealed KCC in exception, detagging: " PRINT_CAP_FMTSTR "\r", PRINT_CAP_ARGS(&env->active_tc.CHWR.KCC));
            env->active_tc.PCC.cr_tag = false;
        }
        env->active_tc.PCC._cr_cursor =  env->active_tc.PC;
#endif /* TARGET_CHERI */

#ifdef CONFIG_MIPS_LOG_INSTR
    if (unlikely(qemu_loglevel_mask(CPU_LOG_INSTR))) {
        if (cs->exception_index == EXCP_EXT_INTERRUPT)
            fprintf (qemu_logfile, "--- Interrupt, vector " TARGET_FMT_lx "\n",
                    env->active_tc.PC);
        else
            fprintf (qemu_logfile, "--- Exception #%u: %s, vector "
                    TARGET_FMT_lx "\n", cause, name, env->active_tc.PC);
    }
    if (unlikely(qemu_loglevel_mask(CPU_LOG_CVTRACE))) {
        env->cvtrace.exception = cause;
    }
#ifdef TARGET_CHERI
    if (unlikely(qemu_loglevel_mask(CPU_LOG_INSTR))) {
         // Print the new PCC value for debugging traces.
         cap_register_t tmp;
         // We use a null cap as oldreg so that we always print it.
         null_capability(&tmp);
         dump_changed_capreg(env, &env->active_tc.PCC, &tmp, "PCC");
    }
#endif /* TARGET_CHERI */
#endif /* CONFIG_MIPS_LOG_INSTR */
    if (qemu_loglevel_mask(CPU_LOG_INT | CPU_LOG_INSTR)
        && cs->exception_index != EXCP_EXT_INTERRUPT) {
        qemu_log("%s: PC " TARGET_FMT_lx " EPC " TARGET_FMT_lx " cause %d\n"
                 "    S %08x C %08x A " TARGET_FMT_lx " D " TARGET_FMT_lx "\n",
                 __func__, env->active_tc.PC, get_CP0_EPC(env), cause,
                 env->CP0_Status, env->CP0_Cause, env->CP0_BadVAddr,
                 env->CP0_DEPC);
#if defined(TARGET_CHERI) && defined(CONFIG_MIPS_LOG_INSTR)
        qemu_log("ErrorEPC " TARGET_FMT_lx "\n", get_CP0_ErrorEPC(env));
        cap_register_t tmp;
        // We use a null cap as oldreg so that we always print it.
        null_capability(&tmp);
        dump_changed_capreg(env, &env->active_tc.CHWR.EPCC, &tmp, "EPCC");
        null_capability(&tmp);
        dump_changed_capreg(env, &env->active_tc.CHWR.ErrorEPCC, &tmp, "ErrorEPCC");
#endif
    }
#endif
    cs->exception_index = EXCP_NONE;
#ifdef TARGET_CHERI
    assert(cap_get_cursor(&env->active_tc.PCC) == env->active_tc.PC);
#endif
}

bool mips_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    if (interrupt_request & CPU_INTERRUPT_HARD) {
        MIPSCPU *cpu = MIPS_CPU(cs);
        CPUMIPSState *env = &cpu->env;

        if (cpu_mips_hw_interrupts_enabled(env) &&
            cpu_mips_hw_interrupts_pending(env)) {
            /* Raise it */
            cs->exception_index = EXCP_EXT_INTERRUPT;
            env->error_code = 0;
            mips_cpu_do_interrupt(cs);
            return true;
        }
    }
    return false;
}

#if !defined(CONFIG_USER_ONLY)
void r4k_invalidate_tlb(CPUMIPSState *env, int idx, int use_extra)
{
    CPUState *cs = env_cpu(env);
    r4k_tlb_t *tlb;
    target_ulong addr;
    target_ulong end;
    uint16_t ASID = env->CP0_EntryHi & env->CP0_EntryHi_ASID_mask;
    target_ulong mask;

    tlb = &env->tlb->mmu.r4k.tlb[idx];
    /*
     * The qemu TLB is flushed when the ASID changes, so no need to
     * flush these entries again.
     */
    if (tlb->G == 0 && tlb->ASID != ASID) {
        return;
    }

    if (use_extra && env->tlb->tlb_in_use < MIPS_TLB_MAX) {
        /*
         * For tlbwr, we can shadow the discarded entry into
         * a new (fake) TLB entry, as long as the guest can not
         * tell that it's there.
         */
        env->tlb->mmu.r4k.tlb[env->tlb->tlb_in_use] = *tlb;
        env->tlb->tlb_in_use++;
        return;
    }

    /* 1k pages are not supported. */
    mask = tlb->PageMask | ~(TARGET_PAGE_MASK << 1);
    if (tlb->V0) {
        addr = tlb->VPN & ~mask;
#if defined(TARGET_MIPS64)
        if (addr >= (0xFFFFFFFF80000000ULL & env->SEGMask)) {
            addr |= 0x3FFFFF0000000000ULL;
        }
#endif
        end = addr | (mask >> 1);
        while (addr < end) {
            tlb_flush_page(cs, addr);
            addr += TARGET_PAGE_SIZE;
        }
    }
    if (tlb->V1) {
        addr = (tlb->VPN & ~mask) | ((mask >> 1) + 1);
#if defined(TARGET_MIPS64)
        if (addr >= (0xFFFFFFFF80000000ULL & env->SEGMask)) {
            addr |= 0x3FFFFF0000000000ULL;
        }
#endif
        end = addr | mask;
        while (addr - 1 < end) {
            tlb_flush_page(cs, addr);
            addr += TARGET_PAGE_SIZE;
        }
    }
}
#endif

#if defined(TARGET_CHERI)
/*
 * Tagged Memory Emulation
 *
 * CHERI requires a 1-bit tag for every capability-aligned,
 * capability-sized word in physical memory.  This allows capabilities
 * to be safely loaded and stored in meory without loss of integrity.
 *
 * For emulation purposes the tag is an one-byte flag.  This makes it
 * easy to set or unset a tag without the need of locking or atomics.
 * This requires eight times the memory, however.  To reduce the amount
 * of memory needed the tag flag array is allocated sparsely, 4K at at
 * time, and on demand.
 *
 * XXX Should consider adding a reference count per tag block so that
 * blocks can be deallocated when no longer used maybe.
 */

#if defined(CHERI_MAGIC128) || defined(CHERI_128)
#define CAP_TAG_SHFT        4           // 5 for 256-bit caps, 4 for 128-bit
#else /* !(CHERI_MAGIC128 || CHERI_128) */
#define CAP_TAG_SHFT        5           // 5 for 256-bit caps, 4 for 128-bit
#endif /* !(CHERI_MAGIC128 || CHERI_128) */
#define CAP_SIZE            (1 << CAP_TAG_SHFT)
#define CAP_MASK            ((1 << CAP_TAG_SHFT) - 1)
#define CAP_TAGBLK_SHFT     12          // 2^12 or 4096 tags per block
#define CAP_TAGBLK_MSK      ((1 << CAP_TAGBLK_SHFT) - 1)
#ifdef CHERI_MAGIC128
         /*
          * With "magic 128-bit" capabilities the object type,
          * permissions, sealed bit, and length are all stored in tag
          * memory along with the tag. This makes the tag memory as
          * large as main memory. Fortunately, for this implementation
          * tags are not needed everywhere and sparsely allocated.
          */
#define CAP_TAGBLK_SZ       ((1 << CAP_TAGBLK_SHFT) * 16)
#define CAP_TAGBLK_IDX(tag_idx) (((tag_idx) & CAP_TAGBLK_MSK) * 16)
#if defined(HOST_WORDS_BIGENDIAN)
#   define CAP_TAG_TPS_SHFT 0
#else
#   define CAP_TAG_TPS_SHFT 8
#endif
#else /* ! CHERI_MAGIC128 */
#define CAP_TAGBLK_SZ       (1 << CAP_TAGBLK_SHFT)
#define CAP_TAGBLK_IDX(tag_idx) ((tag_idx) & CAP_TAGBLK_MSK)
#endif /* ! CHERI_MAGIC128 */

uint8_t **_cheri_tagmem = NULL;
uint64_t cheri_ntagblks = 0ul;

static inline uint8_t* get_cheri_tagmem(size_t index) {
    assert(index < cheri_ntagblks && "Tag index out of bounds");
    return _cheri_tagmem[index];
}

void cheri_tag_init(uint64_t memory_size)
{
    // printf("%s: memory_size=0x%lx\n", __func__, memory_size);
    if (_cheri_tagmem != NULL)
        return;

    cheri_ntagblks = (memory_size >> CAP_TAG_SHFT) >> CAP_TAGBLK_SHFT;
    _cheri_tagmem = (uint8_t **)g_malloc0(cheri_ntagblks * sizeof(uint8_t *));
    if (_cheri_tagmem == NULL) {
        error_report("%s: Can't allocated tag memory", __func__);
        exit (-1);
    }
}

static inline hwaddr v2p_addr(CPUMIPSState *env, target_ulong vaddr, int rw,
        int reg, uintptr_t pc, int *prot)
{
    hwaddr paddr;

    paddr = cpu_mips_translate_address_c2(env, vaddr, rw, reg, prot);

    if (paddr == -1LL) {
        cpu_loop_exit_restore(env_cpu(env), pc);
    } else {
        return paddr;
    }
}

static inline void check_tagmem_writable(CPUMIPSState *env, target_ulong vaddr,
                                         hwaddr paddr, ram_addr_t ram_addr, MemoryRegion *mr, uintptr_t pc)
{
    if (memory_region_is_rom(mr) || memory_region_is_romd(mr)) {
        error_report("QEMU ERROR: attempting change clear tag bit on read-only memory:");
        error_report("%s: vaddr=0x%jx -> ram_addr=0x%jx (paddr=0x%jx)", __func__,
            (uintmax_t)vaddr, (uintmax_t)ram_addr, (uintmax_t)paddr);
        do_raise_c0_exception_impl(env, EXCP_DBE, 0, pc);
    }
}

static inline ram_addr_t p2r_addr(CPUMIPSState *env, hwaddr addr, MemoryRegion** mrp)
{
    hwaddr l;
    MemoryRegion *mr;
    CPUState *cs = env_cpu(env);

    mr = address_space_translate(cs->as, addr, &addr, &l, false, MEMTXATTRS_UNSPECIFIED);
    if (mrp)
        *mrp = mr;

    if (!(memory_region_is_ram(mr) || memory_region_is_romd(mr))) {
        return -1LL;
    }
    return (memory_region_get_ram_addr(mr) & TARGET_PAGE_MASK) + addr;
}

static inline ram_addr_t v2r_addr(CPUMIPSState *env, target_ulong vaddr, MMUAccessType rw,
        int reg, uintptr_t pc)
{
    int prot;
    MemoryRegion* mr = NULL;
    hwaddr paddr = v2p_addr(env, vaddr, rw, reg, pc, &prot);
    ram_addr_t ram_addr = p2r_addr(env, paddr, &mr);
    if (rw == MMU_DATA_CAP_STORE || rw == MMU_DATA_STORE)
        check_tagmem_writable(env, vaddr, paddr, ram_addr, mr, pc);
    return ram_addr;
}

void cheri_tag_invalidate(CPUMIPSState *env, target_ulong vaddr, int32_t size, uintptr_t pc)
{
    // This must not cross a page boundary since we are only translating once!
    assert(size > 0);
    if (((vaddr & TARGET_PAGE_MASK) != ((vaddr + size - 1) & TARGET_PAGE_MASK))) {
#ifdef CHERI_UNALIGNED
        // this can happen with unaligned stores
        if (size == 2 || size == 4 || size == 8) {
            warn_report("Got unaligned load in %d-byte store across page boundary at 0x" TARGET_FMT_lx "\r\n", size, vaddr);
            size_t remaining_in_page = TARGET_PAGE_SIZE - (vaddr & ~TARGET_PAGE_MASK);
            assert(remaining_in_page < (size_t)size);
            // invalidate tags for both pages (two lookups required!)
            cheri_tag_invalidate(env, vaddr, remaining_in_page, pc);
            cheri_tag_invalidate(env, vaddr + remaining_in_page, size - remaining_in_page, pc);
            return;
        }
#endif


        int isa = (env->hflags & MIPS_HFLAG_M16) == 0 ? 0 : (env->insn_flags & ASE_MICROMIPS) ? 1 : 2;
        /* Disassemble and print instruction. */
        qemu_log_flush();
        error_report("FATAL: %s: " TARGET_FMT_lx "+%d crosses a page boundary\r", __func__, vaddr, size);
        char buffer[256];
        FILE* f = fmemopen(buffer, sizeof(buffer), "w");
        assert(f);
        fprintf(f, "Probably caused by guest instruction: ");
        target_disas(f, env_cpu(env),
                     cap_get_cursor(&env->active_tc.PCC), isa == 0 ? 4 : 2);
        fprintf(f, "\r");
        fclose(f);
        buffer[sizeof(buffer) - 1] = '\0';
        error_report("%s", buffer);
        exit(1);
    }

    /*
     * When resolving this address in the TLB, treat it like a data store
     * (MMU_DATA_STORE) rather than a capability store (MMU_DATA_CAP_STORE),
     * so that we don't require that the SC inhibit be clear.
     */

    int prot;
    MemoryRegion* mr = NULL;
    hwaddr paddr = v2p_addr(env, vaddr, MMU_DATA_STORE, 0xFF, pc, &prot);
    ram_addr_t ram_addr = p2r_addr(env, paddr, &mr);
    // Generate a trap if we try to clear tags in ROM instead of crashing
    check_tagmem_writable(env, vaddr, paddr, ram_addr, mr, pc);
    if (ram_addr == -1LL)
        return;

    cheri_tag_phys_invalidate(ram_addr, size);

    /* Check RAM address to see if the linkedflag needs to be reset. */
    if (env->lladdr > paddr && env->lladdr < paddr + size)
        env->linkedflag = 0;
}

void cheri_tag_phys_invalidate(ram_addr_t ram_addr, ram_addr_t len)
{
    uint64_t tag, addr, endaddr, tagmem_idx;
    uint8_t *tagblk;

    endaddr = (uint64_t)(ram_addr + len);

    for(addr = (uint64_t)(ram_addr & ~CAP_MASK); addr < endaddr;
            addr += CAP_SIZE) {
        tag = addr >> CAP_TAG_SHFT;
        tagmem_idx = tag >> CAP_TAGBLK_SHFT;
        if (tagmem_idx > cheri_ntagblks)
            return;
        tagblk = get_cheri_tagmem(tagmem_idx);

        if (tagblk != NULL) {
            if (unlikely(qemu_loglevel_mask(CPU_LOG_INSTR))) {
                qemu_log("    Cap Tag Write [%" HWADDR_PRIx "] %d -> 0\n", addr,
                         tagblk[CAP_TAGBLK_IDX(tag)]);
            }
            tagblk[CAP_TAGBLK_IDX(tag)] = 0;
        }
    }

    /* XXX - linkedflag reset check? */
}

static uint8_t *cheri_tag_new_tagblk(uint64_t tag)
{
    uint8_t *tagblk, *old;

    tagblk = g_malloc0(CAP_TAGBLK_SZ);
    if (tagblk == NULL) {
        error_report("Can't allocate tag block.");
        exit(1);
    }

    /* Possible race here so use atomic compare and swap. */
    assert((tag >> CAP_TAGBLK_SHFT) < cheri_ntagblks && "Tag index out of range");
    old = atomic_cmpxchg(&_cheri_tagmem[tag >> CAP_TAGBLK_SHFT],
            NULL, tagblk);
    if (old != NULL) {
        /* Lost the race, free. */
        g_free(tagblk);
        return old;
    } else {
        return tagblk;
    }
}

void cheri_tag_set(CPUMIPSState *env, target_ulong vaddr, int reg, uintptr_t pc)
{
    ram_addr_t ram_addr;
    uint64_t tag;
    uint8_t *tagblk;

    /*
     * This attempt to resolve a virtual address may cause both a data store
     * TLB fault (entry missing or D bit clear) and a capability store TLB
     * fault (SC bit set).  v2r_addr bypasses the generic QEMU TCG soft-TLB
     * (env->tlb_table and friends) but will populate the MIPS-specific TLB
     * (env->tlb->mmu.r4k.tlb).  Therefore, while a data store to vaddr
     * after a tag has been set might miss in the QEMU TCG soft-TLB, it
     * won't miss in the MIPS model and therefore won't raise a processor
     * exception (and will populate the QEMU TCG soft-TLB for subsequent
     * data stores).
     */

    ram_addr = v2r_addr(env, vaddr, MMU_DATA_CAP_STORE, reg, pc);
    if (ram_addr == -1LL)
        return;

    /* Get the tag number and tag block ptr. */
    tag = ram_addr >> CAP_TAG_SHFT;
    tagblk = get_cheri_tagmem(tag >> CAP_TAGBLK_SHFT);

    if (tagblk == NULL) {
        /* Allocated a tag block. */
        tagblk = cheri_tag_new_tagblk(tag);
    }
    if (unlikely(qemu_loglevel_mask(CPU_LOG_INSTR))) {
        qemu_log("    Cap Tag Write [" RAM_ADDR_FMT "] %d -> 1\n", ram_addr,
                 tagblk[CAP_TAGBLK_IDX(tag)]);
    }
    tagblk[CAP_TAGBLK_IDX(tag)] = 1;

    /* Check RAM address to see if the linkedflag needs to be reset. */
    if (ram_addr == p2r_addr(env, env->lladdr, NULL))
        env->linkedflag = 0;
}

static uint8_t *
cheri_tag_get_block(CPUMIPSState *env, target_ulong vaddr, MMUAccessType at,
    int reg, int xshift, uintptr_t pc,
    hwaddr *ret_paddr, ram_addr_t *ret_ram_addr, uint64_t *ret_tag, int *prot)
{
    hwaddr paddr;
    ram_addr_t ram_addr;
    uint64_t tag;

    paddr = v2p_addr(env, vaddr, at, reg, pc, prot);

    if (ret_paddr)
        *ret_paddr = paddr;

    ram_addr = p2r_addr(env, paddr, NULL);
    if (ram_addr == -1LL)
        return NULL;

    if (ret_ram_addr)
        *ret_ram_addr = ram_addr;

    /* Get the tag number and tag block ptr. */
    tag = (ram_addr >> (CAP_TAG_SHFT + xshift)) << xshift;
    if (ret_tag)
        *ret_tag = tag;

    return get_cheri_tagmem(tag >> CAP_TAGBLK_SHFT);
}

int cheri_tag_get(CPUMIPSState *env, target_ulong vaddr, int reg,
        hwaddr *ret_paddr, int *prot, uintptr_t pc)
{
    uint64_t tag;
    uint8_t *tagblk = cheri_tag_get_block(env, vaddr, MMU_DATA_CAP_LOAD, reg,
                                          0, pc, ret_paddr, NULL, &tag, prot);
    if (tagblk == NULL)
        return 0;
    else
        return tagblk[CAP_TAGBLK_IDX(tag)];
}

/* QEMU currently tells the kernel that there are no caches installed
 * (xref target/mips/translate_init.inc.c MIPS_CONFIG1 definition)
 * so we're kind of free to make up a line size here.  For simplicity,
 * we pretend that our cache lines always contain 8 capabilities.
 */
#define CAP_TAG_GET_MANY_SHFT    3
int cheri_tag_get_many(CPUMIPSState *env, target_ulong vaddr, int reg,
        hwaddr *ret_paddr, uintptr_t pc)
{
    uint64_t tag;
    int prot;
    uint8_t *tagblk = cheri_tag_get_block(env, vaddr, MMU_DATA_CAP_LOAD, reg,
                                          CAP_TAG_GET_MANY_SHFT, pc, ret_paddr,
                                          NULL, &tag, &prot);

    /*
     * XXX Right now, the sole consumer of this function is CLoadTags, and
     * we let it "see around" the TLB capability load inhibit.  That should
     * perhaps change?
     */
    (void) prot;

    if (tagblk == NULL)
        return 0;
    else {
#define TAG_BYTE_TO_BIT(ix) (tagblk[CAP_TAGBLK_IDX(tag+ix)] ? (1 << ix) : 0)
        return TAG_BYTE_TO_BIT(0)
             | TAG_BYTE_TO_BIT(1)
             | TAG_BYTE_TO_BIT(2)
             | TAG_BYTE_TO_BIT(3)
             | TAG_BYTE_TO_BIT(4)
             | TAG_BYTE_TO_BIT(5)
             | TAG_BYTE_TO_BIT(6)
             | TAG_BYTE_TO_BIT(7);
#undef TAG_BYTE_TO_BIT
    }
}

#ifdef CHERI_MAGIC128
void cheri_tag_set_m128(CPUMIPSState *env, target_ulong vaddr, int reg,
        uint8_t tagbit, uint64_t tps, uint64_t length, hwaddr *ret_paddr, uintptr_t pc)
{
    int prot;
    uint64_t tag;
    uint64_t *tagblk64;
    ram_addr_t ram_addr;

    // If the data is untagged we shouldn't get a tlb fault
    uint8_t *tagblk = cheri_tag_get_block(env, vaddr,
					  tagbit ? MMU_DATA_CAP_STORE : MMU_DATA_STORE,
					  reg, 0, pc, ret_paddr, &ram_addr, &tag, &prot);
    if (tagblk == NULL) {
        /* Allocated a tag block. */
        tagblk = cheri_tag_new_tagblk(tag);
    }
    tagblk64 = (uint64_t *)&tagblk[CAP_TAGBLK_IDX(tag)];
    *tagblk64 = (tps << CAP_TAG_TPS_SHFT) | tagbit;
    tagblk64++;
    *tagblk64 = length;


    /* Check RAM address to see if the linkedflag needs to be reset. */
    if (ram_addr == p2r_addr(env, env->lladdr, NULL))
        env->linkedflag = 0;

    return;
}

int cheri_tag_get_m128(CPUMIPSState *env, target_ulong vaddr, int reg,
        uint64_t *ret_tps, uint64_t *ret_length, hwaddr *ret_paddr, int *prot, uintptr_t pc)
{
    uint64_t tag;
    uint8_t *tagblk = cheri_tag_get_block(env, vaddr, MMU_DATA_CAP_LOAD, reg,
                                          0, pc, ret_paddr, NULL, &tag, prot);

    if (tagblk == NULL) {
        *ret_tps = *ret_length = 0ULL;
        return 0;
    } else {
        *ret_tps = (*(uint64_t *)&tagblk[CAP_TAGBLK_IDX(tag)]) >> CAP_TAG_TPS_SHFT;
        *ret_length = *(uint64_t *)&tagblk[CAP_TAGBLK_IDX(tag) + 8];
        return tagblk[CAP_TAGBLK_IDX(tag)];
    }
}
#endif /* CHERI_MAGIC128 */

#endif /* ! TARGET_CHERI */

void QEMU_NORETURN do_raise_exception_err(CPUMIPSState *env,
                                          uint32_t exception,
                                          int error_code,
                                          uintptr_t pc)
{
    CPUState *cs = env_cpu(env);

#ifdef TARGET_CHERI
    // Translate CP0 Unusable to CP2 ASR fault if PCC is missing ASR:
    if (exception == EXCP_CpU && error_code == 0) {
        if ((env->active_tc.PCC.cr_perms & CAP_ACCESS_SYS_REGS) == 0) {
            do_raise_c2_exception_noreg(env, CP2Ca_ACCESS_SYS_REGS, pc);
        }
    }
#endif
    qemu_log_mask(CPU_LOG_INT | CPU_LOG_INSTR, "%s: %d %d\n",
                  __func__, exception, error_code);
    cs->exception_index = exception;
    env->error_code = error_code;

    cpu_loop_exit_restore(cs, pc);
}

static void mips_cpu_add_definition(gpointer data, gpointer user_data)
{
    ObjectClass *oc = data;
    CpuDefinitionInfoList **cpu_list = user_data;
    CpuDefinitionInfoList *entry;
    CpuDefinitionInfo *info;
    const char *typename;

    typename = object_class_get_name(oc);
    info = g_malloc0(sizeof(*info));
    info->name = g_strndup(typename,
                           strlen(typename) - strlen("-" TYPE_MIPS_CPU));
    info->q_typename = g_strdup(typename);

    entry = g_malloc0(sizeof(*entry));
    entry->value = info;
    entry->next = *cpu_list;
    *cpu_list = entry;
}

CpuDefinitionInfoList *qmp_query_cpu_definitions(Error **errp)
{
    CpuDefinitionInfoList *cpu_list = NULL;
    GSList *list;

    list = object_class_get_list(TYPE_MIPS_CPU, false);
    g_slist_foreach(list, mips_cpu_add_definition, &cpu_list);
    g_slist_free(list);

    return cpu_list;
}
