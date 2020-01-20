/*
 * TMS570 GIO QEMU Implementation
 *
 * Copyright (c) 2020 AdaCore
 *
 *  Developed by :
 *  Frederic Konrad   <frederic.konrad@adacore.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "hw/misc/tms570_gio.h"
#include "qemu/log.h"
#include "qemu/bitops.h"
#include "qapi/error.h"

/* This model is a GPIO peripheral used in the TMS570xx device.  */

#ifndef DEBUG_TMS570_GIO
#define DEBUG_TMS570_GIO 0
#endif

#define DPRINTF(fmt, ...) do {                        \
  if (DEBUG_TMS570_GIO) {                                \
    qemu_log(TYPE_TMS570_GIO ": " fmt , ## __VA_ARGS__); \
  }                                                   \
} while (0)

#define TMS570_GIOGCR0    (0x00 >> 2)
#define TMS570_GIOGCR0_RESET (0)
#define TMS570_GIOINTDET  (0x08 >> 2)
#define TMS570_GIOPOL     (0x0C >> 2)
#define TMS570_GIOENASET  (0x10 >> 2)
#define TMS570_GIOENACLR  (0x14 >> 2)
#define TMS570_GIOLVLSET  (0x18 >> 2)
#define TMS570_GIOLVLCLR  (0x1C >> 2)
#define TMS570_GIOFLG     (0x20 >> 2)
#define TMS570_GIOOFF1    (0x24 >> 2)
#define TMS570_GIOOFF2    (0x28 >> 2)
#define TMS570_GIOEMU1    (0x2C >> 2)
#define TMS570_GIOEMU2    (0x30 >> 2)
#define TMS570_GIODIRA    (0x34 >> 2)
#define TMS570_GIODINA    (0x38 >> 2)
#define TMS570_GIODOUTA   (0x3C >> 2)
#define TMS570_GIODSETA   (0x40 >> 2)
#define TMS570_GIODCLRA   (0x44 >> 2)
#define TMS570_GIOPDRA    (0x48 >> 2)
#define TMS570_GIOPULDISA (0x4C >> 2)
#define TMS570_GIOPSLA    (0x50 >> 2)
#define TMS570_GIODIRB    (0x54 >> 2)
#define TMS570_GIODINB    (0x58 >> 2)
#define TMS570_GIODOUTB   (0x5C >> 2)
#define TMS570_GIODSETB   (0x60 >> 2)
#define TMS570_GIODCLRB   (0x64 >> 2)
#define TMS570_GIOPDRB    (0x68 >> 2)
#define TMS570_GIOPULDISB (0x6C >> 2)
#define TMS570_GIOPSLB    (0x70 >> 2)

typedef enum {
    /* Value affected as described in the GIOPOL register.  */
    TMS570_FALLING_EDGE = 0,
    TMS570_RISING_EDGE = 1
} tms570_polarity;

/* Return true if the module is in reset mode.  */
static int tms570_is_reset(TMS570GIOState *s)
{
    /* This bit is inverted which means RESET = 1 acting normally.  */
    return (!extract32(s->regs[TMS570_GIOGCR0], TMS570_GIOGCR0_RESET, 1));
}

/* Return true if any of the @pin is configured as an output.  */
static int tms570_pin_is_output(TMS570GIOState *s, int pin)
{
    return ((s->regs[TMS570_GIODIRB] << 8) | s->regs[TMS570_GIODIRA]) & pin;
}

/* Return true if any of the @pin output is open-drained.  */
static int tms570_pin_is_open_drain(TMS570GIOState *s, int pin)
{
    return ((s->regs[TMS570_GIOPDRB] << 8) | s->regs[TMS570_GIOPDRA]) & pin;
}

