/*
 * qsp_mac.c
 *
 * Copyright (c) 2017 AdaCore
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
#include "qemu/timer.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"

#define QSP_MAC_R_MAX (0x100 / 4)

#define TYPE_QSP_MAC "qsp-mac"
#define QSP_MAC(obj) OBJECT_CHECK(QSPMACState, (obj), \
                                     TYPE_QSP_MAC)

#define QSP_MAC_ID_OFFSET           (0x0000 >> 2)
#define QSP_MAC_STATUS_OFFSET       (0x0004 >> 2)
#define QSP_MAC_CONTROL_OFFSET      (0x0008 >> 2)
#define QSP_MAC_TX_DESC_OFFSET      (0x0010 >> 2)
#define QSP_MAC_TX_SZ_OFFSET        (0x0014 >> 2)
#define QSP_MAC_TX_CI_OFFSET        (0x0018 >> 2)
#define QSP_MAC_TX_PI_OFFSET        (0x001C >> 2)
#define QSP_MAC_RX_DESC_OFFSET      (0x0020 >> 2)
#define QSP_MAC_RX_SZ_OFFSET        (0x0024 >> 2)
#define QSP_MAC_RX_CI_OFFSET        (0x0028 >> 2)
#define QSP_MAC_RX_PI_OFFSET        (0x002C >> 2)
#define QSP_MAC_MAC_ADDR_OFFSET(n) ((0x0030 >> 2) + (n))
#define QSP_MAC_MDIO_OFFSET         (0x0100 >> 2)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t reg[QSP_MAC_R_MAX];
    qemu_irq irq;
} QSPMACState;

#ifdef QSP_MAC_DEBUG
#define DB_PRINT(...) do { \
        fprintf(stderr,  "0x%8.8x: %s: ", (unsigned)s->iomem.addr, __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0);
#else
    #define DB_PRINT(...)
#endif

static void mac_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    QSPMACState *s = QSP_MAC(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset, (unsigned)value);
    offset >>= 2;
    assert(offset < QSP_MAC_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_MAC_ID_OFFSET:
        /* READ ONLY */
        abort();
        break;
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t mac_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    QSPMACState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < QSP_MAC_R_MAX);
    assert(size == 4);

    switch (offset) {
    default:
        val = s->reg[offset];
        break;
    }

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)(offset << 2),
             (unsigned)val);
    return val;
}

static const MemoryRegionOps mac_ops = {
    .read = mac_read,
    .write = mac_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void qsp_mac_reset(DeviceState *dev)
{
    QSPMACState *s = QSP_MAC(dev);

    DB_PRINT(" reset..\n");
    memset(s->reg, 0, sizeof(s->reg));
    s->reg[QSP_MAC_ID_OFFSET] = 0x12340006;
}

static void qsp_mac_realize(DeviceState *dev, Error **errp)
{
//    QSPMACState *s = QSP_MAC(dev);
}

static void qsp_mac_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    QSPMACState *s = QSP_MAC(obj);

    memory_region_init_io(&s->iomem, obj, &mac_ops, s, "qsp-mac", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void qsp_mac_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = qsp_mac_realize;
    dc->reset = qsp_mac_reset;
}

static const TypeInfo qsp_mac_info = {
    .name          = TYPE_QSP_MAC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QSPMACState),
    .instance_init = qsp_mac_init,
    .class_init    = qsp_mac_class_init,
};

static void qsp_mac_register_types(void)
{
    type_register_static(&qsp_mac_info);
}

type_init(qsp_mac_register_types)
