/*
 * QEMU RISC-V Board Compatible with Microchip PolarFire SOC
 *
 * Copyright 2020 AdaCore
 *
 * Base on sifive_u.c:
 *
 * Copyright (c) 2016-2017 Sagar Karandikar, sagark@eecs.berkeley.edu
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "hw/cpu/cluster.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/sifive_plic.h"
#include "hw/riscv/sifive_clint.h"
#include "hw/riscv/sifive_uart.h"
#include "hw/riscv/sifive_test.h"
#include "hw/riscv/polarfiresoc.h"
#include "hw/riscv/boot.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "hw/qdev-properties.h"
#include "hw/adacore/gnat-bus.h"
#include "hw/adacore/hostfs.h"

#include <libfdt.h>

static const struct MemmapEntry {
    hwaddr base;
    hwaddr size;
} polarfire_memmap[] = {
    [POLARFIRE_DEBUG] =    {        0x0,      0x100 },
    [POLARFIRE_MROM] =     {     0x1000,    0x11000 },
    [POLARFIRE_TEST] =     {   0x100000,     0x1000 },
    [POLARFIRE_HOSTFS] =   {   0x101000,     0x1000 },
    [POLARFIRE_CLINT] =    {  0x2000000,    0x10000 },
    [POLARFIRE_L2LIM] =    {  0x8000000,  0x2000000 },
    [POLARFIRE_PLIC] =     {  0xc000000,  0x4000000 },
    [POLARFIRE_MMUART0] =  { 0x20000000,     0x1000 },
    [POLARFIRE_MMUART1] =  { 0x20100000,     0x1000 },
    [POLARFIRE_MMUART2] =  { 0x20102000,     0x1000 },
    [POLARFIRE_MMUART3] =  { 0x20104000,     0x1000 },
    [POLARFIRE_MMUART4] =  { 0x20106000,     0x1000 },
    [POLARFIRE_GPIO] =     { 0x20120000,     0x1000 },
    [POLARFIRE_DRAM] =     { 0x80000000,        0x0 },
    [POLARFIRE_GEM] =      { 0x100900FC,     0x2000 },
};

#define GEM_REVISION        0x10070109

static void riscv_polarfire_init(MachineState *machine)
{
    const struct MemmapEntry *memmap = polarfire_memmap;

    PolarFireState *s = g_new0(PolarFireState, 1);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    int i;

    /* Initialize SoC */
    object_initialize_child(OBJECT(machine), "soc", &s->soc,
                            sizeof(s->soc), TYPE_RISCV_U_SOC,
                            &error_abort, NULL);
    object_property_set_bool(OBJECT(&s->soc), true, "realized",
                            &error_abort);

    /* register RAM */
    memory_region_init_ram(main_mem, NULL, "riscv.sifive.u.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[POLARFIRE_DRAM].base,
                                main_mem);

    if (machine->kernel_filename) {
        riscv_load_kernel(machine->kernel_filename, NULL);
    }

    /* reset vector */
    uint32_t reset_vec[8] = {
        0x00000297,                    /* 1:  auipc  t0, %pcrel_hi(dtb) */
        0x02028593,                    /*     addi   a1, t0, %pcrel_lo(1b) */
        0xf1402573,                    /*     csrr   a0, mhartid  */
#if defined(TARGET_RISCV32)
        0x0182a283,                    /*     lw     t0, 24(t0) */
#elif defined(TARGET_RISCV64)
        0x0182b283,                    /*     ld     t0, 24(t0) */
#endif
        0x00028067,                    /*     jr     t0 */
        0x00000000,
        memmap[POLARFIRE_DRAM].base,   /* start: .dword DRAM_BASE */
        0x00000000,
                                       /* dtb: */
    };

    /* copy in the reset vector in little_endian byte order */
    for (i = 0; i < sizeof(reset_vec) >> 2; i++) {
        reset_vec[i] = cpu_to_le32(reset_vec[i]);
    }
    rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec),
                          memmap[POLARFIRE_MROM].base, &address_space_memory);
}