/* Update the irq lines with the high, low fifos value.  */
static void tms570_irq_update(TMS570GIOState *s)
{
    DPRINTF("tms570_irq_update: high: %u, low: %u\n", s->high[0], s->low[0]);

    s->regs[TMS570_GIOOFF1] = s->high[0];
    s->regs[TMS570_GIOEMU1] = s->high[0];
    s->regs[TMS570_GIOOFF2] = s->low[0];
    s->regs[TMS570_GIOEMU2] = s->low[0];

    qemu_set_irq(s->irq[TMS570_GIO_IRQ_HIGH], s->regs[TMS570_GIOOFF1]);
    qemu_set_irq(s->irq[TMS570_GIO_IRQ_LOW], s->regs[TMS570_GIOOFF2]);
}

static void tms570_gio_reset(DeviceState *dev)
{
    TMS570GIOState *s = TMS570_GIO(dev);

    memset(s->regs, 0, sizeof(s->regs));
    memset(s->high, 0, sizeof(s->high));
    memset(s->low, 0, sizeof(s->low));
    tms570_irq_update(s);
}

/* Acknowledge the IRQ #@pin_id.  Run through the irq lists and drop it, then
 * update the irq line.  Assume that there are no holes (0) in the irq queue,
 * and that there are no duplicate IRQ.  */
static void tms570_acknowledge_irq(TMS570GIOState *s, uint32_t pin_id)
{
    int i, j, k;
    uint8_t * const cur_irq_queue[] = {s->high, s->low};
    uint8_t temp[MAX_PIN];

    DPRINTF("tms570_acknowledge_irq(%u)\n", pin_id);
    /* 0 <= pin_id <= 15, but in the IRQ rings 0 means no IRQ so everything
     * is shifted by 1.  */
    for (k = 0; k < 2; k++) {
        i = 0;
        j = 0;
        while (((i + j) < MAX_PIN) && (cur_irq_queue[k][j + i])) {
            if (cur_irq_queue[k][i] == (pin_id + 1)) {
                j++;
            }
            temp[i] = cur_irq_queue[k][j + i];
            i++;
        }

        for (i = 0; i < MAX_PIN; i++) {
            cur_irq_queue[k][i] = temp[i];
        }
    }

    /* Drop the IRQ from the GIOFLG register.  */
    s->regs[TMS570_GIOFLG] &= ~((uint32_t)1 << (pin_id));
    tms570_irq_update(s);
}

/* For each bit in @clear, clear the corresponding flag in the GIOFLG register
 * and in its offset register, then do the appropriate side effect on the IRQ
 * lines.  */
static void tms570_clear_interrupt_flag(TMS570GIOState *s, uint32_t clear)
{
    int i;

    for (i = 0; i < MAX_PIN; i++) {
        if (!(s->regs[TMS570_GIOFLG] & clear & (1 << i))) {
            continue;
        } else {
            tms570_acknowledge_irq(s, i);
        }
    }
}

/* An interrupt has been triggered for the @pin_id.  Add it to the queue if
 * it's not already done.  Set the IRQ output if needed.  */
static void tms570_trigger_interrupt(TMS570GIOState *s, unsigned int pin_id)
{
    uint8_t *const irq_fifo[] = {s->low, s->high};
    int i;

    assert(!tms570_is_reset(s));

    /* Check if this IRQ is already pending.  */
    for (i = 0; i < MAX_PIN; i++) {
        if (irq_fifo[extract32(s->regs[TMS570_GIOLVLSET],
                               pin_id, 1)][i] == (pin_id + 1)) {
            /* This IRQ is already pending.  */
            return;
        }
        if (!irq_fifo[extract32(s->regs[TMS570_GIOLVLSET],
                               pin_id, 1)][i]) {
            /* pin_id is not pending.  Put it at the end of the fifo.  */
            break;
        }
    }

    assert(i < MAX_PIN);
    irq_fifo[extract32(s->regs[TMS570_GIOLVLSET], pin_id, 1)][i] =
        pin_id + 1;

    /* Update the IRQ lines.  */
    tms570_irq_update(s);
}

/* The following function is called from anything which can change the input
 * for the GPIO and the interrupts.  Basically any writes to the direction
 * register or to the output registers.  */
