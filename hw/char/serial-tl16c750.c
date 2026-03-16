/*
 * Device model for TI TL16C750 UART
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/char/serial-tl16c750.h"
#include "qapi/error.h"
#include "hw/registerfields.h"
#include "qemu/log.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "trace.h"

#define UART_DEFAULT_CLK (18 * 1000 * 1000)

#define UART_REGSHIFT 2

REG32(MDR1, 0x20)
FIELD(MDR1, MODE_SELECT, 0, 3)
#define R_MDR1_MODE_UART_16X       0x0
#define R_MDR1_MODE_SUR            0x1
#define R_MDR1_MODE_UART_16X_AUTO  0x2
#define R_MDR1_MODE_UART_13X       0x3
#define R_MDR1_MODE_MIR            0x4
#define R_MDR1_MODE_FIR            0x5
#define R_MDR1_MODE_CIR            0x6
#define R_MDR1_MODE_DISABLE        0x7

REG32(SSR, 0x44)
FIELD(SSR, TX_FIFO_FULL, 0, 1)
REG32(SYSC, 0x54)
FIELD(SYSC, SOFTRESET, 1, 1)
REG32(SYSS, 0x58)
FIELD(SYSS, RESETDONE, 0, 1)

static void serial_tl16c750_reset(SerialTl16c750State *s)
{
    /* Reset underlying serial first */
    s->serial.reset_fn(&s->serial);
    s->r_mdr1 = 0x7;
    s->r_sysc = 0x0;

    /* Technically 0x0, but set RESETDONE bit directly.  */
    s->r_syss = 0x1;
}

static uint64_t
serial_tl16c750_read(void *opaque, hwaddr addr, unsigned int size)
{
    SerialTl16c750State *s = opaque;
    /* Early registers share the same API as 16550A UARTs. */
    if (addr < 0x8 << UART_REGSHIFT) {
        /* Serial device IO operations are expecting a register offset.  */
        assert (!(addr % (1 << UART_REGSHIFT)));
        return serial_io_ops.read(&s->serial, addr >> UART_REGSHIFT, 1);
    }

    switch(addr) {
    case A_MDR1:
        return s->r_mdr1;
    case A_SSR:
        return s->r_ssr;
    case A_SYSC:
        /* SOFTRESET bit read always return 0. */
        return s->r_sysc & ~(R_SYSC_SOFTRESET_MASK);
    case A_SYSS:
        return s->r_syss;
    }
    qemu_log_mask(LOG_GUEST_ERROR, "%s: bad read: addr=0x%x\n",
                  __func__, (int)addr);
    return 0;
}

static void
serial_tl16c750_write(void *opaque, hwaddr addr,
                      uint64_t value, unsigned int size)
{
    SerialTl16c750State *s = opaque;

    /* Early registers share the same API as 16550A UARTs. */
    if (addr < 0x8 << UART_REGSHIFT) {
        /* Serial device IO operations are expecting a register offset.  */
        assert (!(addr % (1 << UART_REGSHIFT)));
        serial_io_ops.write(&s->serial, addr >> UART_REGSHIFT, value, 1);

        s->r_ssr = FIELD_DP8(s->r_ssr, SSR, TX_FIFO_FULL,
                             fifo8_is_full(&s->serial.xmit_fifo) ? 1: 0);
        return;
    }

    /* 16C750 extended registers. */
    switch(addr) {
    case A_MDR1:
        s->r_mdr1 = FIELD_DP8(s->r_mdr1, MDR1, MODE_SELECT, value);
        switch (s->r_mdr1 & R_MDR1_MODE_SELECT_MASK){
        case R_MDR1_MODE_UART_16X:
        case R_MDR1_MODE_DISABLE:
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: unsupported MDR1 mode v=0x%x\n",
                          __func__, s->r_mdr1 & R_MDR1_MODE_SELECT_MASK);
            break;
        }
        break;
    case A_SYSC:
        if (value & R_SYSC_SOFTRESET_MASK) {
            s->r_syss = FIELD_DP8(s->r_syss, SYSS, RESETDONE, 0);
            serial_tl16c750_reset(s);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write: addr=0x%x v=0x%lx\n",
                      __func__, (int)addr, value);
        break;
    }
}

static const MemoryRegionOps serial_tl16c750_ops = {
    .read = serial_tl16c750_read,
    .write = serial_tl16c750_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void serial_tl16c750_init(Object *obj)
{
    SerialTl16c750State *s = SERIAL_TL16C750(obj);

    object_initialize_child(obj, "serial", &s->serial, TYPE_SERIAL);
    qdev_alias_all_properties(DEVICE(&s->serial), obj);
}

static void serial_tl16c750_realize(DeviceState *dev, Error **errp)
{
    SerialTl16c750State *s = SERIAL_TL16C750(dev);
    SerialState *serial = &s->serial;

    if (!qdev_realize(DEVICE(serial), NULL, errp)) {
        return;
    }
    memory_region_init_io(&serial->io, OBJECT(dev), &serial_tl16c750_ops, s,
                          "serial.tl16c750", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &serial->io);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->serial.irq);
}

static void serial_tl16c750_reset_hold(Object *obj, ResetType type)
{
    SerialTl16c750State *s = SERIAL_TL16C750(obj);
    serial_tl16c750_reset(s);
}

static void serial_tl16c750_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    dc->realize = serial_tl16c750_realize;
    rc->phases.hold  = serial_tl16c750_reset_hold;
}

static const TypeInfo serial_tl16c750_types[] = {
    {
        .name          = TYPE_SERIAL_TL16C750,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(SerialTl16c750State),
        .instance_init = serial_tl16c750_init,
        .class_init    = serial_tl16c750_class_init,
    },
};
DEFINE_TYPES(serial_tl16c750_types);
