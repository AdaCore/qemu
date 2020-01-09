/*
 * MVME133 QEMU Implementation
 *
 * Copyright (c) 2019-2020 AdaCore
 *
 *  Developed by :
 *  Frederic Konrad   <frederic.konrad@adacore.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "cpu.h"
#include "hw/hw.h"
#include "hw/m68k/mcf.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/address-spaces.h"
#include "qemu/error-report.h"
#include "sysemu/qtest.h"
#include "hw/sysbus.h"
#include "sysemu/sysemu.h"
#include "hw/misc/mc68901.h"

/*
 * Memory Map (p53):
 *
 * 0x00000000 -> 0x003FFFFF: DRAM
 * 0x00400000 -> 0x00EFFFFF: VMEbus A32 / A24
 * 0x00F00000 -> 0xFFEFFFFF: VMEbus A32
 * 0xFFF00000 -> 0xFFF1FFFF: ROM BANK 1
 * 0xFFF20000 -> 0xFFF3FFFF: ROM BANK 2
 * 0xFFF40000 -> 0xFFF5FFFF: ROM BANK 1 MIRROR
 * 0xFFF60000 -> 0xFFF7FFFF: ROM BANK 2 MIRROR
 * 0xFFF80000 -> 0xFFF9FFFF: MSR & MC68901 MFP
 * 0xFFFA0000 -> 0xFFFBFFFF: Z8530 Serial Communication Controller (SCC)
 * 0xFFFC0000 -> 0xFFCFFFFF: M48T18 real-time clock (RTC)
 * 0xFFFD0000 -> 0xFFFDFFFF: PWRUP flag
 * 0xFFFE0000 -> 0xFFFEFFFF: VMEbus interrupter
 * 0xFFFF0000 -> 0xFFFFFFFF: VMEbus short I/O space.
 */

#define MVME133_DRAM_ADDR              (0x00000000)
#define MVME133_DRAM_SIZE              (0x00400000)
#define MVME133_VME_ADDR               (0x00F00000)
#define MVME133_VME_SIZE               (0xFF000000)
#define MVME133_ROM_BANK_1_ADDR        (0xFFF00000)
#define MVME133_ROM_BANK_1_SIZE        (0x00020000)
#define MVME133_ROM_BANK_2_ADDR        (0xFFF20000)
#define MVME133_ROM_BANK_2_SIZE        (0x00020000)
#define MVME133_ROM_BANK_1_MIRROR_ADDR (0xFFF00000)
#define MVME133_ROM_BANK_1_MIRROR_SIZE (0x00020000)
#define MVME133_ROM_BANK_2_MIRROR_ADDR (0xFFF20000)
#define MVME133_ROM_BANK_2_MIRROR_SIZE (0x00020000)
#define MVME133_MSR_ADDR               (0xFFF80000)
#define MVME133_MSR_SIZE               (0x00020000)
/* This is on the VME BUS */
#define MVME133_SHARED_DRAM_ADDR       (0x00000000)
#define MVME133_SHARED_DRAM_SIZE       (0x00400000)

/* NOTE: The 0xFFF80000 area is a little bit different since the MSR is mapped
 *       on the even bytes while the MC68901 is mapped on the odd bytes.  */

typedef struct InterleaveIO {
    MemoryRegion IO;
    MemoryRegion *A;
} InterleaveIO;

static void interleave_write(void *opaque, hwaddr offset,
                             uint64_t value, unsigned size)
{
    InterleaveIO *s = (InterleaveIO *)opaque;

    /* size == 2 is supported but multiple 1 bytes access will be performed
     * by the memory code.  */
    assert(size == 1);

    if (offset & 0x01) {
        /* Access to this devices repeats every 0x30 bytes.  */
        offset = offset % 0x30;
        memory_region_dispatch_write(s->A, offset, value, 1,
                                     MEMTXATTRS_UNSPECIFIED);
    }
}

static uint64_t interleave_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    InterleaveIO *s = (InterleaveIO *)opaque;

    /* size == 2 is supported but multiple 1 bytes access will be performed
     * by the memory code.  */
    assert(size == 1);

    if (offset & 0x01) {
        uint64_t value;

        /* Access to this devices repeats every 0x30 bytes.  */
        offset = offset % 0x30;
        memory_region_dispatch_read(s->A, offset, &value, 1,
                                    MEMTXATTRS_UNSPECIFIED);
        return value;
    } else {
        /* PWRUP bit */
        return 0x20;
    }
}

static const MemoryRegionOps interleave_ops = {
    .read = interleave_read,
    .write = interleave_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 2,
        .unaligned = false,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
        .unaligned = false,
    },
};

typedef struct reset_data {
    CPUM68KState *env;
    uint32_t pc;
} reset_data;

