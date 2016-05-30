/*
 * QEMU PowerPC p3041ds board emualtion
 *
 * Copyright (C) 2016 AdaCore
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
#include "qapi/error.h"
#include "qemu-common.h"
#include "net/net.h"
#include "hw/hw.h"
#include "hw/irq.h"
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

#include "hw/adacore/gnat-bus.h"
#include "hw/adacore/hostfs.h"
#include "qemu-traces.h"


/* #define DEBUG_P3041 */

#define EPAPR_MAGIC  (0x45504150)
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

#define P3041DS_CCSRBAR_BASE       0xfe000000
#define P3041DS_LOCAL_CONF         0x00000000
#define P3041DS_DDR_CONTROLLER     0x00008000
#define P3041DS_L3CACHE            0x00010000
#define P3041DS_L3_CACHE_SIZE      0x00008000
#define P3041DS_PAMU_REGS_BASE     0x00020000
#define P3041DS_MPIC_REGS_BASE     0x00040000
#define P3041DS_GLOBAL_UTILITIES   0x000E0000
#define P3041DS_RCPM_BASE          0x000E2000
#define P3041DS_SERIAL0_REGS_BASE  0x0011C500
#define P3041DS_SERIAL1_REGS_BASE  0x0011D500
#define P3041DS_ELBC               0x00124000
#define P3041DS_GPIO               0x00130000
#define P3041DS_PCI_REGS_BASE      0x00200000
#define P3041DS_QMAN_BASE          0x00318000
#define P3041DS_BMAN_BASE          0x0031A000
#define P3041DS_DTSEC1_BASE        0x00400000
#define P3041DS_PCI_REGS_SIZE      0x00001000
#define P3041DS_PCI_IO             0xE1000000
#define P3041DS_PCI_IOLEN          0x00010000

#ifdef DEBUG_P3041

#define DPRINTF(fmt, ...) do { \
fprintf(stderr, fmt, ## __VA_ARGS__); \
} while(0)

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

#else /* DEBUG_P3041 */
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


#define DPRINTF(fmt, ...) do { } while(0)
#endif  /* DEBUG_P3041 */

typedef struct ResetData {
    PowerPCCPU  *cpu;
    uint32_t     entry;         /* save kernel entry in case of reset */
} ResetData;

typedef struct fsl_e500_config {
    const char *cpu_model;
    uint32_t freq;

    int serial_irq;

    int cfi01_flash;
} fsl_e500_config;

static MemoryRegion *ccsr_space;
static hwaddr ccsr_addr = P3041DS_CCSRBAR_BASE;

struct epapr_data {
    hwaddr dtb_entry_ea;
    hwaddr boot_ima_sz;
    hwaddr entry;
};

