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
#include "hw/boards.h"
#include "qemu/option.h"
#include "tcg/tcg.h"

#include "adacore/qemu-traces.h"
#include "adacore/qemu-decision_map.h"

/* #define DEBUG_TRACE */


/* It's not possible to known the TARGET in qemu-traces.h as TARGET_*
   defines are forbidden to be included in "non-target" files such
   as vl.c or cpu-exec.c.
   Thus, we must controlled target related definitions here.  */
#if defined(TARGET_PPC) || defined(TARGET_PPC64)

#define ELF_MACHINE32 EM_PPC
#define ELF_MACHINE64 EM_PPC64

#elif defined(TARGET_ARM) || defined(TARGET_AARCH64)

#define ELF_MACHINE32 EM_ARM
#define ELF_MACHINE64 EM_AARCH64

#elif defined(TARGET_SPARC) || defined(TARGET_SPARC64)

#define ELF_MACHINE32 EM_SPARC
#define ELF_MACHINE64 EM_SPARCV9

#elif defined(TARGET_I386) || defined(TARGET_X86_64)

#define ELF_MACHINE32 EM_386
#define ELF_MACHINE64 EM_386

#elif defined(TARGET_RISCV32) || defined(TARGET_RISCV64)

#define ELF_MACHINE32 EM_RISCV
#define ELF_MACHINE64 EM_RISCV

#else
#error "Unknown architecture"
#endif

static FILE *tracefile;

#define MAX_TRACE_ENTRIES 1024
static trace_entry trace_entries[MAX_TRACE_ENTRIES];
static TranslationBlock *trace_current_tb;

static trace_entry *trace_current = trace_entries;

static struct exec_trace_config config;
int                 tracefile_enabled;

static int           nbr_histmap_entries;
static target_ulong *histmap_entries;
static target_ulong histmap_loadaddr;

/* Implemented in vl.c  */
void qemu_exit_with_debug(const char *fmt, ...);

static void to_external_entry_64(const trace_entry *ent, external_trace_entry64 *ent64) {
    ent64->pc = ent->pc;
    ent64->size = ent->size;
    ent64->op = ent->op;
}

static void to_internal_entry_64(const external_trace_entry64 *ent64, trace_entry *ent) {
    ent->pc = ent64->pc;
    ent->size = ent64->size;
    ent->op = ent64->op;
}

static void to_external_entry_32(const trace_entry *ent, external_trace_entry32 *ent32) {
    ent32->pc = ent->pc;
    ent32->size = ent->size;
    ent32->op = ent->op;
}

static void to_internal_entry_32(const external_trace_entry32 *ent32, trace_entry *ent) {
    ent->pc = ent32->pc;
    ent->size = ent32->size;
    ent->op = ent32->op;
}