static void tms570_input_change(TMS570GIOState *s, unsigned int pin,
                                tms570_polarity pol)
{
    assert(!tms570_is_reset(s));
    assert(pin);

    if (s->regs[TMS570_GIOFLG] & pin) {
        /* A transition has already been taken in account for this PIN.  At
         * this point I guess do nothing.  The question which might be raised
         * is can that trigger an IRQ if the priority change between two
         * transistion?  */
        return;
    }

    /* The interrupt enable register has an effect on the interrupt line but
     * not on the transistion register which remains touched even if the
     * interruption is disabled.  */
    if (!(s->regs[TMS570_GIOINTDET] & pin)) {
        /* The transition happen only one 1 edge of the signal check that the
         * right condition is meet. */
        switch (pol) {
        case TMS570_RISING_EDGE:
            if (!(s->regs[TMS570_GIOPOL] & pin)) {
                return;
            }
            break;
        case TMS570_FALLING_EDGE:
            if (s->regs[TMS570_GIOPOL] & pin) {
                return;
            }
            break;
        default:
            /* Shouldn't happen.  */
            abort();
            break;
        }
    }

    /* At this point all the conditions are meet to set the flag.  */
    s->regs[TMS570_GIOFLG] |= pin;

    /* As stated below GIOENACLR or GIOENASET has the same value.  */
    if (s->regs[TMS570_GIOENACLR] & pin) {
        int pin_id = 0;

        while (pin) {
            pin = pin >> 1;
            pin_id++;
        };
        /* The interrupt is enabled, let's trigger an interrupt update.  */
        tms570_trigger_interrupt(s, pin_id - 1);
    }
}

/* Something changed on the pin reflected by @pin_mask.  This recomputes the
 * GIODINx registers and continue with the side effect it has.  NOTE that
 * this considers that the pin is not connected to anything or more
 * specifically tied to the ground through a pull-down resistor.  */
static void tms570_update_pin(TMS570GIOState *s, int pin_mask)
{
    int i;
    int new_state;
    uint32_t cur_pin;

    for (i = 0; i < MAX_PIN; i++) {
        cur_pin = (1 << i);
        if (!(cur_pin & pin_mask)) {
            /* This pin doesn't need recomputation..  Next.  */
            continue;
        }

        if (!tms570_pin_is_output(s, cur_pin)) {
            /* Let's consider that the PIN is bound to the ground since the
             * GPIO input lines are not modelled.  Also QEMU doesn't have any
             * representation of the hi-z state so the pull-up / pull-down
             * machinery is not modelled.  */
            new_state = 0;
        } else {
            if (tms570_pin_is_open_drain(s, cur_pin)) {
                /* To reflect the behavior described above and the behavior of
                 * the input mode let's drive the input PIN to the ground when
                 * the open drain mode is configured (hi-z output).  */
                new_state = 0;
            } else {
                /* In this mode the PIN will be driven by the output which is
                 * reflected by the GIODOUTx, GIODSETx and GIODCLRx registers.
                 */
                new_state = ((s->regs[TMS570_GIODOUTB] << 8)
                             | (s->regs[TMS570_GIODOUTA])) & cur_pin;
            }
        }

        if ((new_state ^ ((s->regs[TMS570_GIODINB] << 8)
                          | s->regs[TMS570_GIODINA])) & cur_pin) {
            /* Value changed.  Reflect its value in the correct register.  */
            if (i < 8) {
                s->regs[TMS570_GIODINA] = deposit32(s->regs[TMS570_GIODINA],
                                                    i, 1, new_state ? 1 : 0);
            } else {
                s->regs[TMS570_GIODINB] = deposit32(s->regs[TMS570_GIODINB],
                                                    i - 8, 1, new_state ? 1 : 0);
            }

            /* Now trigger the interrupt mechanism.  */
            tms570_input_change(s, cur_pin, (new_state & cur_pin)
                                                    ? TMS570_RISING_EDGE
                                                    : TMS570_FALLING_EDGE);
        }
    }
}

/* The following update the GIOENACLR and GIOENASET register with @new_value
 * and will take care of the other side effect it might have.  Let's consider
 * that having the GIOFLG flag set and enabling the interrupt will trigger an
 * interrupt (this is not clear in the documentation).  */