/* No device tree provided we need to create one from scratch... */
static void *p3041ds_create_dtb(uint64_t ram_size, uint64_t load_addr,
                                int *size)
{
    void *fdt;
    const char *cur_node;
    int i;
    int sysclk_phandle;
    int pic_phandle;
    int platform_phandle;
    int phy_phandle[2];
    uint8_t mac[6] = {0x00, 0xA0, 0x1E, 0x10, 0x20, 0x30};
    uint32_t ccsrbarh = extract64(ccsr_addr, 32, 4);
    uint32_t ccsrbarl = extract64(ccsr_addr, 0, 32);

    fdt = create_device_tree(size);
    if (fdt == NULL) {
        return NULL;
    }

    pic_phandle = qemu_fdt_alloc_phandle(fdt);
    sysclk_phandle = qemu_fdt_alloc_phandle(fdt);
    platform_phandle = qemu_fdt_alloc_phandle(fdt);
    phy_phandle[0] = qemu_fdt_alloc_phandle(fdt);
    phy_phandle[1] = qemu_fdt_alloc_phandle(fdt);

    /*
     * One important stuff here is that everything is done in reverse in the
     * device tree within a node.
     */

    qemu_fdt_setprop_string(fdt, "/", "model", "Freescale P3041DS");
    qemu_fdt_setprop_cell(fdt, "/", "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_string(fdt, "/", "compatible", "fsl,p3041ds");

    cur_node = "/socp3041";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char soc_compat[] = "simple-bus\0fsl,immr";
    qemu_fdt_setprop(fdt, cur_node, "compatible", soc_compat,
                     sizeof(soc_compat));
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "soc");
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 1);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", ccsrbarh, ccsrbarl, 0x0,
                           0x1000);
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", 0, ccsrbarh, ccsrbarl,
                           0x1000000);

    cur_node = "/socp3041/serial@11c500";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x24, 0x2, 0x0, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "platform-div2");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", platform_phandle, 0x1);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x11c500, 0x100);
    const char serial_compat[] = "fsl,ns16550\0ns16550";
    qemu_fdt_setprop(fdt, cur_node, "compatible", serial_compat,
                     sizeof(serial_compat));
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "serial");
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);

    cur_node = "/socp3041/fman@400000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x60, 0x2, 0x0, 0x0,
                           0x10, 0x2, 0x1, 0x1);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "platform");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", platform_phandle, 0x0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x400000, 0x100000);
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", 0x0, 0x400000, 0x100000);
    const char fman_compat[] = "fsl,fman\0simple-bus";
    qemu_fdt_setprop(fdt, cur_node, "compatible", fman_compat,
                     sizeof(fman_compat));
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/socp3041/fman@400000/ethernet@e8000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe8000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "fsl,fman-dtsec");
    qemu_fdt_setprop(fdt, cur_node, "local-mac-address", mac, 6);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "phy-connection-type", "rgmii");
    qemu_fdt_setprop_cell(fdt, cur_node, "phy-handle", phy_phandle[1]);

    cur_node = "/socp3041/fman@400000/ethernet@e6000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe6000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "fsl,fman-dtsec");
    qemu_fdt_setprop(fdt, cur_node, "local-mac-address", mac, 6);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "phy-connection-type", "rgmii");
    qemu_fdt_setprop_cell(fdt, cur_node, "phy-handle", phy_phandle[0]);

    cur_node = "/socp3041/fman@400000/mdio@e1000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x64, 0x1, 0x0, 0x0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe1000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,fman-dtsec-mdio");
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/socp3041/fman@400000/mdio@e1000/ethernet-phy@1";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", phy_phandle[1]);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", phy_phandle[1]);
    qemu_fdt_setprop_cell(fdt, cur_node, "reg", 0x1);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "generic-phy");

    cur_node = "/socp3041/fman@400000/mdio@e1000/ethernet-phy@0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", phy_phandle[0]);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", phy_phandle[0]);
    qemu_fdt_setprop_cell(fdt, cur_node, "reg", 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "generic-phy");

    cur_node = "/socp3041/global-utilities@e0000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop(fdt, cur_node, "fsl,has-rstcr", NULL, 0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "fsl,qoriq-guts");

    cur_node = "/socp3041/pic-timer@420f0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x4, 0x0, 0x3, 0x0,
                           0x5, 0x0, 0x3, 0x0, 0x6, 0x0, 0x3, 0x0, 0x7, 0x0,
                           0x3, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "platform-div2");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", platform_phandle, 0x1);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x420f0, 0x400);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,mpic-global-timer");

    cur_node = "/socp3041/pic-timer@410f0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "interrupts", 0x0, 0x0, 0x3, 0x0,
                           0x1, 0x0, 0x3, 0x0, 0x2, 0x0, 0x3, 0x0, 0x3, 0x0,
                           0x3, 0x0);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "platform-div2");
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", platform_phandle, 0x1);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x410f0, 0x400);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,mpic-global-timer");

    cur_node = "/socp3041/clockgen@e1000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe1000, 0x1000);
    qemu_fdt_setprop_cell(fdt, cur_node, "clock-frequency", 0x0);
    const char clockgen_compat[] = "fsl,p3041-clockgen\0"
                                   "fsl,qoriq-clockgen-2.0";
    qemu_fdt_setprop(fdt, cur_node, "compatible", clockgen_compat,
                     sizeof(clockgen_compat));

    cur_node = "/socp3041/clockgen@e1000/platform";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", platform_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", platform_phandle);
    const char platform_out_name[] = "platform\0platform-div2";
    qemu_fdt_setprop(fdt, cur_node, "clock-output-names", platform_out_name,
                     sizeof(platform_out_name));
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-platform-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x1);

    cur_node = "/socp3041/clockgen@e1000/core-clk3";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "core-clk3");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-core-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x3);

    cur_node = "/socp3041/clockgen@e1000/core-clk2";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "core-clk2");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-core-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x2);

    cur_node = "/socp3041/clockgen@e1000/core-clk1";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "core-clk1");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-core-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x1);

    cur_node = "/socp3041/clockgen@e1000/core-clk0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "core-clk0");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,qoriq-core-clk");
    qemu_fdt_setprop_cell(fdt, cur_node, "clocks", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, cur_node, "cell-index", 0x0);

    cur_node = "/socp3041/clockgen@e1000/sysclk";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", sysclk_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", sysclk_phandle);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-output-names", "sysclk");
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "fsl,qoriq-sysclk");
    qemu_fdt_setprop_cell(fdt, cur_node, "#clock-cells", 0x0);

    cur_node = "/socp3041/pic@40000";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char pic_compat[] = "fsl,mpic\0chrp,open-pic";
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", pic_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", pic_phandle);
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "open-pic");
    qemu_fdt_setprop(fdt, cur_node, "compatible", pic_compat,
                     sizeof(pic_compat));
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x40000, 0x40000);
    qemu_fdt_setprop_cell(fdt, cur_node, "#interrupt-cells", 0x4);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x0);
    qemu_fdt_setprop(fdt, cur_node, "interrupt-controller", NULL, 0);

    cur_node = "/localbus";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char localbus_compat[] = "fsl,ifc\0simple-bus";
    qemu_fdt_setprop(fdt, cur_node, "compatible", localbus_compat,
                     sizeof(localbus_compat));
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);
    qemu_fdt_setprop_cells(fdt, cur_node, "ranges", 0x0, ccsrbarh, ccsrbarl +
                           0x1df0000, 0x8000);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", ccsrbarh, ccsrbarl +
                           0x124000, 0x0, 0x2000);

    cur_node = "/localbus/board-control@0";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char board_ctrl_compat[] = "fsl,fpga-pixis\0fsl,fpga-ngpixis";
    qemu_fdt_setprop_cells(fdt, cur_node, "sysclk-tbl", 0x3f940ab, 0x4f790d5,
                           0x5f5e100, 0x7735940, 0x7f28155, 0x8f0d180,
                           0x9896800, 0x9ef21ab);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x0, 0x300);
    qemu_fdt_setprop(fdt, cur_node, "compatible",
                     board_ctrl_compat, sizeof(board_ctrl_compat));
    qemu_fdt_setprop_cell(fdt, cur_node, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x1);

    cur_node = "/timer@0";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_string(fdt, cur_node, "clock-names", "platform");    
    qemu_fdt_setprop_cells(fdt, cur_node, "clocks", platform_phandle, 0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0, 0, 0, 0);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,p5020-booke-timer");

    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
                            "dtsec(0,0)host:vxWorks"
                            " h=10.10.0.1 e=10.10.0.2:ffffff00 g=10.10.0.1"
                            " u=vxworks pw=vxworks f=0x0");

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
static void p3041ds_compute_dtb(char *filename, uint64_t ram_size,
                                uint64_t load_addr, int *size)
{
    void *fdt;
    Error *err = NULL;
    uint32_t acells, scells;

    if (!filename) {
        /* Create the dtb */
        fdt = p3041ds_create_dtb(ram_size, load_addr, size);
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

    if(qemu_fdt_setprop_sized_cells(fdt, "/memory", "reg",
                                    acells, 0x00,
                                    scells, ram_size) < 0) {
        fprintf(stderr, "Can't set memory prop\n");
        exit(1);
    }

    qemu_fdt_dumpdtb(fdt, *size);
    rom_add_blob_fixed("dtb", fdt, *size, load_addr);

    g_free(fdt);
}

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

    /* Init tlb1 entry 1 (p3041ds)*/
    size = 0xa << MAS1_TSIZE_SHIFT; /* 4Gbytes */
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

    /* enable L2 cache as it is required by vxworks kernel */
    env->spr[SPR_L2CR] |= 0x80000000;
}

