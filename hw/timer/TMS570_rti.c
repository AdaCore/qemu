/*
 * Arm Texas-Instrument Real-Time Interrupt (RTI)
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
#include "qemu/timer.h"
#include "hw/ptimer.h"
#include "qemu/main-loop.h"

/* #define DEBUG_RTI */

#ifdef DEBUG_RTI
#define DPRINTF(fmt, ...) do { printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;

    int64_t clk_start[2];

    struct ptimer_state *cmp[4];
    QEMUBH *cmp_bh[4];

    qemu_irq irq_ovl[2];
    qemu_irq irq_cmp[4];

    uint32_t freq;

    uint32_t GCTRL;
    uint32_t COMPCTRL;

    /* Time when counter is started.  */
    uint32_t UPC[2];
    uint32_t FRC[2];
    uint64_t ns_start[2];

    uint32_t CPUPC[2];

    uint32_t COMP[4];
    uint32_t UDCP[4];

    uint32_t INTENA;
    uint32_t INTFLAG;
} rti_state;

#define TYPE_RTI "RTI"
#define RTI(obj) \
    OBJECT_CHECK(rti_state, (obj), TYPE_RTI)

#define GCTRL_CNT1_EN 0x2
#define GCTRL_CNT0_EN 0x1

#define COMPCTRL_SEL0 0x0001
#define COMPCTRL_SEL1 0x0010
#define COMPCTRL_SEL2 0x0100
#define COMPCTRL_SEL3 0x1000

#define INTFLAG_OVL0 0x00020000
#define INTFLAG_OVL1 0x00040000
#define INTFLAG_INT0 0x00000001
#define INTFLAG_INT1 0x00000002
#define INTFLAG_INT2 0x00000004
#define INTFLAG_INT3 0x00000008

/* Return the counter_id assigned to this compare_id */
#define CMP_ASSIGNED_TO(s, cmp_id) (!!((s)->COMPCTRL & (1 << ((cmp_id) * 4))))

/* Return true if the compare interrupt is enabled */
#define CMP_INT_ENABLED(s, cmp_id) (!!((s)->INTENA & (1 << (cmp_id))))

#define CMP_SET_FLAG(s, cmp_id) ((s)->INTFLAG |= (1 << (cmp_id)))
#define CMP_CLEAR_FLAG(s, cmp_id) ((s)->INTFLAG &= ~(1 << (cmp_id)))

/* Return true if overflow interrupt is enabled */
#define OVERFLOW_INT_ENABLED(s, cnt_id)         \
(!!((s)->INTENA & (1 << (17 + (cnt_id)))))

/* Return true if the counter is enabled */
#define CNT_IS_ENABLED(s, cnt_id) (!!((s)->GCTRL & (1 << (cnt_id))))

static void rti_set_compare(rti_state *s, int cmp_id, uint64_t now, uint32_t value);

static int64_t rti_get_prescaler(rti_state *s, int cnt_id)
{
    uint32_t scalar = s->CPUPC[cnt_id];
    return (scalar == 0 ? (1ULL << 32) : scalar) + 1;
}

/* Time (in ns) of a counter whose up counter is UPC and free running counter
   is FRC.  */
static int64_t rti_get_counter_ns(rti_state *s, int cnt_id, uint32_t upc, uint32_t frc)
{
    int64_t sc = rti_get_prescaler(s, cnt_id);

    return 1000000000ULL * (sc * frc + upc) / s->freq;
}

static uint32_t rti_read_counter(rti_state *s, int cnt_id)
{
    /* Save up-counter value */
    if (CNT_IS_ENABLED(s, cnt_id)) {
	int64_t sc = rti_get_prescaler(s, cnt_id);
	uint64_t el;

	el = (qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->ns_start[cnt_id]);
	el = muldiv64(el, s->freq, 1000000000ULL);
	s->UPC[cnt_id] = el % sc;
	return el / sc;
    } else {
	return s->FRC[cnt_id];
    }
}

static void rti_enable_counter(rti_state *s, int cnt_id)
{
    int i;
    uint64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    DPRINTF("%s: cnt_id:%d\n", __func__, cnt_id);

    s->ns_start[cnt_id] =
	now - rti_get_counter_ns(s, cnt_id, s->UPC[cnt_id], s->FRC[cnt_id]);
    DPRINTF("%s(%d): start=%lu\n", __func__, cnt_id, s->ns_start[cnt_id]);

    /* Update compare timers */
    for (i = 0; i < 4; i++) {
        /* Check if the compare register is assigned to this counter */
        if (CMP_ASSIGNED_TO(s, i) == cnt_id
	    && CMP_INT_ENABLED(s, i)) {
            rti_set_compare(s, i, now, s->COMP[i]);
        }
    }
}

