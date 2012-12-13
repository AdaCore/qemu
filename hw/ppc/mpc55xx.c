/*
 * Qemu MPC5566 board emulation
 *
 * Copyright (C) 2011-2018 AdaCore
 *
 * This file is derived from hw/ppce500_mpc8544ds.c,
 * the copyright for that material belongs to the original owners.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "net/net.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/ppc/ppc.h"
#include "hw/boards.h"
#include "sysemu/sysemu.h"
#include "hw/ppc/openpic.h"
#include "hw/loader.h"
#include "elf.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "exec/memory.h"
#include "hw/char/esci.h"
#include "hw/intc/mpc55xx_intc.h"

#include "hw/net/fsl_etsec/etsec.h"

#include "hw/adacore/hostfs.h"
#include "hw/adacore/gnat-bus.h"
#include "qemu-traces.h"

typedef struct ResetData {
    PowerPCCPU  *cpu;
    uint32_t     entry;         /* save kernel entry in case of reset */
} ResetData;

static void main_cpu_reset(void *opaque)
{
    ResetData    *s    = (ResetData *)opaque;
    PowerPCCPU   *cpu  = s->cpu;
    CPUPPCState  *env  = &cpu->env;
    ppcmas_tlb_t *tlb1 = booke206_get_tlbm(env, 1, 0, 0);
    ppcmas_tlb_t *tlb2 = booke206_get_tlbm(env, 1, 0, 1);
    ppcmas_tlb_t *tlb3 = booke206_get_tlbm(env, 1, 0, 2);
    ppcmas_tlb_t *tlb4 = booke206_get_tlbm(env, 1, 0, 3);
    ppcmas_tlb_t *tlb5 = booke206_get_tlbm(env, 1, 0, 4);
    hwaddr        size;

    cpu_reset(CPU(cpu));

    env->nip = s->entry;
    env->tlb_dirty = true;

    /* Init tlb1 entry 0 (mcp5566's BAM) */
    size = 0x5 << MAS1_TSIZE_SHIFT; /* 1 Mbyte */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb1->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb1->mas2 = 0xfff00000 & TARGET_PAGE_MASK;
    tlb1->mas7_3 = 0xfff00000 & TARGET_PAGE_MASK;
    tlb1->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /* Init tlb1 entry 1 (mcp5566's BAM)*/
    size = 0x7 << MAS1_TSIZE_SHIFT; /* 16 Mbyte */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb2->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb2->mas2 = 0x0 & TARGET_PAGE_MASK;
    tlb2->mas7_3 = 0x0 & TARGET_PAGE_MASK;
    tlb2->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /* Init tlb1 entry 2 (mcp5566's BAM)*/
    size = 0x7 << MAS1_TSIZE_SHIFT; /* 16 Mbyte */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb3->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb3->mas2 = 0x20000000 & TARGET_PAGE_MASK;
    tlb3->mas7_3 = 0x20000000 & TARGET_PAGE_MASK;
    tlb3->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /* Init tlb1 entry 3 (mcp5566's BAM)*/
    size = 0x4 << MAS1_TSIZE_SHIFT; /* 256 Kbyte */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb4->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb4->mas2 = 0x40000000 & TARGET_PAGE_MASK;
    tlb4->mas7_3 = 0x40000000 & TARGET_PAGE_MASK;
    tlb4->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /* Init tlb1 entry 3 (mcp5566's BAM)*/
    size = 0x5 << MAS1_TSIZE_SHIFT; /* 1 Mbyte */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb5->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb5->mas2 = 0xC3F00000 & TARGET_PAGE_MASK;
    tlb5->mas7_3 = 0xC3F00000 & TARGET_PAGE_MASK;
    tlb5->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;
}

static void siu_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    switch (addr & 0xfff) {
    case 0x10:
        if (val & 0xC0000000) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    default:
        break;
    }
}

static const MemoryRegionOps siu_ops = {
    .write = siu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void fmpll_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    switch (addr & 0xf) {
    default:
        break;
    }
}

