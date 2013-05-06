/*
 * Arm Texas-Instrument Vector Interrupt Manager (VIM)
 *
 * The parity related VIM registers are not implemented.
 *
 * Based on PL190 by Paul Brook.
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

#include "sysbus.h"

/* #define DEBUG_VIM */

#ifdef DEBUG_VIM
#define DPRINTF(fmt, ...) do { printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;

    uint8_t  IRQINDEX;
    uint8_t  FIQINDEX;
    uint8_t  last_IRQINDEX;
    uint8_t  last_FIQINDEX;

    /* The FIQ/IRQ program control registers determine whether a given interrupt
     * request will be either FIQ or IRQ.
     */
    uint32_t FIRQPR[3];

    /* The pending interrupt register gives the pending interrupt requests */
    uint32_t INTREQ[3];      /* Read-Only */

    /* The interrupt register enable selectively enables individual request
     * channels.
     *
     * The interrupt register enable selectively disables individual request
     * channels.
     */
    uint32_t ENA[3];

    /* Wakeup Interrupts are not implemented */
    uint32_t WAKEENA[3];

    /* The interrupt vector register gives the address of the enabled and active
     * IRQ interrupt.
     */
    uint32_t IRQVEC;

    /* The interrupt vector register gives the address of the enabled and active
     * FIQ interrupt.
     */
    uint32_t FIQVEC;

    /* Capture Event Sources are not implemented */

    /* These bits determine which interrupt request the priority channel CHANx0
     * maps to.
     */
    union {
        uint8_t  _8[96];
        uint32_t _32[24];
    } CHANMAP;

    qemu_irq irq;
    qemu_irq fiq;
} vim_state;

/* Update interrupts.  */
static void vim_update(vim_state *s)
{
    uint32_t IRQ, FIQ, index;
    int i;
    DPRINTF("%s: s->ENA:   0x%08x%08x%08x\n", __func__, s->ENA[2], s->ENA[1],
            s->ENA[0]);
    DPRINTF("%s: s->INTREQ:0x%08x%08x%08x\n", __func__, s->INTREQ[2],
            s->INTREQ[1], s->INTREQ[0]);
    DPRINTF("%s: s->FIRQPR:0x%08x%08x%08x\n", __func__, s->FIRQPR[2],
            s->FIRQPR[1], s->FIRQPR[0]);

    for (i = 0; i < 3; i++) {
        IRQ = s->INTREQ[i] & s->ENA[i] & ~s->FIRQPR[i];
        index = IRQ ? __builtin_ctzll(IRQ) + i * 32 + 1 : 0;

        if (index != 0) {
            break;
        }
    }
    s->IRQINDEX = index;

    DPRINTF("%s: s->last_IRQINDEX:%d s->IRQINDEX:%d\n", __func__,
            s->last_IRQINDEX, s->IRQINDEX);

    if (s->last_IRQINDEX != s->IRQINDEX) {
        s->last_IRQINDEX = s->IRQINDEX;

        /* Read in Interrupt Vector Table */
        cpu_physical_memory_read(0xFFF82000 + s->IRQINDEX * 4, &s->IRQVEC, 4);

#ifdef TARGET_WORDS_BIGENDIAN
        s->IRQVEC = bswap_32(s->IRQVEC);
#endif
        qemu_set_irq(s->irq, IRQ != 0);
        DPRINTF("%s: qemu_set_irq(s->irq, %d);\n", __func__, IRQ != 0);
    }

   for (i = 0; i < 3; i++) {
        FIQ = s->INTREQ[i] & s->ENA[i] & s->FIRQPR[i];
        index = FIQ ? __builtin_ctzll(FIQ) + i * 32 + 1 : 0;

        if (index != 0) {
            break;
        }
    }
    s->FIQINDEX = index;
    DPRINTF("%s: s->last_FIQINDEX:%d s->FIQINDEX:%d\n", __func__,
            s->last_FIQINDEX, s->FIQINDEX);

    if (s->last_FIQINDEX != s->FIQINDEX) {
        s->last_FIQINDEX = s->FIQINDEX;

        /* Read in Interrupt Vector Table */
        cpu_physical_memory_read(0xFFF82000 + s->FIQINDEX * 4, &s->FIQVEC, 4);

#ifdef TARGET_WORDS_BIGENDIAN
        s->FIQVEC = bswap_32(s->FIQVEC);
#endif
        DPRINTF("%s: qemu   _set_irq(s->fiq, %d);\n", __func__, FIQ != 0);
        qemu_set_irq(s->fiq, FIQ != 0);
    }
}

static void vim_set_irq(void *opaque, int irq, int level)
{
    vim_state *s = (vim_state *)opaque;
    int i, j;

    DPRINTF("%s: irq:%d level:%d\n", __func__, irq, level);

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 32; j++) {
            if (s->CHANMAP._8[i * 32 + j] == irq) {
                if (level) {
                    s->INTREQ[i] |= 1 << j;
                } else {
                    s->INTREQ[i] &= ~(1 << j);
                }
            }
        }
    }
    vim_update(s);
}

