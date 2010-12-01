/*
 * QEMU Leon3 System Emulator
 *
 * Copyright (c) 2010 AdaCore
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
#include "hw.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "sysemu.h"
#include "boards.h"

#include "grlib.h"

/* #define DEBUG_LEON3 */

#ifdef DEBUG_LEON3
#define DPRINTF(fmt, ...)                                       \
    do { printf("Leon3: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

/* Default system clock.  */
#define CPU_CLK (40 * 1000 * 1000)

#define PROM_FILENAME        "u-boot.bin"

#define MAX_PILS 16

typedef struct Leon3State
{
    uint32_t cache_control;
    uint32_t inst_cache_conf;
    uint32_t data_cache_conf;
} Leon3State;

Leon3State leon3_state;

/* Cache control: emulate the behavior of cache control registers but without
   any effect on the emulated CPU */

#define CACHE_DISABLED 0x0
#define CACHE_FROZEN   0x1
#define CACHE_ENABLED  0x3

/* Cache Control register fields */

#define CACHE_CTRL_IF (1 <<  4)  /* Instruction Cache Freeze on Interrupt */
#define CACHE_CTRL_DF (1 <<  5)  /* Data Cache Freeze on Interrupt */
#define CACHE_CTRL_DP (1 << 14)  /* Data cache flush pending */
#define CACHE_CTRL_IP (1 << 15)  /* Instruction cache flush pending */
#define CACHE_CTRL_IB (1 << 16)  /* Instruction burst fetch */
#define CACHE_CTRL_FI (1 << 21)  /* Flush Instruction cache (Write only) */
#define CACHE_CTRL_FD (1 << 22)  /* Flush Data cache (Write only) */
#define CACHE_CTRL_DS (1 << 23)  /* Data cache snoop enable */

static void leon3_cache_control_init(void)
{
    leon3_state.cache_control   = 0x0;

    /* Configuration registers are read and only always keep those predefined
       values */
    leon3_state.inst_cache_conf = 0x10220000;
    leon3_state.data_cache_conf = 0x18220000;
}

void leon3_cache_control_int(void)
{
    uint32_t state = 0;

    if (leon3_state.cache_control & CACHE_CTRL_IF) {
        /* Instruction cache state */
        state = leon3_state.cache_control & 0x3;
        if (state == CACHE_ENABLED) {
            state = CACHE_FROZEN;
            DPRINTF("Instruction cache: freeze\n");
        }

        leon3_state.cache_control &= ~0x3;
        leon3_state.cache_control |= state;
    }

    if (leon3_state.cache_control & CACHE_CTRL_DF) {
        /* Data cache state */
        state = (leon3_state.cache_control >> 2) & 0x3;
        if (state == CACHE_ENABLED) {
            state = CACHE_FROZEN;
            DPRINTF("Data cache: freeze\n");
        }

        leon3_state.cache_control &= ~(0x3 << 2);
        leon3_state.cache_control |= (state << 2);
    }
}

void leon3_cache_control_st(target_ulong addr, uint64_t val, int size)
{
    DPRINTF("cc st addr:%lu, val:0x%x, size:%d\n", (long unsigned int)addr,
            (unsigned int)val, size);

    if (size != 4) {
        DPRINTF(" CC 32bits only\n");
        return;
    }

    switch (addr) {
        case 0x00:              /* Cache control */

            /* These values must always be read as zeros */
            val &= ~CACHE_CTRL_FD;
            val &= ~CACHE_CTRL_FI;
            val &= ~CACHE_CTRL_IB;
            val &= ~CACHE_CTRL_IP;
            val &= ~CACHE_CTRL_DP;

            leon3_state.cache_control = val;
            break;
        case 0x04:              /* Instruction cache configuration */
        case 0x08:              /* Data cache configuration */
            /* Read Only */
            break;
        default:
            DPRINTF(" CC write unknown register 0x%04x\n", (int)addr);
            break;
    };
}

uint64_t leon3_cache_control_ld(target_ulong addr, int size)
{
    uint64_t ret = 0;

    if (size != 4) {
        DPRINTF(" CC 32bits only\n");
        return 0;
    }

    switch (addr) {
        case 0x00:              /* Cache control */
            ret = leon3_state.cache_control;
            break;
        case 0x04:              /* Instruction cache configuration */
            ret = leon3_state.inst_cache_conf;
            break;
        case 0x08:              /* Data cache configuration */
            ret = leon3_state.data_cache_conf;
            break;
        default:
            DPRINTF(" CC read unknown register 0x%04x\n", (int)addr);
            break;
    };
    DPRINTF("cc ld addr:%lu, size:%d, ret:%lu\n", (long unsigned int)addr,
            size, (long unsigned int)ret );
    return ret;
}

void leon3_shutdown(void)
{
    qemu_system_shutdown_request();
}

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;

    cpu_reset(env);
    env->halted = 0;
}

static void leon3_generic_hw_init(ram_addr_t  ram_size,
                                  const char *boot_device,
                                  const char *kernel_filename,
                                  const char *kernel_cmdline,
                                  const char *initrd_filename,
                                  const char *cpu_model)
{
    CPUState     *env;
    ram_addr_t    ram_offset, prom_offset;
    int           ret;
    char         *filename;
    qemu_irq     *cpu_irqs;
    int           bios_size;
    int           aligned_bios_size;

    /* Init CPU */
    if (!cpu_model)
        cpu_model = "LEON3";

    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "qemu: Unable to find Sparc CPU definition\n");
        exit(1);
    }

    cpu_sparc_set_id(env, 0);

    qemu_register_reset(main_cpu_reset, env);

    /* Initialize cache control */
    leon3_cache_control_init();

    /* Allocate IRQ manager */
    grlib_irqmp_create(0x80000200, env, &cpu_irqs, MAX_PILS);

    /* Allocate RAM */
    if ((uint64_t)ram_size > (1UL << 30)) {
        fprintf(stderr,
                "qemu: Too much memory for this machine: %d, maximum 1G\n",
                (unsigned int)(ram_size / (1024 * 1024)));
        exit(1);
    }
    ram_offset = qemu_ram_alloc(ram_size);
    cpu_register_physical_memory(0x40000000, ram_size, ram_offset);

    /* Load boot prom */
    if (bios_name == NULL)
        bios_name = PROM_FILENAME;
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    bios_size = get_image_size(filename);
    if (bios_size > 0) {
        aligned_bios_size =
            (bios_size + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK;
        prom_offset = qemu_ram_alloc(aligned_bios_size);
        cpu_register_physical_memory(0x00000000, aligned_bios_size,
                                     prom_offset /* | IO_MEM_ROM */);
        ret = load_image_targphys(filename, 0x00000000, bios_size);
        if (ret < 0 || ret > bios_size) {
            fprintf(stderr, "qemu: could not load prom '%s'\n", filename);
            exit(1);
        }
    }
    else if (kernel_filename == NULL) {
        fprintf(stderr,"Can't read bios image %s\n", filename);
        exit(1);
    }

    /* Can directly load an application. */
    if (kernel_filename != NULL) {
        long     kernel_size;
        uint64_t entry;

        kernel_size = load_elf(kernel_filename, 0, &entry, NULL, NULL);
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    kernel_filename);
            exit(1);
        }
        if (bios_size <= 0) {
            /* If there is no bios/monitor, start the application.  */
            env->pc = entry;
            env->npc = entry + 4;
        }
    }

    /* Allocate timers */
    grlib_gptimer_create(0x80000300, 2, CPU_CLK, cpu_irqs, 6);

    /* Allocate uart */
    if (serial_hds[0])
        grlib_apbuart_create(0x80000100, serial_hds[0], cpu_irqs[3]);
}

QEMUMachine leon3_generic_machine = {
    .name     = "leon3_generic",
    .desc     = "Leon-3 generic",
    .init     = leon3_generic_hw_init,
    .use_scsi = 0,
};

static void leon3_machine_init(void)
{
    qemu_register_machine(&leon3_generic_machine);
}

machine_init(leon3_machine_init);
