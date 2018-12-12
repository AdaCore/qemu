/*
 * Xilinx Zynq cadence TTC model
 *
 * Copyright (c) 2011 Xilinx Inc.
 * Copyright (c) 2012 Peter A.G. Crosthwaite (peter.crosthwaite@petalogix.com)
 * Copyright (c) 2012 PetaLogix Pty Ltd.
 * Written By Haibing Ma
 *            M. Habib
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hw/timer/cadence_ttc.h"

#ifdef CADENCE_TTC_ERR_DEBUG
#define DB_PRINT(...) do { \
    fprintf(stderr,  ": %s: ", __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0)
#else
#define DB_PRINT(...)
#endif

#define COUNTER_INTR_IV     0x00000001
#define COUNTER_INTR_M1     0x00000002
#define COUNTER_INTR_M2     0x00000004
#define COUNTER_INTR_M3     0x00000008
#define COUNTER_INTR_OV     0x00000010
#define COUNTER_INTR_EV     0x00000020

#define COUNTER_CTRL_DIS    0x00000001
#define COUNTER_CTRL_INT    0x00000002
#define COUNTER_CTRL_DEC    0x00000004
#define COUNTER_CTRL_MATCH  0x00000008
#define COUNTER_CTRL_RST    0x00000010

#define CLOCK_CTRL_PS_EN    0x00000001
#define CLOCK_CTRL_PS_V     0x0000001e

static void cadence_timer_update(CadenceTimerState *s)
{
    qemu_set_irq(s->irq, !!(s->reg_intr & s->reg_intr_en));
}

static CadenceTimerState *cadence_timer_from_addr(void *opaque,
                                        hwaddr offset)
{
    unsigned int index;
    CadenceTTCState *s = (CadenceTTCState *)opaque;

    index = (offset >> 2) % 3;

    return &s->timer[index];
}

static uint64_t cadence_timer_get_ns(CadenceTimerState *s,
                                     uint64_t timer_steps)
{
    uint64_t r = timer_steps;
    int64_t factor = 0;
    /*
     * timer_steps has max value of 1ULL << 48.. NANOSECONDS_PER_SECOND has a
     * maximum value of 1ULL << 30.. So timer_steps * NANOSECONDS_PER_SECOND
     * has a maximum value of 78 which is under the 96bits limit.. The minimal
     * timer frequency to make the result fit into 64bits without overflow
     * with any prescaler is 1ULL << 14 which is 8KHz.. This might not be
     * reachable anyway so let's ensure that we don't go beyond this limit.
     */
    assert(s->freq >= (1ULL << 14));

    if (!s->reg_is_32bits) {
        factor = 16;
    }
    if (s->reg_clock & CLOCK_CTRL_PS_EN) {
        factor -= (((s->reg_clock & CLOCK_CTRL_PS_V) >> 1) + 1);
    }
    r >>= factor;

    return muldiv64(r, NANOSECONDS_PER_SECOND, s->freq);
}

static uint64_t cadence_timer_get_steps(CadenceTimerState *s, uint64_t ns)
{
    int64_t factor = 0;

    if (!s->reg_is_32bits) {
        factor = 16;
    }
    if (s->reg_clock & CLOCK_CTRL_PS_EN) {
        factor -= (((s->reg_clock & CLOCK_CTRL_PS_V) >> 1) + 1);
    }

    return muldiv64(ns, s->freq, NANOSECONDS_PER_SECOND) << factor;
}

/* determine if x is in between a and b, exclusive of a, inclusive of b */

static inline int64_t is_between(int64_t x, int64_t a, int64_t b)
{
    if (a < b) {
        return x > a && x <= b;
    }
    return x < a && x >= b;
}

static uint64_t cadence_timer_get_match_register(CadenceTimerState *s, int i)
{
    uint64_t match_register = s->reg_match[i];

    if (!s->reg_is_32bits) {
        match_register <<= 16;
    }
    return match_register;
}

static uint64_t cadence_timer_get_interval(CadenceTimerState *s)
{
    uint64_t interval = 0x100000000ULL;

    if (s->reg_count & COUNTER_CTRL_INT) {
        interval = s->reg_interval;
        interval++;
        if (!s->reg_is_32bits) {
            interval <<= 16;
        }
    }

    return interval;
}

static void cadence_timer_run(CadenceTimerState *s)
{
    int i;
    int64_t event_interval, next_value, next_time, interval;

    assert(s->cpu_time_valid); /* cadence_timer_sync must be called first */

    if (s->reg_count & COUNTER_CTRL_DIS) {
        s->cpu_time_valid = 0;
        return;
    }

    /* figure out what's going to happen next (rollover or match) */
    interval = cadence_timer_get_interval(s);
    next_value = (s->reg_count & COUNTER_CTRL_DEC) ? -1ULL : interval;

    for (i = 0; i < 3; ++i) {
        int64_t cand = cadence_timer_get_match_register(s, i);

        if (is_between(cand, (uint64_t)s->reg_value, next_value)) {
            next_value = cand;
        }
    }
    DB_PRINT("next timer event value: %09llx\n",
            (unsigned long long)next_value);

    event_interval = next_value - (int64_t)s->reg_value;
    event_interval = (event_interval < 0) ? -event_interval : event_interval;

    next_time = cadence_timer_get_ns(s, event_interval);
    if (next_time) {
        timer_mod(s->timer, s->cpu_time + next_time);
    }
}