static uint64_t vim_read(void *opaque, target_phys_addr_t offset,
                           unsigned size)
{
    vim_state *s = (vim_state *)opaque;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u\n", __func__, offset, size);

    switch (offset) {
    case 0x00: /* IRQINDEX */
        DPRINTF("%s: IRQINDEX:0x%x\n", __func__, s->IRQINDEX);
        return s->IRQINDEX;
    case 0x04: /* FIQINDEX */
        DPRINTF("%s: FIQINDEX:0x%x\n", __func__, s->IRQINDEX);
        return s->FIQINDEX;

    case 0x10: /* FIRQPR0 */
    case 0x14: /* FIRQPR1 */
    case 0x18: /* FIRQPR2 */
        return s->FIRQPR[(offset - 0x10) / 4];

    case 0x20: /* INTREQ0 */
    case 0x24: /* INTREQ1 */
    case 0x28: /* INTREQ2 */
        return s->INTREQ[(offset - 0x20) / 4];

    case 0x30: /* REQENASET0 */
    case 0x40: /* REQENACLR0 */
        return s->ENA[0];
    case 0x34: /* REQENASET1 */
    case 0x44: /* REQENACLR1 */
        return s->ENA[1];
    case 0x38: /* REQENASET2 */
    case 0x48: /* REQENACLR2 */
        return s->ENA[2];

    case 0x50: /* WAKEENASET0 */
    case 0x60: /* WAKEENACLR0 */
        return s->WAKEENA[0];
    case 0x54: /* WAKEENASET1 */
    case 0x64: /* WAKEENACLR1 */
        return s->WAKEENA[1];
    case 0x58: /* WAKEENASET2 */
    case 0x68: /* WAKEENACLR2 */
        return s->WAKEENA[2];

    case 0x70: /* IRQVECREG */
        return s->IRQVEC;
        break;
    case 0x74: /* FIQVECREG */
        return s->FIQVEC;

    case 0x78:
        hw_error("VIM: Capture Event Sources not implemented\n");
        break;

    case 0x80 ... 0xDC: /* CHANCTRL */
        return s->CHANMAP._32[(offset - 0x80) / 4];
    default:
        hw_error("vim_read: Bad offset 0x%x\n", (int)offset);
        return 0;
    }
}

static void vim_write(void *opaque, target_phys_addr_t offset,
                        uint64_t val, unsigned size)
{
    vim_state *s = (vim_state *)opaque;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u val:0x%" PRIx64 "\n",
            __func__, offset, size, val);

    switch (offset) {
    case 0x00: /* IRQINDEX */
        /* Read-Only  */
        break;
    case 0x04: /* FIQINDEX */
        /* Read-Only  */
        break;

    /* Determine whether a given interrupt request will be either FIQ or IRQ */
    case 0x10: /* FIRQPR0 */
        /* Channel 0 and channel 1 are routed exclusively to FIQ */
        s->FIRQPR[0] = val | 0x3;
        break;
    case 0x14: /* FIRQPR1 */
        s->FIRQPR[1] = val;
        break;
    case 0x18: /* FIRQPR2 */
        s->FIRQPR[2] = val;
        break;

    case 0x20 ... 0x28: /* INTREQx */
        /* Read-Only  */
        break;

    case 0x30: /* REQENASET0 */
        s->ENA[0] |= val | 0x3;
        break;
    case 0x34: /* REQENASET1 */
        s->ENA[1] |= val;
        break;
    case 0x38: /* REQENASET2 */
        s->ENA[2] |= val;
        break;

    case 0x40: /* REQENACLR0 */
        s->ENA[0] &= ~val | 0x3;
        break;
    case 0x44: /* REQENACLR1 */
        s->ENA[1] &= ~val;
        break;
    case 0x48: /* REQENACLR2 */
        s->ENA[2] &= ~val;
        break;

    case 0x50: /* WAKEENASET0 */
    case 0x54: /* WAKEENASET1 */
    case 0x58: /* WAKEENASET2 */
        s->WAKEENA[(offset - 0x50) / 4] |= val;
        break;
    case 0x60: /* WAKEENACLR0 */
    case 0x64: /* WAKEENACLR1 */
    case 0x68: /* WAKEENACLR2 */
        s->WAKEENA[(offset - 0x60) / 4] &= ~val;
        break;

    case 0x70: /* IRQVECREG */
        /* Read-Only */
        break;
    case 0x74: /* FIQVECREG */
        /* Read-Only */
        break;

    case 0x78:
        hw_error("VIM: Capture Event Sources not implemented\n");
        break;

    case 0x80 ... 0xBC: /* CHANCTRL */
        s->CHANMAP._32[(offset - 0x80) / 4] = val & 0x7F7F7F7F;
        return;
        break;

    default:
        hw_error("vim_write: Bad offset 0x%x\n", (int)offset);
        return;
    }
    vim_update(s);
}

static const MemoryRegionOps vim_ops = {
    .read = vim_read,
    .write = vim_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void vim_reset(DeviceState *d)
{
    vim_state *s = DO_UPCAST(vim_state, busdev.qdev, d);
    int i;

    s->IRQINDEX = 0;
    s->FIQINDEX = 0;
    s->last_IRQINDEX = 0;
    s->last_FIQINDEX = 0;
    s->IRQVEC   = 0;
    s->FIQVEC   = 0;

    for (i = 0; i < 3; i++) {
        s->FIRQPR[i]   = 0;
        s->INTREQ[i]   = 0;
        s->ENA[i]      = 0;
    }
    s->FIRQPR[0]   = 0x3;

    for (i = 0; i < 96; i++) {
        s->CHANMAP._8[i] = i;
    }
}

static int vim_init(SysBusDevice *dev)
{
    vim_state *s = FROM_SYSBUS(vim_state, dev);

    memory_region_init_io(&s->iomem, &vim_ops, s, "VIM", 0x100);
    sysbus_init_mmio(dev, &s->iomem);
    qdev_init_gpio_in(&dev->qdev, vim_set_irq, 64);
    sysbus_init_irq(dev, &s->irq);
    sysbus_init_irq(dev, &s->fiq);
    return 0;
}

static void vim_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = vim_init;
    dc->no_user = 1;
    dc->reset = vim_reset;
}

static TypeInfo vim_info = {
    .name          = "VIM",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(vim_state),
    .class_init    = vim_class_init,
};

static void vim_register_types(void)
{
    type_register_static(&vim_info);
}

type_init(vim_register_types)
