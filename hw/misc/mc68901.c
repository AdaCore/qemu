/*
 * MC68901 QEMU Implementation
 *
 * Copyright (c) 2019-2020 AdaCore
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
#include "hw/misc/mc68901.h"
#include "qemu/log.h"
#include "qemu/bitops.h"
#include "qapi/error.h"

/* This model is a multi-function peripheral used on the MVME133 board with
 * the M68020 chip.  For the moment only the USART part is implemented.  */

#ifndef DEBUG_MC68901
#define DEBUG_MC68901 0
#endif

#define DPRINTF(fmt, ...) do {                        \
  if (DEBUG_MC68901) {                                \
    qemu_log(TYPE_MC68901 ": " fmt , ## __VA_ARGS__); \
  }                                                   \
} while (0)

/* NOTE: This device is only responding on the odd address (A0 = 1).  The even
 *       address can be used for an other device.  So the device memory space
 *       is interleaved with the other device.  */
#define MC68901_GPIP_ADDR  (0x01 >> 1)
#define MC68901_AER_ADDR   (0x03 >> 1)
#define MC68901_DDR_ADDR   (0x05 >> 1)
#define MC68901_IERA_ADDR  (0x07 >> 1)
#define MC68901_IERB_ADDR  (0x09 >> 1)
#define MC68901_IPRA_ADDR  (0x0B >> 1)
#define MC68901_IPRB_ADDR  (0x0D >> 1)
#define MC68901_ISRA_ADDR  (0x0F >> 1)
#define MC68901_ISRB_ADDR  (0x11 >> 1)
#define MC68901_IMRA_ADDR  (0x13 >> 1)
#define MC68901_IMRB_ADDR  (0x15 >> 1)
#define MC68901_VR_ADDR    (0x17 >> 1)
#define MC68901_TACR_ADDR  (0x19 >> 1)
#define MC68901_TBCR_ADDR  (0x1B >> 1)
#define MC68901_TCDCR_ADDR (0x1D >> 1)
#define MC68901_TADR_ADDR  (0x1F >> 1)
#define MC68901_TBDR_ADDR  (0x21 >> 1)
#define MC68901_TCDR_ADDR  (0x23 >> 1)
#define MC68901_TDDR_ADDR  (0x25 >> 1)
#define MC68901_SCR_ADDR   (0x27 >> 1)
#define MC68901_UCR_ADDR   (0x29 >> 1)
#define MC68901_RSR_ADDR   (0x2B >> 1)
#define MC68901_RSR_BF_SHIFT (7)
#define MC68901_RSR_RE_SHIFT (0)
#define MC68901_TSR_ADDR   (0x2D >> 1)
#define MC68901_TSR_BE_SHIFT  (7)
#define MC68901_TSR_UE_SHIFT  (6)
#define MC68901_TSR_END_SHIFT (4)
#define MC68901_TSR_TE_SHIFT  (0)
#define MC68901_UDR_ADDR   (0x2F >> 1)

static uint64_t mc68901_timer_prescaler(MC68901State *s, int id)
{
    const uint8_t prescaler[8] = {0, 4, 10, 16, 50, 64, 100, 200};

    switch (id) {
    case 0:
    case 1:
        return prescaler[extract32(s->regs[id == 0 ? MC68901_TACR_ADDR
                                           : MC68901_TBCR_ADDR], 0, 3)];
    case 2:
    case 3:
        return prescaler[extract32(s->regs[MC68901_TCDCR_ADDR],
                                   id == 2 ? 4 : 0, 3)];
    default:
        abort();
        return 0;
    }
}

static int mc68901_timer_stopped(MC68901State *s, int id)
{
    return mc68901_timer_prescaler(s, id) == 0;
}

static uint64_t mc68901_timer_counter(MC68901State *s, int id)
{
    assert(id < 4);
    /* A 0 loaded counter will goes through the 256 cycles.  */
    return ((uint8_t)s->regs[MC68901_TADR_ADDR + id] - 1);
}

static uint64_t mc68901_timer_next_deadline(MC68901State *s, int id)
{
    return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
        + muldiv64((mc68901_timer_counter(s, id) + 1)
                    * mc68901_timer_prescaler(s, id),
                   NANOSECONDS_PER_SECOND,
                   s->timer_freq);
}

static void mc68901_timer_start(MC68901State *s, int id)
{
    timer_mod(&s->timers[id].qemu_timer, mc68901_timer_next_deadline(s, id));
}

static void mc68901_timer_expired(void *opaque)
{
    struct MC68901Timer *mc68901_timer = (struct MC68901Timer *)opaque;
    uint8_t id = mc68901_timer->id;
    MC68901State *s = MC68901(DO_UPCAST(MC68901State, timers[id],
                                        mc68901_timer));
    /* Better be carefull with the trick above.  */
    assert(s);

    if (mc68901_timer_stopped(s, id)) {
        /* Timer has been stopped while running.  Do nothing.  */
        return;
    }

    /* XXX: Implements interrupt.  */

    /* Pulse the corresponding TXO GPIO line.  */
    qemu_irq_pulse(s->txo[id]);

    /* Restart the timer for next time.  */
    mc68901_timer_start(s, id);
}