static void riscv_polarfire_soc_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    PolarFireSoCState *s = RISCV_U_SOC(obj);

    object_initialize_child(obj, "e-cluster", &s->e_cluster,
                            sizeof(s->e_cluster), TYPE_CPU_CLUSTER,
                            &error_abort, NULL);
    qdev_prop_set_uint32(DEVICE(&s->e_cluster), "cluster-id", 1);

    object_initialize_child(OBJECT(&s->e_cluster), "e-cpus",
                            &s->e_cpus, sizeof(s->e_cpus),
                            TYPE_RISCV_HART_ARRAY, &error_abort,
                            NULL);
    qdev_prop_set_uint32(DEVICE(&s->e_cpus), "num-harts", 1);
    qdev_prop_set_uint32(DEVICE(&s->e_cpus), "hartid-base", 0);
    qdev_prop_set_string(DEVICE(&s->e_cpus), "cpu-type", SIFIVE_E_CPU);

    object_initialize_child(obj, "u-cluster", &s->u_cluster,
                            sizeof(s->u_cluster), TYPE_CPU_CLUSTER,
                            &error_abort, NULL);
    qdev_prop_set_uint32(DEVICE(&s->u_cluster), "cluster-id", 0);

    object_initialize_child(OBJECT(&s->u_cluster), "u-cpus",
                            &s->u_cpus, sizeof(s->u_cpus),
                            TYPE_RISCV_HART_ARRAY, &error_abort,
                            NULL);
    qdev_prop_set_uint32(DEVICE(&s->u_cpus), "num-harts", ms->smp.cpus - 1);
    qdev_prop_set_uint32(DEVICE(&s->u_cpus), "hartid-base", 1);
    qdev_prop_set_string(DEVICE(&s->u_cpus), "cpu-type", SIFIVE_U_CPU);

    sysbus_init_child_obj(obj, "gem", &s->gem, sizeof(s->gem),
                          TYPE_CADENCE_GEM);


    for (int i = 0; i < POLARFIRE_NUM_GPIO; i++) {
        sysbus_init_child_obj(obj, "riscv.polarfiresoc.gpio[*]",
                              &s->gpio[i], sizeof(s->gpio[i]),
                              TYPE_PSE_GPIO);
    }
}

