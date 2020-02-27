/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2016 Stacey Son <sson@FreeBSD.org>
 * Copyright (c) 2016-2018 Alfredo Mazzinghi <am2419@cl.cam.ac.uk>
 * Copyright (c) 2016-2020 Alex Richardson <Alexander.Richardson@cl.cam.ac.uk>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#pragma once

#include "internal.h"
#include "cheri_defs.h"

#define CHERI_EXC_REGNUM_DDC 0 /* TODO: 32 */
#define CHERI_EXC_REGNUM_PCC 0xff

typedef enum CheriCapExc {
    CapEx_None                          = 0x0,  /* None */
    CapEx_LengthViolation               = 0x1,  /* Length Violation */
    CapEx_TagViolation                  = 0x2,  /* Tag Violation */
    CapEx_SealViolation                 = 0x3,  /* Seal Violation */
    CapEx_TypeViolation                 = 0x4,  /* Type Violation */
    CapEx_CallTrap                      = 0x5,  /* Call Trap */
    CapEx_ReturnTrap                    = 0x6,  /* Return Trap */
    CapEx_TSSUnderFlow                  = 0x7,  /* Underflow of trusted system stack */
    CapEx_UserDefViolation              = 0x8,  /* User-defined Permission Violation */
    CapEx_TLBNoStoreCap                 = 0x9,  /* TLB prohibits store capability */
    CapEx_InexactBounds                 = 0xA,  /* Bounds cannot be represented exactly */
    CapEx_UnalignedBase                 = 0xB,
    CapEx_CapLoadGen                    = 0xC,
    // 0x0D-0x0F Reserved
    CapEx_GlobalViolation               = 0x10,  /* Global Violation */
    CapEx_PermitExecuteViolation        = 0x11,  /* Permit_Execute Violation */
    CapEx_PermitLoadViolation           = 0x12,  /* Permit_Load Violation */
    CapEx_PermitStoreViolation          = 0x13,  /* Permit_Store Violation */
    CapEx_PermitLoadCapViolation        = 0x14,  /* Permit_Load_Capability Violation */
    CapEx_PermitStoreCapViolation       = 0x15,  /* Permit_Store_Capability Violation */
    CapEx_PermitStoreLocalCapViolation  = 0x16,  /* Permit_Store_Local_Capability Violation */
    CapEx_PermitSealViolation           = 0x17,  /* Permit_Seal Violation */
    CapEx_AccessSystemRegsViolation     = 0x18,  /* Access System Registers Violation */
    CapEx_PermitCCallViolation          = 0x19,  /* Permit_CCall Violation */
    CapEx_AccessCCallIDCViolation       = 0x1A,  /* Access IDC in a CCall delay slot */
    CapEx_PermitUnsealViolation         = 0x1B,  /* Permit_Unseal violation */
    CapEx_PermitSetCIDViolation         = 0x1C,  /* Permit_SetCID violation */
    // 0x1d-0x1f Reserved
} CheriCapExcCause;

target_ulong check_ddc(CPUMIPSState *env, uint32_t perm, uint64_t addr, uint32_t len, uintptr_t retpc);

static inline const cap_register_t *cheri_get_ddc(CPUMIPSState *env) {
    return &env->active_tc.CHWR.DDC;
}

static inline const cap_register_t *cheri_get_pcc(CPUMIPSState *env) {
    return &env->active_tc.PCC;
}

static inline unsigned cheri_get_asid(CPUMIPSState *env) {
    uint16_t ASID = env->CP0_EntryHi & env->CP0_EntryHi_ASID_mask;
    return ASID;
}

static inline GPCapRegs *cheri_get_gpcrs(CPUArchState *env) {
    return &env->active_tc.gpcapregs;
}

static inline void QEMU_NORETURN raise_cheri_exception_impl(
    CPUArchState *env, CheriCapExcCause cause, unsigned regnum,
    bool instavail, uintptr_t hostpc)
{
    if (!instavail)
        env->error_code |= EXCP_INST_NOTAVAIL;
    do_raise_c2_exception_impl(env, cause, regnum, hostpc);
}

