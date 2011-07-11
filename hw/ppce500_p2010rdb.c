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
#include "device_tree.h"
#include "openpic.h"
#include "loader.h"
#include "elf.h"
#include "flash.h"
#include "etsec.h"
#include "espi.h"
#include "fsl_I2C.h"

//#define DEBUG_P2010

#define BIOS_SIZE (1 * 1024 * 1024)
#define BIOS_FILENAME "p2010rdb_rom.bin"

#define UIMAGE_LOAD_BASE           0
#define DTC_LOAD_PAD               0x500000
#define DTC_PAD_MASK               0xFFFFF
#define INITRD_LOAD_PAD            0x2000000
#define INITRD_PAD_MASK            0xFFFFFF

#define RAM_SIZE                   (1024 * 1024 * 1204) /* 1Go */

#define P2010RDB_CCSRBAR_BASE       0xf3000000 /* u-boot 0xffe00000 */ /* vxworks bootapp 0xF3000000 */
#define P2010RDB_LOCAL_CONF         (P2010RDB_CCSRBAR_BASE + 0x00000)
#define P2010RDB_DDR_CONTROLLER     (P2010RDB_CCSRBAR_BASE + 0x02000)
#define P2010RDB_SERIAL0_REGS_BASE  (P2010RDB_CCSRBAR_BASE + 0x04500)
#define P2010RDB_SERIAL1_REGS_BASE  (P2010RDB_CCSRBAR_BASE + 0x04600)
#define P2010RDB_ELBC               (P2010RDB_CCSRBAR_BASE + 0x05000)
#define P2010RDB_ESPI_BASE          (P2010RDB_CCSRBAR_BASE + 0x07000)
#define P2010RDB_PCI_REGS_BASE      (P2010RDB_CCSRBAR_BASE + 0x08000)
#define P2010RDB_GPIO               (P2010RDB_CCSRBAR_BASE + 0x0F000)
#define P2010RDB_L2CACHE            (P2010RDB_CCSRBAR_BASE + 0x20000)
#define P2010RDB_ETSEC1_BASE        (P2010RDB_CCSRBAR_BASE + 0x24000)
#define P2010RDB_MPIC_REGS_BASE     (P2010RDB_CCSRBAR_BASE + 0x40000)
#define P2010RDB_GLOBAL_UTILITIES   (P2010RDB_CCSRBAR_BASE + 0xE0000)
#define P2010RDB_I2C_1_BASE         (P2010RDB_CCSRBAR_BASE + 0x03000)
#define P2010RDB_I2C_2_BASE         (P2010RDB_CCSRBAR_BASE + 0x03100)
#define P2010RDB_VSC7385            (0xF1000000)
#define P2010RDB_VSC7385_SIZE       (0x00020000)
#define P2010RDB_PCI_REGS_SIZE      0x1000
#define P2010RDB_PCI_IO             0xE1000000
#define P2010RDB_PCI_IOLEN          0x10000

#ifdef DEBUG_P2010
#define PRINT_WRITE_UNSUPPORTED_REGISTER(name, addr, val, nip)          \
printf("%s\t: Unsupported addr:0x" TARGET_FMT_plx " val:0x%08x nip:"    \
       TARGET_FMT_plx " '" name "'\n",                                  \
       __func__, (addr), (val), (nip));                                 \

#define PRINT_READ_UNSUPPORTED_REGISTER(name, addr, nip)                \
printf("%s\t: Unsupported addr:0x" TARGET_FMT_plx "                nip:" \
       TARGET_FMT_plx " '" name "'\n",                                  \
       __func__, (addr), (nip));                                        \

#else /* DEBUG_P2010 */
#define PRINT_WRITE_UNSUPPORTED_REGISTER(name, addr, val, nip)  \
(void)(name);(void)(addr);(void)(val); (void)(nip)
#define PRINT_READ_UNSUPPORTED_REGISTER(name, addr, nip)        \
(void)(name);(void)(addr);(void)(nip)

#endif  /* DEBUG_P2010 */

typedef struct ResetData {
    CPUState *env;
    uint32_t  entry;            /* save kernel entry in case of reset */
} ResetData;

