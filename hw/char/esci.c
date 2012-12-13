/*
 * mpc55xx eSCI emulation
 *
 * Copyright (c) 2012-2013 Adacore
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
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/char/esci.h"
#include "chardev/char-fe.h"

/* #define DEBUG_ESCI */

#ifdef DEBUG_ESCI
#define DPRINTF(fmt, ...) do { printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

/* Control */
#define ESCI_RECEIVE_ENABLE     0x4
#define ESCI_TRANSMIT_ENABLE    0x8
#define ESCI_RECEIVE_INTERRUPT  0x20
#define ESCI_TRANSMIT_INTERRUPT 0x80

/* Status */
#define ESCI_TRANSMIT_EMPTY    0x80000000
#define ESCI_TRANSMIT_COMPLETE 0x40000000
#define ESCI_DATA_READY        0x20000000

#define FIFO_LENGTH 1024

typedef struct eSCIState {
    MemoryRegion mem;

    qemu_irq     irq;
    bool         irq_raised;
    CharBackend  chr;

    /* Registers */

    uint32_t control;
    uint32_t status;

    /* FIFO */
    char buffer[FIFO_LENGTH];
    int  len;
    int  current;

} eSCIState;

static int esci_data_to_read(eSCIState *esci)
{
    return esci->current < esci->len;
}

static char esci_pop(eSCIState *esci)
{
    char ret;

    if (esci->len == 0) {
        esci->status &= ~ESCI_DATA_READY;
        return 0;
    }

    ret = esci->buffer[esci->current++];

    if (esci->current >= esci->len) {
        /* Flush */
        esci->len     = 0;
        esci->current = 0;
    }

    if (!esci_data_to_read(esci)) {
        esci->status &= ~ESCI_DATA_READY;
    } else {
        esci->status |= ESCI_DATA_READY;
    }

    return ret;
}

static void esci_add_to_fifo(eSCIState     *esci,
                             const uint8_t *buffer,
                             int            length)
{
    if (esci->len + length > FIFO_LENGTH) {
        abort();
    }
    memcpy(esci->buffer + esci->len, buffer, length);
    esci->len += length;
}

static int esci_can_receive(void *opaque)
{
    eSCIState *esci = opaque;

    return FIFO_LENGTH - esci->len;
}

static void check_for_interrupts(eSCIState *esci)
{
    bool old = esci->irq_raised;
    bool new = ((esci->status & ESCI_TRANSMIT_EMPTY &&
                 esci->control & ESCI_TRANSMIT_INTERRUPT)
                || (esci->status & ESCI_DATA_READY &&
                    esci->control & ESCI_RECEIVE_INTERRUPT));

    if (old != new) {
        esci->irq_raised = new;
        if (new) {
            DPRINTF("%s: qemu_irq_raise\n", __func__);
            qemu_irq_raise(esci->irq);
        } else {
            DPRINTF("%s: qemu_irq_lower\n", __func__);
            qemu_irq_lower(esci->irq);
        }
    }
}

static void esci_receive(void *opaque, const uint8_t *buf, int size)
{
    eSCIState *esci = opaque;

    if (esci->control & ESCI_RECEIVE_ENABLE) {
        esci_add_to_fifo(esci, buf, size);

        esci->status |= ESCI_DATA_READY;

        check_for_interrupts(esci);
    }
}

static void esci_event(void *opaque, int event)
{
}

static void esci_reset(void *opaque)
{
    eSCIState *esci = (eSCIState *)opaque;

    esci->control = ESCI_RECEIVE_ENABLE | ESCI_TRANSMIT_ENABLE;
    esci->status  = ESCI_TRANSMIT_EMPTY | ESCI_TRANSMIT_COMPLETE;
    esci->irq_raised = false;
    qemu_irq_lower(esci->irq);
}

static uint64_t esci_read(void *opaque, hwaddr addr, unsigned size)
{
    eSCIState *esci = (eSCIState *)opaque;
    uint64_t   ret;

    /* DPRINTF("%s: addr: "TARGET_FMT_plx" size:%d\n", __func__, addr, size); */

    switch (addr) {
    case 0x000:
        ret = esci->control;
        DPRINTF("%s: control: 0x%08x\n", __func__, ret);
        break;
    case 0x006:
        ret = esci_pop(esci);
        check_for_interrupts(esci);
        DPRINTF("%s: data: 0x%08x\n", __func__, ret);
        break;
    case 0x008:
        ret = esci->status;
        DPRINTF("%s: status: 0x%08x\n", __func__, ret);
        break;

    default:
        DPRINTF("%s: Invalid address: "TARGET_FMT_plx"\n", __func__, addr);
        ret = 0;
    }

    return ret;
}


static void esci_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    eSCIState     *esci = (eSCIState *)opaque;
    unsigned char  c    = 0;

    /* DPRINTF("%s: addr:"TARGET_FMT_plx" size:%d value:0x%08x\n", __func__, */
    /*         addr, size, value); */

    switch (addr) {
    case 0x000:
        DPRINTF("%s: control: 0x%08x\n", __func__, value);
        esci->control = value;
        check_for_interrupts(esci);
        break;
    case 0x006:
        DPRINTF("%s: data: 0x%08x\n", __func__, value);
        c = value & 0xFF;
        qemu_chr_fe_write(&esci->chr, &c, 1);

        check_for_interrupts(esci);
        break;
    case 0x008:
        DPRINTF("%s: status: 0x%08x\n", __func__, value);

        /* Write 1 to clear */
        esci->status &= ~value;

        /* Transmitter is always empty in qemu */
        esci->status |= ESCI_TRANSMIT_EMPTY | ESCI_TRANSMIT_COMPLETE;

        check_for_interrupts(esci);
        break;
    default:
        DPRINTF("%s: Invalid address: "TARGET_FMT_plx"\n", __func__, addr);
    }
}


static const MemoryRegionOps esci_ops = {
    .read       = esci_read,
    .write      = esci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

void esci_init(MemoryRegion *address_space, hwaddr base,
               Chardev *chr, qemu_irq irq)
{
    eSCIState *esci;

    esci = g_malloc0(sizeof(eSCIState));

    esci->irq = irq;

    qemu_chr_fe_init(&esci->chr, chr, NULL);
    qemu_chr_fe_set_handlers(&esci->chr,
                             esci_can_receive,
                             esci_receive,
                             esci_event,
                             NULL, esci, NULL, true);

    memory_region_init_io(&esci->mem, NULL, &esci_ops, esci,
                          "esci", 0x1C);
    memory_region_add_subregion(address_space, base, &esci->mem);

    qemu_register_reset(esci_reset, esci);
}
