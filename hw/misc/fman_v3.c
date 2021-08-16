/*
 * FMan_v3 (T2080)
 *
 * Copyright (c) 2019-2021 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qapi/error.h"

#define TYPE_FMAN "fman_v3"
#define FMAN(obj) OBJECT_CHECK(FMANState, (obj), \
                                     TYPE_FMAN)
#define FMAN_R_MAX (0x100000 >> 2)

/* This depends on the chip.. 384K for T2080RDB */
#define FMAN_INTERNAL_MEM_SZ  (384 * 1024)
#define FMAN_INTERNAL_MEM_MAX (512 * 1024)

#define FMAN_FM_FP_MXD          (0xC300C >> 2)
#define FMAN_FM_RCR_OFFSET      (0xC3070 >> 2)
#define FMAN_FM_IP_REV_1_OFFSET (0xC30C4 >> 2)
#define FMAN_FM_RSTC_OFFSET     (0xC30CC >> 2)
#define FMAN_FM_RSTC_RFM_SFT (31)
#define FMAN_DM_SR_OFFSET       (0xC2000 >> 2)

#define FMAN_SOFT_PARSER_OFFSET (0xC7000 >> 2)
#define FMAN_SOFT_PARSER_SZ     (0x01000 >> 2)

#define FMAN_FM_IP_REVISION (0x0A070603)

/* EMACS MEMORY MAPS */
struct emac_mdio_offset {
    uint32_t emac_offset;
    uint32_t mdio_offset;
};

static const struct emac_mdio_offset emac_mdio_off[] = {
    {
        .emac_offset = 0xE4000,
        .mdio_offset = 0xFC000
    },{
        .emac_offset = 0xE6000,
        .mdio_offset = 0xFD000
    },{
        .emac_offset = 0xE0000,
        .mdio_offset = 0xE1000
    },{
        .emac_offset = 0xE2000,
        .mdio_offset = 0xE3000
    },{
        .emac_offset = 0xE8000,
        .mdio_offset = 0xE9000
    },{
        .emac_offset = 0xEA000,
        .mdio_offset = 0xEB000
    },{
        .emac_offset = 0xEC000,
        .mdio_offset = 0xED000
    },{
        .emac_offset = 0xEE000,
        .mdio_offset = 0xEF000
    },{
        .emac_offset = 0xF0000,
        .mdio_offset = 0xF1000
    },{
        .emac_offset = 0xF2000,
        .mdio_offset = 0xF3000
    }
};

#define FMAN_EMACx_OFFSET(n)      (emac_mdio_off[n].emac_offset)
#define FMAN_EMACx_MDIO_OFFSET(n) (emac_mdio_off[n].mdio_offset)

typedef struct {
    SysBusDevice parent_obj;

    MemoryRegion container;
    MemoryRegion iomem;
    MemoryRegion internal_mem;
    uint32_t reg[FMAN_R_MAX];
    qemu_irq irq;
} FMANState;

#ifdef FMAN_DEBUG
#define DB_PRINT(...) do {                                                \
    fprintf(stderr,  "0x%8.8x: %s: ", (unsigned)s->iomem.addr, __func__); \
    fprintf(stderr, ## __VA_ARGS__);                                      \
    } while (0);
#else
    #define DB_PRINT(...)
#endif

static void fman_reset(DeviceState *dev)
{
    FMANState *s = FMAN(dev);

    memset(s->reg, 0, sizeof(s->reg));

    s->reg[FMAN_FM_IP_REV_1_OFFSET] = FMAN_FM_IP_REVISION;
    s->reg[FMAN_DM_SR_OFFSET] = 0x6830;
}

