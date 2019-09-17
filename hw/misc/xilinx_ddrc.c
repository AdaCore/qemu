/*
 * DDRC
 *
 * Copyright (C) 2019 Adacore
 *
 * Developed by :
 *  Frederic Konrad   <frederic.konrad@adacore.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "sysemu/sysemu.h"
#include "qemu/log.h"

#define TYPE_ZYNQ_DDRC "xilinx,zynq_ddrc"
#define ZYNQ_DDRC(obj) OBJECT_CHECK(ZynqDDRCState, (obj), TYPE_ZYNQ_DDRC)

#define ZYNQ_DDRC_MODE_STS      (0x54 >> 2)
#define ZYNQ_DDRC_MMIO_SIZE     (0x300)
#define ZYNQ_DDRC_NUM_REGS      (ZYNQ_DDRC_MMIO_SIZE >> 2)

typedef struct ZynqDDRCState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t regs[ZYNQ_DDRC_NUM_REGS];
} ZynqDDRCState;

static void zynq_ddrc_reset(DeviceState *d)
{
    ZynqDDRCState *s = ZYNQ_DDRC(d);

    /* Not true on the real HW but since it's not implemented the guest might
     * hang polling on this bit. */
    s->regs[ZYNQ_DDRC_MODE_STS] = 0x00000001;
}

static uint64_t zynq_ddrc_read(void *opaque, hwaddr offset, unsigned size)
{
    ZynqDDRCState *s = ZYNQ_DDRC(opaque);
    offset /= 4;

    return s->regs[offset];
}

static void zynq_ddrc_write(void *opaque, hwaddr offset,
                          uint64_t val, unsigned size)
{
    ZynqDDRCState *s = (ZynqDDRCState *)opaque;
    offset /= 4;

    switch (offset) {
    case ZYNQ_DDRC_MODE_STS:
        break;
    default:
        s->regs[offset] = val;
        break;
    }
}

static const MemoryRegionOps ddrc_ops = {
    .read = zynq_ddrc_read,
    .write = zynq_ddrc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void zynq_ddrc_init(Object *obj)
{
    ZynqDDRCState *s = ZYNQ_DDRC(obj);

    memory_region_init_io(&s->iomem, obj, &ddrc_ops, s, "ddrc",
                          ZYNQ_DDRC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}

static void zynq_ddrc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = zynq_ddrc_reset;
}

static const TypeInfo zynq_ddrc_info = {
    .class_init = zynq_ddrc_class_init,
    .name  = TYPE_ZYNQ_DDRC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(ZynqDDRCState),
    .instance_init = zynq_ddrc_init,
};

static void zynq_ddrc_register_types(void)
{
    type_register_static(&zynq_ddrc_info);
}

type_init(zynq_ddrc_register_types)