void tracefile_history_for_tb_search(TranslationBlock *tb)
{
    tb->tflags |= TRACE_OP_HIST_CACHE;

    if (config.history) {
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
    void *to_be_written;
    size_t len;

    if (config.is_32bit) {
        len = (trace_current - trace_entries) * sizeof(struct external_trace_entry32);
    } else {
        len = (trace_current - trace_entries) * sizeof(struct external_trace_entry64);
    }

    if (!len) {
        return;
    }

    if (config.tracefile_limit) {
        written += len;
        if ((config.tracefile_limit < written)) {
            if (!limit_hit) {
                /* Don't throw the debug message more than one time.. in
                 * particular this code can be triggered from the atexit
                 * handler and we are calling exit(..) in
                 * qemu_exit_with_debug(..).
                 */
                limit_hit++;
                qemu_exit_with_debug("\nQEMU exec-trace limit exceeded (%u)"
                                     "\n", config.tracefile_limit);
            } else {
                /* Don't write anything we already reached the limit. */
                return;
            }
        }
    }

    /* Allocate external entries  */
    to_be_written = g_malloc0(len);
    for (int i = 0; i < (trace_current - trace_entries) ; i++) {
        if (config.is_32bit) {
            to_external_entry_32(
                trace_entries + i,
                ((struct external_trace_entry32*) to_be_written ) + i);
        } else {
            to_external_entry_64(
                trace_entries + i,
                ((struct external_trace_entry64*) to_be_written ) + i);
        }
    }

    if (fwrite(to_be_written, len, 1, tracefile) != 1) {
        fprintf(stderr, "exec_trace_flush failed\n");
        exit(1);
    }

    trace_current = trace_entries;
    if (config.nobuf) {
        fflush(tracefile);
    }
}

void exec_trace_cleanup(void)
{
    if (tracefile_enabled) {
        exec_trace_flush();
        if (config.histmap_filename) {
            free(config.histmap_filename);
        }
        fclose(tracefile);
    }
}

/* Helper function to verify the validity for histmap file header.  */
static void exec_trace_check_hdr_helper(bool cond, const char *msg, char *filename) {
    if (cond) {
        fprintf(stderr, "bad header (%s) for histmap file '%s'\n", msg, filename);
        exit(1);
    }
}

static void exec_read_map_file(char *filename)
{
    FILE *histfile;
    off_t length;
    int i;
    struct trace_header hdr;
    size_t ent_size;

    if (config.is_32bit) {
        ent_size = sizeof(struct external_trace_entry32);
    } else {
        ent_size = sizeof(struct external_trace_entry64);
    }

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

    /* Verify header fields.  */
    exec_trace_check_hdr_helper(
        memcmp(hdr.magic, QEMU_TRACE_MAGIC, sizeof(hdr.magic)) != 0,
        "magic", filename);
    exec_trace_check_hdr_helper(
        hdr.version != QEMU_TRACE_VERSION,
        "version", filename);
    exec_trace_check_hdr_helper(
        hdr.kind != QEMU_TRACE_KIND_DECISION_MAP,
        "kind", filename);
    exec_trace_check_hdr_helper(
        hdr.sizeof_target_pc != (config.is_32bit ? 4 : 8),
        "sizeof pc", filename);
    exec_trace_check_hdr_helper(
        hdr.big_endian != 0 && hdr.big_endian != 1,
        "endianness", filename);
    exec_trace_check_hdr_helper(
        config.is_32bit ?
        (hdr.machine[0] != (ELF_MACHINE32 >> 8) || hdr.machine[1] != (ELF_MACHINE32 & 0xff)):
        (hdr.machine[0] != (ELF_MACHINE64 >> 8) || hdr.machine[1] != (ELF_MACHINE64 & 0xff)),
        "machine", filename);
    exec_trace_check_hdr_helper(hdr._pad != 0, "padding", filename);

    /* Get number of entries. */
    if (fseek(histfile, 0, SEEK_END) != 0
        || (length = ftell(histfile)) == -1
        || fseek(histfile, sizeof(hdr), SEEK_SET) != 0)
    {
        fprintf(stderr, "cannot get size of histmap file '%s'\n", filename);
        exit(1);
    }
    length -= sizeof(hdr);

    if ((length % ent_size) != 0) {
        fprintf(stderr, "bad length of histmap file '%s'\n", filename);
        exit(1);
    }
    nbr_histmap_entries = length / ent_size;
    if (nbr_histmap_entries) {
        histmap_entries =
            g_malloc(nbr_histmap_entries * sizeof(target_ulong));
    }

    for (i = 0; i < nbr_histmap_entries; i++) {
        trace_entry ent;
        void *raw_ent = g_malloc(ent_size);

        if (fread(raw_ent, ent_size, 1, histfile) != 1) {
            fprintf(stderr, "cannot read histmap file entry from '%s'\n",
                    filename);
            exit(1);
        }

        /* Retrieve PC */
        if (config.is_32bit) {
            to_internal_entry_32((struct external_trace_entry32*) raw_ent, &ent);
        } else {
            to_internal_entry_64((struct external_trace_entry64*) raw_ent, &ent);
        }

        if (config.big_endian != hdr.big_endian) {
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
        free(raw_ent);
    }


    fclose(histfile);
}

/*
 * This function retrieves -exec-trace arguments and fill
 * config structure aaccordingly.
 */
void exec_trace_opts_parse(const char *optarg)
{
    static bool opt_trace_seen;

    /* Default is RAW. */
    config.kind = QEMU_TRACE_KIND_RAW;

    if (opt_trace_seen) {
        fprintf(stderr, "option -trace already specified\n");
        exit(1);
    }
    opt_trace_seen = true;

    while (1) {
        if (strstart(optarg, "nobuf,", &optarg)) {
            config.nobuf = true;
        } else if (strstart(optarg, "history,", &optarg)) {
            config.history = true;
            config.kind = QEMU_TRACE_KIND_HISTORY;
        } else if (strstart(optarg, "noappend,", &optarg)) {
            config.noappend = true;
        } else if (strstart(optarg, "histmap=", &optarg)) {
            char *efilename = strchr(optarg, ',');
            if (efilename == NULL) {
                fprintf(stderr, "missing ',' after filename for --trace histmap=");
                exit(1);
            }
            config.histmap_filename=g_strndup(optarg, efilename - optarg);
            config.kind = QEMU_TRACE_KIND_HISTORY;

            optarg = efilename + 1;
        } else {
            break;
        }
    }

    config.trace_filename = g_strdup(optarg);

    atexit(exec_trace_cleanup);
    tracefile_enabled = 1;
}

/* Fetch machine-dependenty configuration, such as 32bit or 64bit CPUs. */
static void exec_trace_finalize_config(void) {
#if defined(TARGET_PPC) || defined(TARGET_SPARC) \
    || defined(TARGET_RISCV32) || defined(TARGET_I386)
    config.is_32bit = true;
#elif defined(TARGET_PPC64) || defined(TARGET_SPACEV9)      \
    || defined(TARGET_RISCV64) || defined(TARGET_X86_64)
    config.is_32bit = false;
#elif defined(TARGET_ARM) || defined(TARGET_AARCH64)
    /*
     * Get architecture information for the CPUs.
     * XXX: doesn't work with boards having CPUs with different
     * architectures.
     */
    ARMCPU *cpu = ARM_CPU(qemu_get_cpu(0));
    CPUARMState *env = &cpu->env;

    config.is_32bit = !is_a64(env);
#endif

#ifdef WORDS_BIGENDIAN
    config.big_endian = 1;
#else
    config.big_endian = 0;
#endif

}

/* Write the trace file header.  */
static void exec_trace_write_header(void){
    struct trace_header hdr = { QEMU_TRACE_MAGIC };

    hdr.version = QEMU_TRACE_VERSION;
    hdr.kind = config.kind;
    hdr.big_endian = config.big_endian ? 1 : 0;
    if (config.is_32bit) {
        hdr.sizeof_target_pc = 4;
        hdr.machine[0] = ELF_MACHINE32 >> 8;
        hdr.machine[1] = ELF_MACHINE32 & 0xff;
    } else {
        hdr.sizeof_target_pc = 8;
        hdr.machine[0] = ELF_MACHINE64 >> 8;
        hdr.machine[1] = ELF_MACHINE64 & 0xff;
    }
    if (fwrite(&hdr, sizeof(hdr), 1, tracefile) != 1) {
        fprintf(stderr, "can't write trace header on %s\n", optarg);
        exit(1);
    }
}

/*
 * This function initialized the trace mechanism based on the options
 * parsed earlier. It expects the machine to be instantiated to
 * retrieve various informations.
 */
void exec_trace_init(void)
{
    if (!tracefile_enabled) {
        return;
    }

    tracefile = fopen(config.trace_filename, config.noappend ? "wb" : "ab");

    if (tracefile == NULL) {
        fprintf(stderr, "can't open file %s\n", optarg);
        exit(1);
    }

    /* Initialize missing part of the configuration.  */
    exec_trace_finalize_config();

    if (config.histmap_filename) {
        exec_read_map_file(config.histmap_filename);
    }

    exec_trace_write_header();
}

void exec_trace_limit(const char *optarg)
{
    parse_option_size("maxsize", optarg, &config.tracefile_limit, NULL);
}

void exec_trace_push_entry(void)
{
#ifdef DEBUG_TRACE
    printf("trace: %08x-%08x op=%04x\n",
           trace_current->pc, trace_current->pc + trace_current->size - 1,
           trace_current->op);
#endif

    if (++trace_current == trace_entries + MAX_TRACE_ENTRIES
        || config.nobuf) {
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
        || config.nobuf) {
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
    vaddr pc;
    uint64_t cs_base;
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