static void tms570_update_int_enable(TMS570GIOState *s, uint32_t new_value)
{
    uint32_t new_enabled;
    int i;

    if (s->regs[TMS570_GIOENACLR] == new_value) {
        /* Nothing changed!  */
        return;
    }

    /* This reflects the 0 -> 1 transition which is actually activating the
     * IRQ.  */
    new_enabled = new_value & (~s->regs[TMS570_GIOENACLR]);

    /* During reads, the interrupt masks are actually mapped on both
     * GIOENACLR and GIOENASET registers, so change their value here to avoid
     * any additional mechanism in tms570_gio_read.  */
    s->regs[TMS570_GIOENACLR] = new_value;
    s->regs[TMS570_GIOENASET] = new_value;

    if (!new_enabled) {
        /* No interrupt activation.  */
        return;
    }

    /* Now trigger an IRQ for everybody whom has it's enabled bit transitioned
     * to one nd its bit set in the TMS570_GIOFLG register.  */
    for (i = 0; i < MAX_PIN; i++) {
        if (s->regs[TMS570_GIOFLG] & new_enabled & (1 << i)) {
            tms570_trigger_interrupt(s, i);
        }
    }
}

static uint64_t tms570_gio_read(void *opaque, hwaddr offset, unsigned size)
{
    TMS570GIOState *s = TMS570_GIO(opaque);
    uint64_t r = 0;

    assert(size == 4);
    offset = offset >> 2;

    switch (offset) {
    case TMS570_GIOOFF1:
    case TMS570_GIOOFF2:
        /* Reading those registers clears the corresponding bit in GIOFLG
         * register and modify GIOEMUx register as well.  In case the device
         * is in reset state we fall through the "!s->regs[offset]" case.  */
        if (!s->regs[offset]) {
            /* Nothing to clear \o/, no interrupt is pending.  */
            r = 0;
            break;
        }
        r = s->regs[offset];
        tms570_clear_interrupt_flag(s, s->regs[offset]);
        break;
    default:
        r = s->regs[offset];
        break;
    }

    DPRINTF("tms570_gio_read: 0x%2.2X @0x%2.2X\n", (unsigned int)r,
            (unsigned int)offset << 2);

    return r;
}

