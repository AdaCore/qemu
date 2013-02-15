/*
 * Arm Texas-Instrument Serial Communication Interface (SCI)
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

/* #define DEBUG_SCI */

#ifdef DEBUG_SCI
#define DPRINTF(fmt, ...) do { printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

#define SCIGCR1_RX_ENA   0x1000000
#define SCIGCR1_TX_ENA   0x2000000
#define SCIFLR_TX_RDY    0x0000100
#define SCIFLR_RX_RDY    0x0000200
#define SCIFLR_TX_EMPTY  0x0000800

#define FIFO_LENGTH 1024

typedef struct {
    SysBusDevice     busdev;
    MemoryRegion     iomem;
    qemu_irq         irq0;
    qemu_irq         irq1;
    int              irq_status_0;
    int              irq_status_1;
    CharBackend      chr;

    /* FIFO */
    char buffer[FIFO_LENGTH];
    int  len;
    int  current;

    /* 0 SCI/LIN module is in reset, 1 SCI/LIN module is out of reset */
    bool reset;

    uint32_t SCIGCR1;
    uint32_t SCIGCR2;
    uint32_t SCIINT;
    uint32_t SCIINTLVL;
    uint32_t SCIFLR;
    uint32_t SCIINTVECT0;
    uint32_t SCIINTVECT1;
    uint32_t SCIFORMAT;
    uint32_t BRS;

    uint32_t SCIPIO0;
    uint32_t SCIPIO8;

} sci_state;

#define TYPE_SCI "SCI"
#define SCI(obj) \
    OBJECT_CHECK(sci_state, (obj), TYPE_SCI)

static void sci_update(sci_state *s)
{
    uint32_t irq0 = s->SCIFLR & s->SCIINT & ~s->SCIINTLVL;
    uint32_t irq1 = s->SCIFLR & s->SCIINT & s->SCIINTLVL;
    int      irq_prev_0 = s->irq_status_0;
    int      irq_prev_1 = s->irq_status_1;

    if (irq0 != 0) {
        s->SCIINTVECT0 = irq0 ? __builtin_ctzll(irq0) + 1 : 0;
        s->irq_status_0 = 1;
    } else {
        s->SCIINTVECT0 = 0;
        s->irq_status_0 = 0;
    }
    if (irq1 != 0) {
        s->SCIINTVECT1 = irq1 ? __builtin_ctzll(irq1) + 1 : 0;
        s->irq_status_1 = 1;
    } else {
        s->SCIINTVECT1 = 0;
        s->irq_status_1 = 0;
    }

    if (s->irq_status_0 != irq_prev_0) {
        qemu_set_irq(s->irq0, s->irq_status_0);
    }
    if (s->irq_status_1 != irq_prev_1) {
        qemu_set_irq(s->irq1, s->irq_status_1);
    }
}

static int sci_data_to_read(sci_state *sci)
{
    return sci->current < sci->len;
}

static char sci_fifo_next(sci_state *sci)
{
    if (sci->len == 0) {
        sci->SCIFLR &= ~SCIFLR_RX_RDY;
        sci_update(sci);
        return 0;
    }

    return sci->buffer[sci->current];
}


static char sci_pop(sci_state *sci)
{
    char ret;

    if (sci->len == 0) {
        sci->SCIFLR &= ~SCIFLR_RX_RDY;
        sci_update(sci);
        return 0;
    }

    ret = sci->buffer[sci->current++];

    if (sci->current >= sci->len) {
        /* Flush */
        sci->len     = 0;
        sci->current = 0;
    }

    if (!sci_data_to_read(sci)) {
        sci_update(sci);
        sci->SCIFLR &= ~SCIFLR_RX_RDY;
    }

    return ret;
}

static void sci_add_to_fifo(sci_state          *sci,
                             const uint8_t *buffer,
                             int            length)
{
    if (sci->len + length > FIFO_LENGTH) {
        abort();
    }
    memcpy(sci->buffer + sci->len, buffer, length);
    sci->len += length;
}

static int sci_can_receive(void *opaque)
{
    sci_state *sci = opaque;

    return sci->reset ? FIFO_LENGTH - sci->len : 0;
}

static void sci_receive(void *opaque, const uint8_t *buf, int size)
{
    sci_state *sci = opaque;

    if (sci->SCIGCR1 & SCIGCR1_RX_ENA) {
        sci_add_to_fifo(sci, buf, size);

        sci->SCIFLR |= SCIFLR_RX_RDY;
        sci_update(sci);
    }
}

static void sci_event(void *opaque, int event)
{
    (void)opaque;
    (void)event;
}

