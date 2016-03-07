/*
 * QEMU PowerPC p2010rdb board emualtion
 *
 * Copyright (C) 2011-2013 AdaCore
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

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "net/net.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/ppc/ppc.h"
#include "hw/boards.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "kvm_ppc.h"
#include "hw/ppc/openpic.h"
#include "hw/loader.h"
#include "elf.h"
#include "hw/sysbus.h"
#include "sysemu/block-backend.h"
#include "hw/block/flash.h"
#include "hw/net/fsl_etsec/etsec.h"
#include "exec/memory.h"
#include "hw/char/serial.h"

#include "hw/adacore/gnat-bus.h"
#include "qemu-traces.h"
#include "hw/adacore/hostfs.h"

/* #define DEBUG_P2010 */

#define MAX_ETSEC_CONTROLLERS 3

#define UIMAGE_LOAD_BASE           0
#define DTC_LOAD_PAD               0x500000
#define DTC_PAD_MASK               0xFFFFF
#define INITRD_LOAD_PAD            0x2000000
#define INITRD_PAD_MASK            0xFFFFFF

/* Offsets in CCSRBAR */
#define PARAMS_ADDR                 0x80000
#define PARAMS_SIZE                 0x01000
#define QTRACE_START                0x81000
#define QTRACE_SIZE                 0x01000
#define HOSTFS_START                0x82000

#define P2010RDB_CCSRBAR_BASE       0xff700000
#if 0
#define P2010RDB_CCSRBAR_BASE       0xf3000000 /* u-boot 0xffe00000 */
                                               /* vxworks bootapp 0xF3000000 */
                                               /* vxworks653: 0xe0000000 */
#endif
#define P2010RDB_LOCAL_CONF         0x00000
#define P2010RDB_DDR_CONTROLLER     0x02000
#define P2010RDB_SERIAL0_REGS_BASE  0x04500
#define P2010RDB_SERIAL1_REGS_BASE  0x04600
#define P2010RDB_ELBC               0x05000
#define P2010RDB_ESPI_BASE          0x07000
#define P2010RDB_PCI_REGS_BASE      0x08000
#define P2010RDB_GPIO               0x0F000
#define P2010RDB_L2CACHE            0x20000
#define P2010RDB_ETSEC1_BASE        0x24000
#define P2010RDB_MPIC_REGS_BASE     0x40000
#define P2010RDB_GLOBAL_UTILITIES   0xE0000
#define P2010RDB_VSC7385            0xF1000000
#define P2010RDB_VSC7385_SIZE       0x00020000
#define P2010RDB_PCI_REGS_SIZE      0x1000
#define P2010RDB_PCI_IO             0xE1000000
#define P2010RDB_PCI_IOLEN          0x10000

#ifdef DEBUG_P2010
#define PRINT_WRITE_UNSUPPORTED_REGISTER(name, addr, val, nip)          \
do {                                                                    \
 printf("%s\t: Unsupported addr:0x" TARGET_FMT_plx " val:0x%08lx nip:"  \
        TARGET_FMT_lx " '" name "'\n",                                  \
        __func__, (addr), (long unsigned int)(val), (nip));             \
 } while (0)

#define PRINT_READ_UNSUPPORTED_REGISTER(name, addr, nip)                \
do {                                                                    \
    printf("%s\t: Unsupported addr:0x" TARGET_FMT_plx "                nip:" \
           TARGET_FMT_lx " '" name "'\n",                               \
           __func__, (addr), (nip));                                    \
 } while (0)

#define PRINT_SUPPORTED_REGISTER(name, addr, val, nip)                  \
do {                                                                    \
    printf("%s\t: Supported addr:0x" TARGET_FMT_plx " val:0x%08lx nip:" \
           TARGET_FMT_lx " '" name "'\n",                               \
           __func__, (addr), (long unsigned int)(val), (nip));          \
 } while (0)

#else /* DEBUG_P2010 */
#define PRINT_WRITE_UNSUPPORTED_REGISTER(name, addr, val, nip)  \
do {                                                            \
    (void)(name); (void)(addr); (void)(val); (void)(nip);       \
 } while (0)
#define PRINT_READ_UNSUPPORTED_REGISTER(name, addr, nip)        \
do {                                                            \
    (void)(name); (void)(addr); (void)(nip);                    \
 } while (0)
