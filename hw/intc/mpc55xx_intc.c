/*
 * mpc55xx INTC emulation
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
#include "hw/sysbus.h"
#include "hw/intc/mpc55xx_intc.h"

/* #define DEBUG_INTC */

#ifdef DEBUG_INTC
#define DPRINTF(fmt, ...) do { printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

#define INTC_MCR_HVEN  0x01
#define INTC_MCR_VTES  0x20

#define INTC_SSCIR_CLR 0x1
#define INTC_SSCIR_SET 0x2

/* Bit 0x1 of PSRn is not defined in the specs. We use it to signal that the IRQ
 * has been raised.
 */
#define INTC_PSR_RAISED 0x10

typedef struct INTCState {
    MemoryRegion mem;

    qemu_irq irq_out;
    int      irq_out_level;
    int max_irq;

    /* Registers */
    uint32_t mcr;
    uint32_t cpr;
    uint32_t iackr_intvec;
    uint32_t iackr_VTBA;
    uint8_t  sscir[8];

#define PSRn_SIZE 332

    uint8_t  psr[PSRn_SIZE];

    /* Priority LIFO with a circular buffer */
    uint8_t lifo[14];
    uint8_t lifo_index;
} INTCState;

static void intc_reset(void *opaque)
{
    INTCState *intc = (INTCState *)opaque;

    intc->irq_out_level = 0;

    intc->mcr          = 0;
    intc->cpr          = 0xF;
    intc->iackr_VTBA   = 0;
    intc->iackr_intvec = 0;

    memset(intc->sscir, 0x0, sizeof(intc->sscir));
    memset(intc->psr, 0x0, sizeof(intc->psr));

    memset(intc->lifo, 0x0, sizeof(intc->lifo));
    intc->lifo_index = 0;
}

#ifdef DEBUG_INTC
static void lifo_print(INTCState *intc)
{
    int i;

    fprintf(stderr, "INTC LIFO");

    for (i = 1; i < 15; i++) {
        fprintf(stderr, ":%02d", intc->lifo[(intc->lifo_index + i) % 14]);
    }
    fprintf(stderr, "\n");

}
#endif  /* DEBUG_INTC */

static void lifo_push(INTCState *intc, uint8_t pri)
{
    intc->lifo_index = (intc->lifo_index + 1) % 14;
    intc->lifo[intc->lifo_index] = pri;

    DPRINTF("%s: pri:%u index:%u\n", __func__, pri, intc->lifo_index);
#ifdef DEBUG_INTC
    lifo_print(intc);
#endif
}

static uint8_t lifo_pop(INTCState *intc)

{
    uint8_t ret = intc->lifo[intc->lifo_index];

    DPRINTF("%s: ret:%u index:%u\n", __func__, ret, intc->lifo_index);

    intc->lifo[intc->lifo_index] = 0;
    if (intc->lifo_index == 0) {
        intc->lifo_index = 13;
    } else {
        intc->lifo_index -= 1;
    }

#ifdef DEBUG_INTC
    lifo_print(intc);
#endif

    return ret & 0xF;
}

static void intc_update_iackr(INTCState *intc)
{
    int      i        = 0;
    int      irq      = -1;
    uint8_t  pri      = intc->cpr;
    uint8_t *curr_psr = 0;

    /* Save current priority */
    lifo_push(intc, intc->cpr);

    DPRINTF("%s: qemu_irq_lower(intc->irq_out);\n", __func__);

    qemu_irq_lower(intc->irq_out);

    /* Search for IRQs raised with the highest priority and above current
     * priority.
     */
    for (i = 0, curr_psr = intc->psr; i < PSRn_SIZE; i++, curr_psr++) {
        if (*curr_psr & INTC_PSR_RAISED && (*curr_psr & 0xF) > pri) {
            irq = i;
            pri = *curr_psr & 0xF;
        }
    }

    if (irq != -1) {
        /* We have an IRQ */
        intc->iackr_intvec = irq;

        /* Set the priority of the asserted interrupt */
        intc->cpr = intc->psr[irq] & 0xF;
    }
}

