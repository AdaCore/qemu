/*
 * QEMU GRLIB GPTimer Emulator
 *
 * Copyright (c) 2010-2011 AdaCore
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
#include "qemu-timer.h"

//#define DEBUG_TIMER

#ifdef DEBUG_TIMER
#define DPRINTF(fmt, ...)                                       \
    do { printf("GPTIMER: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define UNIT_REG_SIZE    16     /* Size of memory mapped regs for the unit */
#define GPTIMER_REG_SIZE 16     /* Size of memory mapped regs for a GPTimer */

#define GPTIMER_MAX_TIMERS 8

/* GPTimer Config register fields */
#define GPTIMER_ENABLE      (1 << 0)
#define GPTIMER_RESTART     (1 << 1)
#define GPTIMER_LOAD        (1 << 2)
#define GPTIMER_INT_ENABLE  (1 << 3)
#define GPTIMER_INT_PENDING (1 << 4)
#define GPTIMER_CHAIN       (1 << 5) /* Not supported */
#define GPTIMER_DEBUG_HALT  (1 << 6) /* Not supported */

/* Memory mapped register offsets */
#define SCALER_OFFSET         0x00
#define SCALER_RELOAD_OFFSET  0x04
#define CONFIG_OFFSET         0x08
#define COUNTER_OFFSET        0x00
#define COUNTER_RELOAD_OFFSET 0x04
#define TIMER_BASE            0x10

typedef struct GPTimer     GPTimer;
typedef struct GPTimerUnit GPTimerUnit;

struct GPTimer {
    QEMUBH *bh;
    struct ptimer_state *ptimer;

    qemu_irq     irq;
    int          id;
    GPTimerUnit *unit;

    /* registers */
    uint32_t counter;
    uint32_t reload;
    uint32_t config;
};

struct GPTimerUnit {
    SysBusDevice  busdev;

    uint32_t nr_timers;         /* Number of timers available */
    uint32_t freq_hz;           /* System frequency */
    uint32_t irq_line;          /* Base irq line */

    GPTimer *timers;

    /* registers */
    uint32_t scaler;
    uint32_t reload;
    uint32_t config;
};

static void grlib_gptimer_enable(GPTimer *timer)
{
    assert(timer != NULL);


    ptimer_stop(timer->ptimer);

    if (!(timer->config & GPTIMER_ENABLE)) {
        /* Timer disabled */
        DPRINTF("%s id:%d Timer disabled (config 0x%x)\n", __func__,
                timer->id, timer->config);
        return;
    }

    /* ptimer is triggered when the counter reach 0 but GPTimer is triggered at
       underflow. Set count + 1 to simulate the GPTimer behavior. */

    DPRINTF("%s id:%d set count 0x%x and run\n",
            __func__,
            timer->id,
            timer->counter + 1);

    ptimer_set_count(timer->ptimer, timer->counter + 1);
    ptimer_run(timer->ptimer, 1);
}

static void grlib_gptimer_restart(GPTimer *timer)
{
    assert(timer != NULL);

    DPRINTF("%s id:%d reload val: 0x%x\n", __func__, timer->id, timer->reload);

    timer->counter = timer->reload;
    grlib_gptimer_enable(timer);
}

static void grlib_gptimer_set_scaler(GPTimerUnit *unit, uint32_t scaler)
{
    int i = 0;
    uint32_t value = 0;

    assert(unit != NULL);

    if (scaler > 0) {
        value = unit->freq_hz / (scaler + 1);
    } else {
        value = unit->freq_hz;
    }

    DPRINTF("%s scaler:%d freq:0x%x\n", __func__, scaler, value);

    for (i = 0; i < unit->nr_timers; i++) {
        ptimer_set_freq(unit->timers[i].ptimer, value);
    }
}