#define PRINT_SUPPORTED_REGISTER(name, addr, val, nip)          \
do {                                                            \
    (void)(name); (void)(addr); (void)(val); (void)(nip);       \
 } while (0)

#endif  /* DEBUG_P2010 */

typedef struct ResetData {
    PowerPCCPU  *cpu;
    uint32_t     entry;         /* save kernel entry in case of reset */
} ResetData;

typedef struct fsl_e500_config {
    uint32_t ccsr_init_addr;
    const char *cpu_model;
    uint32_t freq;

    int serial_irq;

    int cfi01_flash;

    int etsec_irq_err[MAX_ETSEC_CONTROLLERS];
    int etsec_irq_tx[MAX_ETSEC_CONTROLLERS];
    int etsec_irq_rx[MAX_ETSEC_CONTROLLERS];
} fsl_e500_config;

static MemoryRegion *ccsr_space;
static uint64_t      ccsr_addr = P2010RDB_CCSRBAR_BASE;

static void sec_cpu_reset(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);

    cpu_reset(cs);

    /* Secondary CPU starts in halted state for now. Needs to change when
       implementing non-kernel boot. */
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
}

static void main_cpu_reset(void *opaque)
{
    ResetData    *s    = (ResetData *)opaque;
    PowerPCCPU   *cpu  = s->cpu;
    CPUPPCState  *env  = &cpu->env;
    ppcmas_tlb_t *tlb1 = booke206_get_tlbm(env, 1, 0, 0);
    ppcmas_tlb_t *tlb2 = booke206_get_tlbm(env, 1, 0, 1);
    hwaddr        size;

    cpu_reset(CPU(cpu));

    env->nip = s->entry;
    env->tlb_dirty = true;

    /* Init tlb1 entry 0 (e500 )*/
    size = 0x1 << MAS1_TSIZE_SHIFT; /* 4 KBytes */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb1->mas1    = MAS1_VALID | MAS1_IPROT | size;
    tlb1->mas2    = 0xfffff000 &   TARGET_PAGE_MASK;
    tlb1->mas7_3  = 0xfffff000 & TARGET_PAGE_MASK;
    tlb1->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /* Init tlb1 entry 1 (p2010rdb)*/
    size = 0xb << MAS1_TSIZE_SHIFT; /* 4Gbytes */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb2->mas1    = MAS1_VALID | MAS1_IPROT | size;
    tlb2->mas2    = 0x0 &   TARGET_PAGE_MASK;
    tlb2->mas7_3  = 0x0 & TARGET_PAGE_MASK;
    tlb2->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /*
     * I'm still not sure to understand why, but the bootApp set PID to 1 before
     * starting the kernel.
     */
    env->spr[SPR_BOOKE_PID] = 0x1;
}

uint32_t g_DEVDISR  = 0x0;
uint32_t g_DDRDLLCR = 0x0;
uint32_t g_LBDLLCR  = 0x0;

static uint64_t p2010_gbu_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_GLOBAL_UTILITIES + ccsr_addr;

    switch (addr) {
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
    case 0x70:
        return g_DEVDISR;
        break;
    case 0xe10:
        PRINT_READ_UNSUPPORTED_REGISTER("DDRLLCR", full_addr, env->nip);
        return g_DDRDLLCR | 0x100;
        break;
    case 0xe20:
        PRINT_READ_UNSUPPORTED_REGISTER("LBDLLCR", full_addr, env->nip);
        return g_LBDLLCR | 0x100;
        break;
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_gbu_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_GLOBAL_UTILITIES + ccsr_addr;

    switch (addr) {
    case 0xb0:
        if (val & 2) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    case 0x70:
        g_DEVDISR = val;
        break;
    case 0xe10:
        g_DDRDLLCR = val;
        PRINT_WRITE_UNSUPPORTED_REGISTER("DDRLLCR", full_addr , val, env->nip);
        break;
    case 0xe20:
        g_LBDLLCR = val;
        PRINT_WRITE_UNSUPPORTED_REGISTER("LBDLLCR", full_addr , val, env->nip);
        break;
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                         full_addr , val, env->nip);
    }
}