static void rti_disable_counter(rti_state *s, int cnt_id)
{
    int i;

    DPRINTF("%s: cnt_id:%d\n", __func__, cnt_id);

    s->FRC[cnt_id] = rti_read_counter(s, cnt_id); /* Also set UPC */

    /* Stop compare timers */
    for (i = 0; i < 4; i++) {
        /* Check if the compare register is assigned to this counter */
        if (CMP_ASSIGNED_TO(s, i) == cnt_id
	    && CMP_INT_ENABLED(s, i)) {
            ptimer_stop(s->cmp[i]);
        }
    }
}

static void rti_compare_hit(rti_state *s, int cmp_id)
{
    int cnt_id = CMP_ASSIGNED_TO(s, cmp_id);

    DPRINTF("%s: cmp_id:%d\n", __func__, cmp_id);

    ptimer_stop(s->cmp[cmp_id]);

    if (CNT_IS_ENABLED(s, cnt_id)) {
        CMP_SET_FLAG(s, cmp_id);

        if (CMP_INT_ENABLED(s, cmp_id)) {
            DPRINTF("%s: qemu_irq_raise(s->irq_cmp[%d]);\n", __func__, cmp_id);
            qemu_irq_raise(s->irq_cmp[cmp_id]);
        }

        /* Restart compare timer with updated value */
        DPRINTF("%s: Restart compare timer with updated value 0x%x + 0x%x\n",
                __func__, s->COMP[cmp_id], s->UDCP[cmp_id]);
        rti_set_compare(s, cmp_id, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
			s->COMP[cmp_id] + s->UDCP[cmp_id]);
    } else {
        /* The assigned counter was disabled, so this interrupt doesn't mean
         * anything.
         */
        hw_error("%s:The assigned counter was disabled, so this interrupt "
                 "doesn't mean anything\n", __func__);
    }
}

#define DEF_RTI_COMPARE_HIT(n) void rti_compare_hit_##n(void *opaque);  \
 void rti_compare_hit_##n(void *opaque)                                 \
 {                                                                      \
     rti_compare_hit((rti_state *)opaque, n);                           \
 }

DEF_RTI_COMPARE_HIT(0)
DEF_RTI_COMPARE_HIT(1)
DEF_RTI_COMPARE_HIT(2)
DEF_RTI_COMPARE_HIT(3)

static void rti_set_compare(rti_state *s, int cmp_id, uint64_t now, uint32_t value)
{
    int cnt_id = CMP_ASSIGNED_TO(s, cmp_id);
    uint64_t timeout;

    DPRINTF("%s: cmp_id:%d cnt_id:%d value:%u\n", __func__, cmp_id, cnt_id,
            value);

    s->COMP[cmp_id] = value;
    ptimer_stop(s->cmp[cmp_id]);

    if (!CNT_IS_ENABLED(s, cnt_id)) {
        /* The counter is disabled, do nothing */
        DPRINTF("%s: Counter disabled\n", __func__);
        return;
    }

    timeout = s->ns_start[cnt_id] + rti_get_counter_ns(s, cnt_id, 0, value);

    if (timeout >= now) {
        /* Set the same frequency as the counter */
	DPRINTF ("%s: delta=%lu\n", __func__, timeout - now);
        ptimer_set_period(s->cmp[cmp_id], timeout - now);
        ptimer_set_count(s->cmp[cmp_id], 1);
        ptimer_run(s->cmp[cmp_id], 1);
    } else {
        /* Do nothing and, wait for counter overflow */
        DPRINTF("%s: value expired\n", __func__);
    }
}

