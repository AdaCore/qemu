/*
 * QEMU System Emulator
 *
 * Copyright (C) 2009-2021, AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "elf.h"
#include "qemu/option.h"

#include "adacore/qemu-traces.h"
#include "adacore/qemu-decision_map.h"

/* #define DEBUG_TRACE */


/* It's not possible to known the TARGET in qemu-traces.h as TARGET_*
   defines are forbidden to be included in "non-target" files such
   as vl.c or cpu-exec.c.
   Thus, we must controlled target related definitions here.  */
#if defined(TARGET_PPC)

#define ELF_MACHINE EM_PPC
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#elif defined(TARGET_PPC64)

#define ELF_MACHINE EM_PPC64
#define TRACE_TARGET_SIZE TRACE_TARGET_64BIT

#elif defined(TARGET_AARCH64)

#define ELF_MACHINE EM_AARCH64
#define TRACE_TARGET_SIZE TRACE_TARGET_64BIT

#elif defined(TARGET_ARM)

#define ELF_MACHINE EM_ARM
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#elif defined(TARGET_SPARC)

#define ELF_MACHINE EM_SPARC
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#elif defined(TARGET_SPARC64)

#define ELF_MACHINE EM_SPARCV9
#define TRACE_TARGET_SIZE TRACE_TARGET_64BIT

#elif defined(TARGET_I386)

#define ELF_MACHINE EM_386
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#elif defined(TARGET_X86_64)

#define ELF_MACHINE EM_386_64
#define TRACE_TARGET_SIZE TRACE_TARGET_64BIT

#elif defined(TARGET_RISCV64)

#define ELF_MACHINE EM_RISCV
#define TRACE_TARGET_SIZE TRACE_TARGET_64BIT

#elif defined(TARGET_RISCV32)

#define ELF_MACHINE EM_RISCV
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#elif defined(TARGET_M68K)

#define ELF_MACHINE EM_68K
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#elif defined(TARGET_AVR)

#define ELF_MACHINE EM_AVR
#define TRACE_TARGET_SIZE TRACE_TARGET_32BIT

#else
#error "Unknown architecture"
#endif

#if TRACE_TARGET_SIZE == TRACE_TARGET_64BIT
typedef struct trace_entry64 trace_entry;
#else
typedef struct trace_entry32 trace_entry;
#endif

static uint64_t tracefile_limit = 0;
static FILE *tracefile;

#define MAX_TRACE_ENTRIES 1024
static trace_entry trace_entries[MAX_TRACE_ENTRIES];
static TranslationBlock *trace_current_tb;

static trace_entry *trace_current = trace_entries;
int                 tracefile_enabled;
static int          tracefile_nobuf;
static int          tracefile_history;

static int           nbr_histmap_entries;
static target_ulong *histmap_entries;
static target_ulong histmap_loadaddr;

/* Implemented in vl.c  */
void qemu_exit_with_debug(const char *fmt, ...);

void tracefile_history_for_tb_search(TranslationBlock *tb)
{
    tb->tflags |= TRACE_OP_HIST_CACHE;

    if (tracefile_history) {
        tb->tflags |= TRACE_OP_HIST_SET;
        return;
    }
    if (nbr_histmap_entries) {
        int low  = 0;
        int high = nbr_histmap_entries - 1;

        while (low <= high) {
            int          mid = low + (high - low) / 2;
            target_ulong pc  = histmap_loadaddr + histmap_entries[mid];

            if (pc >= tb->pc && pc < tb->pc + tb->size) {
                tb->tflags |= TRACE_OP_HIST_SET;
                return;
            }
            if (tb->pc < pc) {
                high = mid - 1;
            } else {
                low = mid + 1;
            }
        }
    }
}

static void exec_trace_flush(void)
{
    /* The header has already been written to the file.. So just take it in
     * account here.
     */
    static uint64_t written = sizeof(struct trace_header);
    static int limit_hit = 0;
    size_t len = (trace_current - trace_entries) * sizeof(trace_entries[0]);

    if (!len) {
        return;
    }

    if (tracefile_limit) {
        written += len;
        if ((tracefile_limit < written)) {
            if (!limit_hit) {
                /* Don't throw the debug message more than one time.. in
                 * particular this code can be triggered from the atexit
                 * handler and we are calling exit(..) in
                 * qemu_exit_with_debug(..).
                 */
                limit_hit++;
                qemu_exit_with_debug("\nQEMU exec-trace limit exceeded (%u)"
                                     "\n", tracefile_limit);
            } else {
                /* Don't write anything we already reached the limit. */
                return;
            }
        }
    }

    if (fwrite(trace_entries, len, 1, tracefile) != 1) {
        fprintf(stderr, "exec_trace_flush failed\n");
        exit(1);
    }

    trace_current = trace_entries;
    if (tracefile_nobuf) {
        fflush(tracefile);
    }
}

void exec_trace_cleanup(void)
{
    if (tracefile_enabled) {
        exec_trace_flush();
        fclose(tracefile);
    }
}