static uint32_t g_DEVDISR  = 0x0;
static uint32_t g_DDRDLLCR = 0x0;
static uint32_t g_LBDLLCR  = 0x0;

static uint64_t p3041_gbu_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_GLOBAL_UTILITIES + ccsr_addr;

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

static void p3041_gbu_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_GLOBAL_UTILITIES + ccsr_addr;

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

static const MemoryRegionOps p3041_gbu_ops = {
    .read = p3041_gbu_read,
    .write = p3041_gbu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static uint64_t p3041_pamu_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_PAMU_REGS_BASE + ccsr_addr;

    switch (addr) {
        default:
            PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p3041_pamu_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_PAMU_REGS_BASE + ccsr_addr;

    switch (addr) {
         default:
            PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                             full_addr , val, env->nip);
    }
}

static const MemoryRegionOps p3041_pamu_ops = {
    .read = p3041_pamu_read,
    .write = p3041_pamu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static uint64_t p3041_rcpm_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_RCPM_BASE + ccsr_addr;

    switch (addr) {
        case 0x84:
            return 1;
        default:
            PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p3041_rcpm_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_RCPM_BASE + ccsr_addr;

    switch (addr) {
        default:
            PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                             full_addr , val, env->nip);
    }
}


static const MemoryRegionOps p3041_rcpm_ops = {
    .read = p3041_rcpm_read,
    .write = p3041_rcpm_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static uint64_t p3041_qman_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_QMAN_BASE + ccsr_addr;

    switch (addr) {
        case 0xc04:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x7fd00000, env->nip);
            return 0x7fd00000;

        case 0xc10:
        case 0xc30:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x80000013, env->nip);
            return 0x80000013;

        case 0xc24:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x7fe00000, env->nip);
            return 0x7fe00000;

        case 0xb00:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x01000000, env->nip);
            return 0x01000000;

        case 0xb04:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x00000008, env->nip);
            return 0x00000008;

        case 0xb08:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x00003fff, env->nip);
            return 0x00003fff;

        default:
            PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p3041_qman_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_QMAN_BASE + ccsr_addr;

    switch (addr) {
        default:
            PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                             full_addr , val, env->nip);
    }
}


