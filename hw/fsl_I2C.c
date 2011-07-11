/*
 * QEMU Freescale fsl_I2C Emulator
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

#include "fsl_I2C.h"

//#define DEBUG_REGISTER

typedef struct fsl_I2C_Register_Definition {
    uint32_t offset;
    const char *name;
    const char *desc;
    uint32_t access;
    uint32_t reset;
} fsl_I2C_Register_Definition;

typedef struct fsl_I2C_Register {
    const char *name;
    const char *desc;
    uint32_t    access;
    uint32_t    value;
} fsl_I2C_Register;

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

const fsl_I2C_Register_Definition fsl_I2C_registers_def[] = {
{0x000, "I2CADR",   "I2C address register",                      ACC_RW, 0x00},
{0x004, "I2CFDR",   "I2C frequency divider register",            ACC_RW, 0x00},
{0x008, "I2CCR",    "I2C control register",                      ACC_RW, 0x00},
{0x00C, "I2CSR",    "I2C status register",                       ACC_RW, 0x81},
{0x010, "I2CDR",    "I2C data register",                         ACC_RW, 0x00},
{0x014, "I2CDFSRR", "I2C digital filter sampling rate register", ACC_RW, 0x10},
{0x0, NULL, NULL, 0x0, 0x0},
};

#define REG_NUMBER 6

typedef struct fsl_I2C {
    SysBusDevice busdev;

    MemoryRegion io_area;

    fsl_I2C_Register regs[REG_NUMBER];

    /* IRQ */
    qemu_irq     irq;

} fsl_I2C;

static uint64_t fsl_i2c_read(void *opaque, target_phys_addr_t addr,
                             unsigned size)
{
    fsl_I2C          *fsl_i2c   = opaque;
    uint32_t          reg_index = (addr & 0xff) / 4;
    fsl_I2C_Register *reg       = NULL;
    uint32_t          ret       = 0x0;

    assert (reg_index < REG_NUMBER);

    reg = &fsl_i2c->regs[reg_index];

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
    printf("Read  0x%08x @ 0x" TARGET_FMT_plx"                             : %s (%s)\n",
           ret, addr, reg->name, reg->desc);
#endif
    return ret;
}

static void fsl_i2c_write(void *opaque, target_phys_addr_t addr,
                          uint64_t value, unsigned size)
{
    fsl_I2C          *fsl_i2c   = opaque;
    uint32_t          reg_index = (addr & 0xff) / 4;
    fsl_I2C_Register *reg       = NULL;
    uint32_t          before    = 0x0;

    assert (reg_index < REG_NUMBER);

    reg = &fsl_i2c->regs[reg_index];
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
           (unsigned int)value, addr, before, reg->value, reg->name, reg->desc);
#endif
}

static const MemoryRegionOps fsl_i2c_ops = {
    .read = fsl_i2c_read,
    .write = fsl_i2c_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void fsl_i2c_reset(DeviceState *d)
{
    fsl_I2C *fsl_i2c = container_of(d, fsl_I2C, busdev.qdev);
    int i = 0;
    int reg_index = 0;

    /* Default value for all registers */
    for (i = 0; i < REG_NUMBER; i++) {
        fsl_i2c->regs[i].name   = "Reserved";
        fsl_i2c->regs[i].desc   = "";
        fsl_i2c->regs[i].access = ACC_UNKNOWN;
        fsl_i2c->regs[i].value  = 0x00000000;
    }

    /* Set-up known registers */
    for (i = 0; fsl_I2C_registers_def[i].name != NULL; i++) {

        reg_index = fsl_I2C_registers_def[i].offset / 4;

        fsl_i2c->regs[reg_index].name   = fsl_I2C_registers_def[i].name;
        fsl_i2c->regs[reg_index].desc   = fsl_I2C_registers_def[i].desc;
        fsl_i2c->regs[reg_index].access = fsl_I2C_registers_def[i].access;
        fsl_i2c->regs[reg_index].value  = fsl_I2C_registers_def[i].reset;
    }
}

static int fsl_i2c_init(SysBusDevice *dev)
{
    fsl_I2C *fsl_i2c = FROM_SYSBUS(typeof(*fsl_i2c), dev);

    memory_region_init_io(&fsl_i2c->io_area, &fsl_i2c_ops,
                          fsl_i2c, "FSL I2C", 0x18);

    sysbus_init_mmio(dev, &fsl_i2c->io_area);

    sysbus_init_irq(dev, &fsl_i2c->irq);

    return 0;
}

static void fsl_i2c_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = fsl_i2c_init;
    dc->reset = fsl_i2c_reset;
}

static TypeInfo fsl_i2c_info = {
    .name          = "fsl_I2C",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(fsl_I2C),
    .class_init    = fsl_i2c_class_init,
};

static void fsl_i2c_register_types(void)
{
    type_register_static(&fsl_i2c_info);
}

type_init(fsl_i2c_register_types)

DeviceState *fsl_i2c_create(target_phys_addr_t  base,
                            MemoryRegion       *mr,
                            qemu_irq            irq)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "fsl_I2C");

    if (qdev_init(dev)) {
        return NULL;
    }

    sysbus_connect_irq(sysbus_from_qdev(dev), 0, irq);
    memory_region_add_subregion(mr, base,
                                sysbus_from_qdev(dev)->mmio[0].memory);

    return dev;
}
