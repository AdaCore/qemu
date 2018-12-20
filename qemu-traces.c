/*
 * QEMU System Emulator
 *
 * Copyright (C) 2009-2011, AdaCore
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
#include "qemu-common.h"
#include "qemu-traces.h"
#include "qemu-decision_map.h"
#include "elf.h"
#include "qemu/option.h"

/* #define DEBUG_TRACE */

#ifdef PPC_ELF_MACHINE
#define ELF_MACHINE PPC_ELF_MACHINE
#elif defined(TARGET_AARCH64)
#  define ELF_MACHINE EM_AARCH64
#elif defined(TARGET_ARM)
#  define ELF_MACHINE EM_ARM
#elif defined(TARGET_SPARC)
#  define ELF_MACHINE EM_SPARC
#elif defined(TARGET_SPARC64)
#  define ELF_MACHINE EM_SPARCV9
#elif defined(TARGET_I386)
#  define ELF_MACHINE EM_386
#elif defined(TARGET_X86_64)
#  define ELF_MACHINE EM_386_64
#elif defined(TARGET_RISCV64)
#  define ELF_MACHINE EM_RISCV
#else
#error "Unknown architecture"
#endif

static uint64_t tracefile_limit = 0;
static FILE *tracefile;

#define MAX_TRACE_ENTRIES 1024
static struct trace_entry trace_entries[MAX_TRACE_ENTRIES];

struct trace_entry *trace_current = trace_entries;
int                 tracefile_enabled;
static int          tracefile_nobuf;
static int          tracefile_history;

static int           nbr_histmap_entries;
static target_ulong *histmap_entries;
static target_ulong histmap_loadaddr;

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

    fwrite(trace_entries, len, 1, tracefile);
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
    struct trace_header hdr;
    off_t length;
    int ent_sz;
    int i;
    int my_endian;

    if (efilename == NULL) {
        fprintf(stderr, "missing ',' after filename for --trace histmap=");
        exit(1);
    }
    *efilename = 0;
    *poptarg = efilename + 1;

    histfile = fopen(filename, "rb");
    if (histfile == NULL) {
        fprintf(stderr, "cannot open histmap file '%s': %m\n", filename);
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
    if (sizeof(target_ulong) == 4) {
        ent_sz = sizeof(struct trace_entry32);
    } else {
        ent_sz = sizeof(struct trace_entry64);
    }

    if ((length % ent_sz) != 0) {
        fprintf(stderr, "bad length of histmap file '%s'\n", filename);
        exit(1);
    }
    nbr_histmap_entries = length / ent_sz;
    if (nbr_histmap_entries) {
        histmap_entries =
            g_malloc(nbr_histmap_entries * sizeof(target_ulong));
    }

#ifdef WORDS_BIGENDIAN
    my_endian = 1;
#else
    my_endian = 0;
#endif

    if (sizeof(target_ulong) == 4) {
        for (i = 0; i < nbr_histmap_entries; i++) {
            struct trace_entry32 ent;

            if (fread(&ent, sizeof(ent), 1, histfile) != 1) {
                fprintf(stderr, "cannot read histmap file entry from '%s'\n",
                        filename);
                exit(1);
            }
            if (my_endian != hdr.big_endian) {
                ent.pc = bswap32(ent.pc);
            }
            if (i > 0 && ent.pc < histmap_entries[i - 1]) {
                fprintf(stderr, "unordered entry #%d in histmap file '%s'\n",
                        i, filename);
                exit(1);
            }

            histmap_entries[i] = ent.pc;
        }
    } else {
        for (i = 0; i < nbr_histmap_entries; i++) {
            struct trace_entry64 ent;

            if (fread(&ent, sizeof(ent), 1, histfile) != 1) {
                fprintf(stderr, "cannot read histmap file entry from '%s'\n",
                        filename);
                exit(1);
            }
            if (my_endian != hdr.big_endian) {
                ent.pc = bswap64(ent.pc);
            }
            if (i > 0 && ent.pc < histmap_entries[i - 1]) {
                fprintf(stderr, "unordered entry #%d in histmap file '%s'\n",
                        i, filename);
                exit(1);
            }

            histmap_entries[i] = ent.pc;
        }
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
        fprintf(stderr, "can't open file %s: %m\n", optarg);
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