static void tms570_gio_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    TMS570GIOState *s = TMS570_GIO(opaque);

    DPRINTF("tms570_gio_write: 0x%2.2X @0x%2.2X\n", (unsigned int)value,
            (unsigned int)offset);

    assert(size == 4);
    offset = offset >> 2;

    if (tms570_is_reset(s) && offset != TMS570_GIOGCR0) {
        /* The only thing allowed in case the device is reset is to bring
         * the device out of reset.  So let's just ignore this write in the
         * case we are not addressing the TMS570_GIOGCR0 register.  */
        return;
    }

    switch (offset) {
    case TMS570_GIOGCR0:
        if (!extract32(value, TMS570_GIOGCR0_RESET, 1)) {
            /* Resets the device.  This register will be reset as well.  No
             * need to write to it.  */
            tms570_gio_reset(DEVICE(s));
        }
        else
        {
            s->regs[offset] = deposit32(s->regs[offset],
                                        TMS570_GIOGCR0_RESET,
                                        1,
                                        value);
        }
        break;
    case TMS570_GIOENACLR:
        /* Writing one to this register will disable the corresponding pin
         * interrupt.  Writing 0 has no effect.  */
        tms570_update_int_enable(s, s->regs[offset] & ~value);
        break;
    case TMS570_GIOENASET:
        /* Writing one to this register will enable the corresponding pin
         * interrupt.  Writing 0 has no effect.  */
        tms570_update_int_enable(s, s->regs[offset] | value);
        break;
    case TMS570_GIOFLG:
        /* Writing one to this register will clear the corresponding pin
         * interrupt and it's value in its offset register.  Don't bother
         * clearing the interrupt flags which are already cleared.  */
        tms570_clear_interrupt_flag(s, s->regs[offset] & value);
        break;
    case TMS570_GIODCLRA:
    case TMS570_GIODCLRB:
    {
        uint32_t old = s->regs[offset];

        s->regs[offset] &= ~s->regs[offset];
        /* Propagate the change to TMS570_GIODSETx and TMS570_GIODOUTx
         * registers.  */
        s->regs[offset - 1] = s->regs[offset];
        s->regs[offset - 2] = s->regs[offset];
        if (old != s->regs[offset]) {
            tms570_update_pin(s, offset == TMS570_GIODCLRA ? 0xFF : 0xFF00);
        }
        break;
    }
    case TMS570_GIODSETA:
    case TMS570_GIODSETB:
    {
        uint32_t old = s->regs[offset];

        s->regs[offset] |= value;
        /* Propagate the change to TMS570_GIODCLRx and TMS570_GIODOUTx
         * registers.  */
        s->regs[offset - 1] = s->regs[offset];
        s->regs[offset + 1] = s->regs[offset];
        if (old != s->regs[offset]) {
            tms570_update_pin(s, offset == TMS570_GIODSETA ? 0xFF : 0xFF00);
        }
        break;
    }
    case TMS570_GIODOUTA:
    case TMS570_GIODOUTB:
        /* Check that this write really has effect or don't do anything.  */
        if (s->regs[offset] == (value & 0xFF)) {
            break;
        }

        s->regs[offset] = value & 0xFF;
        /* Propagate the change to TMS570_GIODCLRx and TMS570_GIODOUTx
         * registers.  */
        s->regs[offset - 1] = s->regs[offset];
        s->regs[offset - 2] = s->regs[offset];
        tms570_update_pin(s, offset == TMS570_GIODOUTA ? 0xFF : 0xFF00);
        break;
    case TMS570_GIODIRA:
    case TMS570_GIOPDRA:
    case TMS570_GIODIRB:
    case TMS570_GIOPDRB:
        /* Check that this write really has effect or don't do anything.  */
        if (s->regs[offset] == (value & 0xFF)) {
            break;
        }

        s->regs[offset] = value & 0xFF;
        tms570_update_pin(s, offset < TMS570_GIODIRB ? 0xFF : 0xFF00);
        break;
    case TMS570_GIOOFF1:
    case TMS570_GIOOFF2:
    case TMS570_GIOEMU1:
    case TMS570_GIOEMU2:
        /* Those are all read-only registers.  */
        break;
    case TMS570_GIOLVLSET:
        /* Note that the doc indicate GIOC, and GIOD which doesn't seem to
         * exist in other register so ignore them.  */
        s->regs[offset] |= (value & 0xFFFF);
        s->regs[TMS570_GIOLVLCLR] = s->regs[offset];
        break;
    case TMS570_GIOLVLCLR:
        /* Note that the doc indicate GIOC, and GIOD which doesn't seem to
         * exist in other register so ignore them.  */
        s->regs[offset] &= ~(value & 0xFFFF);
        s->regs[TMS570_GIOLVLSET] = s->regs[offset];
        break;
    default:
        s->regs[offset] = value;
        break;
    }
}

static const MemoryRegionOps tms570_gio_ops = {
    .read = tms570_gio_read,
    .write = tms570_gio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

static const VMStateDescription vmstate_tms570_gio = {
    .name = TYPE_TMS570_GIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static Property tms570_gio_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void tms570_gio_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    TMS570GIOState *s = TMS570_GIO(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &tms570_gio_ops, s,
                          TYPE_TMS570_GIO, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq[TMS570_GIO_IRQ_LOW]);
    sysbus_init_irq(sbd, &s->irq[TMS570_GIO_IRQ_HIGH]);
}

static void tms570_gio_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = tms570_gio_realize;
    dc->vmsd = &vmstate_tms570_gio;
    dc->props = tms570_gio_properties;
    dc->reset = tms570_gio_reset;
}

static const TypeInfo tms570_gio_info = {
    .name          = TYPE_TMS570_GIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TMS570GIOState),
    .class_init    = tms570_gio_class_init,
};

static void tms570_gio_register_types(void)
{
    type_register_static(&tms570_gio_info);
}

type_init(tms570_gio_register_types)