static void grlib_gptimer_hit(void *opaque)
{
    GPTimer *timer = opaque;
    assert(timer != NULL);

    DPRINTF("%s id:%d\n", __func__, timer->id);

    /* Timer expired */

    if (timer->config & GPTIMER_INT_ENABLE) {
        /* Set the pending bit (only unset by write in the config register) */
        timer->config |= GPTIMER_INT_PENDING;
        qemu_irq_pulse(timer->irq);
    }

    if (timer->config & GPTIMER_RESTART) {
        grlib_gptimer_restart(timer);
    }
}

static uint32_t grlib_gptimer_readl(void *opaque, target_phys_addr_t addr)
{
    GPTimerUnit        *unit  = opaque;
    target_phys_addr_t  timer_addr;
    int                 id;
    uint32_t            value = 0;

    addr &= 0xff;

    /* Unit registers */
    switch (addr) {
    case SCALER_OFFSET:
        DPRINTF("%s scaler: 0x%x\n", __func__, unit->scaler);
        return unit->scaler;

    case SCALER_RELOAD_OFFSET:
        DPRINTF("%s reload: 0x%x\n", __func__, unit->reload);
        return unit->reload;

    case CONFIG_OFFSET:
        DPRINTF("%s unit config: 0x%x\n", __func__, unit->config);
        return unit->config;

    default:
        break;
    }

    timer_addr = (addr % TIMER_BASE);
    id         = (addr - TIMER_BASE) / TIMER_BASE;

    if (id >= 0 && id < unit->nr_timers) {

        /* GPTimer registers */
        switch (timer_addr) {
        case COUNTER_OFFSET:
            value = ptimer_get_count(unit->timers[id].ptimer);
            DPRINTF("%s counter value for timer %d: 0x%x\n",
                    __func__, id, value);
            return value;

        case COUNTER_RELOAD_OFFSET:
            value = unit->timers[id].reload;
            DPRINTF("%s reload value for timer %d: 0x%x\n",
                    __func__, id, value);
            return value;

        case CONFIG_OFFSET:
            DPRINTF("%s config for timer %d: 0x%x\n",
                    __func__, id, unit->timers[id].config);
            return unit->timers[id].config;

        default:
            break;
        }

    }

    DPRINTF("read unknown register " TARGET_FMT_plx "\n", addr);
    return 0;
}

static void
grlib_gptimer_writel(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    GPTimerUnit        *unit = opaque;
    target_phys_addr_t  timer_addr;
    int                 id;

    addr &= 0xff;

    /* Unit registers */
    switch (addr) {
    case SCALER_OFFSET:
        value &= 0xFFFF; /* clean up the value */
        unit->scaler = value;
        return;

    case SCALER_RELOAD_OFFSET:
        value &= 0xFFFF; /* clean up the value */
        unit->reload = value;
        grlib_gptimer_set_scaler(unit, value);
        return;

    case CONFIG_OFFSET:
        /* Read Only (disable timer freeze not supported) */
        return;

    default:
        break;
    }

    timer_addr = (addr % TIMER_BASE);
    id         = (addr - TIMER_BASE) / TIMER_BASE;

    if (id >= 0 && id < unit->nr_timers) {

        /* GPTimer registers */
        switch (timer_addr) {
        case COUNTER_OFFSET:
            DPRINTF("%s counter value for timer %d: 0x%x\n",
                    __func__, id, value);
            unit->timers[id].counter = value;
            grlib_gptimer_enable(&unit->timers[id]);
            return;

        case COUNTER_RELOAD_OFFSET:
            DPRINTF("%s reload value for timer %d: 0x%x\n",
                    __func__, id, value);
            unit->timers[id].reload = value;
            return;

        case CONFIG_OFFSET:
            DPRINTF("%s config for timer %d: 0x%x\n", __func__, id, value);

            if (value & GPTIMER_INT_PENDING) {
                /* clear pending bit */
                value &= ~GPTIMER_INT_PENDING;
            } else {
                /* keep pending bit */
                value |= unit->timers[id].config & GPTIMER_INT_PENDING;
            }

            unit->timers[id].config = value;

            /* gptimer_restart calls gptimer_enable, so if "enable" and "load"
               bits are present, we just have to call restart. */

            if (value & GPTIMER_LOAD) {
                grlib_gptimer_restart(&unit->timers[id]);
            } else if (value & GPTIMER_ENABLE) {
                grlib_gptimer_enable(&unit->timers[id]);
            }

            /* These fields must always be read as 0 */
            value &= ~(GPTIMER_LOAD & GPTIMER_DEBUG_HALT);

            unit->timers[id].config = value;
            return;

        default:
            break;
        }

    }

    DPRINTF("write unknown register " TARGET_FMT_plx "\n", addr);
}