static void main_cpu_reset(void *opaque)
{
    ResetData *s   = (ResetData *)opaque;
    CPUState  *env = s->env;
    ppcemb_tlb_t *tlb1 = booke206_get_tlbe(env, 1, 0, 0);
    ppcemb_tlb_t *tlb2 = booke206_get_tlbe(env, 1, 0, 1);

    cpu_reset(env);

    env->nip = s->entry;

    /* Init tlb1 entry 0 (e500 )*/
    tlb1->RPN  = 0xfffff;
    tlb1->EPN  = 0xfffff;
    tlb1->PID  = 0;
    tlb1->size = 4 * 1024; /* 4 Kbytes */
    tlb1->prot = PAGE_VALID
        | (PAGE_READ | PAGE_WRITE | PAGE_EXEC) << 4;
    tlb1->attr = MAS1_IPROT; /* Protected */

    /* Init tlb1 entry 1 (p2010rdb)*/
    tlb2->RPN  = 0x0;
    tlb2->EPN  = 0x0;
    tlb2->PID  = 0;
    tlb2->size = (1 << (0xb << 1)) << 10;  /* 4 Gbytes */
    tlb2->prot = PAGE_VALID
        | (PAGE_READ | PAGE_WRITE | PAGE_EXEC) << 4;
    tlb2->attr = MAS1_IPROT; /* Protected */
}

static uint32_t p2010_gbu_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_GLOBAL_UTILITIES;

    switch (addr){
    case 0x0:
        /* e500_1_Ratio = 100
         * e500_0_Ratio = 100
         * DDR_Ratio = 1010
         * plat_Ratio = 101
         */
        return 0x0404140a;
        break;
    case 0xa0:                  /* PVR */
        return env->spr[SPR_PVR];
        break;
    case 0xa4:                  /* SVR */
        return env->spr[SPR_E500_SVR];
        break;
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_gbu_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_GLOBAL_UTILITIES;

    PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr , val, env->nip);
}

static CPUReadMemoryFunc *p2010_gbu_io_read[3] = {
    NULL,
    NULL,
    p2010_gbu_io_readl
};

static CPUWriteMemoryFunc *p2010_gbu_io_write[3] = {
    NULL,
    NULL,
    p2010_gbu_io_writel,
};

