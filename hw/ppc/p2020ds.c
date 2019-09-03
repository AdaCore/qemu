/*
 * QEMU PowerPC p2020ds board emulation
 *
 * Copyright (C) 2011-2019 AdaCore
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 */

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
#include "sysemu/device_tree.h"
#include <libfdt.h>

#include "hw/adacore/hostfs.h"

/* #define DEBUG_P2020 */

#define MAX_ETSEC_CONTROLLERS 3

#define EPAPR_MAGIC                (0x45504150)
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

#define P2020DS_CCSRBAR_BASE       0xf3000000
#define P2020DS_LOCAL_CONF         0x00000
#define P2020DS_DDR_CONTROLLER     0x02000
#define P2020DS_SERIAL0_REGS_BASE  0x04500
#define P2020DS_SERIAL1_REGS_BASE  0x04600
#define P2020DS_SERIAL_IRQ         (16 + 26)
#define P2020DS_ELBC               0x05000
#define P2020DS_ESPI_BASE          0x07000
#define P2020DS_PCI_REGS_BASE      0x08000
#define P2020DS_GPIO               0x0F000
#define P2020DS_L2CACHE            0x20000
#define P2020DS_ETSEC1_BASE        0x24000
#define P2020DS_MPIC_REGS_BASE     0x40000
#define P2020DS_GLOBAL_UTILITIES   0xE0000
#define P2020DS_VSC7385            0xF1000000
#define P2020DS_VSC7385_SIZE       0x00020000
#define P2020DS_PCI_REGS_SIZE      0x1000
#define P2020DS_PCI_IO             0xE1000000
#define P2020DS_PCI_IOLEN          0x10000

#define P2020DS_BOOKE_TIMER_FREQ   (20000000UL)

typedef struct ResetData {
    PowerPCCPU *cpu;
    uint32_t entry;         /* save kernel entry in case of reset */
} ResetData;

static MemoryRegion *ccsr_space;
static uint64_t ccsr_addr = P2020DS_CCSRBAR_BASE;

static uint32_t g_DEVDISR  = 0x0;
static uint32_t g_DDRDLLCR = 0x0;
static uint32_t g_LBDLLCR  = 0x0;

static uint64_t p2020ds_gbu_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;

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
        return g_DDRDLLCR | 0x100;
        break;
    case 0xe20:
        return g_LBDLLCR | 0x100;
        break;
    default:
        break;
    }
    return 0;
}

