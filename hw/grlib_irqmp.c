/*
 * QEMU GRLIB IRQMP Emulator
 *
 * (Multiprocessor and extended interrupt not supported)
 *
 * Copyright (c) 2010 AdaCore
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
#include "cpu.h"

#include "grlib.h"

/* #define DEBUG_IRQ */

#ifdef DEBUG_IRQ
#define DPRINTF(fmt, ...)                                       \
    do { printf("IRQMP: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define IRQMP_REG_SIZE 256      /* Size of memory mapped registers */

/* Memory mapped register offsets */
#define LEVEL_OFFSET     0x00
#define PENDING_OFFSET   0x04
#define FORCE0_OFFSET    0x08
#define CLEAR_OFFSET     0x0C
#define MP_STATUS_OFFSET 0x10
#define BROADCAST_OFFSET 0x14
#define MASK_OFFSET      0x40
#define FORCE_OFFSET     0x80
#define EXTENDED_OFFSET  0xC0

IRQMPState grlib_irqmp_state;

static void grlib_irqmp_check_irqs(CPUState *env)
{
    uint32_t pend   = 0;
    uint32_t level0 = 0;
    uint32_t level1 = 0;

    assert(env != NULL);

    /* IRQ for CPU 0 (no SMP support) */
    pend = (grlib_irqmp_state.pending | grlib_irqmp_state.force[0])
        & grlib_irqmp_state.mask[0];


    level0 = pend & ~grlib_irqmp_state.level;
    level1 = pend &  grlib_irqmp_state.level;

    DPRINTF("(pend | force) & mask:0x%04x lvl1:0x%04x lvl0:0x%04x\n",
            pend, level1, level0);

    /* Trigger level1 interrupt first and level0 if there is no level1 */
    if (level1 != 0) {
        env->pil_in = level1;
    } else {
        env->pil_in = level0;
    }

    if (env->pil_in && (env->interrupt_index == 0 ||
                        (env->interrupt_index & ~15) == TT_EXTINT)) {
        unsigned int i;

        for (i = 15; i > 0; i--) {
            if (env->pil_in & (1 << i)) {
                int old_interrupt = env->interrupt_index;

                env->interrupt_index = TT_EXTINT | i;
                if (old_interrupt != env->interrupt_index) {
                    DPRINTF("Set CPU IRQ %d\n", i);
                    cpu_interrupt(env, CPU_INTERRUPT_HARD);
                }
                break;
            }
        }
    } else if (!env->pil_in && (env->interrupt_index & ~15) == TT_EXTINT) {
        DPRINTF("Reset CPU IRQ %d\n", env->interrupt_index & 15);
        env->interrupt_index = 0;
        cpu_reset_interrupt(env, CPU_INTERRUPT_HARD);
    }
}

void grlib_irqmp_ack(CPUSPARCState *env, int intno)
{
    assert(env != NULL);

    uint32_t mask;

    intno &= 15;
    mask = 1 << intno;

    DPRINTF ("grlib_irqmp_ack %d\n", intno);

    /* Clear registers */
    grlib_irqmp_state.pending  &= ~mask;
    grlib_irqmp_state.force[0] &= ~mask; /* Only CPU 0 (No SMP support) */

    grlib_irqmp_check_irqs(env);
}

void grlib_irqmp_set_irq(void *opaque, int irq, int level)
{
    IRQMPState *s = opaque;
    int         i = 0;

    assert(opaque != NULL);
    assert(s->parent != NULL);

    if (level) {
        DPRINTF("Raise CPU IRQ %d\n", irq);

        if (s->broadcast & 1 << irq) {
            /* Broadcasted IRQ */
            for (i = 0; i < IRQMP_MAX_CPU; i++) {
                s->force[i] |= 1 << irq;
            }
        } else {
            s->pending |= 1 << irq;
        }
        grlib_irqmp_check_irqs(s->parent->env);

    } else {

        DPRINTF("Lower CPU IRQ %d\n", irq);
        if (s->broadcast & 1 << irq) {
            /* Broadcasted IRQ */
            for (i = 0; i < IRQMP_MAX_CPU; i++) {
                s->force[i] &= ~(1 << irq);
            }
        } else {
            s->pending &= ~(1 << irq);
        }
        grlib_irqmp_check_irqs(s->parent->env);
    }
}

static uint32_t grlib_irqmp_readl (void *opaque, target_phys_addr_t addr)
{
    IRQMP *irqmp = opaque;

    assert(irqmp != NULL);

    addr -= 0x200;              /* FIXME */

    /* global registers */
    switch (addr)
    {
        case LEVEL_OFFSET:
            return grlib_irqmp_state.level;

        case PENDING_OFFSET:
            return grlib_irqmp_state.pending;

        case FORCE0_OFFSET:
            /* This register is an "alias" for the force register of CPU 0 */
            return grlib_irqmp_state.force[0];

        case CLEAR_OFFSET:
        case MP_STATUS_OFFSET:
            /* Always read as 0 */
            return 0;

        case BROADCAST_OFFSET:
            return grlib_irqmp_state.broadcast;

        default:
            break;
    }

    /* mask registers */
    if (addr >= MASK_OFFSET && addr < FORCE_OFFSET) {
        int cpu = (addr - MASK_OFFSET) / 4;
        assert(cpu >= 0 && cpu < IRQMP_MAX_CPU);

        return grlib_irqmp_state.mask[cpu] ;
    }

    /* force registers */
    if (addr >= FORCE_OFFSET && addr < EXTENDED_OFFSET) {
        int cpu = (addr - FORCE_OFFSET) / 4;
        assert(cpu >= 0 && cpu < IRQMP_MAX_CPU);

        return grlib_irqmp_state.force[cpu];
    }

    /* extended (not supported) */
    if (addr >= EXTENDED_OFFSET && addr < IRQMP_REG_SIZE) {
        int cpu = (addr - EXTENDED_OFFSET) / 4;
        assert(cpu >= 0 && cpu < IRQMP_MAX_CPU);

        return grlib_irqmp_state.extended[cpu];
    }

    DPRINTF("read unknown register 0x%04x\n", (int)addr);
    return 0;
}

static void
grlib_irqmp_writel (void *opaque, target_phys_addr_t addr, uint32_t value)
{
    IRQMP *irqmp = opaque;

    assert(irqmp != NULL);

    addr -= 0x200;              /* FIXME */

    /* global registers */
    switch (addr)
    {
        case LEVEL_OFFSET:
            value &= 0xFFFF << 1; /* clean up the value */
            grlib_irqmp_state.level = value;
            return;

        case PENDING_OFFSET:
            /* Read Only */
            return;

        case FORCE0_OFFSET:
            /* This register is an "alias" for the force register of CPU 0 */

            value &= 0xFFFE; /* clean up the value */
            grlib_irqmp_state.force[0] = value;
            grlib_irqmp_check_irqs(irqmp->env);
            return;

        case CLEAR_OFFSET:
            value &= ~1; /* clean up the value */
            grlib_irqmp_state.pending &= ~value;
            return;

        case MP_STATUS_OFFSET:
            /* Read Only (no SMP support) */
            return;

        case BROADCAST_OFFSET:
            value &= 0xFFFE; /* clean up the value */
            grlib_irqmp_state.broadcast = value;
            return;

        default:
            break;
    }

    /* mask registers */
    if (addr >= MASK_OFFSET && addr < FORCE_OFFSET) {
        int cpu = (addr - MASK_OFFSET) / 4;
        assert(cpu >= 0 && cpu < IRQMP_MAX_CPU);

        value &= ~1; /* clean up the value */
        grlib_irqmp_state.mask[cpu] = value;
        grlib_irqmp_check_irqs(irqmp->env);
        return;
    }

    /* force registers */
    if (addr >= FORCE_OFFSET && addr < EXTENDED_OFFSET) {
        int cpu = (addr - FORCE_OFFSET) / 4;
        assert(cpu >= 0 && cpu < IRQMP_MAX_CPU);

        uint32_t force = value & 0xFFFE;
        uint32_t clear = (value >> 16) & 0xFFFE;
        uint32_t old   = grlib_irqmp_state.force[cpu];

        grlib_irqmp_state.force[cpu] = (old | force) & ~clear;
        grlib_irqmp_check_irqs(irqmp->env);
        return;
    }

    /* extended (not supported) */
    if (addr >= EXTENDED_OFFSET && addr < IRQMP_REG_SIZE) {
        int cpu = (addr - EXTENDED_OFFSET) / 4;
        assert(cpu >= 0 && cpu < IRQMP_MAX_CPU);

        value &= 0xF; /* clean up the value */
        grlib_irqmp_state.extended[cpu] = value;
        return;
    }

    DPRINTF("write unknown register 0x%04x\n", (int)addr);
}

static CPUReadMemoryFunc *grlib_irqmp_read[] = {
    NULL, NULL, &grlib_irqmp_readl,
};

static CPUWriteMemoryFunc *grlib_irqmp_write[] = {
    NULL, NULL, &grlib_irqmp_writel,
};

static void grlib_irqmp_init(SysBusDevice *dev)
{
    IRQMP *irqmp = FROM_SYSBUS(typeof (*irqmp), dev);
    int    irqmp_regs;

    assert(irqmp != NULL);
    assert(irqmp->env != NULL);

    memset(&grlib_irqmp_state, 0, sizeof grlib_irqmp_state);
    grlib_irqmp_state.parent = irqmp;

    irqmp_regs = cpu_register_io_memory(grlib_irqmp_read,
                                        grlib_irqmp_write,
                                        irqmp);
    sysbus_init_mmio(dev, IRQMP_REG_SIZE, irqmp_regs);
}

static SysBusDeviceInfo grlib_irqmp_info = {
    .init = grlib_irqmp_init,
    .qdev.name  = "grlib,irqmp",
    .qdev.size  = sizeof(IRQMP),
    .qdev.props = (Property[]) {
        {
            .name   = "cpustate",
            .info   = &qdev_prop_ptr,
            .offset = offsetof(IRQMP, env),
            .defval = (void*[]) { NULL },
        },
        {/* end of list */}
    }
};

static void grlib_irqmp_register(void)
{
    sysbus_register_withprop(&grlib_irqmp_info);
}

device_init(grlib_irqmp_register)