static uint64_t rti_read(void *opaque, hwaddr offset, unsigned size)
{
    rti_state *s = (rti_state *)opaque;
    int        cnt_id;
    int        cmp_id;
    int        reg_offset;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u\n", __func__, offset, size);

    switch (offset) {
    case 0x00: /* RTIGCTRL */
        return s->GCTRL;
    case 0x0C: /* RTICOMPCTRL */
        return s->COMPCTRL;
    case 0x10 ... 0x44: /* Counter 0 and 1 */
        cnt_id = (offset > 0x24) ? 1 : 0;
        reg_offset = offset - (cnt_id ? 0x30 : 0x10);

        switch (reg_offset) {
        case 0x00: /* Free Running */
            return rti_read_counter(s, cnt_id);
        case 0x04: /* Up Counter */
	    return s->UPC[cnt_id];
            break;
        case 0x08: /* Compare Up Counter */
            return s->CPUPC[cnt_id];
        case 0x10: /* Capture Free running */
        case 0x14: /* Capture Up Counter */
            hw_error("%s: Capture not supported\n", __func__);
            break;
        default:
            hw_error("%s: Bad offset 0x%x, reg_offset:0x%x\n", __func__,
                     (int)offset, reg_offset);
        }
        break;

    case 0x50 ... 0x6C: /* Compare 0, 1, 2, 3 */
        cmp_id = (offset - 0x50) / 8;
        reg_offset = (offset - 0x50) % 8;

        switch (reg_offset) {
        case 0x0: /* Compare */
            return s->COMP[cmp_id];
            break;
        case 0x4: /* Update Compare */
            return s->UDCP[cmp_id];
            break;
        }
        break;
    case 0x70 ... 0x74:
        hw_error("%s: Timebase not supported\n", __func__);
        break;

    case 0x80: /* SETINTENA */
    case 0x84: /* CLEARINTENA */
        return s->INTENA;
    case 0x88: /* INTFLAG */
        return s->INTFLAG;

    case 0x90 ... 0xA8:
        hw_error("%s: Register not supported 0x%x\n", __func__, (int)offset);
        break;
    default:
        hw_error("%s: Bad offset 0x%x\n", __func__, (int)offset);
    }
    return 0;
}

static void rti_write(void *opaque, hwaddr offset, uint64_t val, unsigned size)
{
    rti_state     *s = (rti_state *)opaque;
    uint32_t       prev;
    uint32_t       cleared_flags;
    int            cnt_id;
    int            cmp_id;
    int            reg_offset;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u val:0x%" PRIx64 "\n",
            __func__, offset, size, val);

    switch (offset) {
    case 0x00: /* RTIGCTRL */
        prev = s->GCTRL;
        s->GCTRL = val & 0x00038003;
        if ((val & GCTRL_CNT0_EN) != (prev & GCTRL_CNT0_EN)) {
            if (val & GCTRL_CNT0_EN) {
                rti_enable_counter(s, 0);
            } else {
                rti_disable_counter(s, 0);
            }
        }
        if ((val & GCTRL_CNT1_EN) != (prev & GCTRL_CNT1_EN)) {
            if (val & GCTRL_CNT1_EN) {
                rti_enable_counter(s, 1);
            } else {
                rti_disable_counter(s, 1);
            }
        }
        break;

    case 0x04: /* RTITBCTRL */
        break;

    case 0x0C: /* RTICOMPCTRL */
        prev = s->COMPCTRL;
        s->COMPCTRL = val & 0x1111;

        if (s->COMPCTRL != prev) {
            /* Compare assignment changed, re-enable all the counters. It's an
             * easy way to update all the frequencies, timer value, etc...
             */
            if (CNT_IS_ENABLED(s, 0)) {
                rti_enable_counter(s, 0);
                rti_disable_counter(s, 0);
            }
            if (CNT_IS_ENABLED(s, 1)) {
                rti_enable_counter(s, 1);
                rti_disable_counter(s, 1);
            }
        }
        break;

    case 0x10 ... 0x44: /* Counter 0 and 1 */
        cnt_id = (offset > 0x24) ? 1 : 0;
        reg_offset = offset - (cnt_id ? 0x30 : 0x10);

        switch (reg_offset) {
        case 0x00: /* Free Running */
	    if (CNT_IS_ENABLED(s, cnt_id)) {
		hw_error("%s: setting FRC%d while enabled\n",
			 __func__, cnt_id);
	    }
	    s->FRC[cnt_id] = val;
            break;
        case 0x04: /* Up Counter */
	    if (CNT_IS_ENABLED(s, cnt_id)) {
		hw_error("%s: setting UPC%d while enabled\n",
			 __func__, cnt_id);
	    }
	    s->UPC[cnt_id] = val;
            break;
        case 0x08: /* Compare Up Counter */
            s->CPUPC[cnt_id] = val;
	    if (CNT_IS_ENABLED(s, cnt_id)) {
		rti_disable_counter(s, cnt_id);
		rti_enable_counter(s, cnt_id);
	    }
            break;
        case 0x10: /* Capture Free running */
        case 0x14: /* Capture Up Counter */
            hw_error("%s: Capture not supported\n", __func__);
            break;
        default:
            hw_error("%s: Bad offset 0x%x, reg_offset:0x%x\n", __func__,
                     (int)offset, reg_offset);
        }
        break;

    case 0x50 ... 0x6C: /* Compare 0, 1, 2, 3 */
        cmp_id = (offset - 0x50) / 8;
        reg_offset = (offset - 0x50) % 8;

        switch (reg_offset) {
        case 0x0: /* Compare */
            rti_set_compare(s, cmp_id,
			    qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL), val);
            break;
        case 0x4: /* Update Compare */
            s->UDCP[cmp_id] = val;
            break;
        }
        break;
    case 0x70 ... 0x74:
        hw_error("%s: Timebase not supported\n", __func__);
        break;

    case 0x80: /* SETINTENA */
        s->INTENA |= val & 0x00070F0F;
        break;
    case 0x84: /* CLEARINTENA */
        s->INTENA &= ~val;
        break;
    case 0x88: /* INTFLAG */
        cleared_flags = s->INTFLAG & val;

        s->INTFLAG &= ~val;

        if (cleared_flags & INTFLAG_OVL0) {
            qemu_irq_lower(s->irq_ovl[0]);
        }
        if (cleared_flags & INTFLAG_OVL1) {
            qemu_irq_lower(s->irq_ovl[1]);
        }
        if (cleared_flags & INTFLAG_INT0) {
            qemu_irq_lower(s->irq_cmp[0]);
        }
        if (cleared_flags & INTFLAG_INT1) {
            qemu_irq_lower(s->irq_cmp[1]);
        }
        if (cleared_flags & INTFLAG_INT2) {
            qemu_irq_lower(s->irq_cmp[2]);
        }
        if (cleared_flags & INTFLAG_INT3) {
            qemu_irq_lower(s->irq_cmp[3]);
        }
        break;

    case 0x90 ... 0xA8:
        hw_error("%s: Register not supported 0x%x\n", __func__, (int)offset);
        break;
    default:
        hw_error("%s: Bad offset 0x%x\n", __func__, (int)offset);
        return;
    }
}