static void fman_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    FMANState *s = FMAN(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset, (unsigned)value);
    offset >>= 2;
    assert(offset < FMAN_R_MAX);
    assert(size == 4);

    switch (offset) {
    case 0 ... ((FMAN_INTERNAL_MEM_MAX >> 2) - 1):
        /* Shouldn't happen. But the guest can try to write out of the
         * internal memory as the size varies from SOCs to SOCs. */
        return;
    case FMAN_FM_IP_REV_1_OFFSET:
        abort();
        /* Read Only */
        return;
    case FMAN_FM_RCR_OFFSET:
        s->reg[offset] = (value & ~(0xC000))
            | (((~value) & s->reg[offset]) & 0xC000);
        break;
    case FMAN_FM_RSTC_OFFSET:
        if (extract32(value, FMAN_FM_RSTC_RFM_SFT, 1)) {
            /* Reset and don't keep the written value */
            fman_reset(DEVICE(s));
        }
        break;
    case FMAN_DM_SR_OFFSET:
        /* w1c */
        s->reg[offset] = (~value & s->reg[offset]);
        break;
    case FMAN_FM_FP_MXD:
    case FMAN_SOFT_PARSER_OFFSET ...
         FMAN_SOFT_PARSER_OFFSET + FMAN_SOFT_PARSER_SZ:
        s->reg[offset] = value;
        break;
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t fman_read(void *opaque, hwaddr offset, unsigned size)
{
    FMANState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < FMAN_R_MAX);
    assert(size == 4);

    switch (offset) {
    case 0 ... ((FMAN_INTERNAL_MEM_MAX >> 2) - 1):
        /* Shouldn't happen. But the guest can try to read out of the internal
         * memory as the size varies from SOCs to SOCs. */
        val = 0;
        break;
    case FMAN_FM_IP_REV_1_OFFSET:
    case FMAN_FM_RSTC_OFFSET:
    case FMAN_FM_RCR_OFFSET:
    case FMAN_SOFT_PARSER_OFFSET ...
         FMAN_SOFT_PARSER_OFFSET + FMAN_SOFT_PARSER_SZ:
        val = s->reg[offset];
        break;
    default:
        val = s->reg[offset];
        break;
    }

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)(offset << 2),
             (unsigned)val);
    return val;
}

static const MemoryRegionOps fman_ops = {
    .read = fman_read,
    .write = fman_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void fman_realize(DeviceState *dev, Error **errp)
{
    fman_reset(dev);
}

static void fman_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    FMANState *s = FMAN(obj);
    SysBusDevice *dev;
    int i;

    memory_region_init(&s->container, obj, "fman", 0x100000);

    /* Map the IOs */
    memory_region_init_io(&s->iomem, obj, &fman_ops, obj, "fman_io",
                          0x100000);
    memory_region_add_subregion_overlap(&s->container, 0, &s->iomem, -1);

    /* Map the Internal Memory */
    memory_region_init_ram(&s->internal_mem, obj, "internal_memory",
                           FMAN_INTERNAL_MEM_SZ, &error_fatal);
    memory_region_add_subregion(&s->container, 0, &s->internal_mem);

    /* Create and Map MEMACs */
    for (i = 0; i < 10; i++) {
        dev = SYS_BUS_DEVICE(object_new("memac"));
        memory_region_add_subregion(&s->container, FMAN_EMACx_OFFSET(i),
                                    sysbus_mmio_get_region(dev, 0));
        memory_region_add_subregion(&s->container, FMAN_EMACx_MDIO_OFFSET(i),
                                    sysbus_mmio_get_region(dev, 1));
        sysbus_realize_and_unref(dev, &error_fatal);
    }

    sysbus_init_mmio(sbd, &s->container);
    sysbus_init_irq(sbd, &s->irq);
}

static void fman_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = fman_realize;
    dc->reset = fman_reset;
}

static const TypeInfo fman_info = {
    .name          = TYPE_FMAN,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FMANState),
    .instance_init = fman_init,
    .class_init    = fman_class_init,
};

static void fman_register_types(void)
{
    type_register_static(&fman_info);
}

type_init(fman_register_types)