static void p2020ds_gbu_write(void *opaque, hwaddr addr,
                              uint64_t val, unsigned size)
{
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
        break;
    case 0xe20:
        g_LBDLLCR = val;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps p2020ds_gbu_ops = {
    .read = p2020ds_gbu_read,
    .write = p2020ds_gbu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void sec_cpu_reset(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);

    cpu_reset(cs);
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
}

static void main_cpu_reset(void *opaque)
{
    ResetData *s = (ResetData *)opaque;
    PowerPCCPU *cpu = s->cpu;
    CPUPPCState *env = &cpu->env;
    ppcmas_tlb_t *tlb1 = booke206_get_tlbm(env, 1, 0, 0);
    ppcmas_tlb_t *tlb2 = booke206_get_tlbm(env, 1, 0, 1);
    hwaddr size;

    cpu_reset(CPU(cpu));
    env->nip = s->entry;
    env->tlb_dirty = true;

    /* Init tlb1 entry 0 (e500 )*/
    size = 0x1 << MAS1_TSIZE_SHIFT; /* 4 KBytes */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb1->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb1->mas2 = 0xfffff000 &   TARGET_PAGE_MASK;
    tlb1->mas7_3 = 0xfffff000 & TARGET_PAGE_MASK;
    tlb1->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /* Init tlb1 entry 1 (p2020ds)*/
    size = 0xb << MAS1_TSIZE_SHIFT; /* 4Gbytes */
    size <<= 1;                     /* Shift because MAS2's TSIZE field is
                                     * implemented as PowerISA MAV=2.0
                                     * compliant.
                                     */
    tlb2->mas1 = MAS1_VALID | MAS1_IPROT | size;
    tlb2->mas2 = 0x0 & TARGET_PAGE_MASK;
    tlb2->mas7_3 = 0x0 & TARGET_PAGE_MASK;
    tlb2->mas7_3 |= MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW | MAS3_SX;

    /*
     * I'm still not sure to understand why, but the bootApp set PID to 1
     * before starting the kernel.
     */
    env->spr[SPR_BOOKE_PID] = 0x1;
}

/* No device tree provided we need to create one from scratch... */
static void *p2020ds_create_dtb(uint64_t ram_size, uint64_t load_addr,
                                int *size)
{
    void *fdt;
    const char *cur_node;
    int i;
    int sysclk_phandle;
    int pic_phandle;
    int ccb_phandle;
    int phy_phandle[3];
    uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    uint32_t ccsrbarh = extract64((uint64_t)ccsr_addr, 32, 4);
    uint32_t ccsrbarl = extract64(ccsr_addr, 0, 32);

    fdt = create_device_tree(size);
    if (fdt == NULL) {
        return NULL;
    }

    pic_phandle = qemu_fdt_alloc_phandle(fdt);
    sysclk_phandle = qemu_fdt_alloc_phandle(fdt);
    ccb_phandle = qemu_fdt_alloc_phandle(fdt);
    phy_phandle[0] = qemu_fdt_alloc_phandle(fdt);
    phy_phandle[1] = qemu_fdt_alloc_phandle(fdt);
    phy_phandle[2] = qemu_fdt_alloc_phandle(fdt);

    /*
     * One important stuff here is that everything is done in reverse in the
     * device tree within a node.
     */
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x1);
    qemu_fdt_setprop_string(fdt, "/", "compatible", "fsl,p2020ds");
    qemu_fdt_setprop_string(fdt, "/", "model", "Freescale P2020DS");

    cur_node = "/soc";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char soc_compat[] = "simple-bus\0fsl,immr";
    qemu_fdt_setprop(fdt, cur_node, "compatible", soc_compat,
                     sizeof(soc_compat));
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", ccsrbarh, ccsrbarl,
                           0x100000);
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "soc");
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 1);

    cur_node = "/soc/ethernet@26000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "phy-connection-type", "rgmii-id");
    qemu_fdt_setprop_cell(fdt, cur_node, "phy-handle", phy_phandle[2]);
    qemu_fdt_setprop_cell(fdt, cur_node, "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x1f, 0x2, 0x0, 0x0,
                           0x20, 0x2, 0x0, 0x0, 0x21, 0x2, 0x0, 0x0);
    mac[5] += 2;
    qemu_fdt_setprop(fdt, cur_node, "local-mac-address", mac, 6);
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", 0x0, 0x26000, 0x1000);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x26000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "motetsec");
    qemu_fdt_setprop_string(fdt, cur_node, "model", "eTSEC");
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "network");
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x2);
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/soc/ethernet@25000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "phy-connection-type", "rgmii-id");
    qemu_fdt_setprop_cell(fdt, cur_node, "phy-handle", phy_phandle[1]);
    qemu_fdt_setprop_cell(fdt, cur_node, "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x23, 0x2, 0x0, 0x0,
                           0x24, 0x2, 0x0, 0x0, 0x28, 0x2, 0x0, 0x0);
    mac[5] -= 1;
    qemu_fdt_setprop(fdt, cur_node, "local-mac-address", mac, 6);
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", 0x0, 0x25000, 0x1000);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x25000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "motetsec");
    qemu_fdt_setprop_string(fdt, cur_node, "model", "eTSEC");
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "network");
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/soc/mdio";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x24520, 0x20);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "tsecMdio");
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/soc/mdio/ethernet-phy@2";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", phy_phandle[2]);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", phy_phandle[2]);
    qemu_fdt_setprop_cell(fdt, cur_node, "reg", 0x2);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x2);

    cur_node = "/soc/mdio/ethernet-phy@1";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", phy_phandle[1]);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", phy_phandle[1]);
    qemu_fdt_setprop_cell(fdt, cur_node, "reg", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x1);

    cur_node = "/soc/mdio/ethernet-phy@0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", phy_phandle[0]);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", phy_phandle[0]);
    qemu_fdt_setprop_cell(fdt, cur_node, "reg", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);

    cur_node = "/soc/ethernet@24000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phy-handle", phy_phandle[0]);
    qemu_fdt_setprop_string(fdt, cur_node, "phy-connection-type", "rgmii-id");
    qemu_fdt_setprop_cell(fdt, cur_node, "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x1d, 0x2, 0x0, 0x0,
                           0x1e, 0x2, 0x0, 0x0, 0x22, 0x2, 0x0, 0x0);
    mac[5] -= 1;
    qemu_fdt_setprop(fdt, cur_node, "local-mac-address", mac, 6);
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", 0x0, 0x24000, 0x1000);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x24000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "motetsec");
    qemu_fdt_setprop_string(fdt, cur_node, "model", "eTSEC");
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "network");
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/soc/serial@4600";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", P2020DS_SERIAL_IRQ,
                           0x2, 0x0, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "ccbclk");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", ccb_phandle, 0x0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", P2020DS_SERIAL1_REGS_BASE,
                           0x100);
    const char serial_compat[] = "fsl,ns16550\0ns16550";
    qemu_fdt_setprop(fdt, cur_node, "compatible", serial_compat,
                     sizeof(serial_compat));
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "serial");
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);

    cur_node = "/soc/serial@4500";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", P2020DS_SERIAL_IRQ,
                           0x2, 0x0, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "ccbclk");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", ccb_phandle, 0x0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", P2020DS_SERIAL0_REGS_BASE,
                           0x100);
    qemu_fdt_setprop(fdt, cur_node, "compatible", serial_compat,
                     sizeof(serial_compat));
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "serial");
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);

    cur_node = "/soc/l2-cache-controller";
    const char l2_cache_compat[] = "fsl,p2020-l2-cache-controller\0"
                                   "fsl,qoriq-l2-cache-controller";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x10, 0x2, 0x0, 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cache-size", 0x80000);
    qemu_fdt_setprop_cell(fdt, cur_node, "cache-line-size", 0x20);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x20000, 0x1000);
    qemu_fdt_setprop(fdt, cur_node, "compatible", l2_cache_compat,
                     sizeof(l2_cache_compat));

    cur_node = "/soc/pic-timer@420f0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x4, 0x0, 0x3, 0x0,
                           0x5, 0x0, 0x3, 0x0, 0x6, 0x0, 0x3, 0x0, 0x7, 0x0,
                           0x3, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "ccbclk");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", ccb_phandle, 0x0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x420f0, 0x400);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,mpic-global-timer");

    cur_node = "/soc/pic-timer@410f0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x0, 0x0, 0x3, 0x0,
                           0x1, 0x0, 0x3, 0x0, 0x2, 0x0, 0x3, 0x0, 0x3, 0x0,
                           0x3, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "ccbclk");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", ccb_phandle, 0x0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x410f0, 0x400);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,mpic-global-timer");

    const char global_compat[] = "fsl,p2020-guts\0fsl,qoriq-guts";
    cur_node = "/soc/global-utilities@e0000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop(fdt, cur_node, "fsl,has-rstcr", NULL, 0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0000, 0x1000);
    qemu_fdt_setprop(fdt, cur_node, "compatible", global_compat,
                     sizeof(global_compat));

    cur_node = "/soc/pic";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char pic_compat[] = "fsl,mpic\0chrp,open-pic";
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", pic_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", pic_phandle);
    qemu_fdt_setprop(fdt, cur_node, "compatible", pic_compat,
                     sizeof(pic_compat));
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x40000, 0x40000);
    qemu_fdt_setprop_cell(fdt, cur_node, "#interrupt-cells", 0x4);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x0);
    qemu_fdt_setprop(fdt, cur_node, "interrupt-controller", NULL, 0);

    cur_node = "/soc/clockgen";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char clockgen_compat[] = "fsl,p2020-clockgen\0"
                                   "fsl,qoriq-clockgen-1.0";
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xE0000, 0x1000);
    qemu_fdt_setprop_cell(fdt, cur_node, "clock-frequency", 0x0);
    qemu_fdt_setprop(fdt, cur_node, "compatible", clockgen_compat,
                     sizeof(clockgen_compat));

    cur_node = "/soc/clockgen/ccb-clk";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", ccb_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", ccb_phandle);
    const char ccb_output_name[] = "ccbclk\0ccbclk-div2";
    qemu_fdt_setprop(fdt, cur_node, "clock-output-names", ccb_output_name,
                     sizeof(ccb_output_name));
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-platform-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x1);

    cur_node = "/soc/clockgen/core-clk1";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "core-clk1");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-core-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x1);

    cur_node = "/soc/clockgen/core-clk0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "core-clk0");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-core-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);

    cur_node = "/soc/clockgen/sysclk";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", sysclk_phandle);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "sysclk");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "fsl,qoriq-sysclk");
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);

    cur_node = "/timer@0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "ccbclk");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", ccb_phandle, 0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0, 0);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,p2020-booke-timer");

    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
                            "motetsec(0,0)host:vxWorks"
                            " h=10.10.0.1 e=10.10.0.2:ffffff00 g=10.10.0.1"
                            " u=vxworks pw=vxworks f=0x0");

    cur_node = "/board_control";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char board_ctrl_compat[] = "fsl,p2020ds-fpga\0fsl,fpga-ngpixis";
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xffdf0000, 0x1000);
    qemu_fdt_setprop(fdt, cur_node, "compatible", board_ctrl_compat,
                     sizeof(board_ctrl_compat));

    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 1);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0);

    for (i = smp_cpus - 1; i >= 0; i--) {
        char cpu_node[128];
        sprintf(cpu_node, "/cpus/cpu@%d", i);

        qemu_fdt_add_subnode(fdt, cpu_node);
        if (i) {
            qemu_fdt_setprop_string(fdt, cpu_node, "enable-method",
                                    "fsl,spin-table");
            qemu_fdt_setprop_cells(fdt, cpu_node, "cpu-release-addr", 0, 0);
        }
        qemu_fdt_setprop_cells(fdt, cpu_node, "reg", i);
        qemu_fdt_setprop_string(fdt, cpu_node, "device_type", "cpu");
    }

    return fdt;
}