static void intc_set_irq(void *opaque, int n_IRQ, int level)
{
    INTCState *intc = opaque;

    assert(n_IRQ < PSRn_SIZE);

    DPRINTF("%s: IRQ:%d prio:%d (intc->cpr:%x) level:%d\n", __func__,
            n_IRQ, intc->psr[n_IRQ] & 0xF, intc->cpr, level);

    if (level) {
        intc->psr[n_IRQ] |= INTC_PSR_RAISED;
        if ((intc->psr[n_IRQ] & 0xF) > intc->cpr) {
            DPRINTF("%s qemu_irq_raise(irq_out)\n", __func__);
            qemu_irq_raise(intc->irq_out);
        }
    } else {
        intc->psr[n_IRQ] &= ~INTC_PSR_RAISED;
    }
}

static void check_for_interrupts(INTCState *intc)
{
    int i;

    DPRINTF("%s\n", __func__);

    if (intc->cpr == 0xF) {
        DPRINTF("%s current priority = 0xF, all masked\n", __func__);
        return;
    }
    /* Search for at least one IRQ raised with a priority above current
     * priority.
     */
    for (i = 0; i < PSRn_SIZE; i++) {
        /* DPRINTF("intc->psr[%d] : 0x%02x\n", i, intc->psr[i]); */
        if (intc->psr[i] & INTC_PSR_RAISED &&
            (intc->psr[i] & 0xF) > intc->cpr) {

            DPRINTF("%s qemu_irq_raise(irq_out)\n", __func__);
            qemu_irq_raise(intc->irq_out);
            break;
        }
    }
}

static uint64_t intc_read(void *opaque, hwaddr addr, unsigned size)
{
    INTCState *intc = (INTCState *)opaque;
    uint64_t   ret  = 0;

    DPRINTF("%s: addr: "TARGET_FMT_plx" size:%d\n", __func__, addr, size);

    /* Any combination of accessing the 4 bytes of a register with a single
     * access is supported.
     */

    switch (addr) {
    case 0x000 ... 0x003:       /* Module configuration register */
        ret = intc->mcr >> (addr * 8);
        break;

    case 0x008 ... 0x00B: {     /* Current priority register */
        int offset = addr - 0x008;
        ret = intc->cpr >> (offset * 8);

    } break;

    case 0x010 ... 0x013: {     /* Interrupt acknowledge register */
        int      offset = addr - 0x10;
        uint32_t iackr  = 0;

        intc_update_iackr(intc);

        if (intc->mcr & INTC_MCR_VTES) {
            iackr  = (intc->iackr_intvec & 0x1FF) << 3;
            iackr |= intc->iackr_VTBA & ~0xFFF;
        } else {
            iackr  = (intc->iackr_intvec & 0x1FF) << 2;
            iackr |= intc->iackr_VTBA & ~0x7FF;
        }
        ret = iackr >> (offset * 8);
    } break;

    case 0x018 ... 0x01B:        /* End-of-interrupt register */
        ret = 0;
        break;

    case 0x020 ... 0x027: {     /* Software set/clear interrupt registers */
        int offset = addr - 0x020;
        int i;

        /* Split the operation in 8-bits read */
        for (i = 0; i < size; i++) {
            ret += intc->sscir[offset + i] << (8 * i);
        }
    } break;

    case 0x40 ... 0x18C: {      /* Priority select registers */
        int offset = addr - 0x040;
        int i;

        /* Split the operation in 8-bits read */
        for (i = 0; i < size; i++) {
            ret += (intc->psr[offset + i] & 0xF) << (8 * i);
        }
    } break;

    default:
        DPRINTF("%s: Invalid address: "TARGET_FMT_plx"\n", __func__, addr);
        ret = 0;
    }

    return ret;
}