static const MemoryRegionOps p2010_gbu_ops = {
    .read = p2010_gbu_read,
    .write = p2010_gbu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static uint64_t p2010_lca_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_LOCAL_CONF + ccsr_addr;

    switch (addr & 0xfff) {
    case 0x0:
        PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                 full_addr, ccsr_addr >> 12, env->nip);
        return ccsr_addr >> 12;
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

static void p2010_lca_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_LOCAL_CONF + ccsr_addr;

    switch (addr & 0xfff) {
    case 0x0:
        ccsr_addr = val << 12;
        memory_region_del_subregion(get_system_memory(), ccsr_space);
        memory_region_add_subregion(get_system_memory(), ccsr_addr, ccsr_space);
        PRINT_SUPPORTED_REGISTER("CCSRBAR", full_addr, val << 12, env->nip);
        break;

    case 0xc08 ... 0xd70:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Local access window",
                                         full_addr, val, env->nip);
        break;
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                         full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p2010_lca_ops = {
    .read = p2010_lca_read,
    .write = p2010_lca_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p2010_elbc_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P2010RDB_ELBC + ccsr_addr;

    switch (addr & 0xfff) {
    case 0x00:    case 0x08:    case 0x10:    case 0x18:
    case 0x20:    case 0x28:    case 0x30:    case 0x38:
        PRINT_READ_UNSUPPORTED_REGISTER("Base registers", full_addr, env->nip);
        break;

    case 0x04:    case 0x0c:    case 0x14:    case 0x1c:
    case 0x24:    case 0x2c:    case 0x34:    case 0x3c:
        PRINT_READ_UNSUPPORTED_REGISTER("Base registers", full_addr, env->nip);
        break;

    case 0xd4:
        PRINT_READ_UNSUPPORTED_REGISTER("Clock Ratio Register",
                                        full_addr, env->nip);
        break;

    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_elbc_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P2010RDB_ELBC + ccsr_addr;

    switch (addr & 0xfff) {
    case 0x00:    case 0x08:    case 0x10:    case 0x18:
    case 0x20:    case 0x28:    case 0x30:    case 0x38:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Base registers",
                                         full_addr, val, env->nip);
        break;

    case 0x04:    case 0x0c:    case 0x14:    case 0x1c:
    case 0x24:    case 0x2c:    case 0x34:    case 0x3c:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Base registers",
                                         full_addr, val, env->nip);
        break;

    case 0xd4:
        PRINT_WRITE_UNSUPPORTED_REGISTER("Clock Ratio Register",
                                         full_addr, val, env->nip);
        break;

    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                         full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p2010_elbc_ops = {
    .read = p2010_elbc_read,
    .write = p2010_elbc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p2010_ddr_read(void *opaque, hwaddr addr,
                               unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_DDR_CONTROLLER + ccsr_addr;

    switch (addr & 0xfff) {

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

static void p2010_ddr_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_DDR_CONTROLLER + ccsr_addr;

    switch (addr & 0xfff) {
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
        PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                         full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p2010_ddr_ops = {
    .read = p2010_ddr_read,
    .write = p2010_ddr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p2010_gpio_read(void *opaque, hwaddr addr,
                                unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P2010RDB_GPIO + ccsr_addr;

    switch (addr & 0xfff) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_gpio_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P2010RDB_GPIO + ccsr_addr;

    switch (addr & 0xfff) {
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "? Unknown ?", full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p2010_gpio_ops = {
    .read = p2010_gpio_read,
    .write = p2010_gpio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p2010_l2cache_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_L2CACHE + ccsr_addr;

    switch (addr & 0xfff) {
    case 0x00:
        PRINT_READ_UNSUPPORTED_REGISTER(
            "L2 Control Register (return 0x20000000)", full_addr, env->nip);
        return 0x20000000;
        break;
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_l2cache_write(void *opaque, hwaddr addr,
                                uint64_t val, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P2010RDB_L2CACHE + ccsr_addr;

    switch (addr & 0xfff) {
    case 0x00:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 Control Register",
            full_addr, val, env->nip);
        break;
    case 0x10:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register 0",
            full_addr, val, env->nip);
        break;
    case 0x14:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register extended address 0",
            full_addr, val, env->nip);
        break;
    case 0x18:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write control register 0",
            full_addr, val, env->nip);
        break;
    case 0x20:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register 1",
            full_addr, val, env->nip);
        break;
    case 0x24:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register extended address 1",
            full_addr, val, env->nip);
        break;
    case 0x28:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write control register 1",
            full_addr, val, env->nip);
        break;
    case 0x30:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register 2",
            full_addr, val, env->nip);
        break;
    case 0x34:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register extended address 2",
            full_addr, val, env->nip);
        break;
    case 0x38:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write control register 2",
            full_addr, val, env->nip);
        break;
    case 0x40:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register 3",
            full_addr, val, env->nip);
        break;
    case 0x44:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write address register extended address 3",
            full_addr, val, env->nip);
        break;
    case 0x48:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 cache external write control register 3",
            full_addr, val, env->nip);
        break;
    case 0x100:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 memory-mapped SRAM base address register 0",
            full_addr, val, env->nip);
        break;
    case 0x104:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 memory-mapped SRAM base address register extended address 0",
            full_addr, val, env->nip);
        break;
    case 0x108:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 memory-mapped SRAM base address register 1",
            full_addr, val, env->nip);
        break;
    case 0x10c:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "L2 memory-mapped SRAM base address register extended address 1",
            full_addr, val, env->nip);
        break;
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "? Unknown ?", full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p2010_l2cache_ops = {
    .read = p2010_l2cache_read,
    .write = p2010_l2cache_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p2010_vsc7385_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_VSC7385 + ccsr_addr;

    switch (addr & 0xfff) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p2010_vsc7385_write(void *opaque, hwaddr addr,
                                uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P2010RDB_VSC7385 + ccsr_addr;

    switch (addr & 0xfff) {
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "? Unknown ?", full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p2010_vsc7385_ops = {
    .read = p2010_vsc7385_read,
    .write = p2010_vsc7385_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void write_qtrace(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    switch (addr & 0xfff) {
    case 0x00:
        exec_trace_special(TRACE_SPECIAL_LOADADDR, value);
        break;
    default:
#ifdef DEBUG_P2010
        printf("%s: writing non implemented register 0x" TARGET_FMT_plx "\n",
               __func__, QTRACE_START + addr);
#endif
        break;
    }
}

static const MemoryRegionOps qtrace_ops = {
    .write = write_qtrace,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static qemu_irq *init_mpic(MemoryRegion *ccsr, qemu_irq **irqs)
{
    qemu_irq     *mpic;
    DeviceState  *dev = NULL;
    SysBusDevice *s;
    int           i, j, k;

    mpic = g_new(qemu_irq, 256);

    dev = qdev_create(NULL, TYPE_OPENPIC);
    qdev_prop_set_uint32(dev, "model", OPENPIC_MODEL_FSL_MPIC_20);
    qdev_prop_set_uint32(dev, "nb_cpus", 1);

    qdev_init_nofail(dev);
    s = SYS_BUS_DEVICE(dev);

    k = 0;
    for (i = 0; i < smp_cpus; i++) {
        for (j = 0; j < OPENPIC_OUTPUT_NB; j++) {
            sysbus_connect_irq(s, k++, irqs[i][j]);
        }
    }

    for (i = 0; i < 256; i++) {
        mpic[i] = qdev_get_gpio_in(dev, i);
    }

    memory_region_add_subregion(ccsr, P2010RDB_MPIC_REGS_BASE,
                                s->mmio[0].memory);

    return mpic;
}

static void fsl_e500_init(fsl_e500_config *config, MachineState *args)
{
    CPUPPCState  *env             = NULL;
    CPUPPCState  *firstenv        = NULL;
    uint64_t      elf_entry;
    uint64_t      elf_lowaddr;
    const char   *kernel_filename = args->kernel_filename;
    target_long   kernel_size     = 0;
    target_ulong  dt_base         = 0;
    int           bios_size       = 0;
    MemoryRegion *ram, *misc_io;
    MemoryRegion  *params = g_new(MemoryRegion, 1);
    qemu_irq     *mpic, **irqs;
    ResetData    *reset_info;
    DriveInfo    *dinfo;
    int           i;

    /* Setup CPU */
    if (args->cpu_type == NULL) {
        if (config->cpu_model != NULL) {
            args->cpu_type = config->cpu_model;
        } else {
            args->cpu_type = "e500v2_v30";
        }
    }

    reset_info = g_malloc0(sizeof(ResetData));

    irqs = g_malloc0(smp_cpus * sizeof(qemu_irq *));
    irqs[0] = g_malloc0(smp_cpus * sizeof(qemu_irq) * OPENPIC_OUTPUT_NB);
    for (i = 0; i < smp_cpus; i++) {
        PowerPCCPU *cpu;
        CPUState *cs;
        qemu_irq *input;

        cpu = POWERPC_CPU(cpu_create(args->cpu_type));

        if (cpu == NULL) {
            fprintf(stderr, "Unable to initialize CPU!\n");
            exit(1);
        }
        env = &cpu->env;
        cs = CPU(cpu);

        if (!firstenv) {
            firstenv = env;
        }

        irqs[i] = irqs[0] + (i * OPENPIC_OUTPUT_NB);
        input = (qemu_irq *)env->irq_inputs;
        irqs[i][OPENPIC_OUTPUT_INT] = input[PPCE500_INPUT_INT];
        irqs[i][OPENPIC_OUTPUT_CINT] = input[PPCE500_INPUT_CINT];
        env->spr[SPR_BOOKE_PIR] = cs->cpu_index = i;
        env->mpic_iack = config->ccsr_init_addr +
            P2010RDB_MPIC_REGS_BASE + 0xa0;

        ppc_booke_timers_init(cpu, config->freq, PPC_TIMER_E500);

        /* Register reset handler */
        if (!i) {
            /* Primary CPU */
            reset_info->cpu = cpu;
            qemu_register_reset(main_cpu_reset, reset_info);

            reset_info->entry = 0xfffffffc;

        } else {
            /* Secondary CPUs */
            qemu_register_reset(sec_cpu_reset, cpu);
        }
    }

    /* Register Memory */
    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "p2010.ram", ram_size, &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x0, ram);

    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "p2010.ram_L2",
                           0x1000000 /* 512 * 1024 */, &error_abort);
    memory_region_add_subregion(get_system_memory(),
                                0xf8000000 /* 0xf8b00000 */, ram);

    ccsr_addr = config->ccsr_init_addr;

    /* Configuration, Control, and Status Registers */
    ccsr_space = g_malloc0(sizeof(*ccsr_space));
    memory_region_init(ccsr_space, NULL, "CCSR_space", 0x100000);
    memory_region_add_subregion_overlap(get_system_memory(), ccsr_addr,
                                        ccsr_space, 2);

    /* Global Utilities */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_gbu_ops, env,
                          "Global Utilities", 0x1000);
    memory_region_add_subregion(ccsr_space, P2010RDB_GLOBAL_UTILITIES, misc_io);

    /* Local configuration/access */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_lca_ops, env,
                          "Local configuration/access", 0x1000);
    memory_region_add_subregion(ccsr_space, P2010RDB_LOCAL_CONF, misc_io);

    /* eLBC */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_elbc_ops, env, "eLBC", 0x1000);
    memory_region_add_subregion(ccsr_space, P2010RDB_ELBC, misc_io);

    /* DDR Memory Controller */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_ddr_ops, env,
                          "DDR memory controller", 0x1000);
    memory_region_add_subregion(ccsr_space, P2010RDB_DDR_CONTROLLER, misc_io);

    /* GPIO */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_gpio_ops, env, "GPIO", 0x1000);
    memory_region_add_subregion(ccsr_space, P2010RDB_GPIO, misc_io);

    /* L2CACHE */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_l2cache_ops, env, "L2CACHE",
                          0x1000);
    memory_region_add_subregion(ccsr_space, P2010RDB_L2CACHE, misc_io);

    /* VSC7385 */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p2010_vsc7385_ops, env,
                          "VSC7385", P2010RDB_VSC7385_SIZE);
    memory_region_add_subregion(get_system_memory(), P2010RDB_VSC7385, misc_io);

    /* MPIC */
    mpic = init_mpic(ccsr_space, irqs);

    if (!mpic) {
        cpu_abort(CPU(env), "MPIC failed to initialize\n");
    }

    /* Serial */
    if (serial_hd(0)) {
        serial_mm_init(ccsr_space, P2010RDB_SERIAL0_REGS_BASE,
                       0, mpic[config->serial_irq], 399193,
                       serial_hd(0), DEVICE_BIG_ENDIAN);
    }

    if (serial_hd(1)) {
        serial_mm_init(ccsr_space, P2010RDB_SERIAL1_REGS_BASE,
                       0, mpic[config->serial_irq], 399193,
                       serial_hd(1), DEVICE_BIG_ENDIAN);
    }

    for (i = 0; i < nb_nics && i < MAX_ETSEC_CONTROLLERS; i++) {
        etsec_create(P2010RDB_ETSEC1_BASE + 0x1000 * i, ccsr_space,
                     &nd_table[i],
                     mpic[config->etsec_irq_tx[i]],
                     mpic[config->etsec_irq_rx[i]],
                     mpic[config->etsec_irq_err[i]]);
    }

    /* Load pflash after serial initialization because pflash_cfi02_register
     * causes a race condition in monitor initialization which makes the monitor
     * to print in stdout. This is because the monitor is not in mux mode until
     * the serial port is started.
     */

    /* allocate and load BIOS */
    dinfo           = drive_get(IF_PFLASH, 0, 0);
    if (dinfo) {
        BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
        bios_size   = blk_getlength(blk);

        if (config->cfi01_flash) {
            if (pflash_cfi01_register(0xff800000, "8548.flash.1",
                                      bios_size, blk, 65536,
                                      2, 0x0001, 0x22DA, 0x0000, 0x0000,
                                      1) == NULL) {
                fprintf(stderr, "%s: Failed to load flash image\n", __func__);
                abort();
            }
        } else {
            if (pflash_cfi02_register((uint32_t)(-bios_size),
                                      "p2010.bios", bios_size, blk,
                                      65536, 1, 2, 0x0001, 0x22DA,
                                      0x0, 0x0, 0x555, 0x2AA, 1) == NULL) {
                fprintf(stderr, "%s: Failed to load flash image\n", __func__);
                abort();
            }
        }

    }

    /* Load kernel. */
    if (kernel_filename) {
        kernel_size = load_elf(kernel_filename, NULL, NULL, NULL, &elf_entry,
                               &elf_lowaddr, NULL, 1, PPC_ELF_MACHINE, 0, 0);

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

        /* Set initial guest state. */
        env->gpr[1] = elf_lowaddr + 4 * 1024 * 1024; /* FIXME: sp? */
        env->gpr[3] = dt_base;
        env->nip = elf_entry;    /* FIXME: entry? */
        reset_info->entry = elf_entry;
    }

    /* Set params.  */
    memory_region_init_ram(params, NULL, "p2010rdb.params", PARAMS_SIZE,
                           &error_abort);
    memory_region_add_subregion(ccsr_space, PARAMS_ADDR, params);

    if (args->kernel_cmdline) {
        cpu_physical_memory_write(ccsr_addr + PARAMS_ADDR, args->kernel_cmdline,
                                  strlen(args->kernel_cmdline) + 1);
    } else {
        stb_phys(CPU(env)->as, ccsr_addr + PARAMS_ADDR, 0);
    }

    /* Set read-only after writing command line */
    memory_region_set_readonly(params, true);

    /* Qtrace*/
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &qtrace_ops, env,
                          "Exec-traces", QTRACE_SIZE);
    memory_region_add_subregion(ccsr_space, QTRACE_START, misc_io);

    /* HostFS */
    hostfs_create(HOSTFS_START, ccsr_space);

    /* Initialize the GnatBus Master */
    gnatbus_master_init(mpic, 16);
    gnatbus_device_init();
}

