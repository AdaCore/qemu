/*
 * QEMU STM32 UART Emulator
 *
 * Copyright (c) 2013 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#include "trace.h"

#define SR_RXNE (1 << 5)
#define SR_TC   (1 << 6)
#define SR_TXE  (1 << 7)

#define CR1_TXEIE (1 << 7)
#define CR1_RXNEIE (1 << 5)

#define FIFO_LENGTH 1024

typedef struct {
    SysBusDevice busdev;
    qemu_irq irq;
    MemoryRegion iomem;

    CharBackend chr;

    uint32_t status;
    uint32_t control;

    /* FIFO */
    char buffer[FIFO_LENGTH];
    int  len;
    int  current;

} stm32_UART_state;

#define TYPE_STM32_UART "stm32_UART"
#define STM32_UART(obj) \
    OBJECT_CHECK(stm32_UART_state, (obj), TYPE_STM32_UART)

static int uart_data_to_read(stm32_UART_state *uart)
{
    return uart->current < uart->len;
}

static char uart_pop(stm32_UART_state *uart)
{
    char ret;

    if (uart->len == 0) {
        uart->status &= ~SR_RXNE;
        return 0;
    }

    ret = uart->buffer[uart->current++];

    if (uart->current >= uart->len) {
        /* Flush */
        uart->len     = 0;
        uart->current = 0;
    }

    if (!uart_data_to_read(uart)) {
        uart->status &= ~SR_RXNE;
    }

    return ret;
}

static void uart_add_to_fifo(stm32_UART_state *uart,
                             const uint8_t    *buffer,
                             int               length)
{
    if (uart->len + length > FIFO_LENGTH) {
        abort();
    }

    memcpy(uart->buffer + uart->len, buffer, length);
    uart->len += length;
}

static int stm32_UART_can_receive(void *opaque)
{
    stm32_UART_state *uart = (stm32_UART_state *)opaque;

    return FIFO_LENGTH - uart->len;
}

static void stm32_UART_receive(void *opaque, const uint8_t *buf, int size)
{
    stm32_UART_state *uart = (stm32_UART_state *)opaque;

    uart_add_to_fifo(uart, buf, size);
    uart->status |= SR_RXNE;

    if (uart->control & CR1_RXNEIE) {
        qemu_irq_pulse(uart->irq);
    }
}

static void stm32_UART_event(void *opaque, int event)
{
    /* Nothing */
}

static uint64_t stm32_UART_read(void *opaque, hwaddr offset, unsigned size)
{
    stm32_UART_state *uart = (stm32_UART_state *)opaque;

    switch (offset) {
    case 0x00: /* Status Register */
        return uart->status;
    case 0x04: /* Data Register */
        return uart_pop(uart);
    case 0x0C: /* Data Register */
        return uart->control;
    default:
        return 0;
    }
}

static void stm32_UART_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    stm32_UART_state *uart = (stm32_UART_state *)opaque;
    unsigned char     c    = 0;

    switch (offset) {
    case 0x00: /* Status Register */
        break;
    case 0x04: /* Data Register */
        c = value & 0xFF;
        qemu_chr_fe_write(&uart->chr, &c, 1);

        /* Generate interrupt */
        if (uart->control & CR1_TXEIE) {
            qemu_irq_pulse(uart->irq);
        }

        break;
    case 0x0C: /* Data Register */
        uart->control = value;
        break;
    }
}

static const MemoryRegionOps stm32_UART_ops = {
    .read = stm32_UART_read,
    .write = stm32_UART_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 2,
        .max_access_size = 2,
    },
};

static void stm32_UART_realize(DeviceState *dev, Error **errp)
{
    stm32_UART_state *uart = STM32_UART(dev);

    qemu_chr_fe_set_handlers(&uart->chr,
                             stm32_UART_can_receive,
                             stm32_UART_receive,
                             stm32_UART_event,
                             NULL, uart, NULL, true);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &uart->irq);

    memory_region_init_io(&uart->iomem, OBJECT(dev), &stm32_UART_ops, uart,
                          TYPE_STM32_UART, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &uart->iomem);
}

