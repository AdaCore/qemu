/*
 * qsp_timer.c
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
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"

#define QSP_TIMER_R_MAX (0x10 >> 2)

#define TYPE_QSP_TIMER "qsp-timer"
#define QSP_TIMER(obj) OBJECT_CHECK(QSPTIMERState, (obj), \
                                    TYPE_QSP_TIMER)

#define QSP_TIMER_ID_OFFSET      (0x0000 >> 2)
#define QSP_TIMER_FREQ_OFFSET    (0x0004 >> 2)
#define QSP_TIMER_COUNT_OFFSET   (0x0008 >> 2)
#define QSP_TIMER_ONESHOT_OFFSET (0x000C >> 2)
#define QSP_TIMER_PERIOD_OFFSET  (0x0010 >> 2)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t reg[QSP_TIMER_R_MAX];
    qemu_irq irq;
} QSPTIMERState;

#ifdef QSP_TIMER_DEBUG
#define DB_PRINT(...) do { \
        fprintf(stderr,  "0x%8.8x: %s: ", (unsigned)s->iomem.addr, __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0);
#else
    #define DB_PRINT(...)
#endif

static void timer_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    QSPTIMERState *s = QSP_TIMER(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset, (unsigned)value);
    offset >>= 2;
    assert(offset < QSP_TIMER_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_TIMER_ID_OFFSET:
    case QSP_TIMER_FREQ_OFFSET:
        /* READ ONLY */
        abort();
        break;
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t timer_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    QSPTIMERState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < QSP_TIMER_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_TIMER_ONESHOT_OFFSET:
    case QSP_TIMER_PERIOD_OFFSET:
        /* WRITE ONLY */
        abort();
        break;
    default:
        val = s->reg[offset];
        break;
    }

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)(offset << 2),
             (unsigned)val);
    return val;
}

static const MemoryRegionOps timer_ops = {
    .read = timer_read,
    .write = timer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void qsp_timer_reset(DeviceState *dev)
{
    QSPTIMERState *s = QSP_TIMER(dev);

    DB_PRINT(" reset..\n");
    memset(s->reg, 0, sizeof(s->reg));
    s->reg[QSP_TIMER_ID_OFFSET] = 0x12340007;
    s->reg[QSP_TIMER_FREQ_OFFSET] = 1000000;
}

static void qsp_timer_realize(DeviceState *dev, Error **errp)
{
//    QSPTIMERState *s = QSP_TIMER(dev);
}

static void qsp_timer_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    QSPTIMERState *s = QSP_TIMER(obj);

    memory_region_init_io(&s->iomem, obj, &timer_ops, s, "qsp-timer", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void qsp_timer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = qsp_timer_realize;
    dc->reset = qsp_timer_reset;
}

static const TypeInfo qsp_timer_info = {
    .name          = TYPE_QSP_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QSPTIMERState),
    .instance_init = qsp_timer_init,
    .class_init    = qsp_timer_class_init,
};

static void qsp_timer_register_types(void)
{
    type_register_static(&qsp_timer_info);
}

type_init(qsp_timer_register_types)
