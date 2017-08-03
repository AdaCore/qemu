/*
 * qsp_pic.c
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
#include "qom/cpu.h"

#define QSP_PIC_MAX_IRQ        (1024)
#define QSP_PIC_MAX_CPU        (128)
#define QSP_PIC_NO_IRQ_PENDING (0x401)
#define QSP_PIC_R_MAX          (0x24 >> 2)

#define TYPE_QSP_PIC "qsp-pic"
#define QSP_PIC(obj) OBJECT_CHECK(QSPPICState, (obj), \
                                    TYPE_QSP_PIC)

#define QSP_PIC_ID_OFFSET      (0x0000 >> 2)
#define QSP_PIC_ENABLE_OFFSET  (0x0004 >> 2)
#define QSP_PIC_DISABLE_OFFSET (0x0008 >> 2)
#define QSP_PIC_DST_OFFSET     (0x0010 >> 2)
#define QSP_PIC_GEN_OFFSET     (0x0014 >> 2)
#define QSP_PIC_TYPE_OFFSET    (0x0018 >> 2)
#define QSP_PIC_PENDING_OFFSET (0x0020 >> 2)
#define QSP_PIC_EOI_OFFSET     (0x0024 >> 2)

typedef enum {
    QSP_PIC_EDGE = 1,
    QSP_PIC_LEVEL = 0
} QSPPICType;

typedef struct {
    bool enabled;
    int level;
    QSPPICType type;
    int cpu;
} QSPPICIRQ;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t reg[QSP_PIC_R_MAX];
    int cpu_pending[QSP_PIC_MAX_CPU];
    qemu_irq out[QSP_PIC_MAX_CPU];
    QSPPICIRQ irq[QSP_PIC_MAX_IRQ];
} QSPPICState;

#ifndef QSP_PIC_DEBUG
#define QSP_PIC_DEBUG 0
#endif

#define DB_PRINT(...) do { \
    if (QSP_PIC_DEBUG) { \
        fprintf(stderr,  "0x%8.8x: %s: ", (unsigned)s->iomem.addr, __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } \
    } while (0);

static void qsp_pic_dump_irqs(QSPPICState *s, bool only_enabled)
{
    int i;

    printf("IRQ#   type   enabled\n");
    for (i = 0; i < QSP_PIC_MAX_IRQ; i++) {
        if (!only_enabled || s->irq[i].enabled) {
            printf("%4.4d   %s  %s\n", i,
                   s->irq[i].type == QSP_PIC_EDGE ? "EDGE " : "LEVEL",
                   s->irq[i].enabled ? "yes" : "no");
        }
    }
}

static void qsp_pic_update_irq(QSPPICState *s)
{
    int cpu_level[QSP_PIC_MAX_CPU];
    int i;

    memset(cpu_level, 0, sizeof(cpu_level));

    for (i = 0; i < QSP_PIC_MAX_CPU; i++) {
        s->cpu_pending[i] = QSP_PIC_NO_IRQ_PENDING;
    }

    for (i = 0; i < QSP_PIC_MAX_IRQ; i++) {
        if ((s->irq[i].level) && (!cpu_level[s->irq[i].cpu])) {
            s->cpu_pending[s->irq[i].cpu] = i;
            cpu_level[s->irq[i].cpu] = s->irq[i].level;
        }
    }

    for (i = 0; i < QSP_PIC_MAX_CPU; i++) {
        qemu_set_irq(s->out[i], cpu_level[i]);
    }
}

static void qsp_pic_change_irq_type(QSPPICState *s, uint32_t irq_n,
                                    QSPPICType type)
{
    QSPPICType old;

    assert(s);
    assert((type == QSP_PIC_EDGE) || (type == QSP_PIC_LEVEL));

    old = s->irq[irq_n].type;
    s->irq[irq_n].type = type;

    DB_PRINT(" changed irq#%d type from %s to %s\n", irq_n,
             old == QSP_PIC_EDGE ? "EDGE TRIGERED" : "LEVEL TRIGERED",
             type == QSP_PIC_EDGE ? "EDGE TRIGERED" : "LEVEL TRIGERED");

    if (QSP_PIC_DEBUG) {
        qsp_pic_dump_irqs(s, true);
    }
}

static void qsp_pic_enable_irq(QSPPICState *s, uint32_t irq_n,
                               bool enabled)
{
    bool old;

    assert(s);
    old = s->irq[irq_n].enabled;
    s->irq[irq_n].enabled = enabled;

    if (old != enabled) {
    /* Update IRQ */
        DB_PRINT(" changed irq#%d state from %s to %s\n", irq_n,
                 old ? "enabled" : "disabled",
                 enabled ? "enabled" : "disabled");
    }

    if (!enabled) {
        s->irq[irq_n].level = 0;
        qsp_pic_update_irq(s);
    }
}