static void mc68901_timer_control_update(MC68901State *s, uint32_t offset,
                                         uint8_t value)
{
    if ((offset == MC68901_TACR_ADDR) || (offset == MC68901_TBCR_ADDR)) {
        value = value & 0x1F;
    } else {
        value = value & 0x77;
    }

    if (s->regs[offset] == value) {
        /* The registers didn't change, nothing to do.  */
        return;
    }

    switch (offset) {
    case MC68901_TACR_ADDR:
    case MC68901_TBCR_ADDR:
        if (extract32(value, 3, 1) || extract32(value, 4, 1)) {
            /* Pulse Width Mode is not implemented.  Let's ignore that.  */
            qemu_log_mask(LOG_GUEST_ERROR, "mc68901: Pulse Width Modulation "
                          "is not implemented (ignored).\n");
            /* But write the anyway so we detect further changes on the
             * register.  */
            s->regs[offset] = value;
            return;
        }

        if (!extract32(value, 0, 4)) {
            /* Timer has been stopped, this will be handled at the next timer
             * expiration so there is nothing to do.  */
            s->regs[offset] = value;
            return;
        }

        /* At this point this is either starting the timer, or just changing
         * it's prescaler.  For the former just start the timer.  For the
         * latter just ignore the side effect it will be handled at the next
         * timer expiration.  */
        if (extract32(s->regs[offset], 0, 4)) {
            /* The timer was active.  Handle the change in the next timer
             * expiration.  */
            s->regs[offset] = value;
            return;
        }

        s->regs[offset] = value;
        mc68901_timer_start(s, offset == MC68901_TACR_ADDR ? 0 : 1);
        break;
    case MC68901_TCDCR_ADDR:
    {
        int i;

        for (i = 0; i < 2; i++) {
            int timer_bitshift = i << 2;

            if ((s->regs[offset] ^ value) & (i == 0 ? 0x07 : 0x70)) {
                /* Something changed for TimerC.  */
                if (!extract32(value, timer_bitshift, 3)) {
                    /* Timer has been stopped.  Update its control register
                     * bits and do nothing more about it.  This will be
                     * handled at next timer expiration.  */
                    s->regs[offset] = deposit32(s->regs[offset],
                                                timer_bitshift, 3, 0);
                } else {
                    /* Same process as above:  Either we are starting the
                     * timer, or we are changing its prescaler.  The former
                     * is handled here, the latter is handled at the next
                     * timer expiration.  */
                    if (extract32(s->regs[offset], timer_bitshift, 3)) {
                        s->regs[offset] = deposit32(s->regs[offset],
                                                    timer_bitshift, 3,
                                                    extract32(value,
                                                    timer_bitshift, 3));
                        mc68901_timer_start(s, 2 + i);
                    }
                }
            }
        }
    }
    break;
    default:
        /* This shouldn't hap.. Aborted (core dumped).  */
        abort();
        break;
    }
}

static uint64_t mc68901_read(void *opaque, hwaddr offset, unsigned size)
{
    MC68901State *s = MC68901(opaque);
    uint64_t r = 0;

    DPRINTF("mc68901_read: 0x%2.2X @0x%2.2X\n", (unsigned int)r,
            (unsigned int)offset);

    assert(size == 1);
    if (!(offset & 0x1)) {
        /* Let's assume that even address returns 0. */
        qemu_log_mask(LOG_GUEST_ERROR, "mc68901_read: Even offset %x"
                                       " are not supposed to be accessed\n",
                      (int)offset);
        return 0;
    }
    offset = offset >> 1;

    switch (offset) {
    case MC68901_UDR_ADDR:
        if (s->buffer_full) {
            r = s->receive_buf;
            s->buffer_full = 0;
        }
        break;
    case MC68901_RSR_ADDR:
        r = s->regs[offset];

        if (extract32(r, MC68901_RSR_RE_SHIFT, 1)) {
            /* Receiver is enabled we only model the Buffer Full bit yet. */
            r = deposit32(r, MC68901_RSR_BF_SHIFT, 1, s->buffer_full ? 1 : 0);
        }
        break;
    case MC68901_TSR_ADDR:
        r = s->regs[offset];

        if (extract32(r, MC68901_TSR_TE_SHIFT, 1)) {
            /*
             * Transmitter is enabled we only model the Buffer Empty bit
             * yet.. Which is always the case as we are sending the
             * characters immediately after when we get them.
             */
            r = deposit32(r, MC68901_TSR_BE_SHIFT, 1, 1);
        }
        break;
    default:
        r = s->regs[offset];
        break;
    }

    return r;
}

