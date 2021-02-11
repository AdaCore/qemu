/*
 * ARM RealView Emulation Baseboard Interrupt Controller
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL.
 */

#ifndef HW_INTC_REALVIEW_GIC_H
#define HW_INTC_REALVIEW_GIC_H

#include "hw/sysbus.h"
#include "hw/intc/arm_gic.h"
#include "qom/object.h"

#define TYPE_REALVIEW_GIC "realview_gic"
typedef struct RealViewGICState RealViewGICState;
DECLARE_INSTANCE_CHECKER(RealViewGICState, REALVIEW_GIC,
                         TYPE_REALVIEW_GIC)

struct RealViewGICState {
    SysBusDevice parent_obj;

    MemoryRegion container;

    GICState gic;
};

#endif
