/*
 * QEMU Leon3 System Emulator
 *
 * Copyright (c) 2010-2018 AdaCore
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
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "cpu.h"
#include "hw/hw.h"
#include "qemu/timer.h"
#include "hw/ptimer.h"
#include "sysemu/sysemu.h"
#include "sysemu/qtest.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "trace.h"
#include "exec/address-spaces.h"
#include "hw/adacore/gnat-bus.h"
#include "hw/adacore/hostfs.h"

#include "hw/sparc/grlib.h"

/* Default system clock.  */
#define CPU_CLK (40 * 1000 * 1000)

#define PROM_FILENAME        "u-boot.bin"

#define MAX_PILS 16
#define MAX_CPUs 4

typedef struct BoardData BoardData;

struct BoardData {
    SPARCCPU *cpu[MAX_CPUs];
    uint32_t  entry;            /* save kernel entry in case of reset */

    DeviceState *irqmp;
    target_long reset_sp[MAX_CPUs]; 	/* initial stack pointer */
};

static void leon3_cpu_reset(void *opaque)
{
    SPARCCPU *sc = (SPARCCPU *)opaque;
    CPUState *cpu = CPU(sc);
    CPUSPARCState  *env = &sc->env;
    BoardData      *b = (BoardData *)env->irq_manager;

    cpu_reset(cpu);

    cpu->halted = env->cpu_id != 0;
    env->pc     = b->entry;
    env->npc    = b->entry + 4;
    env->regbase[6] = b->reset_sp[env->cpu_id];
    if (env->pc && env->cpu_id == 0) {
	/* enable traps and FPU if running a RAM image */
        env->psret = 1;
        env->psref = 1;
        env->wim = 2;
    }
}

void leon3_irq_ack(CPUSPARCState *env, int intno)
{
    BoardData *board = env->irq_manager;
    grlib_irqmp_ack(board->irqmp, env->cpu_id, intno);
}

static void leon3_set_pil_in(void *opaque, int cpuid, uint32_t pil_in)
{
    BoardData     *board = (BoardData *)opaque;
    SPARCCPU      *sc = board->cpu[cpuid];
    CPUSPARCState *env = &sc->env;

    if (sc == NULL) {
	/* CPU is not present. */
	return;
    }

    env->pil_in = pil_in;

    if (env->pil_in && (env->interrupt_index == 0 ||
                        (env->interrupt_index & ~15) == TT_EXTINT)) {
        unsigned int i;

        for (i = 15; i > 0; i--) {
            if (env->pil_in & (1 << i)) {
                int old_interrupt = env->interrupt_index;

                env->interrupt_index = TT_EXTINT | i;
                if (old_interrupt != env->interrupt_index) {
                    trace_leon3_set_irq(i);
                    cpu_interrupt(CPU(sc), CPU_INTERRUPT_HARD);
                }
                break;
            }
        }
    } else if (!env->pil_in && (env->interrupt_index & ~15) == TT_EXTINT) {
        trace_leon3_reset_irq(env->interrupt_index & 15);
        env->interrupt_index = 0;
        cpu_reset_interrupt(CPU(sc), CPU_INTERRUPT_HARD);
    }
}

static void leon3_start_cpu(void *opaque, int cpuid)
{
    BoardData *board = (BoardData *)opaque;
    CPUState  *cs = CPU(board->cpu[cpuid]);

    cs->halted = 0;
}

