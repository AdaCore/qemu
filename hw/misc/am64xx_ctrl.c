/*
 * Device model for TI AM64xx Control Module
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/log.h"
#include "hw/qdev-properties.h"
#include "hw/registerfields.h"
#include "hw/misc/am64xx_ctrl.h"
#include "sysemu/runstate.h"

#define AM64XX_CTRL_REGS_SIZE   (128 * KiB)
#define JTAGID_VALUE 0xBB3802F

REG32(JTAGID, 0x14)
REG32(RST_CTRL, 0x18170)
FIELD(RST_CTRL, SW_MAIN_WARMRST, 0, 4)
FIELD(RST_CTRL, SW_MAIN_POR, 4, 4)

static uint64_t am64xx_ctrl_read(void *opaque, hwaddr addr, unsigned size)
{
    Am64xxCtrlState *s = AM64XX_CTRL(opaque);

    switch(addr) {
    case A_JTAGID:
        return JTAGID_VALUE;
    case A_RST_CTRL:
        return s->r_rst_ctrl;
    }
    qemu_log_mask(LOG_GUEST_ERROR, "%s: bad read: addr=0x%x\n",
                  __func__, (int)addr);
    return 0;
}

static void am64xx_ctrl_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned size)
{
    switch(addr) {
    case A_RST_CTRL:
        if (FIELD_EX8(value, RST_CTRL, SW_MAIN_WARMRST) == 0x6) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        } else if (FIELD_EX8(value, RST_CTRL, SW_MAIN_POR) == 0x6) {
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: unsupported RST_CTRL value: 0x%lx\n",
                          __func__, value);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write: addr=0x%x v=0x%lx\n",
                      __func__, (int)addr, value);
        break;
    }

}

static const MemoryRegionOps am64xx_ctrl_ops = {
    .read = am64xx_ctrl_read,
    .write = am64xx_ctrl_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void am64xx_ctrl_init(Object *obj)
{
    Am64xxCtrlState *s = AM64XX_CTRL(obj);

    memory_region_init_io(&s->iomem, obj, &am64xx_ctrl_ops, s, "regs",
                          AM64XX_CTRL_REGS_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static void am64xx_ctrl_enter_reset(Object *obj, ResetType type)
{
    Am64xxCtrlState *s = AM64XX_CTRL(obj);

    s->r_rst_ctrl = 0x200FF;
}


static void am64xx_ctrl_class_init(ObjectClass *klass, void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->desc = "AM64xx Control Module";
    rc->phases.enter = am64xx_ctrl_enter_reset;
}

static const TypeInfo am64xx_ctrl_types[] = {
    {
        .name = TYPE_AM64XX_CTRL,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(Am64xxCtrlState),
        .class_init = am64xx_ctrl_class_init,
        .instance_init = am64xx_ctrl_init,
    },
};
DEFINE_TYPES(am64xx_ctrl_types);
