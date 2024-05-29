/*
 * STM32 PWR Emulator
 *
 * Copyright (c) 2023-2023 AdaCore
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
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"

#define TYPE_STM32F2XX_PWR "stm32f2xx-pwr"
OBJECT_DECLARE_SIMPLE_TYPE(STM32F2XXPwrState, STM32F2XX_PWR)

struct STM32F2XXPwrState{
    SysBusDevice busdev;
    MemoryRegion iomem;
};

static uint64_t stm32f2xx_pwr_read(void *opaque, hwaddr offset, unsigned size)
{
    /* Just statically set the flags expected by the run-time */

    switch (offset) {
    case 0x4: /* CSR */
        return 1 << 14; /* VOSRDY */
    default:
        return 0;
    }
}

static void stm32f2xx_pwr_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    /* Nothing to do here */
}

static const MemoryRegionOps stm32f2xx_pwr_ops = {
    .read = stm32f2xx_pwr_read,
    .write = stm32f2xx_pwr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void stm32f2xx_pwr_realize(DeviceState *dev, Error **errp)
{
    STM32F2XXPwrState *uart = STM32F2XX_PWR(dev);

    memory_region_init_io(&uart->iomem, OBJECT(dev), &stm32f2xx_pwr_ops, uart,
                          TYPE_STM32F2XX_PWR, 0x8);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &uart->iomem);
}

static void stm32f2xx_pwr_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32f2xx_pwr_realize;
}

static TypeInfo stm32f2xx_pwr_info = {
    .name          = TYPE_STM32F2XX_PWR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F2XXPwrState),
    .class_init    = stm32f2xx_pwr_class_init,
};

static void stm32f2xx_pwr_register_types(void)
{
    type_register_static(&stm32f2xx_pwr_info);
}

type_init(stm32f2xx_pwr_register_types)