static void cadence_timer_sync(CadenceTimerState *s)
{
    int i;
    int64_t r, x;
    int64_t interval;
    uint64_t old_time = s->cpu_time;

    interval = cadence_timer_get_interval(s);
    s->cpu_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    DB_PRINT("cpu time: %lld ns\n", (long long)old_time);

    if (!s->cpu_time_valid || old_time == s->cpu_time) {
        s->cpu_time_valid = 1;
        return;
    }

    r = (int64_t)cadence_timer_get_steps(s, s->cpu_time - old_time);
    x = (int64_t)s->reg_value + ((s->reg_count & COUNTER_CTRL_DEC) ? -r : r);

    for (i = 0; i < 3; ++i) {
        int64_t m = cadence_timer_get_match_register(s, i);

        if (m > interval) {
            continue;
        }
        /* check to see if match event has occurred. check m +/- interval
         * to account for match events in wrap around cases */
        if (is_between(m, s->reg_value, x) ||
            is_between(m + interval, s->reg_value, x) ||
            is_between(m - interval, s->reg_value, x)) {
            s->reg_intr |= (2 << i);
        }
    }
    if ((x < 0) || (x >= interval)) {
        s->reg_intr |= (s->reg_count & COUNTER_CTRL_INT) ?
            COUNTER_INTR_IV : COUNTER_INTR_OV;
    }
    while (x < 0) {
        x += interval;
    }
    s->reg_value = (uint32_t)(x % interval);
    cadence_timer_update(s);
}

static void cadence_timer_tick(void *opaque)
{
    CadenceTimerState *s = opaque;

    DB_PRINT("\n");
    cadence_timer_sync(s);
    cadence_timer_run(s);
}

static uint64_t cadence_clock_rate_update(void *opaque, uint64_t in_rate)
{
    CadenceTTCState *s = CADENCE_TTC(opaque);
    int i;

    for (i = 0; i < 3; i++) {
        cadence_timer_sync(&s->timer[i]);
        s->timer[i].freq = in_rate;
        cadence_timer_run(&s->timer[i]);
        cadence_timer_update(&s->timer[i]);
    }
    return in_rate;
}

static uint32_t cadence_ttc_read_imp(void *opaque, hwaddr offset)
{
    CadenceTimerState *s = cadence_timer_from_addr(opaque, offset);
    uint32_t value;

    cadence_timer_sync(s);
    cadence_timer_run(s);

    switch (offset) {
    case 0x00: /* clock control */
    case 0x04:
    case 0x08:
        return s->reg_clock;

    case 0x0c: /* counter control */
    case 0x10:
    case 0x14:
        return s->reg_count;

    case 0x18: /* counter value */
    case 0x1c:
    case 0x20:
        if (s->reg_is_32bits) {
            return s->reg_value;
        } else {
            return (uint16_t)(s->reg_value >> 16);
        }
    case 0x24: /* reg_interval counter */
    case 0x28:
    case 0x2c:
        return s->reg_interval;

    case 0x30: /* match 1 counter */
    case 0x34:
    case 0x38:
        return s->reg_match[0];

    case 0x3c: /* match 2 counter */
    case 0x40:
    case 0x44:
        return s->reg_match[1];

    case 0x48: /* match 3 counter */
    case 0x4c:
    case 0x50:
        return s->reg_match[2];

    case 0x54: /* interrupt register */
    case 0x58:
    case 0x5c:
        /* cleared after read */
        value = s->reg_intr;
        s->reg_intr = 0;
        cadence_timer_update(s);
        return value;
    case 0x60: /* interrupt enable */
    case 0x64:
    case 0x68:
        return s->reg_intr_en;

    case 0x6c:
    case 0x70:
    case 0x74:
        return s->reg_event_ctrl;

    case 0x78:
    case 0x7c:
    case 0x80:
        return s->reg_event;

    default:
        return 0;
    }
}

static uint64_t cadence_ttc_read(void *opaque, hwaddr offset,
    unsigned size)
{
    uint32_t ret = cadence_ttc_read_imp(opaque, offset);

    DB_PRINT("addr: %08x data: %08x\n", (unsigned)offset, (unsigned)ret);
    return ret;
}