static void exec_read_map_file(char **poptarg)
{
    char *filename = *poptarg;
    char *efilename = strchr(filename, ',');
    FILE *histfile;
    off_t length;
    int i;
    int my_endian;
    struct trace_header hdr;
    trace_entry ent;

    if (efilename == NULL) {
        fprintf(stderr, "missing ',' after filename for --trace histmap=");
        exit(1);
    }
    *efilename = 0;
    *poptarg = efilename + 1;

    histfile = fopen(filename, "rb");
    if (histfile == NULL) {
        fprintf(stderr, "cannot open histmap file '%s'\n", filename);
        exit(1);
    }
    if (fread(&hdr, sizeof(hdr), 1, histfile) != 1) {
        fprintf(stderr, "cannot read trace header for histmap file '%s'\n",
                filename);
        exit(1);
    }
    if (memcmp(hdr.magic, QEMU_TRACE_MAGIC, sizeof(hdr.magic)) != 0
        || hdr.version != QEMU_TRACE_VERSION
        || hdr.kind != QEMU_TRACE_KIND_DECISION_MAP
        || hdr.sizeof_target_pc != sizeof(target_ulong)
        || (hdr.big_endian != 0 && hdr.big_endian != 1)
        || hdr.machine[0] != (ELF_MACHINE >> 8)
        || hdr.machine[1] != (ELF_MACHINE & 0xff)
        || hdr._pad != 0) {
        fprintf(stderr, "bad header for histmap file '%s'\n", filename);
        exit(1);
    }

    /* Get number of entries. */
    if (fseek(histfile, 0, SEEK_END) != 0
        || (length = ftell(histfile)) == -1
        || fseek(histfile, sizeof(hdr), SEEK_SET) != 0)
    {
        fprintf(stderr, "cannot get size of histmap file '%s'\n", filename);
        exit(1);
    }
    length -= sizeof(hdr);

    if ((length % sizeof(trace_entry)) != 0) {
        fprintf(stderr, "bad length of histmap file '%s'\n", filename);
        exit(1);
    }
    nbr_histmap_entries = length / sizeof(trace_entry);
    if (nbr_histmap_entries) {
        histmap_entries =
            g_malloc(nbr_histmap_entries * sizeof(target_ulong));
    }

#ifdef WORDS_BIGENDIAN
    my_endian = 1;
#else
    my_endian = 0;
#endif

    for (i = 0; i < nbr_histmap_entries; i++) {
        if (fread(&ent, sizeof(ent), 1, histfile) != 1) {
            fprintf(stderr, "cannot read histmap file entry from '%s'\n",
                    filename);
            exit(1);
        }
        if (my_endian != hdr.big_endian) {
            if (sizeof(ent.pc) == 4) {
                ent.pc = bswap32(ent.pc);
            } else {
                ent.pc = bswap64(ent.pc);
            }
        }
        if (i > 0 && ent.pc < histmap_entries[i - 1]) {
            fprintf(stderr, "unordered entry #%d in histmap file '%s'\n",
                    i, filename);
            exit(1);
        }

        histmap_entries[i] = ent.pc;
    }

    fclose(histfile);
    *efilename = ',';
}

void exec_trace_init(const char *optarg)
{
    static struct trace_header hdr  = { QEMU_TRACE_MAGIC };
    static int opt_trace_seen;
    int noappend = 0;
    int kind = QEMU_TRACE_KIND_RAW;

    if (opt_trace_seen) {
        fprintf(stderr, "option -trace already specified\n");
        exit(1);
    }
    opt_trace_seen = 1;

    while (1) {
        if (strstart(optarg, "nobuf,", &optarg)) {
            tracefile_nobuf = 1;
        } else if (strstart(optarg, "history,", &optarg)) {
            tracefile_history = 1;
            kind = QEMU_TRACE_KIND_HISTORY;
        } else if (strstart(optarg, "noappend,", &optarg)) {
            noappend = 1;
        } else if (strstart(optarg, "histmap=", &optarg)) {
            exec_read_map_file((char **)&optarg);
            kind = QEMU_TRACE_KIND_HISTORY;
        } else {
            break;
        }
    }

    tracefile = fopen(optarg, noappend ? "wb" : "ab");

    if (tracefile == NULL) {
        fprintf(stderr, "can't open file %s\n", optarg);
        exit(1);
    }

    hdr.version = QEMU_TRACE_VERSION;
    hdr.sizeof_target_pc = sizeof(target_ulong);
    hdr.kind = kind;
#ifdef WORDS_BIGENDIAN
    hdr.big_endian = 1;
#else
    hdr.big_endian = 0;
#endif
    hdr.machine[0] = ELF_MACHINE >> 8;
    hdr.machine[1] = ELF_MACHINE & 0xff;
    if (fwrite(&hdr, sizeof(hdr), 1, tracefile) != 1) {
        fprintf(stderr, "can't write trace header on %s\n", optarg);
        exit(1);
    }

    atexit(exec_trace_cleanup);
    tracefile_enabled = 1;
}

