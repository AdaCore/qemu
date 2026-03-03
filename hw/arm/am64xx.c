/*
 * TI AM64xx SoC emulation
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "target/arm/cpu.h"
#include "qapi/error.h"
#include "qapi/qmp/qlist.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/misc/unimp.h"
#include "hw/arm/boot.h"
#include "hw/arm/bsa.h"

#include "hw/arm/am64xx.h"

static const struct {
    hwaddr addr;
    size_t size;
    const char *name;
} am64xx_memmap[] = {
    [AM64XX_RAM] = {AM64XX_RAM_START, AM64XX_RAM_SIZE_MAX, "ram" },
    [AM64XX_PADCFG_CTRL0_CFG0] = {0x0000F0000, 32*KiB, "padcfg.crtl0.cfg0"},
    [AM64XX_TIMESYNC_EVENT_INTROUTER0_CFG] = {0x000A40000, 2*KiB, "timesync.event_introuter0.cfg"},
    [AM64XX_GIC_TRANSLATER] = {0x001000000, 4*MiB, "gic.translater"},
    [AM64XX_GIC_DIST] = {0x001800000, 64*KiB, "gic"},
    [AM64XX_GIC_MSG_SPI] = {0x001810000, 64*KiB, "gic.msg_spi"},
    [AM64XX_GIC_ITS] = {0x001820000, 128*KiB, "gic.its"},
    [AM64XX_GIC_REDIST] = {0x001840000, 64*KiB, "gic.redist"},
    [AM64XX_UART0] = {0x002800000, 512, "uart0"},
    [AM64XX_UART1] = {0x002810000, 512, "uart1"},
    [AM64XX_UART2] = {0x002820000, 512, "uart2"},
    [AM64XX_UART3] = {0x002830000, 512, "uart3"},
    [AM64XX_UART4] = {0x002840000, 512, "uart4"},
    [AM64XX_UART5] = {0x002850000, 512, "uart5"},
    [AM64XX_UART6] = {0x002860000, 512, "uart6"},
    [AM64XX_MAILBOX0_REGS0] = {0x029000000, 512, "mailbox.regs0"},
    [AM64XX_MAILBOX0_REGS1] = {0x029010000, 512, "mailbox.regs1"},
    [AM64XX_MAILBOX0_REGS2] = {0x029020000, 512, "mailbox.regs2"},
    [AM64XX_MAILBOX0_REGS3] = {0x029030000, 512, "mailbox.regs3"},
    [AM64XX_MAILBOX0_REGS4] = {0x029040000, 512, "mailbox.regs4"},
    [AM64XX_MAILBOX0_REGS5] = {0x029050000, 512, "mailbox.regs5"},
    [AM64XX_MAILBOX0_REGS6] = {0x029060000, 512, "mailbox.regs6"},
    [AM64XX_MAILBOX0_REGS7] = {0x029070000, 512, "mailbox.regs7"},
    [AM64XX_SPINLOCK] = {0x02A000000, 512, "spinlock"},
    [AM64XX_CTRL_MMR0_CFG0] = {0x043000000, 128*KiB, "ctrl.mmr0.cfg0"},
    [AM64XX_DMASS0_PKTDMA_GCFG] = {0x0485C0000, 256, "dmass0.pktdma.gcfg"},
    [AM64XX_DMASS0_BCDMA_GCFG] = {0x0485C0100, 256, "dmass0.bcdma.gcfg"},
    [AM64XX_DMASS0_SEC_PROXY_SCFG] = {0x04A400000, 512*KiB, "dmass0.sec_proxy.scfg"},
    [AM64XX_DMASS0_SEC_PROXY_RT] = {0x04A600000, 512*KiB, "dmass0.sec_proxy.rt"},
};

static struct arm_boot_info am64xx_binfo = {
    .loader_start = AM64XX_RAM_START,
    .psci_conduit = QEMU_PSCI_CONDUIT_SMC,
};

void am64xx_load_kernel(MachineState *ms, Am64xxState *s)
{
    am64xx_binfo.ram_size = ms->ram_size;

    arm_load_kernel(&s->apu_cpu[0], ms, &am64xx_binfo);
}


static void am64xx_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    Am64xxState *s = AM64XX(obj);
    int i;

    for (i = 0; i < MIN(ms->smp.cpus, AM64XX_NUM_APU_CPUS); i++) {
        object_initialize_child(obj, "apu-cpu[*]",
                                &s->apu_cpu[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
    }

    object_initialize_child(OBJECT(s), "gic", &s->gic, TYPE_ARM_GICV3);

    for (i = 0; i < AM64XX_NUM_UARTS; i++) {
        g_autofree char *name = g_strdup_printf("uart%d", i + 1);
        object_initialize_child(obj, name, &s->uart[i], TYPE_SERIAL_TL16C750);
    }

    object_initialize_child(obj, "ctrl.mmr0.cfg0", &s->ctrl_mmr0_cfg0,
                            TYPE_AM64XX_CTRL);

}

static void create_gic(Am64xxState *s, MachineState *ms, MemoryRegion *sysmem)
{
    DeviceState *gicdev;
    QList *redist_region_count;

    gicdev = DEVICE(&s->gic);
    qdev_prop_set_uint32(gicdev, "num-cpu", ms->smp.cpus);
    qdev_prop_set_uint32(gicdev, "num-irq", AM64XX_NUM_IRQS + GIC_INTERNAL);

    redist_region_count = qlist_new();
    qlist_append_int(redist_region_count, ms->smp.cpus);
    qdev_prop_set_array(gicdev, "redist-region-count", redist_region_count);
    object_property_set_link(OBJECT(&s->gic), "sysmem",
                             OBJECT(sysmem), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->gic), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 0, am64xx_memmap[AM64XX_GIC_DIST].addr);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 1, am64xx_memmap[AM64XX_GIC_REDIST].addr);

    /*
     * Wire the outputs from each CPU's generic timer and the GICv3
     * maintenance interrupt signal to the appropriate GIC PPI inputs,
     * and the GIC's IRQ/FIQ/VIRQ/VFIQ interrupt outputs to the CPU's inputs.
     */
    for (int i = 0; i < ms->smp.cpus; i++) {
        DeviceState *cpudev = DEVICE(&s->apu_cpu[i]);
        SysBusDevice *gicsbd = SYS_BUS_DEVICE(&s->gic);
        int intidbase = AM64XX_NUM_IRQS + i * GIC_INTERNAL;
        int irq;

        /*
         * Mapping from the output timer irq lines from the CPU to the
         * GIC PPI inputs used for this board. This isn't a BSA board,
         * but it uses the standard convention for the PPI numbers.
         */
        const int timer_irq[] = {
            [GTIMER_PHYS] = ARCH_TIMER_NS_EL1_IRQ,
            [GTIMER_VIRT] = ARCH_TIMER_VIRT_IRQ,
            [GTIMER_HYP]  = ARCH_TIMER_NS_EL2_IRQ,
        };

        for (irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
            qdev_connect_gpio_out(cpudev, irq,
                                  qdev_get_gpio_in(gicdev,
                                                   intidbase + timer_irq[irq]));
        }

        qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt", 0,
                                    qdev_get_gpio_in(gicdev,
                                                     intidbase + ARCH_GIC_MAINT_IRQ));

        qdev_connect_gpio_out_named(cpudev, "pmu-interrupt", 0,
                                    qdev_get_gpio_in(gicdev,
                                                     intidbase + VIRTUAL_PMU_IRQ));

        sysbus_connect_irq(gicsbd, i,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(gicsbd, i + ms->smp.cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(gicsbd, i + 2 * ms->smp.cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(gicsbd, i + 3 * ms->smp.cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
    }

}

static void am64xx_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    Am64xxState *s = AM64XX(dev);
    MemoryRegion *sysmem = get_system_memory();
    DeviceState *gicdev;
    int i;

    for (i = 0; i < MIN(ms->smp.cpus, AM64XX_NUM_APU_CPUS); i++) {
        /*
         * Secondary CPUs start in powered-down state.
         */
        if (i != 0) {
            object_property_set_bool(OBJECT(&s->apu_cpu[i]),
                                     "start-powered-off", true, &error_abort);
        }
        if (!qdev_realize(DEVICE(&s->apu_cpu[i]), NULL, &error_fatal)) {
            return;
        }
    }

    create_gic(s, ms, sysmem);
    gicdev = DEVICE(&s->gic);

    /* UARTs */
    for (i = 0; i < AM64XX_NUM_UARTS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } serial_table[AM64XX_NUM_UARTS] = {
            { am64xx_memmap[AM64XX_UART0].addr, AM64XX_UART0_IRQ },
            { am64xx_memmap[AM64XX_UART1].addr, AM64XX_UART1_IRQ },
            { am64xx_memmap[AM64XX_UART2].addr, AM64XX_UART2_IRQ },
            { am64xx_memmap[AM64XX_UART3].addr, AM64XX_UART3_IRQ },
            { am64xx_memmap[AM64XX_UART4].addr, AM64XX_UART4_IRQ },
            { am64xx_memmap[AM64XX_UART5].addr, AM64XX_UART5_IRQ },
            { am64xx_memmap[AM64XX_UART6].addr, AM64XX_UART6_IRQ },
        };

        qdev_prop_set_chr(DEVICE(&s->uart[i]), "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart[i]), 0, serial_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[i]), 0,
                           qdev_get_gpio_in(gicdev, serial_table[i].irq));
    }

    /* Power Management */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->ctrl_mmr0_cfg0), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ctrl_mmr0_cfg0), 0,
                    am64xx_memmap[AM64XX_CTRL_MMR0_CFG0].addr);

    /* Unimplemented devices */
    for (i = 0; i < ARRAY_SIZE(am64xx_memmap); i++) {
        switch (i) {
        case AM64XX_RAM:
        case AM64XX_GIC_DIST:
        case AM64XX_GIC_REDIST:
        case AM64XX_UART0:
        case AM64XX_UART1:
        case AM64XX_UART2:
        case AM64XX_UART3:
        case AM64XX_UART4:
        case AM64XX_UART5:
        case AM64XX_UART6:
        case AM64XX_CTRL_MMR0_CFG0:
            /* device implemented and treated above */
            break;

        default:
            create_unimplemented_device(am64xx_memmap[i].name,
                                        am64xx_memmap[i].addr,
                                        am64xx_memmap[i].size);
            break;
        }
    }
}


static void am64xx_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = am64xx_realize;
}

static const TypeInfo am64xx_soc_types[] = {
    {
        .name           = TYPE_AM64XX,
        .parent         = TYPE_DEVICE,
        .instance_size  = sizeof(Am64xxState),
        .instance_init  = am64xx_init,
        .class_init     = am64xx_class_init,
    },
};

DEFINE_TYPES(am64xx_soc_types);