static const MemoryRegionOps p3041_qman_ops = {
    .read = p3041_qman_read,
    .write = p3041_qman_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};


static uint64_t p3041_bman_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_BMAN_BASE + ccsr_addr;

    switch (addr) {
        case 0xc04:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x7ff00000, env->nip);
            return 0x7ff00000;

        case 0xc10:
            PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                     full_addr, 0x13, env->nip);
            return 0x13;

        default:
            PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p3041_bman_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_BMAN_BASE + ccsr_addr;

    switch (addr) {
        default:
            PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                             full_addr , val, env->nip);
    }
}

static const MemoryRegionOps p3041_bman_ops = {
    .read = p3041_bman_read,
    .write = p3041_bman_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static uint64_t p3041_lca_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_LOCAL_CONF + ccsr_addr;

    PRINT_SUPPORTED_REGISTER("? Unknown ?",
                              full_addr, 0, env->nip);

    switch (addr & 0xfff) {
    case 0x0:
        PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                 full_addr, ccsr_addr >> 12, env->nip);
        return ccsr_addr >> 12;
        break;

    case 0xc04:
        return 0xf4000000;

    case 0xc08:
        return 0x81800014;

    case 0xc10:
        PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                 full_addr, 0, env->nip);
        return 0;

    case 0xc14:
        PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                 full_addr, 0, env->nip);
        return 0;

    case 0xc18:
        PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                 full_addr, 0x8100001a, env->nip);
        return 0x8100001a;

    case 0xc24:
    case 0xc34:
    case 0xc44:
    case 0xc54:
    case 0xc64:
    case 0xc74:
    case 0xc84:
    case 0xc94:
    case 0xca4:
    case 0xcb4:
    case 0xcc4:
    case 0xcd4:
    case 0xce4:
    case 0xcf4:
    case 0xd04:
    case 0xd14:
    case 0xd24:
    case 0xd34:
    case 0xd44:
    case 0xd54:
    case 0xd64:
    case 0xd74:
    case 0xd84:
    case 0xd94:
    case 0xda4:
    case 0xdb4:
    case 0xdc4:
    case 0xdd4:
    case 0xde4:
    case 0xdf4:
        return 0xd0000000;

    case 0xc28:
        PRINT_SUPPORTED_REGISTER("CCSRBAR",
                                 full_addr, 0x0, env->nip);
        return 0x0;
    case 0xc38:
    case 0xc48:
    case 0xc58:
    case 0xc68:
    case 0xc78:
    case 0xc88:
    case 0xc98:
    case 0xca8:
    case 0xcb8:
    case 0xcc8:
    case 0xcd8:
    case 0xce8:
    case 0xcf8:
    case 0xd08:
    case 0xd18:
    case 0xd28:
    case 0xd38:
    case 0xd48:
    case 0xd58:
    case 0xd68:
    case 0xd78:
    case 0xd88:
    case 0xd98:
    case 0xda8:
    case 0xdb8:
    case 0xdc8:
    case 0xdd8:
    case 0xde8:
    case 0xdf8:

        return 0x81d00015;

