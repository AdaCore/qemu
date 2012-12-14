/*
 * Qemu PowerPC p2010rdb board emualtion
 *
 * Copyright (C) 2011 AdaCore
 *
 * This file is derived from hw/ppce500_mpc8544ds.c,
 * the copyright for that material belongs to the original owners.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 */

#include <dirent.h>

#include "config.h"
#include "qemu-common.h"
#include "qemu-plugin.h"
#include "net.h"
#include "hw.h"
#include "pc.h"
#include "pci.h"
#include "ppc.h"
#include "boards.h"
#include "sysemu.h"
#include "kvm.h"
#include "kvm_ppc.h"
#include "openpic.h"
#include "loader.h"
#include "elf.h"
#include "sysbus.h"
#include "flash.h"
#include "etsec.h"
#include "espi.h"
#include "fsl_I2C.h"
#include "exec-memory.h"
#include "hostfs.h"

#include "qemu-plugin.h"
#include "gnat-bus.h"
#include "qemu-traces.h"

qemu_irq *mpc55xx_intc_init(MemoryRegion *address_space,
                            target_phys_addr_t base, qemu_irq irq_out);

void esci_init(MemoryRegion *address_space, target_phys_addr_t base,
               CharDriverState *chr, qemu_irq irq);

typedef struct ResetData {
    CPUPPCState *env;
    uint32_t     entry;         /* save kernel entry in case of reset */
} ResetData;

static void main_cpu_reset(void *opaque)
{
    ResetData          *s    = (ResetData *)opaque;
    CPUPPCState        *env  = s->env;
    ppcmas_tlb_t       *tlb1 = booke206_get_tlbm(env, 1, 0, 0);
    ppcmas_tlb_t       *tlb2 = booke206_get_tlbm(env, 1, 0, 1);
    ppcmas_tlb_t       *tlb3 = booke206_get_tlbm(env, 1, 0, 2);
    ppcmas_tlb_t       *tlb4 = booke206_get_tlbm(env, 1, 0, 3);
    ppcmas_tlb_t       *tlb5 = booke206_get_tlbm(env, 1, 0, 4);
    target_phys_addr_t  size;

    cpu_state_reset(env);

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

static void siu_write(void *opaque, target_phys_addr_t addr,
                                uint64_t val, unsigned size)
{

    switch (addr & 0xfff) {
    case 0x10:
        if (val & 0xC0000000) {
            qemu_system_reset_request();
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

static void mpc5566_init(ram_addr_t ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{
    CPUPPCState        *env;
    uint64_t            elf_entry;
    uint64_t            elf_lowaddr;
    target_long         kernel_size = 0;
    MemoryRegion       *ram, *misc_io;;
    qemu_irq           *intc;
    ResetData          *reset_info;

    /* Setup CPU */
    if (cpu_model == NULL) {
        cpu_model = "e500v1";
    }
    env = cpu_ppc_init(cpu_model);
    if (!env) {
        fprintf(stderr, "Unable to initialize CPU! (%s)\n", cpu_model);
        exit(1);
    }

    /* Reset data */
    reset_info        = g_malloc0(sizeof(ResetData));
    reset_info->env   = env;
    qemu_register_reset(main_cpu_reset, reset_info);

    reset_info->entry = 0xfffffffc;

    ppc_booke_timers_init(env, 700000000UL >> 3, PPC_TIMER_E500);

    /* Register Memory */
    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, "mpc5566.sram1", 32 * 1024);
    memory_region_add_subregion(get_system_memory(), 0x40000000, ram);

    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, "mpc5566.sram2", 96 * 1024);
    memory_region_add_subregion(get_system_memory(), 0x40008000, ram);

    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, "mpc5566.external_ram", 512 * 1024);
    memory_region_add_subregion(get_system_memory(), 0x20000000, ram);

    /* System integration unit */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, &siu_ops, env, "System integration unit",
                          0x10);
    memory_region_add_subregion(get_system_memory(), 0xC3F90000, misc_io);

    /* INTC */
    intc = mpc55xx_intc_init(get_system_memory(), 0xFFF48000,
                             ((qemu_irq *)env->irq_inputs)[PPCE500_INPUT_INT]);

    if (!intc) {
        cpu_abort(env, "INTC failed to initialize\n");
    }

    if (serial_hds[0]) {
        esci_init(get_system_memory(), 0xFFFB0000, serial_hds[0], intc[146]);
    }

    if (serial_hds[1]) {
        esci_init(get_system_memory(), 0xFFFB4000, serial_hds[1], intc[149]);
    }

    /* Load kernel. */
    if (kernel_filename) {
        kernel_size = load_elf(kernel_filename, NULL, NULL, &elf_entry,
                               &elf_lowaddr, NULL, 1, ELF_MACHINE, 0);

        /* XXX try again as binary */
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    kernel_filename);
            exit(1);
        }
    }

    /* If we're loading a kernel directly, we must load the device tree too. */
    if (kernel_filename) {
        cpu_synchronize_state(env);

        /* Set initial guest state. */
        reset_info->entry = elf_entry;
    }

    /* Initialize plug-ins */
    plugin_init(intc, 330);
    plugin_device_init();

    /* Initialize the GnatBus Master */
    gnatbus_master_init(intc, 330);
    gnatbus_device_init();

}

static QEMUMachine mpc5566_machine = {
    .name = "mpc5566",
    .desc = "mpc5566",
    .init = mpc5566_init,
};

static void mpc5566_machine_init(void)
{
    qemu_register_machine(&mpc5566_machine);
}

machine_init(mpc5566_machine_init);