static void stm32_UART_reset(DeviceState *d)
{
    stm32_UART_state *uart = STM32_UART(d);

    uart->status =  SR_TC | SR_TXE;

    /* Flush receive FIFO */
    uart->len = 0;
    uart->current = 0;
}

static Property stm32_UART_properties[] = {
    DEFINE_PROP_CHR("chrdev", stm32_UART_state, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32_UART_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32_UART_realize;
    dc->reset = stm32_UART_reset;
    dc->props = stm32_UART_properties;
}

static TypeInfo stm32_UART_info = {
    .name          = TYPE_STM32_UART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(stm32_UART_state),
    .class_init    = stm32_UART_class_init,
};

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
} stm32_RCC_state;

#define TYPE_STM32_RCC "stm32_RCC"
#define STM32_RCC(obj) \
    OBJECT_CHECK(stm32_RCC_state, (obj), TYPE_STM32_RCC)

static uint64_t stm32_RCC_read(void *opaque, hwaddr offset, unsigned size)
{
    /* Just statically set the flags expected by the run-time */

    switch (offset) {
    case 0x0: /* CR */
        return (1 << 1) | (1 << 17) | (1 << 25); /* HSIRDY | HSERDY | PLLRDY */
    case 0x8: /* CFGR */
        return 1 << 3; /* SWS1 */
    case 0x74: /* CSR */
        return 1 << 1; /* LSIRDY */
    default:
        return 0;
    }
}

static void stm32_RCC_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    /* Nothing to do here */
}

static const MemoryRegionOps stm32_RCC_ops = {
    .read = stm32_RCC_read,
    .write = stm32_RCC_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void stm32_RCC_realize(DeviceState *dev, Error **errp)
{
    stm32_RCC_state *uart = STM32_RCC(dev);

    memory_region_init_io(&uart->iomem, OBJECT(dev), &stm32_RCC_ops, uart,
                          TYPE_STM32_RCC, 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &uart->iomem);
}

static void stm32_RCC_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32_RCC_realize;
}

static TypeInfo stm32_RCC_info = {
    .name          = TYPE_STM32_RCC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(stm32_RCC_state),
    .class_init    = stm32_RCC_class_init,
};

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
} stm32_PWR_state;

#define TYPE_STM32_PWR "stm32_PWR"
#define STM32_PWR(obj) \
    OBJECT_CHECK(stm32_PWR_state, (obj), TYPE_STM32_PWR)


static uint64_t stm32_PWR_read(void *opaque, hwaddr offset, unsigned size)
{
    /* Just statically set the flags expected by the run-time */

    switch (offset) {
    case 0x4: /* CSR */
        return 1 << 14; /* VOSRDY */
    default:
        return 0;
    }
}

static void stm32_PWR_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    /* Nothing to do here */
}

static const MemoryRegionOps stm32_PWR_ops = {
    .read = stm32_PWR_read,
    .write = stm32_PWR_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void stm32_PWR_realize(DeviceState *dev, Error **errp)
{
    stm32_PWR_state *uart = STM32_PWR(dev);

    memory_region_init_io(&uart->iomem, OBJECT(dev), &stm32_PWR_ops, uart,
                          TYPE_STM32_PWR, 0x8);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &uart->iomem);
}

static void stm32_PWR_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32_PWR_realize;
}

static TypeInfo stm32_PWR_info = {
    .name          = TYPE_STM32_PWR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(stm32_PWR_state),
    .class_init    = stm32_PWR_class_init,
};

static void stm32_UART_register_types(void)
{
    type_register_static(&stm32_UART_info);
    type_register_static(&stm32_RCC_info);
    type_register_static(&stm32_PWR_info);
}

type_init(stm32_UART_register_types)