/******************************************************************************/

static fsl_e500_config p2010rdb_vxworks_config = {
    .ccsr_init_addr = 0xf3000000,
    .cpu_model      = POWERPC_CPU_TYPE_NAME("p2010"),
    .freq           = 700000000UL >> 3,
    .serial_irq     = 16 + 26,
    .etsec_irq_err  = {16 + 18, 16 + 24, 16 + 17},
    .etsec_irq_tx   = {16 + 13, 16 + 19, 16 + 15},
    .etsec_irq_rx   = {16 + 14, 16 + 20, 16 + 16},
    .cfi01_flash    = FALSE,
};

static void p2010rdb_vxworks_generic_hw_init(MachineState *machine)
{
    fsl_e500_init(&p2010rdb_vxworks_config, machine);
}

static void p2010rdb_vxworks_generic_machine_init(MachineClass *mc)
{
    mc->desc = "p2010rdb initialized for VxWorks6";
    mc->init = p2010rdb_vxworks_generic_hw_init;
}

DEFINE_MACHINE("p2010rdb_vxworks", p2010rdb_vxworks_generic_machine_init)

/******************************************************************************/


static fsl_e500_config p2010rdb_config = {
    .ccsr_init_addr = 0xff700000,
    .cpu_model      = POWERPC_CPU_TYPE_NAME("p2010"),
    .freq           = 700000000UL >> 3,
    .serial_irq     = 16 + 26,
    .etsec_irq_err  = {16 + 18, 16 + 24, 16 + 17},
    .etsec_irq_tx   = {16 + 13, 16 + 19, 16 + 15},
    .etsec_irq_rx   = {16 + 14, 16 + 20, 16 + 16},
    .cfi01_flash    = FALSE,
};