//     case 0xc0c ... 0xd70:
//         PRINT_READ_UNSUPPORTED_REGISTER("Local access window",
//                                         full_addr, env->nip);
//         break;
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p3041_lca_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_LOCAL_CONF + ccsr_addr;

    PRINT_WRITE_UNSUPPORTED_REGISTER("? Unknown ?",
                                     full_addr, val, env->nip);

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

static const MemoryRegionOps p3041_lca_ops = {
    .read = p3041_lca_read,
    .write = p3041_lca_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p3041_elbc_read(void *opaque, hwaddr addr, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P3041DS_ELBC + ccsr_addr;

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

static void p3041_elbc_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P3041DS_ELBC + ccsr_addr;

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

static const MemoryRegionOps p3041_elbc_ops = {
    .read = p3041_elbc_read,
    .write = p3041_elbc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p3041_ddr_read(void *opaque, hwaddr addr,
                               unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_DDR_CONTROLLER + ccsr_addr;

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

static void p3041_ddr_write(void *opaque, hwaddr addr,
                            uint64_t val, unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_DDR_CONTROLLER + ccsr_addr;

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

static const MemoryRegionOps p3041_ddr_ops = {
    .read = p3041_ddr_read,
    .write = p3041_ddr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p3041_gpio_read(void *opaque, hwaddr addr,
                                unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P3041DS_GPIO + ccsr_addr;

    switch (addr & 0xfff) {
    default:
        PRINT_READ_UNSUPPORTED_REGISTER("? Unknown ?", full_addr, env->nip);
    }
    return 0;
}

static void p3041_gpio_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P3041DS_GPIO + ccsr_addr;

    switch (addr & 0xfff) {
    default:
        PRINT_WRITE_UNSUPPORTED_REGISTER(
            "? Unknown ?", full_addr, val, env->nip);
    }
}

static const MemoryRegionOps p3041_gpio_ops = {
    .read = p3041_gpio_read,
    .write = p3041_gpio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t p3041_l3cache_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    CPUPPCState *env  = opaque;
    hwaddr  full_addr = (addr & 0xfff) + P3041DS_L3CACHE + ccsr_addr;

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

static void p3041_l3cache_write(void *opaque, hwaddr addr,
                                uint64_t val, unsigned size)
{
    CPUPPCState *env       = opaque;
    hwaddr       full_addr = (addr & 0xfff) + P3041DS_L3CACHE + ccsr_addr;

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

static const MemoryRegionOps p3041_l3cache_ops = {
    .read = p3041_l3cache_read,
    .write = p3041_l3cache_write,
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
#ifdef DEBUG_P3041
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

    memory_region_add_subregion(ccsr, P3041DS_MPIC_REGS_BASE,
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
    MemoryRegion *ram, *misc_io;
    MemoryRegion  *params = g_new(MemoryRegion, 1);
    qemu_irq     *mpic, **irqs;
    ResetData    *reset_info;
    int           i;

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
        env->mpic_iack = ccsr_addr + P3041DS_MPIC_REGS_BASE + 0xa0;

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
    memory_region_init_ram(ram, NULL, "3041.ram", ram_size, &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x0, ram);

    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "p3041.ram_L3",
                           0x1000000 /* 512 * 1024 */, &error_abort);
    memory_region_add_subregion(get_system_memory(),
                                0xf8000000 /* 0xf8b00000 */, ram);

    /* Configuration, Control, and Status Registers */
    ccsr_space = g_malloc0(sizeof(*ccsr_space));
    memory_region_init(ccsr_space, NULL, "CCSR_space", 0x1000000);
    memory_region_add_subregion_overlap(get_system_memory(), ccsr_addr,
                                        ccsr_space, 2);

    /* Global Utilities */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_gbu_ops, env,
                          "Global Utilities", 0x1000);
    memory_region_add_subregion(ccsr_space, P3041DS_GLOBAL_UTILITIES,
                                misc_io);

    /* PAMU */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_pamu_ops, env,
                          "PAMU", 0xffff);
    memory_region_add_subregion(ccsr_space, P3041DS_PAMU_REGS_BASE, misc_io);

    /* RCPM */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_rcpm_ops, env,
                          "RCPM", 0xfff);
    memory_region_add_subregion(ccsr_space, P3041DS_RCPM_BASE, misc_io);

    /* QMAN */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_qman_ops, env,
                          "QMAN", 0xfff);
    memory_region_add_subregion(ccsr_space, P3041DS_QMAN_BASE, misc_io);

    /* BMAN */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_bman_ops, env,
                          "BMAN", 0xfff);
    memory_region_add_subregion(ccsr_space, P3041DS_BMAN_BASE, misc_io);

    /* Local configuration/access */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_lca_ops, env,
                          "Local configuration/access", 0x1000);
    memory_region_add_subregion(ccsr_space, P3041DS_LOCAL_CONF, misc_io);

    /* eLBC */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_elbc_ops, env, "eLBC",
                          0x1000);
    memory_region_add_subregion(ccsr_space, P3041DS_ELBC, misc_io);

    /* DDR Memory Controller */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_ddr_ops, env,
                          "DDR memory controller", 0x1000);
    memory_region_add_subregion(ccsr_space, P3041DS_DDR_CONTROLLER, misc_io);

    /* GPIO */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_gpio_ops, env, "GPIO",
                          0x1000);
    memory_region_add_subregion(ccsr_space, P3041DS_GPIO, misc_io);

    /* L3CACHE */
    misc_io = g_malloc0(sizeof(*misc_io));
    memory_region_init_io(misc_io, NULL, &p3041_l3cache_ops, env, "L3CACHE",
                          P3041DS_L3_CACHE_SIZE);
    memory_region_add_subregion(ccsr_space, P3041DS_L3CACHE, misc_io);

    /* MPIC */
    mpic = init_mpic(ccsr_space, irqs);

    if (!mpic) {
        cpu_abort(CPU(env), "MPIC failed to initialize\n");
    }

    /* Serial */
    if (serial_hd(0)) {
        serial_mm_init(ccsr_space, P3041DS_SERIAL0_REGS_BASE,
                       0, mpic[config->serial_irq], 115200,
                       serial_hd(0), DEVICE_BIG_ENDIAN);
    }

    if (serial_hd(1)) {
        serial_mm_init(ccsr_space, P3041DS_SERIAL1_REGS_BASE,
                       0, mpic[config->serial_irq], 115200,
                       serial_hd(1), DEVICE_BIG_ENDIAN);
    }

    /* Load kernel. */
    if (kernel_filename) {
        int dtb_size;

        kernel_size = load_elf(kernel_filename, NULL, NULL, NULL, &elf_entry,
                               &elf_lowaddr, NULL, 1, PPC_ELF_MACHINE, 0, 0);

        /* XXX try again as binary */
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    kernel_filename);
            exit(1);
        }

        dt_base = (elf_lowaddr + kernel_size + DTC_LOAD_PAD) & ~DTC_PAD_MASK;

        p3041ds_compute_dtb(args->dtb, ram_size, dt_base, &dtb_size);
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
    memory_region_init_ram(params, NULL, "p3041ds.params", PARAMS_SIZE,
                           &error_abort);
    memory_region_add_subregion(ccsr_space, PARAMS_ADDR, params);

    if (args->kernel_cmdline) {
        cpu_physical_memory_write(ccsr_addr + PARAMS_ADDR,
                                  args->kernel_cmdline,
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


static fsl_e500_config p3041ds_config = {
    .freq = 700000000UL >> 3,
    .serial_irq = 16 + 20,
    .cfi01_flash = FALSE,
};

static void p3041ds_generic_hw_init(MachineState *machine)
{
    fsl_e500_init(&p3041ds_config, machine);
}

static void p3041ds_generic_machine_init(MachineClass *mc)
{
    mc->desc = "p3041ds reset state";
    mc->init = p3041ds_generic_hw_init;
    mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("e500mc");
}

DEFINE_MACHINE("p3041ds", p3041ds_generic_machine_init)