static void qsp_pic_ack_irq(QSPPICState *s, uint32_t irq_n)
{
    s->irq[irq_n].level = 0;
    qsp_pic_update_irq(s);
}

static void pic_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    QSPPICState *s = QSP_PIC(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset,
             (unsigned)value);
    offset >>= 2;
    assert(offset <= QSP_PIC_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_PIC_ID_OFFSET:
    case QSP_PIC_PENDING_OFFSET:
        /* READ ONLY */
        abort();
        break;
    case QSP_PIC_GEN_OFFSET:
        printf("unimp\n");
        abort();
        break;
    case QSP_PIC_TYPE_OFFSET:
        qsp_pic_change_irq_type(s, extract32(value, 0, 16),
                                   extract32(value, 16, 1));
        break;
    case QSP_PIC_ENABLE_OFFSET:
        qsp_pic_enable_irq(s, extract32(value, 0, 16), true);
        break;
    case QSP_PIC_DISABLE_OFFSET:
        qsp_pic_enable_irq(s, extract32(value, 0, 16), false);
        break;
    case QSP_PIC_EOI_OFFSET:
        qsp_pic_ack_irq(s, extract32(value, 0, 32));
        break;
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t pic_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    QSPPICState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset <= QSP_PIC_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_PIC_ENABLE_OFFSET:
    case QSP_PIC_DISABLE_OFFSET:
    case QSP_PIC_DST_OFFSET:
    case QSP_PIC_GEN_OFFSET:
    case QSP_PIC_EOI_OFFSET:
    case QSP_PIC_TYPE_OFFSET:
        /* WRITE ONLY */
        abort();
        break;
    case QSP_PIC_PENDING_OFFSET:
        assert(current_cpu);
        val = s->cpu_pending[current_cpu->cpu_index];
        break;
    default:
        val = s->reg[offset];
        break;
    }

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)(offset << 2),
             (unsigned)val);
    return val;
}

static const MemoryRegionOps pic_ops = {
    .read = pic_read,
    .write = pic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void qsp_pic_reset(DeviceState *dev)
{
    QSPPICState *s = QSP_PIC(dev);
    int i;

    DB_PRINT(" reset..\n");
    memset(s->irq, 0, sizeof(s->irq));
    memset(s->reg, 0, sizeof(s->reg));
    s->reg[QSP_PIC_ID_OFFSET] = 0x12340001;

    for (i = 0; i < QSP_PIC_MAX_CPU; i++) {
        s->cpu_pending[i] = QSP_PIC_NO_IRQ_PENDING;
    }

    if (QSP_PIC_DEBUG) {
        qsp_pic_dump_irqs(s, false);
    }
}

static void qsp_pic_realize(DeviceState *dev, Error **errp)
{
}

static void qsp_pic_set_irq(void *opaque, int irq, int level)
{
    QSPPICState *s = QSP_PIC(opaque);

    if (!s->irq[irq].enabled) {
        return;
    }

    switch (s->irq[irq].type) {
    case QSP_PIC_EDGE:
        if (!s->irq[irq].level) {
            if (level) {
                s->irq[irq].level = level;
            }
        }
        break;
    case QSP_PIC_LEVEL:
        s->irq[irq].level = level;
        break;
    default:
        abort();
        break;
    }

    qsp_pic_update_irq(s);
}

static void qsp_pic_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    QSPPICState *s = QSP_PIC(obj);
    int i;

    memory_region_init_io(&s->iomem, obj, &pic_ops, s, "qsp-pic",
                          0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    qdev_init_gpio_in(DEVICE(obj), qsp_pic_set_irq, QSP_PIC_MAX_IRQ);

    for (i = 0; i < QSP_PIC_MAX_CPU; i++) {
        sysbus_init_irq(sbd, &s->out[i]);
    }
}

static void qsp_pic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = qsp_pic_realize;
    dc->reset = qsp_pic_reset;
}

static const TypeInfo qsp_pic_info = {
    .name          = TYPE_QSP_PIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QSPPICState),
    .instance_init = qsp_pic_init,
    .class_init    = qsp_pic_class_init,
};

static void qsp_pic_register_types(void)
{
    type_register_static(&qsp_pic_info);
}

type_init(qsp_pic_register_types)