static void leon3_generic_hw_init(MachineState *machine)
{
    ram_addr_t ram_size = machine->ram_size;
    const char *kernel_filename = machine->kernel_filename;
    BoardData *b;
    SPARCCPU *cpu;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *prom = g_new(MemoryRegion, 1);
    int         ret;
    int         i;
    char       *filename;
    qemu_irq   *cpu_irqs = NULL;
    int         bios_size;
    int         prom_size;

    /* Init CPU */
    b = g_new(BoardData, 1);

    for (i = 0; i < smp_cpus; i++) {
        cpu = SPARC_CPU(cpu_create(machine->cpu_type));
	if (cpu == NULL) {
	    fprintf(stderr, "qemu: Unable to find Sparc CPU definition\n");
	    exit(1);
	}
	b->cpu[i] = cpu;

	cpu_sparc_set_id(&cpu->env, i);
	cpu->env.irq_manager = b;
	cpu->env.qemu_irq_ack = leon3_irq_manager;

	b->reset_sp[i] = 0x40000000 + ram_size - (0x4000 * i);

        qemu_register_reset(leon3_cpu_reset, cpu);
    }

    /* Allocate IRQ manager */
    b->irqmp = grlib_irqmp_create(0x80000200, smp_cpus, &cpu_irqs, MAX_PILS,
				  leon3_set_pil_in, leon3_start_cpu, b);

    /* Allocate RAM */
    if (ram_size > 1 * GiB) {
        error_report("Too much memory for this machine: %" PRId64 "MB,"
                     " maximum 1G",
                     ram_size / MiB);
        exit(1);
    }

    memory_region_allocate_system_memory(ram, NULL, "leon3.ram", ram_size);
    memory_region_add_subregion(address_space_mem, 0x40000000, ram);

    /* Allocate BIOS */
    prom_size = 8 * MiB;
    memory_region_init_ram(prom, NULL, "Leon3.bios", prom_size, &error_fatal);
    memory_region_set_readonly(prom, true);
    memory_region_add_subregion(address_space_mem, 0x00000000, prom);

    /* Load boot prom */
    if (bios_name == NULL) {
        bios_name = PROM_FILENAME;
    }
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);

    if (filename) {
        bios_size = get_image_size(filename);
    } else {
        bios_size = -1;
    }

    if (bios_size > prom_size) {
        error_report("could not load prom '%s': file too big", filename);
        exit(1);
    }

    if (bios_size > 0) {
        ret = load_image_targphys(filename, 0x00000000, bios_size);
        if (ret < 0 || ret > prom_size) {
            error_report("could not load prom '%s'", filename);
            exit(1);
        }
    } else if (kernel_filename == NULL && !qtest_enabled()) {
        error_report("Can't read bios image %s", filename);
        exit(1);
    }
    g_free(filename);

    /* Can directly load an application. */
    if (kernel_filename != NULL) {
        long     kernel_size;
        uint64_t entry;

        kernel_size = load_elf(kernel_filename, NULL, NULL, NULL,
                               &entry, NULL, NULL,
                               1 /* big endian */, EM_SPARC, 0, 0);
        if (kernel_size < 0) {
            error_report("could not load kernel '%s'", kernel_filename);
            exit(1);
        }
        if (bios_size <= 0) {
            /* If there is no bios/monitor, start the application.  */
            b->entry = entry;
        }
    }

    /* Allocate AHB PNP */
    grlib_ahbpnp_create(0xFFFFF000);

    /* Allocate APB PNP */
    grlib_apbpnp_create(0x800FF000);

    /* Allocate timers */
    grlib_gptimer_create(0x80000300, 4, CPU_CLK, cpu_irqs, 6);

    /* Allocate uart */
    if (serial_hd(0)) {
        grlib_apbuart_create(0x80000100, serial_hd(0), cpu_irqs[3]);
    }

    /* HostFS */
    hostfs_create(0x80001000, get_system_memory());

    /* Initialize the GnatBus Master */
    gnatbus_master_init(cpu_irqs, MAX_PILS);
    gnatbus_device_init();
}

static void leon3_generic_machine_init(MachineClass *mc)
{
    mc->desc = "Leon-3 generic";
    mc->init = leon3_generic_hw_init;
    mc->default_cpu_type = SPARC_CPU_TYPE_NAME("LEON3");
    mc->max_cpus = MAX_CPUs;
}

DEFINE_MACHINE("leon3_generic", leon3_generic_machine_init)