static void riscv_polarfire_soc_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    PolarFireSoCState *s = RISCV_U_SOC(dev);
    const struct MemmapEntry *memmap = polarfire_memmap;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    MemoryRegion *l2lim_mem = g_new(MemoryRegion, 1);
    qemu_irq plic_gpios[POLARFIRE_PLIC_NUM_SOURCES];
    char *plic_hart_config;
    size_t plic_hart_config_len;
    int i;
    Error *err = NULL;
    NICInfo *nd = &nd_table[0];
    qemu_irq *cpu_irqs;

    object_property_set_bool(OBJECT(&s->e_cpus), true, "realized",
                             &error_abort);
    object_property_set_bool(OBJECT(&s->u_cpus), true, "realized",
                             &error_abort);

    /*
     * The cluster must be realized after the RISC-V hart array container,
     * as the container's CPU object is only created on realize, and the
     * CPU must exist and have been parented into the cluster before the
     * cluster is realized.
     */
    object_property_set_bool(OBJECT(&s->e_cluster), true, "realized",
                             &error_abort);
    object_property_set_bool(OBJECT(&s->u_cluster), true, "realized",
                             &error_abort);

    /* boot rom */
    memory_region_init_rom(mask_rom, NULL, "riscv.sifive.u.mrom",
                           memmap[POLARFIRE_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[POLARFIRE_MROM].base,
                                mask_rom);

    /*
     * Add L2-LIM at reset size.
     * This should be reduced in size as the L2 Cache Controller WayEnable
     * register is incremented. Unfortunately I don't see a nice (or any) way
     * to handle reducing or blocking out the L2 LIM while still allowing it
     * be re returned to all enabled after a reset. For the time being, just
     * leave it enabled all the time. This won't break anything, but will be
     * too generous to misbehaving guests.
     */
    memory_region_init_ram(l2lim_mem, NULL, "riscv.sifive.u.l2lim",
                           memmap[POLARFIRE_L2LIM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[POLARFIRE_L2LIM].base,
                                l2lim_mem);

    /* create PLIC hart topology configuration string */
    plic_hart_config_len = (strlen(POLARFIRE_PLIC_HART_CONFIG) + 1) *
                           ms->smp.cpus;
    plic_hart_config = g_malloc0(plic_hart_config_len);
    for (i = 0; i < ms->smp.cpus; i++) {
        if (i != 0) {
            strncat(plic_hart_config, "," POLARFIRE_PLIC_HART_CONFIG,
                    plic_hart_config_len);
        } else {
            strncat(plic_hart_config, "M", plic_hart_config_len);
        }
        plic_hart_config_len -= (strlen(POLARFIRE_PLIC_HART_CONFIG) + 1);
    }

    /* MMIO */
    s->plic = sifive_plic_create(memmap[POLARFIRE_PLIC].base,
        plic_hart_config,
        POLARFIRE_PLIC_NUM_SOURCES,
        POLARFIRE_PLIC_NUM_PRIORITIES,
        POLARFIRE_PLIC_PRIORITY_BASE,
        POLARFIRE_PLIC_PENDING_BASE,
        POLARFIRE_PLIC_ENABLE_BASE,
        POLARFIRE_PLIC_ENABLE_STRIDE,
        POLARFIRE_PLIC_CONTEXT_BASE,
        POLARFIRE_PLIC_CONTEXT_STRIDE,
        memmap[POLARFIRE_PLIC].size);

    for (i = 0; i < serial_max_hds(); i++) {
        if (serial_hd(i)) {
            serial_mm_init(system_memory,
                           memmap[POLARFIRE_MMUART0 + i].base,
                           2,
                           qdev_get_gpio_in(DEVICE(s->plic),
                                            POLARFIRE_MMUART0_IRQ + i),
                           399193,
                           serial_hd(i),
                           DEVICE_NATIVE_ENDIAN);
        }
    }

    sifive_clint_create(memmap[POLARFIRE_CLINT].base,
        memmap[POLARFIRE_CLINT].size, ms->smp.cpus,
                        SIFIVE_SIP_BASE, SIFIVE_TIMECMP_BASE, SIFIVE_TIME_BASE, false);

    for (i = 0; i < POLARFIRE_PLIC_NUM_SOURCES; i++) {
        plic_gpios[i] = qdev_get_gpio_in(DEVICE(s->plic), i);
    }

    if (nd->used) {
        qemu_check_nic_model(nd, TYPE_CADENCE_GEM);
        qdev_set_nic_properties(DEVICE(&s->gem), nd);
    }
    object_property_set_int(OBJECT(&s->gem), GEM_REVISION, "revision",
                            &error_abort);
    object_property_set_bool(OBJECT(&s->gem), true, "realized", &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gem), 0, memmap[POLARFIRE_GEM].base);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->gem), 0,
                       plic_gpios[POLARFIRE_GEM_IRQ]);

    /* GPIO */
    for (int i = 0; i < POLARFIRE_NUM_GPIO; i++) {
        object_property_set_bool(OBJECT(&s->gpio), true, "realized", &err);
        if (err) {
            error_propagate(errp, err);
            return;
        }

        /* Map GPIO registers */
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio[i]), 0,
                        memmap[POLARFIRE_GPIO].base + i * 0x1000);

        /* Pass all GPIOs to the SOC layer so they are available to the board */
        qdev_pass_gpios(DEVICE(&s->gpio[i]), dev, "gpio[*]");

        /* Connect GPIO interrupt to the PLIC */
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 0,
                           qdev_get_gpio_in(DEVICE(s->plic),
                                            POLARFIRE_GPIO_IRQ + i));
    }

    sifive_test_create(memmap[POLARFIRE_TEST].base);

    /* Initialize the GnatBus Master */
    cpu_irqs = qemu_allocate_irqs(NULL, NULL, POLARFIRE_PLIC_NUM_SOURCES);
    for (i = 0; i < POLARFIRE_PLIC_NUM_SOURCES; i++) {
        cpu_irqs[i] = qdev_get_gpio_in(DEVICE(s->plic), i);
    }
    gnatbus_master_init(cpu_irqs, POLARFIRE_PLIC_NUM_SOURCES);
    gnatbus_device_init();

    /* HostFS */
    hostfs_create(memmap[POLARFIRE_HOSTFS].base, get_system_memory());
}

static void riscv_polarfire_machine_init(MachineClass *mc)
{
    mc->desc = "RISC-V Board compatible with Microsemi PolarFire SOC";
    mc->init = riscv_polarfire_init;
    /* The real hardware has 5 CPUs, but one of them is a small embedded power
     * management CPU.
     */
    mc->max_cpus = 5;
}

DEFINE_MACHINE("polarfire_soc", riscv_polarfire_machine_init)

static void riscv_polarfire_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = riscv_polarfire_soc_realize;
    /* Reason: Uses serial_hds in realize function, thus can't be used twice */
    dc->user_creatable = false;
}

static const TypeInfo riscv_polarfire_soc_type_info = {
    .name = TYPE_RISCV_U_SOC,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(PolarFireSoCState),
    .instance_init = riscv_polarfire_soc_init,
    .class_init = riscv_polarfire_soc_class_init,
};

static void riscv_polarfire_soc_register_types(void)
{
    type_register_static(&riscv_polarfire_soc_type_info);
}

type_init(riscv_polarfire_soc_register_types)
