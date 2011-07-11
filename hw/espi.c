/*
 * QEMU Freescale eSPI Emulator
 *
 * Copyright (c) 2011 AdaCore
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

#include "sysemu.h"
#include "sysbus.h"
#include "trace.h"

#include "espi.h"

#define DEBUG_REGISTER

typedef struct eSPI_Register_Definition {
    uint32_t offset;
    const char *name;
    const char *desc;
    uint32_t access;
    uint32_t reset;
} eSPI_Register_Definition;

typedef struct eSPI_Register {
    const char *name;
    const char *desc;
    uint32_t    access;
    uint32_t    value;
} eSPI_Register;

#define ACC_RW      1           /* Read/Write */
#define ACC_RO      2           /* Read Only */
#define ACC_WO      3           /* Write Only */
#define ACC_w1c     4           /* Write 1 to clear */
#define ACC_UNKNOWN 4           /* Unknown register*/

/* Index of each register */
#define SPMODE  (0x000 / 4)
#define SPIE    (0x004 / 4)
#define SPIM    (0x008 / 4)
#define SPCOM   (0x00C / 4)
#define SPITF   (0x010 / 4)
#define SPIRF   (0x014 / 4)
#define SPMODE0 (0x020 / 4)
#define SPMODE1 (0x024 / 4)
#define SPMODE2 (0x028 / 4)

const eSPI_Register_Definition eSPI_registers_def[] = {
{0x000, "SPMODE",  "eSPI mode register",                 ACC_RW, 0x0000100F},
{0x004, "SPIE",    "eSPI event register",                ACC_RW, 0x00200000},
{0x008, "SPIM",    "eSPI mask register",                 ACC_RW, 0x00000000},
{0x00C, "SPCOM",   "eSPI command register",              ACC_WO, 0x00000000},
{0x010, "SPITF",   "eSPI transmit FIFO access register", ACC_WO, 0x00000000},
{0x014, "SPIRF",   "eSPI receive FIFO access register",  ACC_RO, 0x00000000},
{0x020, "SPMODE0", "eSPI CS0 mode register",             ACC_RW, 0x00100000},
{0x024, "SPMODE1", "eSPI CS1 mode register",             ACC_RW, 0x00100000},
{0x028, "SPMODE2", "eSPI CS2 mode register",             ACC_RW, 0x00100000},
{0x02C, "SPMODE3", "eSPI CS3 mode register",             ACC_RW, 0x00100000},
};

#define REG_NUMBER 12

typedef struct eSPI {
    SysBusDevice  busdev;

    eSPI_Register regs[REG_NUMBER];

    /* IRQ */
    qemu_irq     irq;

} eSPI;

uint32_t get_nip(void);

static uint32_t espi_readl(void *opaque, target_phys_addr_t addr)
{
    eSPI          *espi      = opaque;
    uint32_t       reg_index = addr / 4;
    eSPI_Register *reg       = NULL;
    uint32_t       ret       = 0x0;

    assert (reg_index < REG_NUMBER);

    reg = &espi->regs[reg_index];

    switch (reg->access) {
    case ACC_WO:
        ret = 0x00000000;
        break;

    case ACC_RW:
    case ACC_w1c:
    case ACC_RO:
    default:
        ret = reg->value;
        break;
    }

#ifdef DEBUG_REGISTER
    printf("Read  0x%08x @ 0x" TARGET_FMT_plx"                            : %s (%s)\n",
           ret, addr, reg->name, reg->desc);
#endif
    return ret;
}

static void
espi_writel(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    eSPI          *espi      = opaque;
    uint32_t       reg_index = addr / 4;
    eSPI_Register *reg       = NULL;
    uint32_t       before    = 0x0;

    assert (reg_index < REG_NUMBER);

    reg = &espi->regs[reg_index];
    before = reg->value;

    switch (reg_index) {
    default:
        /* Default handling */
        switch (reg->access) {

        case ACC_RW:
        case ACC_WO:
            reg->value = value;
            break;

        case ACC_w1c:
            reg->value &= ~value;
            break;

        case ACC_RO:
        default:
            /* Read         Only or Unknown register */
            break;
        }
    }

#ifdef DEBUG_REGISTER
    printf("Write 0x%08x @ 0x" TARGET_FMT_plx" val:0x%08x->0x%08x : %s (%s)\n",
           value, addr, before, reg->value, reg->name, reg->desc);
#endif
}

static CPUReadMemoryFunc * const espi_read[] = {
    NULL, NULL, espi_readl,
};

static CPUWriteMemoryFunc * const espi_write[] = {
    NULL, NULL, espi_writel,
};

static void espi_reset(DeviceState *d)
{
    eSPI *espi = container_of(d, eSPI, busdev.qdev);
    int i = 0;
    int reg_index = 0;

    /* Default value for all registers */
    for (i = 0; i < REG_NUMBER; i++) {
        espi->regs[i].name   = "Reserved";
        espi->regs[i].desc   = "";
        espi->regs[i].access = ACC_UNKNOWN;
        espi->regs[i].value  = 0x00000000;
    }

    /* Set-up known registers */
    for (i = 0; eSPI_registers_def[i].name != NULL; i++) {

        reg_index = eSPI_registers_def[i].offset / 4;

        espi->regs[reg_index].name   = eSPI_registers_def[i].name;
        espi->regs[reg_index].desc   = eSPI_registers_def[i].desc;
        espi->regs[reg_index].access = eSPI_registers_def[i].access;
        espi->regs[reg_index].value  = eSPI_registers_def[i].reset;
    }
}

static int espi_init(SysBusDevice *dev)
{
    eSPI *espi = FROM_SYSBUS(typeof(*espi), dev);
    int   regs;

    regs = cpu_register_io_memory(espi_read,
                                  espi_write,
                                  espi, DEVICE_NATIVE_ENDIAN);
    if (regs < 0) {
        return -1;
    }

    sysbus_init_mmio(dev, 0x30, regs);

    sysbus_init_irq(dev, &espi->irq);

    return 0;
}

static SysBusDeviceInfo espi_info = {
    .init       = espi_init,
    .qdev.name  = "eSPI",
    .qdev.desc  = "",
    .qdev.reset = espi_reset,
    .qdev.size  = sizeof(eSPI),
    .qdev.props = (Property[]) {
        DEFINE_PROP_END_OF_LIST(),
    }
};

static void espi_register(void)
{
    sysbus_register_withprop(&espi_info);
}

device_init(espi_register)

DeviceState *espi_create(target_phys_addr_t base,
                         qemu_irq           irq)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "eSPI");

    if (qdev_init(dev)) {
        return NULL;
    }

    sysbus_connect_irq(sysbus_from_qdev(dev), 0, irq);
    sysbus_mmio_map(sysbus_from_qdev(dev), 0, base);

    return dev;
}