static void cadence_ttc_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    CadenceTimerState *s = cadence_timer_from_addr(opaque, offset);

    DB_PRINT("addr: %08x data %08x\n", (unsigned)offset, (unsigned)value);

    cadence_timer_sync(s);

    switch (offset) {
    case 0x00: /* clock control */
    case 0x04:
    case 0x08:
        s->reg_clock = value & 0x3F;
        break;

    case 0x0c: /* counter control */
    case 0x10:
    case 0x14:
        if (value & COUNTER_CTRL_RST) {
            s->reg_value = 0;
        }
        s->reg_count = value & 0x3f & ~COUNTER_CTRL_RST;
        break;

    case 0x24: /* interval register */
    case 0x28:
    case 0x2c:
        s->reg_interval = value;
        if (!s->reg_is_32bits) {
            s->reg_interval &= 0xffff;
        }
        break;

    case 0x30: /* match register */
    case 0x34:
    case 0x38:
        s->reg_match[0] = value;
        if (!s->reg_is_32bits) {
            s->reg_match[0] &= 0xffff;
        }
        break;

    case 0x3c: /* match register */
    case 0x40:
    case 0x44:
        s->reg_match[1] = value;
        if (!s->reg_is_32bits) {
            s->reg_match[1] &= 0xffff;
        }
        break;

    case 0x48: /* match register */
    case 0x4c:
    case 0x50:
        s->reg_match[2] = value;
        if (!s->reg_is_32bits) {
            s->reg_match[2] &= 0xffff;
        }
        break;

    case 0x54: /* interrupt register */
    case 0x58:
    case 0x5c:
        break;
    case 0x60: /* interrupt enable */
    case 0x64:
    case 0x68:
        s->reg_intr_en = value & 0x3f;
        break;

    case 0x6c: /* event control */
    case 0x70:
    case 0x74:
        s->reg_event_ctrl = value & 0x07;
        break;

    default:
        return;
    }

    cadence_timer_run(s);
    cadence_timer_update(s);
}

static const MemoryRegionOps cadence_ttc_ops = {
    .read = cadence_ttc_read,
    .write = cadence_ttc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void cadence_timer_reset(CadenceTimerState *s)
{
   s->reg_count = 0x21;
}

static void cadence_timer_init(uint32_t freq, CadenceTimerState *s)
{
    memset(s, 0, sizeof(CadenceTimerState));
    s->freq = freq;
    s->reg_is_32bits = 0;

    cadence_timer_reset(s);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, cadence_timer_tick, s);
}

static void cadence_ttc_init(Object *obj)
{
    CadenceTTCState *s = CADENCE_TTC(obj);
    int i;

    for (i = 0; i < 3; ++i) {
        cadence_timer_init(133000000, &s->timer[i]);
        sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->timer[i].irq);
    }

    object_initialize(&s->clock_in, sizeof(s->clock_in), TYPE_CLOCK);
    qemu_clk_device_add_clock(DEVICE(obj), &s->clock_in, "clock_in");
    qemu_clk_set_callback(&s->clock_in, cadence_clock_rate_update, obj);

    memory_region_init_io(&s->iomem, obj, &cadence_ttc_ops, s,
                          "timer", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}

static void zynqmp_ttc_init(Object *obj)
{
    CadenceTTCState *s = CADENCE_TTC(obj);
    int i;

    for (i = 0; i < 3; ++i) {
        s->timer[i].reg_is_32bits = 1;
    }
}

static int cadence_timer_pre_save(void *opaque)
{
    cadence_timer_sync((CadenceTimerState *)opaque);

    return 0;
}

static int cadence_timer_post_load(void *opaque, int version_id)
{
    CadenceTimerState *s = opaque;

    s->cpu_time_valid = 0;
    cadence_timer_sync(s);
    cadence_timer_run(s);
    cadence_timer_update(s);
    return 0;
}

static const VMStateDescription vmstate_cadence_timer = {
    .name = "cadence_timer",
    .version_id = 2,
    .minimum_version_id = 1,
    .pre_save = cadence_timer_pre_save,
    .post_load = cadence_timer_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(reg_clock, CadenceTimerState),
        VMSTATE_UINT32(reg_count, CadenceTimerState),
        VMSTATE_UINT32(reg_value, CadenceTimerState),
        VMSTATE_UINT32(reg_interval, CadenceTimerState),
        VMSTATE_UINT32_ARRAY(reg_match, CadenceTimerState, 3),
        VMSTATE_UINT32(reg_intr, CadenceTimerState),
        VMSTATE_UINT32(reg_intr_en, CadenceTimerState),
        VMSTATE_UINT32(reg_event_ctrl, CadenceTimerState),
        VMSTATE_UINT32(reg_event, CadenceTimerState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_cadence_ttc = {
    .name = "cadence_TTC",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(timer, CadenceTTCState, 3, 0,
                            vmstate_cadence_timer,
                            CadenceTimerState),
        VMSTATE_END_OF_LIST()
    }
};

static void cadence_ttc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_cadence_ttc;
}

static const TypeInfo cadence_ttc_info = {
    .name  = TYPE_CADENCE_TTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(CadenceTTCState),
    .instance_init = cadence_ttc_init,
    .class_init = cadence_ttc_class_init,
};

static const TypeInfo zynqmp_ttc_info = {
    .name  = TYPE_ZYNQMP_TTC,
    .parent = TYPE_CADENCE_TTC,
    .instance_size  = sizeof(CadenceTTCState),
    .instance_init = zynqmp_ttc_init,
    .class_init = cadence_ttc_class_init,
};

static void cadence_ttc_register_types(void)
{
    type_register_static(&cadence_ttc_info);
    type_register_static(&zynqmp_ttc_info);
}

type_init(cadence_ttc_register_types)