static uint32_t p2010_lca_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_LOCAL_CONF;

    switch (addr){
    case 0x0:
        PRINT_READ_UNSUPPORTED_REGISTER("CCSRBAR", full_addr, env->nip);
        break;
    case 0xc08 ... 0xd70:
        PRINT_READ_UNSUPPORTED_REGISTER("Local access window",
                                        full_addr, env->nip);
        break;
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_lca_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_LOCAL_CONF;

    switch (addr){
    case 0x0:
        PRINT_WRITE_UNSUPPORTED_REGISTER("CCSRBAR", full_addr, val, env->nip);
        break;

    case 0xc08 ... 0xd70:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Local access window",
                                         full_addr, val, env->nip);
        break;
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_lca_io_read[3] = {
    NULL,
    NULL,
    p2010_lca_io_readl
};

static CPUWriteMemoryFunc *p2010_lca_io_write[3] = {
    NULL,
    NULL,
    p2010_lca_io_writel,
};

static uint32_t p2010_elbc_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_ELBC;

    switch (addr){
    case 0x00:    case 0x08:    case 0x10:    case 0x18:
    case 0x20:    case 0x28:    case 0x30:    case 0x38:
        PRINT_READ_UNSUPPORTED_REGISTER("Base registers", full_addr, env->nip);
        break;

    case 0x04:    case 0x0c:    case 0x14:    case 0x1c:
    case 0x24:    case 0x2c:    case 0x34:    case 0x3c:
        PRINT_READ_UNSUPPORTED_REGISTER("Base registers", full_addr, env->nip);
        break;

    case 0xd4:
        PRINT_READ_UNSUPPORTED_REGISTER("Clock Ratio Register", full_addr, env->nip);
        break;

    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_elbc_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_ELBC;

    switch (addr){
    case 0x00:    case 0x08:    case 0x10:    case 0x18:
    case 0x20:    case 0x28:    case 0x30:    case 0x38:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Base registers", full_addr, val, env->nip);
        break;

    case 0x04:    case 0x0c:    case 0x14:    case 0x1c:
    case 0x24:    case 0x2c:    case 0x34:    case 0x3c:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Base registers", full_addr, val, env->nip);
        break;

    case 0xd4:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Clock Ratio Register",
                                         full_addr, val, env->nip);
        break;

    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_elbc_io_read[3] = {
    NULL,
    NULL,
    p2010_elbc_io_readl
};

static CPUWriteMemoryFunc *p2010_elbc_io_write[3] = {
    NULL,
    NULL,
    p2010_elbc_io_writel,
};

static uint32_t p2010_ddr_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_DDR_CONTROLLER;

    switch (addr){

    case 0x00:        case 0x08:        case 0x10:         case 0x18:
        PRINT_READ_UNSUPPORTED_REGISTER("Chip Select Memory Bounds",
                                        full_addr, env->nip);
        break;

    case 0x80 ... 0x8c:
    case 0xc0 ... 0xcc:
        PRINT_READ_UNSUPPORTED_REGISTER("Chip Select Config",
                                        full_addr, env->nip);
        break;

    case 0x110 ... 0x11c:
        PRINT_READ_UNSUPPORTED_REGISTER("Control Config",
                                        full_addr, env->nip);
        break;

    case 0x120:
        PRINT_READ_UNSUPPORTED_REGISTER("Mode Control",
                                        full_addr, env->nip);
        break;

    case 0x124:
        PRINT_READ_UNSUPPORTED_REGISTER("Interval Configuration",
                                        full_addr, env->nip);
        break;

    case 0x128:
        PRINT_READ_UNSUPPORTED_REGISTER("Data Initialization",
                                        full_addr, env->nip);
        break;

    case 0x130:
        PRINT_READ_UNSUPPORTED_REGISTER("Clock Control",
                                        full_addr, env->nip);
        break;

    case 0x100 ... 0x10c:
    case 0x160 ... 0x164:
        PRINT_READ_UNSUPPORTED_REGISTER("Timing config",
                                        full_addr, env->nip);
        break;

    case 0x170:
        PRINT_READ_UNSUPPORTED_REGISTER("Calibration Control",
                                        full_addr, env->nip);

    case 0x174:
        PRINT_READ_UNSUPPORTED_REGISTER("Write Leveling Control",
                                        full_addr, env->nip);
        break;

    case 0xe48:
        PRINT_READ_UNSUPPORTED_REGISTER("Memory error interrupt enable",
                                        full_addr, env->nip);
        break;

    case 0xe58:
        PRINT_READ_UNSUPPORTED_REGISTER("Single-Bit ECC memory error mgmt",
                                        full_addr, env->nip);
        break;

    case 0xb28 ... 0xb2c:
        PRINT_READ_UNSUPPORTED_REGISTER("Control Driver Register",
                                        full_addr, env->nip);
        break;

    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_ddr_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_DDR_CONTROLLER;

    switch (addr){

    case 0x00:        case 0x08:        case 0x10:         case 0x18:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Chip Select Memory Bounds",
                                         full_addr, val, env->nip);
        break;

    case 0x80 ... 0x8c:
    case 0xc0 ... 0xcc:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Chip Select Config",
                                        full_addr, val, env->nip);
        break;

    case 0x110 ... 0x11c:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Control Config",
                                         full_addr, val, env->nip);
        break;

    case 0x120:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Mode Control",
                                         full_addr, val, env->nip);
        break;

    case 0x124:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Interval Configuration",
                                         full_addr, val, env->nip);
        break;

    case 0x128:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Data Initialization",
                                         full_addr, val, env->nip);
        break;

    case 0x130:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Clock Control",
                                         full_addr, val, env->nip);
        break;

    case 0x100 ... 0x10c:
    case 0x160 ... 0x164:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Timing config",
                                         full_addr, val, env->nip);
        break;

    case 0x170:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Calibration Control",
                                         full_addr, val, env->nip);
        break;

    case 0x174:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Write Leveling Control",
                                         full_addr, val, env->nip);
        break;

    case 0xe48:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Memory error interrupt enable",
                                        full_addr, val, env->nip);
        break;

    case 0xe58:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Single-Bit ECC memory error mgmt",
                                        full_addr, val, env->nip);
        break;

    case 0xb28 ... 0xb2c:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Control Driver Register",
                                        full_addr, val, env->nip);
        break;

    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_ddr_io_read[3] = {
    NULL,
    NULL,
    p2010_ddr_io_readl
};

static CPUWriteMemoryFunc *p2010_ddr_io_write[3] = {
    NULL,
    NULL,
    p2010_ddr_io_writel,
};

