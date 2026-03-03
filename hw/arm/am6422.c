/*
 * TI AM64xx SoC emulation
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "target/arm/cpu.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/arm/am64xx.h"

static void am6422_init(MachineState *ms)
{

    Am64xxState *s;

    s = AM64XX(object_new(TYPE_AM64XX));
    object_property_add_child(OBJECT(ms), "soc", OBJECT(s));
    qdev_realize(DEVICE(s), NULL, &error_fatal);

    memory_region_add_subregion(get_system_memory(), AM64XX_RAM_START,
                                ms->ram);

    am64xx_load_kernel(ms, s);
}

static void am6422_machine_init(MachineClass *mc)
{
    mc->desc = "TI AM6422";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a53");
    mc->max_cpus = 2;
    mc->default_cpus = 2;
    mc->init = am6422_init;
    mc->default_ram_id = "am64xx.ram";
}

DEFINE_MACHINE("am6422", am6422_machine_init)