static void p2010rdb_generic_hw_init(MachineState *machine)
{
    fsl_e500_init(&p2010rdb_config, machine);
}

static void p2010rdb_generic_machine_init(MachineClass *mc)
{
    mc->desc = "p2010rdb reset state";
    mc->init = p2010rdb_generic_hw_init;
}

DEFINE_MACHINE("p2010rdb", p2010rdb_generic_machine_init)

/******************************************************************************/

static fsl_e500_config wrsbc8548_vxworks_config = {
    .ccsr_init_addr = 0xE0000000,
    .cpu_model      = POWERPC_CPU_TYPE_NAME("p2010"),
    .freq           = 700000000UL >> 3,
    .serial_irq     = 16 + 26,
    .etsec_irq_err  = {16 + 18, 16 + 24, 16 + 17},
    .etsec_irq_tx   = {16 + 13, 16 + 19, 16 + 15},
    .etsec_irq_rx   = {16 + 14, 16 + 20, 16 + 16},
    .cfi01_flash    = TRUE,
};

static void wrsbc8548_vxworks_generic_hw_init(MachineState *machine)
{
    fsl_e500_init(&wrsbc8548_vxworks_config, machine);
}

static void wrsbc8548_vxworks_generic_machine_init(MachineClass *mc)
{
    mc->desc = "wrsbc8548 initialized for VxWorks6";
    mc->init = wrsbc8548_vxworks_generic_hw_init;
}