static uint64_t fmpll_read(void *opaque, hwaddr addr, unsigned size)
{

    switch (addr & 0xf) {
    case 0x4:
        return 0xB;             /* Lock, CALDONE, CALPASS */
        break;
    default:
        return 0;
        break;
    }
}

static const MemoryRegionOps fmpll_ops = {
    .read  = fmpll_read,
    .write = fmpll_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void mpc5566_init(MachineState *args)
{
    PowerPCCPU   *cpu;
    CPUPPCState  *env;
    uint64_t      elf_entry;
    uint64_t      elf_lowaddr;
    target_long   kernel_size = 0;
    MemoryRegion *ram, *misc_io, *rom;
    qemu_irq     *intc;
    ResetData    *reset_info;

    cpu = POWERPC_CPU(cpu_create(args->cpu_type));

    if (cpu == NULL) {
        fprintf(stderr, "Unable to initialize CPU! (%s)\n", args->cpu_type);
        exit(1);
    }

    env = &cpu->env;

    /* Reset data */
    reset_info      = g_malloc0(sizeof(ResetData));
    reset_info->cpu = cpu;
    qemu_register_reset(main_cpu_reset, reset_info);

    reset_info->entry = 0xfffffffc;

    ppc_booke_timers_init(cpu, 700000000UL >> 3, PPC_TIMER_E500);

    /* Register Memory */
    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "mpc5566.sram1", 32 * 1024,
                           &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x40000000, ram);

    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "mpc5566.sram2", 96 * 1024,
                           &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x40008000, ram);

    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "mpc5566.external_ram", 512 * 1024,
                           &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x20000000, ram);

    rom = g_malloc0(sizeof(*ram));
    memory_region_init_ram(rom, NULL, "mpc5566.rom", 3 * 1024 * 1024,
                           &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x00000000, rom);

    /* System integration unit */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &siu_ops, env,
                          "System integration unit", 0x20);
    memory_region_add_subregion(get_system_memory(), 0xC3F90000, misc_io);

    /* Frequency Modulated Phase Locked Loop and System Clocks */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &fmpll_ops, env, "FMPLL", 0x8);
    memory_region_add_subregion(get_system_memory(), 0xC3F80000, misc_io);

    /* INTC */
    intc = mpc55xx_intc_init(get_system_memory(), 0xFFF48000,
                             ((qemu_irq *)env->irq_inputs)[PPCE500_INPUT_INT]);

    if (!intc) {
        cpu_abort(CPU(env), "INTC failed to initialize\n");
    }

    if (serial_hd(0)) {
        esci_init(get_system_memory(), 0xFFFB0000, serial_hd(0), intc[146]);
    }

    if (serial_hd(1)) {
        esci_init(get_system_memory(), 0xFFFB4000, serial_hd(1), intc[149]);
    }

    /* Load kernel. */
    if (args->kernel_filename) {
        kernel_size = load_elf(args->kernel_filename, NULL, NULL, NULL,
                               &elf_entry, &elf_lowaddr, NULL, 1,
                               PPC_ELF_MACHINE, 0, 0);

        /* XXX try again as binary */
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    args->kernel_filename);
            exit(1);
        }
    }

    /* Set read-only after loading executable */
    memory_region_set_readonly(rom, true);

    /* If we're loading a kernel directly, we must load the device tree too. */
    if (args->kernel_filename) {
        /* Set initial guest state. */
        reset_info->entry = elf_entry;
    }

    /* HostFS */
    hostfs_create(0xFFFA0000, get_system_memory());

    /* Initialize the GnatBus Master */
    gnatbus_master_init(intc, 330);
    gnatbus_device_init();

}

static void mpc5566_generic_machine_init(MachineClass *mc)
{
    mc->desc = "mpc5566";
    mc->init = mpc5566_init;
    /* It was e500v1 which is an alias of e500v20 and can only be used on
     * command-line. Theorically it should be e200z6 but unfortunately the
     * interrupt controller is not implemented for this core and it just badly
     * crash. So let's take e500_v20 which stand for e500v1 on the
     * command-line.
     */
    mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("e500_v20");
}

DEFINE_MACHINE("mpc5566", mpc5566_generic_machine_init)