static void mvme133_watchdog_expired(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

static void mvme133_reset(void *opaque)
{
    reset_data *data = opaque;
    CPUM68KState *env = data->env;

    /* Fix that */
    env->vbr = 0x00000000;
    env->aregs[7] = 0x90000;
    env->pc = data->pc;
}

static void mvme133_init(MachineState *machine)
{
    const char *kernel_filename = machine->kernel_filename;
    M68kCPU *cpu;
    CPUM68KState *env;
    int kernel_size;
    hwaddr kernel_end;
    uint64_t elf_entry;
    hwaddr entry;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *rom_b1 = g_new(MemoryRegion, 1);
    MemoryRegion *rom_b2 = g_new(MemoryRegion, 1);
    MemoryRegion *vme_bus = g_new(MemoryRegion, 1);
    MemoryRegion *shared_dram = g_new(MemoryRegion, 1);
    InterleaveIO *inter = g_new0(InterleaveIO, 1);
    MC68901State *mc68901;
    reset_data *reset = g_new0(reset_data, 1);

    cpu = M68K_CPU(cpu_create(machine->cpu_type));
    env = &cpu->env;
    env->vbr = 0;
    env->mbar = 0 | 1;
    env->rambar0 = MVME133_DRAM_ADDR | 1;
    reset->env = env;

    qemu_register_reset(mvme133_reset, reset);

    /* DRAM */
    memory_region_allocate_system_memory(ram, NULL, "dram",
                                         MVME133_DRAM_SIZE);
    memory_region_add_subregion(address_space_mem, MVME133_DRAM_ADDR, ram);

    /* ROM */
    memory_region_init_ram(rom_b1, NULL, "rom bank 1",
                           MVME133_ROM_BANK_1_SIZE, &error_fatal);
    memory_region_add_subregion(address_space_mem, MVME133_ROM_BANK_1_ADDR,
                                rom_b1);
    memory_region_init_ram(rom_b2, NULL, "rom bank 2",
                           MVME133_ROM_BANK_2_SIZE, &error_fatal);
    memory_region_add_subregion(address_space_mem, MVME133_ROM_BANK_2_ADDR,
                                rom_b2);

    memory_region_init(vme_bus, NULL, "VME", MVME133_VME_SIZE);
    memory_region_add_subregion(address_space_mem, MVME133_VME_ADDR, vme_bus);

    memory_region_init_alias(shared_dram, NULL, "shared dram", ram, 0,
                             MVME133_SHARED_DRAM_SIZE);
    memory_region_add_subregion(vme_bus, MVME133_SHARED_DRAM_ADDR,
                                shared_dram);

    /* MSR / MC68901 */
    memory_region_init_io(&inter->IO, NULL, &interleave_ops, inter,
                          "msr/mc68901", MVME133_MSR_SIZE);
    memory_region_add_subregion(address_space_mem, MVME133_MSR_ADDR,
                                &inter->IO);
    mc68901 = MC68901(object_new(TYPE_MC68901));
    qdev_prop_set_chr(DEVICE(mc68901), "chardev", serial_hd(0));
    /* The MC68901 is apparently driven by a 1.23MHz XTAL according to the
     * MVME133 Users Manual (1986) section 4.3.10.1.  */
    qdev_prop_set_uint32(DEVICE(mc68901), "timer_freq", 1230000);
    object_property_set_bool(OBJECT(mc68901), true, "realized", &error_fatal);

    inter->A = sysbus_mmio_get_region(SYS_BUS_DEVICE(mc68901), 0);

    reset->pc = 0xffff0008;

    /* Here is the layout of the different Timer output signals from the
     * MC68901 (MVME133 UM (1986) section 4.3.10.2):
     *
     * TimerA output is capable of generating a periodic interrupt.  This is
     * unimplemented for now.
     *
     * TimerB output is connected through the Watchdog Reset Enable jumper
     * (J2) to the RESET signal.  Let's assume that this is always enabled
     * and thus that the TimerB going up will always reset the board.
     *
     * TimerC output is used as a baudrate generator for the serial port, but
     * we will simply ignore that since we won't do a cycle accurate modeling
     * and the qemu_clock framework is not compatible with the GPIO framework.
     *
     * TimerD output is not bound.
     */
    qdev_connect_gpio_out(DEVICE(mc68901), MC68901_TBO,
                          qemu_allocate_irq(&mvme133_watchdog_expired, NULL,
                                            0));

    if (kernel_filename) {
        kernel_size = load_elf(kernel_filename, NULL, NULL, NULL, &elf_entry,
                               NULL, &kernel_end, 1, EM_68K, 0, 0);

        if (kernel_size < 0) {
            kernel_size = load_uimage(kernel_filename, &entry, NULL, NULL,
                                      NULL, NULL);
        }

        if (kernel_size < 0) {
            error_report("Could not load kernel '%s'", kernel_filename);
            exit(1);
        }
        entry = elf_entry;
        reset->pc = entry;
    }
}

static void mvme133_machine_init(MachineClass *mc)
{
    mc->desc = "MVME133";
    mc->init = mvme133_init;
    mc->default_cpu_type = M68K_CPU_TYPE_NAME("m68020");
}

DEFINE_MACHINE("mvme133", mvme133_machine_init)
