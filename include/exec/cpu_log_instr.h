/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alfredo Mazzinghi
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifdef CONFIG_TCG_LOG_INSTR

/* TODO(am2419): there is no need for this to be visible, hide in util/log_instr.c */
struct cvtrace {
    uint8_t version;
#define CVT_GPR     1   /* GPR change (val2) */
#define CVT_LD_GPR  2   /* Load into GPR (val2) from address (val1) */
#define CVT_ST_GPR  3   /* Store from GPR (val2) to address (val1) */
#define CVT_NO_REG  4   /* No register is changed. */
#define CVT_CAP     11  /* Cap change (val2,val3,val4,val5) */
#define CVT_LD_CAP  12  /* Load Cap (val2,val3,val4,val5) from addr (val1) */
#define CVT_ST_CAP  13  /* Store Cap (val2,val3,val4,val5) to addr (val1) */
    uint8_t exception;  /* 0=none, 1=TLB Mod, 2=TLB Load, 3=TLB Store, etc. */
    uint16_t cycles;    /* Currently not used. */
    uint32_t inst;      /* Encoded instruction. */
    uint64_t pc;        /* PC value of instruction. */
    uint64_t val1;      /* val1 is used for memory address. */
    uint64_t val2;      /* val2, val3, val4, val5 are used for reg content. */
    uint64_t val3;
    uint64_t val4;
    uint64_t val5;
    uint8_t thread;     /* Hardware thread/CPU (i.e. cpu->cpu_index ) */
    uint8_t asid;       /* Address Space ID (i.e. CP0_TCStatus & 0xff) */
} __attribute__((packed));
typedef struct cvtrace cvtrace_t;

/* Version 3 Cheri Stream Trace header info */
#define CVT_QEMU_VERSION    (0x80U + 3)
#define CVT_QEMU_MAGIC      "CheriTraceV03"

/*
 * The log buffer is associated with the cpu arch state, each target should
 * define the cpu_get_log_buffer() function/macro to retrieve a pointer to
 * the log buffer. The per-cpu log buffer structure is defined below.
 */
struct cpu_log_instr_info {
    bool user_mode_tracing;

    bool force_drop;
    uint16_t asid;

    int flags;
#define LI_FLAG_INTR_TRAP 1
#define LI_FLAG_INTR_ASYNC 2
#define LI_FLAG_INTR_MASK 0x3

    uint32_t intr_code;
    // TODO(am2419): should be target_ulong
    uint64_t intr_vector;
    uint64_t intr_faultaddr;

    uint64_t pc;
    /*
     * For now we allow multiple accesses to be tied to one instruction.
     * Some architectures may have multiple memory accesses
     * in the same instruction (e.g. x86-64 pop r/m64,
     * vector/matrix instructions, load/store pair). It is unclear
     * whether we would treat these as multiple trace "entities".
     *
     * Array of log_meminfo_t
     */
    GArray *mem;
    /* Register modifications. Array of log_reginfo_t */
    GArray *regs;
    /* Extra text-only log */
    GString *txt_buffer;
};
typedef struct cpu_log_instr_info cpu_log_instr_info_t;
#endif /* CONFIG_TCG_LOG_INSTR */