static uint64_t sci_read(void *opaque, hwaddr offset, unsigned size)
{
    sci_state *s = (sci_state *)opaque;
    uint32_t ret;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u\n", __func__, offset, size);

    switch (offset) {
    case 0x00:
        return s->reset;
    case 0x04:
        return s->SCIGCR1;
    case 0x08:
        return s->SCIGCR2;
    case 0x0C:
    case 0x10:
        return s->SCIINT;
    case 0x14:
    case 0x18:
        return s->SCIINTLVL;
    case 0x1C:
        return s->SCIFLR;
    case 0x20:
        ret = s->SCIINTVECT0;
        if (ret != 10 && ret != 11) {
            /* The RXRDY and TXRDY flags cannot be cleared by reading the
             * corresponding interrupt offset in the SCIINTVECT0/1 register.
             */
            s->SCIFLR &=  ~(1 << s->SCIINTVECT0);
            sci_update(s);
        }
        return ret;
    case 0x24:
        ret = s->SCIINTVECT1;
        if (ret != 10 && ret != 11) {
            /* The RXRDY and TXRDY flags cannot be cleared by reading the
             * corresponding interrupt offset in the SCIINTVECT0/1 register.
             */
            s->SCIFLR &=  ~(1 << s->SCIINTVECT1);
            sci_update(s);
        }
        return ret;
    case 0x28:
        return s->SCIFORMAT;
    case 0x2C:
        return s->BRS;
    case 0x30:
        return sci_fifo_next(s);
        break;
    case 0x34:
        return sci_pop(s);
        break;
    case 0x38:
        return 0;
    case 0x3C:
        return s->SCIPIO0;
    case 0x5C:
        return s->SCIPIO8;
    default:
        hw_error("sci_read: Bad offset 0x%x\n", (int)offset);
        return 0;
    }
}

static void sci_write(void *opaque, hwaddr offset, uint64_t val, unsigned size)
{
    sci_state     *s = (sci_state *)opaque;
    unsigned char  c = 0;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u val:0x%" PRIx64 "\n",
            __func__, offset, size, val);

    if (!s->reset && offset != 0) {
        /* The registers are writable only after the RESET bit is set to 1 */
        return;
    }

    switch (offset) {
    case 0x00:
        s->reset = val & 1;
        break;
    case 0x04:
        s->SCIGCR1 = val & 0x03033FFF;
        break;
    case 0x08:
        s->SCIGCR2 = val & 0x00030101;
        break;
    case 0x0C:
        s->SCIINT |= val & 0xFF0723D3;
        sci_update(s);
        break;
    case 0x10:
        s->SCIINT &= ~(val & 0xFF0723D3);
        sci_update(s);
        break;
    case 0x14:
        s->SCIINTLVL |= val & 0xFF0421D3;
        sci_update(s);
        break;
    case 0x18:
        s->SCIINTLVL &= ~(val & 0xFF0421D3);
        sci_update(s);
        break;
    case 0x1C:
        s->SCIFLR &= ~(val & 0xFF007FDF);
        break;
    case 0x20:
    case 0x24:
        /* Read-Only */
        break;
    case 0x28:
        s->SCIFORMAT = val & 0x70007;
        break;
    case 0x2C:
        s->BRS = val & 0xEFFFFFFF;
        break;
    case 0x30 ... 0x34:
        /* Read-Only */
        break;
    case 0x38:
        if (qemu_chr_fe_get_driver(&s->chr) && (s->SCIGCR1 & SCIGCR1_TX_ENA)) {
            c = val & 0xFF;
            qemu_chr_fe_write(&s->chr, &c, 1);
            sci_update(s);
        }
        break;
    case 0x3C:
        s->SCIPIO0 = val & 0x3;
        break;
    case 0x5C:
        s->SCIPIO8 = val & 0x3;
        break;
    default:
        hw_error("sci_write: Bad offset 0x%x\n", (int)offset);
        return;
    }
}

static const MemoryRegionOps sci_ops = {
    .read = sci_read,
    .write = sci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void sci_reset(DeviceState *d)
{
    sci_state *s = SCI(d);

    s->reset        = 0;
    s->SCIGCR1      = 0;
    s->SCIGCR2      = 0;
    s->SCIINT       = 0;
    s->SCIINTLVL    = 0;
    s->SCIFLR       = 0;
    s->SCIINTVECT0  = 0;
    s->SCIINTVECT1  = 0;
    s->SCIFORMAT    = 0;
    s->BRS          = 0;
    s->SCIFLR       = SCIFLR_TX_RDY | SCIFLR_TX_EMPTY;
    s->irq_status_0 = 0;
    s->irq_status_1 = 0;

    /* Flush receive FIFO */
    s->len = 0;
    s->current = 0;
}

static void sci_realize(DeviceState *dev, Error **errp)
{
    sci_state *s = SCI(dev);

    qemu_chr_fe_set_handlers(&s->chr,
                             sci_can_receive,
                             sci_receive,
                             sci_event,
                             NULL, s, NULL, true);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq0);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq1);

    memory_region_init_io(&s->iomem, NULL, &sci_ops, s, TYPE_SCI, 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static Property sci_properties[] = {
    DEFINE_PROP_CHR("chrdev", sci_state, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void sci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sci_realize;
    dc->reset = sci_reset;
    dc->props = sci_properties;
}

static TypeInfo sci_info = {
    .name          = TYPE_SCI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(sci_state),
    .class_init    = sci_class_init,
};

static void sci_register_types(void)
{
    type_register_static(&sci_info);
}

type_init(sci_register_types)
