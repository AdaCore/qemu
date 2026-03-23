/*
 * Device model for TI AM64xx Control Module
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AM64XX_CTRL_H
#define HW_AM64XX_CTRL_H

#include "qom/object.h"
#include "hw/sysbus.h"

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t r_rst_ctrl;

} Am64xxCtrlState;

#define TYPE_AM64XX_CTRL "am64xx.ctrl"
#define AM64XX_CTRL(obj)                                    \
    OBJECT_CHECK(Am64xxCtrlState, (obj), TYPE_AM64XX_CTRL);

#endif /* HW_AM64XX_CTRL_H */