static void mc68901_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    MC68901State *s = MC68901(opaque);

    DPRINTF("mc68901_write: 0x%2.2X @0x%2.2X\n", (unsigned int)value,
            (unsigned int)offset);

    assert(size == 1);
    if (!(offset & 0x1)) {
        /* Let's assume that writing even address has no effects. */
        qemu_log_mask(LOG_GUEST_ERROR, "mc68901_write: Even offset %x"
                                       " are not supposed to be accessed\n",
                      (int)offset);
        return;
    }
    offset = offset >> 1;

    switch (offset) {
    case MC68901_RSR_ADDR:
        /* Flags are not writable. */
        s->regs[offset] = (s->regs[offset] & ~((uint8_t)0x0B))
                        | (value & 0x0B);
        if (!extract32(value, MC68901_RSR_RE_SHIFT, 1)) {
            /* Disabling the receiver clears its flags */
            s->regs[offset] &= 0x0B;
        }
        break;
    case MC68901_TSR_ADDR:
        /* Flags are not writable. */
        s->regs[offset] = (s->regs[offset] & ~((uint8_t)0x5F))
                        | (value & 0x5F);
        if (!extract32(value, MC68901_TSR_TE_SHIFT, 1)) {
            /* Disabling the transmitter clears UE and set END */
            s->regs[offset] = deposit32(s->regs[offset],
                                        MC68901_TSR_END_SHIFT,
                                        1,
                                        1);
            s->regs[offset] = deposit32(s->regs[offset],
                                        MC68901_TSR_UE_SHIFT,
                                        1,
                                        0);
        } else {
            s->regs[offset] = deposit32(s->regs[offset],
                                        MC68901_TSR_END_SHIFT,
                                        1,
                                        0);
        }
        break;
    case MC68901_UDR_ADDR:
        /* XXX: Send the character if the transmitter is enabled.  Maybe the
         *      first character written while the transmitter is disabled is
         *      not lost on the real hardware.  But let's keep it simple for
         *      the moment and drop all the characters written while the
         *      transmitter is disabled. */
        if (extract32(s->regs[MC68901_TSR_ADDR], MC68901_TSR_TE_SHIFT, 1)) {
            uint8_t c = value;
            /* Transmitter is enabled lets send the character. */
            qemu_chr_fe_write_all(&s->chr, &c, 1);
        }
        break;
    case MC68901_UCR_ADDR:
        s->regs[offset] = (uint8_t)(value & 0xFE);
        break;
    case MC68901_TACR_ADDR:
    case MC68901_TBCR_ADDR:
    case MC68901_TCDCR_ADDR:
        mc68901_timer_control_update(s, offset, value);
        break;
    default:
        s->regs[offset] = (uint8_t)value;
        break;
    }
}

static int mc68901_can_receive(void *opaque)
{
    MC68901State *s = MC68901(opaque);

    /* Say we can't receive when the buffer is full. */
    return s->buffer_full ? 0 : 1;
}

static void mc68901_receive(void *opaque, const uint8_t *buf, int size)
{
    MC68901State *s = MC68901(opaque);

    /* Receive a character */
    s->buffer_full = 1;
    s->receive_buf = buf[0];
}

static void mc68901_event(void *opaque, int event)
{
/*
    if (event == CHR_EVENT_BREAK)
        mc68901_put_fifo(opaque, 0x400);
*/
}

static const MemoryRegionOps mc68901_ops = {
    .read = mc68901_read,
    .write = mc68901_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    }
};

static const VMStateDescription vmstate_mc68901 = {
    .name = TYPE_MC68901,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static Property mc68901_properties[] = {
    DEFINE_PROP_CHR("chardev", MC68901State, chr),
    DEFINE_PROP_UINT32("timer_freq", MC68901State, timer_freq, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void mc68901_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    MC68901State *s = MC68901(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &mc68901_ops, s, TYPE_MC68901,
                          0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void mc68901_realize(DeviceState *dev, Error **errp)
{
    MC68901State *s = MC68901(dev);
    int i;

    qemu_chr_fe_set_handlers(&s->chr, mc68901_can_receive, mc68901_receive,
                             mc68901_event, NULL, s, NULL, true);
    memset(s->regs, 0, sizeof(s->regs));

    if (!s->timer_freq) {
        error_setg(errp, "timer_freq property not set");
        return;
    }
    qdev_init_gpio_out(dev, s->txo, MC68901_TIMER_COUNT);

    for (i = 0; i < MC68901_TIMER_COUNT; i++) {
        s->timers[i].id = i;
        timer_init_ns(&s->timers[i].qemu_timer,
                      QEMU_CLOCK_VIRTUAL, mc68901_timer_expired,
                      &s->timers[i]);
    }
}

static void mc68901_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = mc68901_realize;
    dc->vmsd = &vmstate_mc68901;
    dc->props = mc68901_properties;
}

static const TypeInfo mc68901_info = {
    .name          = TYPE_MC68901,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MC68901State),
    .instance_init = mc68901_init,
    .class_init    = mc68901_class_init,
};

static void mc68901_register_types(void)
{
    type_register_static(&mc68901_info);
}

type_init(mc68901_register_types)
