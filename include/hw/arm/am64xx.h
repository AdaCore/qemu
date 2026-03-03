/*
 * TI AM64xx SoC emulation
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AM64XX_H
#define AM64XX_H

#include "qemu/units.h"
#include "hw/sysbus.h"
#include "hw/intc/arm_gicv3.h"
#include "hw/char/serial-tl16c750.h"
#include "hw/misc/am64xx_ctrl.h"

#define AM64XX_NUM_APU_CPUS (4)
#define AM64XX_NUM_UARTS (7)
#define AM64XX_NUM_IRQS (256)

struct Am64xxState {
    DeviceState parent_obj;

    ARMCPU apu_cpu[AM64XX_NUM_APU_CPUS];
    GICv3State gic;
    SerialTl16c750State uart[AM64XX_NUM_UARTS];
    Am64xxCtrlState ctrl_mmr0_cfg0;
    MemoryRegion ecco_ram;
};

#define TYPE_AM64XX "am64xx"
OBJECT_DECLARE_SIMPLE_TYPE(Am64xxState, AM64XX);

#define AM64XX_RAM_START        0x80000000
#define AM64XX_RAM_SIZE_MAX     (2 * GiB)

enum Am64xxMemoryRegions {
    AM64XX_RAM,
    AM64XX_PADCFG_CTRL0_CFG0,
    AM64XX_TIMESYNC_EVENT_INTROUTER0_CFG,
    AM64XX_GIC_TRANSLATER,
    AM64XX_GIC_DIST,
    AM64XX_GIC_MSG_SPI,
    AM64XX_GIC_ITS,
    AM64XX_GIC_REDIST,
    AM64XX_UART0,
    AM64XX_UART1,
    AM64XX_UART2,
    AM64XX_UART3,
    AM64XX_UART4,
    AM64XX_UART5,
    AM64XX_UART6,
    AM64XX_MAILBOX0_REGS0,
    AM64XX_MAILBOX0_REGS1,
    AM64XX_MAILBOX0_REGS2,
    AM64XX_MAILBOX0_REGS3,
    AM64XX_MAILBOX0_REGS4,
    AM64XX_MAILBOX0_REGS5,
    AM64XX_MAILBOX0_REGS6,
    AM64XX_MAILBOX0_REGS7,
    AM64XX_SPINLOCK,
    AM64XX_CTRL_MMR0_CFG0,
    AM64XX_DMASS0_PKTDMA_GCFG,
    AM64XX_DMASS0_BCDMA_GCFG,
    AM64XX_DMASS0_SEC_PROXY_SCFG,
    AM64XX_DMASS0_SEC_PROXY_RT,
};

enum Am64xxIrqs {
    AM64XX_UART0_IRQ    = 210,
    AM64XX_UART1_IRQ    = 211,
    AM64XX_UART2_IRQ    = 212,
    AM64XX_UART3_IRQ    = 213,
    AM64XX_UART4_IRQ    = 214,
    AM64XX_UART5_IRQ    = 215,
    AM64XX_UART6_IRQ    = 216,
};

void am64xx_load_kernel(MachineState *machine, Am64xxState *soc);

#endif