static inline bool
cheri_tag_prot_clear_or_trap(CPUMIPSState *env, int cb, const cap_register_t* cbp,
                             int prot, uintptr_t retpc, target_ulong tag)
{
    if (tag && ((prot & PAGE_LC_CLEAR) || !(cbp->cr_perms & CAP_PERM_LOAD_CAP))) {
        if (qemu_loglevel_mask(CPU_LOG_INSTR)) {
            qemu_log("Clearing tag bit due to missing %s\n",
                     prot & PAGE_LC_CLEAR ? "TLB_L" : "CAP_PERM_LOAD_CAP");
        }
        return 0;
    }
    if (tag && (prot & PAGE_LC_TRAP)) {
      do_raise_c2_exception_impl(env, CapEx_CapLoadGen, cb, retpc);
    }
    return tag;
}

#ifdef CONFIG_MIPS_LOG_INSTR

#define cvtrace_dump_cap_load(trace, addr, cr)          \
    cvtrace_dump_cap_ldst(trace, CVT_LD_CAP, addr, cr)
#define cvtrace_dump_cap_store(trace, addr, cr)         \
    cvtrace_dump_cap_ldst(trace, CVT_ST_CAP, addr, cr)

/*
* Dump cap load or store to cvtrace
*/
static inline void cvtrace_dump_cap_ldst(cvtrace_t *cvtrace, uint8_t version,
                                         uint64_t addr, const cap_register_t *cr)
{
    if (unlikely(qemu_loglevel_mask(CPU_LOG_CVTRACE))) {
        cvtrace->version = version;
        cvtrace->val1 = tswap64(addr);
        cvtrace->val2 = tswap64(((uint64_t)cr->cr_tag << 63) |
            ((uint64_t)(cr->cr_otype & CAP_MAX_REPRESENTABLE_OTYPE) << 32) |
            ((((cr->cr_uperms & CAP_UPERMS_ALL) << CAP_UPERMS_SHFT) |
                (cr->cr_perms & CAP_PERMS_ALL)) << 1) |
            (uint64_t)(cap_is_unsealed(cr) ? 0 : 1));
    }
}
/*
 * Dump cap tag, otype, permissions and seal bit to cvtrace entry
 */
static inline void
cvtrace_dump_cap_perms(cvtrace_t *cvtrace, const cap_register_t *cr)
{
    if (unlikely(qemu_loglevel_mask(CPU_LOG_CVTRACE))) {
        cvtrace->val2 = tswap64(((uint64_t)cr->cr_tag << 63) |
            ((uint64_t)(cr->cr_otype & CAP_MAX_REPRESENTABLE_OTYPE)<< 32) |
            ((((cr->cr_uperms & CAP_UPERMS_ALL) << CAP_UPERMS_SHFT) |
                (cr->cr_perms & CAP_PERMS_ALL)) << 1) |
            (uint64_t)(cap_is_unsealed(cr) ? 0 : 1));
    }
}

/*
 * Dump capability cursor, base and length to cvtrace entry
 */
static inline void cvtrace_dump_cap_cbl(cvtrace_t *cvtrace, const cap_register_t *cr)
{
    if (unlikely(qemu_loglevel_mask(CPU_LOG_CVTRACE))) {
        cvtrace->val3 = tswap64(cr->_cr_cursor);
        cvtrace->val4 = tswap64(cr->cr_base);
        cvtrace->val5 = tswap64(cap_get_length64(cr)); // write UINT64_MAX for 1 << 64
    }
}
#endif // CONFIG_MIPS_LOG_INSTR

static inline void QEMU_NORETURN raise_unaligned_load_exception(
    CPUArchState *env, target_ulong addr, uintptr_t retpc)
{
    do_raise_c0_exception_impl(env, EXCP_AdEL, addr, retpc);
}

static inline void QEMU_NORETURN raise_unaligned_store_exception(
    CPUArchState *env, target_ulong addr, uintptr_t retpc)
{
    do_raise_c0_exception_impl(env, EXCP_AdES, addr, retpc);
}

static inline bool validate_cjalr_target(CPUMIPSState *env,
                                         const cap_register_t *target,
                                         unsigned regnum,
                                         uintptr_t retpc)
{
    if (!QEMU_IS_ALIGNED(cap_get_cursor(target), 4)) {
        do_raise_c0_exception_impl(env, EXCP_AdEL, cap_get_cursor(target),
                                   retpc);
    }
    return true;
}