static uint32_t p2010_gpio_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_GPIO;

    switch (addr) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_gpio_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_GPIO;

    switch (addr){
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_gpio_io_read[3] = {
    NULL,
    NULL,
    p2010_gpio_io_readl
};

static CPUWriteMemoryFunc *p2010_gpio_io_write[3] = {
    NULL,
    NULL,
    p2010_gpio_io_writel,
};

static uint32_t p2010_l2cache_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_L2CACHE;

    switch (addr) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_l2cache_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_L2CACHE;

    switch (addr){
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_l2cache_io_read[3] = {
    NULL,
    NULL,
    p2010_l2cache_io_readl
};

static CPUWriteMemoryFunc *p2010_l2cache_io_write[3] = {
    NULL,
    NULL,
    p2010_l2cache_io_writel,
};

static uint32_t p2010_vsc7385_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_VSC7385;

    switch (addr) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_vsc7385_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + P2010RDB_VSC7385;

    switch (addr){
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_vsc7385_io_read[3] = {
    NULL,
    NULL,
    p2010_vsc7385_io_readl
};

static CPUWriteMemoryFunc *p2010_vsc7385_io_write[3] = {
    NULL,
    NULL,
    p2010_vsc7385_io_writel,
};

static uint32_t p2010_ccsbar_io_readl(void *opaque, target_phys_addr_t addr)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + 0xff700000;

    switch (addr) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("CCSRBAR", full_addr, env->nip);
    }
    return 0;
}

static void p2010_ccsbar_io_writel(void *opaque, target_phys_addr_t addr,
                            uint32_t val)
{
    CPUState           *env       = opaque;
    target_phys_addr_t  full_addr = addr + 0xff700000;

    switch (addr){
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("CCSRBAR", full_addr, val, env->nip);
    }
}

static CPUReadMemoryFunc *p2010_ccsbar_io_read[3] = {
    NULL,
    NULL,
    p2010_ccsbar_io_readl
};

static CPUWriteMemoryFunc *p2010_ccsbar_io_write[3] = {
    NULL,
    NULL,
    p2010_ccsbar_io_writel,
};

static void p2010rdb_init(ram_addr_t ram_size,
                         const char *boot_device,
                         const char *kernel_filename,
                         const char *kernel_cmdline,
                         const char *initrd_filename,
                         const char *cpu_model)
{
    CPUState           *env;
    uint64_t            elf_entry;
    uint64_t            elf_lowaddr;
    target_phys_addr_t  entry       = 0;
    target_phys_addr_t  loadaddr    = UIMAGE_LOAD_BASE;
    target_long         kernel_size = 0;
    target_ulong        dt_base     = 0;
    ram_addr_t          bios_offset = 0;
    int                 bios_size   = 0;
    qemu_irq           *irqs, *mpic;
    ResetData          *reset_info;
    DriveInfo          *dinfo;
    int                 fl_sectors;
    int                 io_memory;
    int                 ram_offset;
    int                 i;

    /* Setup CPU */
    env = cpu_ppc_init("p2010");
    if (!env) {
        fprintf(stderr, "Unable to initialize CPU!\n");
        exit(1);
    }

    /* Reset data */
    reset_info        = qemu_mallocz(sizeof(ResetData));
    reset_info->env   = env;
    qemu_register_reset(main_cpu_reset, reset_info);

    reset_info->entry = 0xfffffffc;

    ppc_booke_timers_init(env, 700000000UL >> 3);
    /* Register Memory */
    ram_size = RAM_SIZE;
    ram_offset = qemu_ram_alloc(NULL, "p2010rdb.ram", ram_size);
    cpu_register_physical_memory(0x0, ram_size, ram_offset);

    /* allocate and load BIOS */
    dinfo           = drive_get(IF_PFLASH, 0, 0);
    if (dinfo) {
        bios_size   = bdrv_getlength(dinfo->bdrv);
        bios_offset = qemu_ram_alloc(NULL, "p2010.bios", bios_size);
        fl_sectors  = (bios_size + 65535) >> 16;

        if (pflash_cfi02_register((uint32_t)(-bios_size), bios_offset,
                                  dinfo->bdrv, 65536, fl_sectors, 1,
                                  2, 0x0001, 0x22DA, 0x0000, 0x0000, 0x555, 0x2AA,
                                  1) == NULL) {
            fprintf(stderr, "%s: Failed to load flash image\n", __func__);
            abort();
        }
    }

    /* CCSBAR */
    io_memory = cpu_register_io_memory(p2010_ccsbar_io_read,
                                       p2010_ccsbar_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(0xff700000, 0x1, io_memory);

    /* Global Utilities */
    io_memory = cpu_register_io_memory(p2010_gbu_io_read,
                                       p2010_gbu_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_GLOBAL_UTILITIES, 0x1000, io_memory);

    /* Local configuration/access */
    io_memory = cpu_register_io_memory(p2010_lca_io_read,
                                       p2010_lca_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_LOCAL_CONF, 0x1000, io_memory);

    /* eLBC */
    io_memory = cpu_register_io_memory(p2010_elbc_io_read,
                                       p2010_elbc_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_ELBC, 0x1000, io_memory);

    /* DDR Memory Controller */
    io_memory = cpu_register_io_memory(p2010_ddr_io_read,
                                       p2010_ddr_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_DDR_CONTROLLER, 0x1000, io_memory);

    /* GPIO */
    io_memory = cpu_register_io_memory(p2010_gpio_io_read,
                                       p2010_gpio_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_GPIO, 0x1000, io_memory);

    /* L2CACHE */
    io_memory = cpu_register_io_memory(p2010_l2cache_io_read,
                                       p2010_l2cache_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_L2CACHE, 0x1000, io_memory);

    /* VSC7385 */
    io_memory = cpu_register_io_memory(p2010_vsc7385_io_read,
                                       p2010_vsc7385_io_write,
                                       env, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(P2010RDB_VSC7385, P2010RDB_VSC7385_SIZE, io_memory);

    /* MPIC */
    irqs = qemu_mallocz(sizeof(qemu_irq) * OPENPIC_OUTPUT_NB);
    irqs[OPENPIC_OUTPUT_INT] = ((qemu_irq *)env->irq_inputs)[PPCE500_INPUT_INT];
    irqs[OPENPIC_OUTPUT_CINT] = ((qemu_irq *)env->irq_inputs)[PPCE500_INPUT_CINT];
    mpic = mpic_init(P2010RDB_MPIC_REGS_BASE, 1, &irqs, NULL);

    /* Serial */
    if (serial_hds[0]) {
        serial_mm_init(P2010RDB_SERIAL0_REGS_BASE,
                       0, mpic[12+26], 399193,
                       serial_hds[0], 1, 1);
    }

    if (serial_hds[1]) {
        serial_mm_init(P2010RDB_SERIAL1_REGS_BASE,
                       0, mpic[12+26], 399193,
                       serial_hds[0], 1, 1);
    }

    for (i = 0; i < nb_nics; i++) {
        etsec_create(P2010RDB_ETSEC1_BASE + 0x1000 * i, &nd_table[i],
        mpic[12+13], mpic[12+14], mpic[12+18]);
    }

    /* eSPI */
    espi_create(P2010RDB_ESPI_BASE, mpic[12+43]);

    /* I2C */
    fsl_i2c_create(P2010RDB_I2C_1_BASE, mpic[12+27]);
    fsl_i2c_create(P2010RDB_I2C_2_BASE, mpic[12+27]);

    /* Load kernel. */
    if (kernel_filename) {
        kernel_size = load_elf(kernel_filename, NULL, NULL, &elf_entry,
                               &elf_lowaddr, NULL, 1, ELF_MACHINE, 0);
        entry = elf_entry;
        loadaddr = elf_lowaddr;

        /* XXX try again as binary */
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    kernel_filename);
            exit(1);
        }
    }

    /* If we're loading a kernel directly, we must load the device tree too. */
    if (kernel_filename) {
        dt_base = (kernel_size + DTC_LOAD_PAD) & ~DTC_PAD_MASK;

        cpu_synchronize_state(env);

        /* Set initial guest state. */
        env->gpr[1] = elf_lowaddr + 4 * 1024 * 1024; /* FIXME: sp? */
        env->gpr[3] = dt_base;
        env->nip = elf_entry;    /* FIXME: entry? */
        reset_info->entry = elf_entry;
    }
}

static QEMUMachine p2010rdb_machine = {
    .name = "p2010rdb",
    .desc = "p2010rdb",
    .init = p2010rdb_init,
};

static void p2010rdb_machine_init(void)
{
    qemu_register_machine(&p2010rdb_machine);
}

machine_init(p2010rdb_machine_init);
