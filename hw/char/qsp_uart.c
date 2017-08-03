/*
 * qsp_uart.c
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
#include "chardev/char-fe.h"
#include "qemu/fifo8.h"

#define QSP_UART_R_MAX (0x14/4)

#define TYPE_QSP_UART "qsp-uart"
#define QSP_UART(obj) OBJECT_CHECK(QSPUARTState, (obj), \
                                       TYPE_QSP_UART)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t reg[QSP_UART_R_MAX];
    CharBackend chr;
    qemu_irq irq;
    QEMUTimer *fifo_trigger_handle;

    Fifo8 rx_fifo;
} QSPUARTState;

#ifdef QSPI_UART_DEBUG
#define DB_PRINT(...) do { \
        fprintf(stderr,  "0x%8.8x: %s: ", (unsigned)s->iomem.addr, __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0);
#else
    #define DB_PRINT(...)
#endif

static void qsp_uart_update_irq(QSPUARTState *s)
{
    int level;

    level = s->reg[0x01] & s->reg[0x02] & 0x2;
    level |= s->reg[0x02] & 0x1;

    if (level) {
        qemu_irq_pulse(s->irq);
    }
}

static int uart_can_receive(void *opaque)
{
    QSPUARTState *s = QSP_UART(opaque);

    return !fifo8_is_full(&s->rx_fifo);
}

static void uart_receive(void *opaque, const uint8_t *buf, int size)
{
    QSPUARTState *s = QSP_UART(opaque);

    fifo8_push_all(&s->rx_fifo, buf, size);
    s->reg[0x01] |= 0x2;
    qsp_uart_update_irq(s);
}

static void uart_event(void *opaque, int event)
{
}

static void uart_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    QSPUARTState *s = QSP_UART(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset, (unsigned)value);

    offset >>= 2;
    assert(offset < QSP_UART_R_MAX);
    assert(size == 4);

    switch (offset) {
    case 0x00:
    case 0x01:
        /* RO values */
        break;
    case 0x03:
    {
        unsigned char ch;

        ch = value;
        qemu_chr_fe_write_all(&s->chr, &ch, 1);
        qsp_uart_update_irq(s);
    }
    break;
    case 0x2:
        s->reg[offset] = value;
        qsp_uart_update_irq(s);
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t uart_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    QSPUARTState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < QSP_UART_R_MAX);
    assert(size == 4);

    switch (offset) {
    case 0x04:
        val = fifo8_pop(&s->rx_fifo) & 0xFF;
        if (fifo8_is_empty(&s->rx_fifo)) {
            s->reg[0x1] &= 0x1;
            qsp_uart_update_irq(s);
        }
        break;
    default:
        val = s->reg[offset];
        break;
    }

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)(offset << 2),
             (unsigned)val);
    return val;
}

static const MemoryRegionOps uart_ops = {
    .read = uart_read,
    .write = uart_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void qsp_uart_reset(DeviceState *dev)
{
    QSPUARTState *s = QSP_UART(dev);

    DB_PRINT(" reset..\n");
    memset(s->reg, 0, sizeof(s->reg));
    s->reg[0x00] = 0x12340002;
    s->reg[0x01] = 0x00000001; /* Can take data */
    fifo8_reset(&s->rx_fifo);
}

static void qsp_uart_realize(DeviceState *dev, Error **errp)
{
    QSPUARTState *s = QSP_UART(dev);

    qemu_chr_fe_set_handlers(&s->chr, uart_can_receive, uart_receive,
                             uart_event, NULL, s, NULL, true);
}

static void qsp_uart_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    QSPUARTState *s = QSP_UART(obj);

    memory_region_init_io(&s->iomem, obj, &uart_ops, s, "qsp-uart", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    fifo8_create(&s->rx_fifo, 1024);
}

static Property qsp_uart_properties[] = {
    DEFINE_PROP_CHR("chardev", QSPUARTState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void qsp_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = qsp_uart_realize;
    dc->reset = qsp_uart_reset;
    dc->props = qsp_uart_properties;
}

static const TypeInfo qsp_uart_info = {
    .name          = TYPE_QSP_UART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QSPUARTState),
    .instance_init = qsp_uart_init,
    .class_init    = qsp_uart_class_init,
};

static void qsp_uart_register_types(void)
{
    type_register_static(&qsp_uart_info);
}

type_init(qsp_uart_register_types)