void exec_trace_limit(const char *optarg)
{
    parse_option_size("maxsize", optarg, &tracefile_limit, NULL);
}

void exec_trace_push_entry(void)
{
#ifdef DEBUG_TRACE
    printf("trace: %08x-%08x op=%04x\n",
           trace_current->pc, trace_current->pc + trace_current->size - 1,
           trace_current->op);
#endif

    if (++trace_current == trace_entries + MAX_TRACE_ENTRIES
        || tracefile_nobuf) {
        exec_trace_flush();
    }
}

void exec_trace_special(uint16_t subop, uint32_t data)
{
    if (!tracefile_enabled) {
        return;
    }

    trace_current->pc = data;
    trace_current->size = subop;
    trace_current->op = TRACE_OP_SPECIAL;

    /* Save the load address to rebase the history map.  */
    if (subop == TRACE_SPECIAL_LOADADDR)
      histmap_loadaddr = data;

    if (++trace_current == trace_entries + MAX_TRACE_ENTRIES
        || tracefile_nobuf) {
        exec_trace_flush();
    }
}

void exec_trace_before_exec(TranslationBlock *tb)
{
#ifdef DEBUG_TRACE
    printf("From " TARGET_FMT_lx " - "
           TARGET_FMT_lx "\n", tb->pc, tb->pc + tb->size - 1);
#endif
    trace_current_tb = tb;
}

/* TB is the tb we jumped to, LAST_TB (if not null) is the last executed tb.  */
void exec_trace_after_exec(uintptr_t next_tb)
{
    TranslationBlock *last_tb =
        tcg_splitwx_to_rw((void *)(next_tb & ~TB_EXIT_MASK));
    int exit_val = next_tb & TB_EXIT_MASK;
    int br = exit_val & (TB_EXIT_IDX1 | TB_EXIT_IDX0);

    if (exit_val == TB_EXIT_ICOUNT_EXPIRED || exit_val == TB_EXIT_REQUESTED) {
        /* Those two values mean that the TB was not executed, see tcg.h for
         * details.
         */
        return;
    }


#ifdef DEBUG_TRACE
    printf("... to " TARGET_FMT_lx,
           trace_current_tb->pc + trace_current_tb->size - 1);
    if (last_tb) {
        printf(" (last_ip=" TARGET_FMT_lx ", tflags=%04x)",
               last_tb->pc + last_tb->size - 1, last_tb->tflags);
    }
    printf("[br=%d tb->tflags=%04x, op=%04x]\n",
           br, trace_current_tb->tflags, trace_current->op);
#endif

    if (last_tb) {
        /* Last instruction is a branch (because last_tb is set).  */
        /* If last_tb != tb, then this is a threaded execution and tb has
           already been executed.  */
        unsigned char op = (1 << br);

        if (last_tb == trace_current_tb) {
            op |= TRACE_OP_BLOCK;
        }

        if ((last_tb->tflags & op) == op
            && !tracefile_history_for_tb(last_tb)) {
            return;
        }

        trace_current->pc = last_tb->pc;
        trace_current->size = last_tb->size;
        trace_current->op = op;
        last_tb->tflags |= op;
    } else {
        /* Note: if last_tb is not set, we don't know if we exited from tb
           or not.  We just know that tb has been executed and the last
           instruction was not a branch.  */
        if (trace_current_tb->tflags & TRACE_OP_BLOCK) {
            return;
        }
        trace_current->pc = trace_current_tb->pc;
        trace_current->size = trace_current_tb->size;
        trace_current->op = TRACE_OP_BLOCK;
        trace_current_tb->tflags |= TRACE_OP_BLOCK;
    }
    exec_trace_push_entry();
}

void exec_trace_at_fault(CPUArchState *e)
{
    target_ulong cs_base, pc;
    uint32_t flags;

    cpu_get_tb_cpu_state(e, &pc, &cs_base, &flags);

#ifdef DEBUG_TRACE
    printf("... fault at " TARGET_FMT_lx "\n", pc);
#endif
    if (trace_current_tb
        && pc >= trace_current_tb->pc
        && pc < trace_current_tb->pc + trace_current_tb->size) {
        if (!tracefile_history_for_tb(trace_current_tb)
            && (trace_current_tb->tflags & TRACE_OP_BLOCK)) {
            return;
        }
        trace_current->pc = trace_current_tb->pc;
        trace_current->op = TRACE_OP_FAULT;
        trace_current->size = pc - trace_current->pc;
        if (trace_current->size == trace_current_tb->size) {
            trace_current->op = TRACE_OP_FAULT | TRACE_OP_BLOCK;
        }
    } else {
        if (trace_current_tb &&
            !tracefile_history_for_tb(trace_current_tb)) {
            /* Discard single fault.  */
            return;
        }
        trace_current->pc = pc;
        trace_current->size = 0;
        trace_current->op = TRACE_OP_FAULT;
    }

    exec_trace_push_entry();
}