/*
 * The DTB need to be patched in some case because u-boot does that before
 * jumping in the vxworks kernel.
 */
static void p2020ds_compute_dtb(char *filename, uint64_t ram_size,
                                 uint64_t load_addr, int *size)
{
    void *fdt;
    Error *err = NULL;
    uint32_t acells, scells;

    if (!filename) {
        /* Create the dtb */
        fdt = p2020ds_create_dtb(ram_size, load_addr, size);
    } else {
        /* Load it */
        fdt = load_device_tree(filename, size);
    }

    if (!fdt) {
        fprintf(stderr, "Can't load the dtb.\n");
        exit(1);
    }

    acells = qemu_fdt_getprop_cell(fdt, "/", "#address-cells",
                                   NULL, &error_fatal);
    scells = qemu_fdt_getprop_cell(fdt, "/", "#size-cells",
                                   NULL, &error_fatal);
    if (acells == 0 || scells == 0) {
        fprintf(stderr, "dtb file invalid"
                        " (#address-cells or #size-cells 0)\n");
        exit(1);
    }

    if (fdt_path_offset(fdt, "/memory") < 0) {
        qemu_fdt_add_subnode(fdt, "/memory");
    }

    if (!qemu_fdt_getprop(fdt, "/memory", "device_type", NULL, &err)) {
        qemu_fdt_setprop_string(fdt, "/memory", "device_type", "memory");
    }

    if (qemu_fdt_setprop_sized_cells(fdt, "/memory", "reg",
                                     acells, 0x00,
                                     scells, ram_size) < 0) {
        fprintf(stderr, "Can't set memory prop\n");
        exit(1);
    }

    qemu_fdt_dumpdtb(fdt, *size);
    rom_add_blob_fixed("dtb", fdt, *size, load_addr);

    g_free(fdt);
}

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

    memory_region_add_subregion(ccsr, P2020DS_MPIC_REGS_BASE,
                                s->mmio[0].memory);

    return mpic;
}