static void sscir_write(INTCState *intc, hwaddr addr, uint8_t value)
{
    assert(addr < 8);

    DPRINTF("%s: reg:"TARGET_FMT_plx" value:0x%02x\n", __func__, addr, value);

    if (value & INTC_SSCIR_CLR) {
        intc->sscir[addr] = 0;
        intc_set_irq(intc, addr, 0);
    }

    if (value & INTC_SSCIR_SET) {
        intc->sscir[addr] = 1;
        intc_set_irq(intc, addr, 1);
    }
}

static void intc_write(void *opaque, hwaddr addr,
                       uint64_t value, unsigned size)
{
    INTCState *intc = (INTCState *)opaque;

    DPRINTF("%s: addr:"TARGET_FMT_plx" size:%d value:0x%08x\n", __func__,
            addr, size, value);

    /* Any combination of accessing the 4 bytes of a register with a single
     * access is supported.
     */

    switch (addr) {
    case 0x000:                 /* Module configuration register */
        intc->mcr = value & 0xFF;

        if (intc->mcr & INTC_MCR_HVEN) {
            fprintf(stderr, "%s: Hardware vector mode not supported\n",
                    __func__);
            abort();
        }
        break;

    case 0x001 ... 0x003:
        /* This part of MCR is not writable */
        break;

    case 0x008:                 /* Current priority register */
        if (intc->cpr != (value & 0xF)) {
            intc->cpr = value & 0xF;
            check_for_interrupts(intc);
        }
        break;

    case 0x009 ... 0x00B:       /* This part of CPR is not writable */
        break;

    case 0x010 ... 0x013: {     /* Interrupt acknowledge register */
        /* Only VTBA is writable */
        int      i;
        int      offset = addr - 0x010;
        uint8_t *preg   = (uint8_t *)&intc->iackr_VTBA + offset;

        /* This trick to handle 8, 16, and 32 bits operation */
        for (i = 0; i < size; i++, preg++) {
            *preg = value & 0xFF;
            value = value >> 8;
        }
        DPRINTF("%s: VTBA = 0x%08x\n", __func__, intc->iackr_VTBA);
    } break;

    case 0x018 ... 0x01B: {    /* End-of-interrupt register */
        uint32_t prio = lifo_pop(intc);

        /* Restore priority */
        if (prio != intc->cpr) {
            intc->cpr = prio;
            check_for_interrupts(intc);
        }
    } break;

    case 0x020 ... 0x027: {     /* Software set/clear interrupt registers */
        int offset = addr - 0x020;
        int i;

        /* Split the operation in 8-bits writes */
        for (i = 0; i < size; i++) {
            sscir_write(intc, offset + i, value & 0xFF);
            value = value >> 8;
        }
    } break;

    case 0x40 ... 0x18C: {      /* Priority select registers */
        int offset = addr - 0x040;
        int i;

        /* Split the operation in 8-bits writes */
        for (i = 0; i < size; i++) {
            intc->psr[offset + i] |= value & 0xF;

            if (((value & 0xF) > intc->cpr)
                && (intc->psr[offset + i] & INTC_PSR_RAISED)) {

                printf("%s qemu_irq_raise(irq_out)\n", __func__);
                qemu_irq_raise(intc->irq_out);
            }

            value = value >> 8;
        }
    } break;
    default:
        DPRINTF("%s: Invalid address: "TARGET_FMT_plx"\n", __func__, addr);
    }
}

static const MemoryRegionOps intc_ops = {
    .read = intc_read,
    .write = intc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

qemu_irq *mpc55xx_intc_init(MemoryRegion *address_space,
                            hwaddr base, qemu_irq irq_out)
{
    INTCState *intc;

    intc = g_malloc0(sizeof(INTCState));


    memory_region_init_io(&intc->mem, NULL, &intc_ops, intc, "mpc55xx_intc", 0x4000);
    memory_region_add_subregion(address_space, base, &intc->mem);

    intc->max_irq = 298;
    intc->irq_out = irq_out;

    qemu_register_reset(intc_reset, intc);

    return qemu_allocate_irqs(intc_set_irq, intc, intc->max_irq);
}