static const MemoryRegionOps rti_ops = {
    .read = rti_read,
    .write = rti_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void rti_reset(DeviceState *d)
{
    rti_state *s = RTI(d);
    int i;

    s->GCTRL = 0;
    s->COMPCTRL = 0;

    for (i = 0; i < 2; i++) {
        s->FRC[i] = 0;
        s->CPUPC[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        s->COMP[i] = 0;
        s->UDCP[i] = 0;
    }

    s->INTENA = 0;
    s->INTFLAG = 0;

    ptimer_stop(s->cmp[0]);
    ptimer_stop(s->cmp[1]);
    ptimer_stop(s->cmp[2]);
    ptimer_stop(s->cmp[3]);
}

static void rti_realize(DeviceState *dev, Error **errp)
{
    rti_state *s = RTI(dev);
    int i;

    s->cmp_bh[0] = qemu_bh_new(rti_compare_hit_0, s);
    s->cmp_bh[1] = qemu_bh_new(rti_compare_hit_1, s);
    s->cmp_bh[2] = qemu_bh_new(rti_compare_hit_2, s);
    s->cmp_bh[3] = qemu_bh_new(rti_compare_hit_3, s);

    s->cmp[0] = ptimer_init(s->cmp_bh[0], PTIMER_POLICY_DEFAULT);
    s->cmp[1] = ptimer_init(s->cmp_bh[1], PTIMER_POLICY_DEFAULT);
    s->cmp[2] = ptimer_init(s->cmp_bh[2], PTIMER_POLICY_DEFAULT);
    s->cmp[3] = ptimer_init(s->cmp_bh[3], PTIMER_POLICY_DEFAULT);


    for (i = 0; i < 4; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_cmp[i]);
    }
    for (i = 0; i < 2; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_ovl[i]);
    }

    memory_region_init_io(&s->iomem, OBJECT(dev), &rti_ops, s, TYPE_RTI, 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static Property rti_properties[] = {
    DEFINE_PROP_UINT32("freq", rti_state, freq, 100000000), /* 100 MHz */
    DEFINE_PROP_END_OF_LIST(),
};

static void rti_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = rti_realize;
    dc->reset = rti_reset;
    dc->props = rti_properties;
}

static TypeInfo rti_info = {
    .name          = TYPE_RTI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(rti_state),
    .class_init    = rti_class_init,
};

static void rti_register_types(void)
{
    type_register_static(&rti_info);
}

type_init(rti_register_types)