static CPUReadMemoryFunc * const grlib_gptimer_read[] = {
    NULL, NULL, grlib_gptimer_readl,
};

static CPUWriteMemoryFunc * const grlib_gptimer_write[] = {
    NULL, NULL, grlib_gptimer_writel,
};

static void grlib_gptimer_reset(DeviceState *d)
{
    GPTimerUnit *unit = container_of(d, GPTimerUnit, busdev.qdev);
    int          i    = 0;

    assert(unit != NULL);

    unit->scaler = 0;
    unit->reload = 0;
    unit->config = 0;

    unit->config  = unit->nr_timers;
    unit->config |= unit->irq_line << 3;
    unit->config |= 1 << 8;     /* separate interrupt */
    unit->config |= 1 << 9;     /* Disable timer freeze */


    for (i = 0; i < unit->nr_timers; i++) {
        GPTimer *timer = &unit->timers[i];

        timer->counter = 0;
        timer->reload = 0;
        timer->config = 0;
        ptimer_stop(timer->ptimer);
        ptimer_set_count(timer->ptimer, 0);
        ptimer_set_freq(timer->ptimer, unit->freq_hz);
    }
}

static int grlib_gptimer_init(SysBusDevice *dev)
{
    GPTimerUnit  *unit = FROM_SYSBUS(typeof(*unit), dev);
    unsigned int  i;
    int           timer_regs;

    assert(unit->nr_timers > 0);
    assert(unit->nr_timers <= GPTIMER_MAX_TIMERS);

    unit->timers = qemu_mallocz(sizeof unit->timers[0] * unit->nr_timers);

    for (i = 0; i < unit->nr_timers; i++) {
        GPTimer *timer = &unit->timers[i];

        timer->unit   = unit;
        timer->bh     = qemu_bh_new(grlib_gptimer_hit, timer);
        timer->ptimer = ptimer_init(timer->bh);
        timer->id     = i;

        /* One IRQ line for each timer */
        sysbus_init_irq(dev, &timer->irq);

        ptimer_set_freq(timer->ptimer, unit->freq_hz);
    }

    timer_regs = cpu_register_io_memory(grlib_gptimer_read,
                                        grlib_gptimer_write,
                                        unit);
    if (timer_regs < 0) {
        return -1;
    }

    sysbus_init_mmio(dev, UNIT_REG_SIZE + GPTIMER_REG_SIZE * unit->nr_timers,
                     timer_regs);
    return 0;
}

static SysBusDeviceInfo grlib_gptimer_info = {
    .init       = grlib_gptimer_init,
    .qdev.name  = "grlib,gptimer",
    .qdev.reset = grlib_gptimer_reset,
    .qdev.size  = sizeof(GPTimerUnit),
    .qdev.props = (Property[]) {
        DEFINE_PROP_UINT32("frequency", GPTimerUnit, freq_hz,   40000000),
        DEFINE_PROP_UINT32("irq-line",  GPTimerUnit, irq_line,  8),
        DEFINE_PROP_UINT32("nr-timers", GPTimerUnit, nr_timers, 2),
        DEFINE_PROP_END_OF_LIST()
    }
};

static void grlib_gptimer_register(void)
{
    sysbus_register_withprop(&grlib_gptimer_info);
}

device_init(grlib_gptimer_register)
