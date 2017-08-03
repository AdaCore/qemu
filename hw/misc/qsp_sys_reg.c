/*
 * qsp_sys_reg.c
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

#define QSP_SYSREG_R_MAX (0x110 / 4)

#define TYPE_QSP_SYSREG "qsp-sysreg"
#define QSP_SYSREG(obj) OBJECT_CHECK(QSPSYSREGState, (obj), \
                                     TYPE_QSP_SYSREG)

#define QSP_SYSREG_ID_OFFSET            (0x0000 >> 2)
#define QSP_SYSREG_ENABLE_OFFSET        (0x0004 >> 2)
#define QSP_SYSREG_DISABLE_OFFSET       (0x0008 >> 2)
#define QSP_SYSREG_BOOT_PC_OFFSET       (0x000C >> 2)
#define QSP_SYSREG_CPU_ID_OFFSET        (0x0010 >> 2)
#define QSP_SYSREG_CPU_STATUS_OFFSET    (0x0014 >> 2)
#define QSP_SYSREG_CPU_PROBE_OFFSET     (0x0018 >> 2)
#define QSP_SYSREG_MEM_AVAIL_OFFSET     (0x0020 >> 2)
#define QSP_SYSREG_MEM_START_OFFSET     (0x0024 >> 2)
#define QSP_SYSREG_CPU_FREQ_OFFSET      (0x0028 >> 2)
#define QSP_SYSREG_TIMEBASE_FREQ_OFFSET (0x002C >> 2)
#define QSP_SYSREG_REMAP_OFFSET         (0x0030 >> 2)
#define QSP_SYSREG_RELEASE_OFFSET       (0x0040 >> 2)
#define QSP_SYSREG_TAKE_TB_OFFSET       (0x0044 >> 2)
#define QSP_SYSREG_SRESET_OFFSET        (0x0048 >> 2)
#define QSP_SYSREG_BOOT_INFO_OFFSET(n)  ((0x0100 >> 2) + n)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t reg[QSP_SYSREG_R_MAX];
    qemu_irq irq;
} QSPSYSREGState;

#ifdef QSP_SYSREG_DEBUG
#define DB_PRINT(...) do { \
        fprintf(stderr,  "0x%8.8x: %s: ", (unsigned)s->iomem.addr, __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0);
#else
    #define DB_PRINT(...)
#endif

static void sysreg_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    QSPSYSREGState *s = QSP_SYSREG(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset, (unsigned)value);
    offset >>= 2;
    assert(offset < QSP_SYSREG_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_SYSREG_ID_OFFSET:
    case QSP_SYSREG_CPU_ID_OFFSET:
    case QSP_SYSREG_CPU_STATUS_OFFSET:
    case QSP_SYSREG_MEM_AVAIL_OFFSET:
    case QSP_SYSREG_MEM_START_OFFSET:
    case QSP_SYSREG_CPU_FREQ_OFFSET:
    case QSP_SYSREG_TIMEBASE_FREQ_OFFSET:
        /* READ ONLY */
        abort();
        break;
    case QSP_SYSREG_CPU_PROBE_OFFSET:
        /* Only CPU 0 is okay for now */
        s->reg[QSP_SYSREG_CPU_STATUS_OFFSET] =
            value ? 0 : 1;
        s->reg[offset] = value;
        break;
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t sysreg_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    QSPSYSREGState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < QSP_SYSREG_R_MAX);
    assert(size == 4);

    switch (offset) {
    case QSP_SYSREG_ENABLE_OFFSET:
    case QSP_SYSREG_DISABLE_OFFSET:
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

static const MemoryRegionOps sysreg_ops = {
    .read = sysreg_read,
    .write = sysreg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void qsp_sysreg_reset(DeviceState *dev)
{
    QSPSYSREGState *s = QSP_SYSREG(dev);

    DB_PRINT(" reset..\n");
    memset(s->reg, 0, sizeof(s->reg));
    s->reg[QSP_SYSREG_ID_OFFSET] = 0x12340008;
    s->reg[QSP_SYSREG_MEM_AVAIL_OFFSET] = 3328ULL * 1024 * 1024;
}

static void qsp_sysreg_realize(DeviceState *dev, Error **errp)
{
//    QSPSYSREGState *s = QSP_SYSREG(dev);
}

static void qsp_sysreg_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    QSPSYSREGState *s = QSP_SYSREG(obj);

    memory_region_init_io(&s->iomem, obj, &sysreg_ops, s, "qsp-sysreg", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void qsp_sysreg_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = qsp_sysreg_realize;
    dc->reset = qsp_sysreg_reset;
}

static const TypeInfo qsp_sysreg_info = {
    .name          = TYPE_QSP_SYSREG,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QSPSYSREGState),
    .instance_init = qsp_sysreg_init,
    .class_init    = qsp_sysreg_class_init,
};

static void qsp_sysreg_register_types(void)
{
    type_register_static(&qsp_sysreg_info);
}

type_init(qsp_sysreg_register_types)