DEFINE_MACHINE("wrsbc8548_vxworks", wrsbc8548_vxworks_generic_machine_init)

/******************************************************************************/

static fsl_e500_config wrsbc8548_vx653_config = {
    .ccsr_init_addr = 0xE0000000,
    .cpu_model      = POWERPC_CPU_TYPE_NAME("p2010"),
    .freq           = 700000000UL >> 3,
    .serial_irq     = 16 + 26,
    .etsec_irq_err  = {16 + 18, 16 + 24, 16 + 17},
    .etsec_irq_tx   = {16 + 13, 16 + 19, 16 + 15},
    .etsec_irq_rx   = {16 + 14, 16 + 20, 16 + 16},
    .cfi01_flash    = TRUE,
};

static void wrsbc8548_vx653_generic_hw_init(MachineState *machine)
{
    fsl_e500_init(&wrsbc8548_vx653_config, machine);
}

static void wrsbc8548_vx653_generic_machine_init(MachineClass *mc)
{
    mc->desc = "wrsbc8548 initialized for VxWorks653";
    mc->init = wrsbc8548_vx653_generic_hw_init;
}

DEFINE_MACHINE("wrsbc8548_vx653", wrsbc8548_vx653_generic_machine_init)

/******************************************************************************/

static fsl_e500_config wrsbc8548_config = {
    .ccsr_init_addr = 0xff700000,
    .cpu_model      = POWERPC_CPU_TYPE_NAME("p2010"),
    .freq           = 700000000UL >> 3,
    .serial_irq     = 16 + 26,
    .etsec_irq_err  = {16 + 18, 16 + 24, 16 + 17},
    .etsec_irq_tx   = {16 + 13, 16 + 19, 16 + 15},
    .etsec_irq_rx   = {16 + 14, 16 + 20, 16 + 16},
    .cfi01_flash    = TRUE,
};

static void wrsbc8548_generic_hw_init(MachineState *machine)
{
    fsl_e500_init(&wrsbc8548_config, machine);
}

static void wrsbc8548_generic_machine_init(MachineClass *mc)
{
    mc->desc = "wrsbc8548";
    mc->init = wrsbc8548_generic_hw_init;
}

DEFINE_MACHINE("wrsbc8548", wrsbc8548_generic_machine_init)

/******************************************************************************/