typedef struct etsec_irq {
    uint8_t etsec_irq_err[3];
    uint8_t etsec_irq_tx[3];
    uint8_t etsec_irq_rx[3];
} etsec_irq;

static etsec_irq etsec_irqs = {
    .etsec_irq_err = {16 + 18, 16 + 24, 16 + 17},
    .etsec_irq_tx = {16 + 13, 16 + 19, 16 + 15},
    .etsec_irq_rx = {16 + 14, 16 + 20, 16 + 16},
};

static void p2020ds_init(MachineState *machine)
{
    CPUPPCState *env = NULL;
    CPUPPCState *firstenv = NULL;
    uint64_t elf_entry;
    uint64_t elf_lowaddr;
    const char *kernel_filename = machine->kernel_filename;
    target_long kernel_size = 0;
    target_ulong dt_base = 0;
    MemoryRegion *ram, *iomem;
    MemoryRegion *params = g_new(MemoryRegion, 1);
    qemu_irq *mpic, **irqs;
    ResetData *reset_info;
    int i;

    if (machine->cpu_type == NULL) {
        machine->cpu_type = POWERPC_CPU_TYPE_NAME("p2010");
    }
    reset_info = g_malloc0(sizeof(ResetData));

    irqs = g_malloc0(smp_cpus * sizeof(qemu_irq *));
    irqs[0] = g_malloc0(smp_cpus * sizeof(qemu_irq) * OPENPIC_OUTPUT_NB);
    for (i = 0; i < smp_cpus; i++) {
        PowerPCCPU *cpu;
        CPUState *cs;
        qemu_irq *input;

        cpu = POWERPC_CPU(cpu_create(machine->cpu_type));

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
        env->mpic_iack = P2020DS_CCSRBAR_BASE + P2020DS_MPIC_REGS_BASE + 0xa0;

        ppc_booke_timers_init(cpu, P2020DS_BOOKE_TIMER_FREQ, PPC_TIMER_E500);

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
    memory_region_init_ram(ram, NULL, "ram", ram_size, &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x0, ram);

    /* Configuration, Control, and Status Registers */
    ccsr_space = g_malloc0(sizeof(*ccsr_space));
    memory_region_init(ccsr_space, NULL, "ccsr", 0x100000);
    memory_region_add_subregion_overlap(get_system_memory(), ccsr_addr,
                                        ccsr_space, 2);

    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, NULL, &p2020ds_gbu_ops, env,
                          "Global Utilities", 0x1000);
    memory_region_add_subregion(ccsr_space, P2020DS_GLOBAL_UTILITIES, iomem);

    /* MPIC */
    mpic = init_mpic(ccsr_space, irqs);

    if (!mpic) {
        cpu_abort(CPU(env), "MPIC failed to initialize\n");
    }

    /* Serial */
    serial_mm_init(ccsr_space, P2020DS_SERIAL0_REGS_BASE, 0,
                   mpic[P2020DS_SERIAL_IRQ], 399193, serial_hd(0),
                   DEVICE_BIG_ENDIAN);

    serial_mm_init(ccsr_space, P2020DS_SERIAL1_REGS_BASE, 0,
                   mpic[P2020DS_SERIAL_IRQ], 399193, serial_hd(1),
                   DEVICE_BIG_ENDIAN);

    for (i = 0; i < MAX_ETSEC_CONTROLLERS; i++) {
        etsec_create(P2020DS_ETSEC1_BASE + 0x1000 * i, ccsr_space,
                     &nd_table[i],
                     mpic[etsec_irqs.etsec_irq_tx[i]],
                     mpic[etsec_irqs.etsec_irq_rx[i]],
                     mpic[etsec_irqs.etsec_irq_err[i]]);
    }

    /* Load kernel. */
    if (kernel_filename) {
        int dtb_size;

        kernel_size = load_elf(kernel_filename, NULL, NULL, &elf_entry,
                               &elf_lowaddr, NULL, 1, PPC_ELF_MACHINE, 0, 0);

        /* XXX try again as binary */
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    kernel_filename);
            exit(1);
        }

        dt_base = (kernel_size + DTC_LOAD_PAD) & ~DTC_PAD_MASK;

        p2020ds_compute_dtb(machine->dtb, ram_size, dt_base, &dtb_size);
        if (dtb_size < 0) {
            fprintf(stderr, "device tree error\n");
            exit(1);
        }
        /* Set initial guest state. */
        env->gpr[1] = elf_lowaddr + 4 * 1024 * 1024; /* FIXME: sp? */
        env->gpr[3] = dt_base;
        env->gpr[6] = EPAPR_MAGIC;
        env->gpr[7] = dt_base + dtb_size - elf_lowaddr;
        env->nip = elf_entry;    /* FIXME: entry? */
        reset_info->entry = elf_entry;
    }

    /* Set params.  */
    memory_region_init_ram(params, NULL, "p2020ds.params", PARAMS_SIZE,
                           &error_abort);
    memory_region_add_subregion(ccsr_space, PARAMS_ADDR, params);

    if (machine->kernel_cmdline) {
        cpu_physical_memory_write(ccsr_addr + PARAMS_ADDR,
                                  machine->kernel_cmdline,
                                  strlen(machine->kernel_cmdline) + 1);
    } else {
        stb_phys(CPU(env)->as, ccsr_addr + PARAMS_ADDR, 0);
    }

    /* Set read-only after writing command line */
    memory_region_set_readonly(params, true);

    /* HostFS */
    hostfs_create(HOSTFS_START, ccsr_space);
}

static void p2020ds_generic_machine_init(MachineClass *mc)
{
    mc->desc = "p2020ds";
    mc->init = p2020ds_init;
}

DEFINE_MACHINE("p2020ds", p2020ds_generic_machine_init)
