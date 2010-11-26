#ifndef _GRLIB_H_
#define _GRLIB_H_

#include "qdev.h"
#include "sysbus.h"

/* IRQMP */

#define IRQMP_MAX_CPU 16

typedef struct IRQMP
{
    SysBusDevice busdev;

    CPUSPARCState *env;
} IRQMP;

typedef struct IRQMPState
{
    uint32_t level;
    uint32_t pending;
    uint32_t clear;
    uint32_t mp_status;
    uint32_t broadcast;

    uint32_t mask[IRQMP_MAX_CPU];
    uint32_t force[IRQMP_MAX_CPU];
    uint32_t extended[IRQMP_MAX_CPU];

    IRQMP    *parent;
} IRQMPState;

extern IRQMPState grlib_irqmp_state;

void grlib_irqmp_set_irq(void *opaque, int irq, int level);

static inline DeviceState
*grlib_irqmp_create(target_phys_addr_t   base,
                    CPUState            *env,
                    qemu_irq           **cpu_irqs,
                    uint32_t             nr_irqs)
{
    DeviceState *dev;

    assert(cpu_irqs != NULL);

    dev = qdev_create(NULL, "grlib,irqmp");
    qdev_prop_set_ptr(dev, "cpustate", env);
    qdev_init(dev);
    sysbus_mmio_map(sysbus_from_qdev(dev), 0, base);

    cpu_sparc_set_intctl(env, intctl_grlib_irqmp);
    *cpu_irqs = qemu_allocate_irqs(grlib_irqmp_set_irq,
                                   &grlib_irqmp_state,
                                   nr_irqs);

    return dev;
}

/* GPTimer */

static inline DeviceState
*grlib_gptimer_create(target_phys_addr_t  base,
                      uint32_t            nr_timers,
                      uint32_t            freq,
                      qemu_irq           *cpu_irqs,
                      int                 base_irq)
{
    DeviceState *dev;
    int i;

    dev = qdev_create(NULL, "grlib,gptimer");
    qdev_prop_set_uint32(dev, "nr-timers", nr_timers);
    qdev_prop_set_uint32(dev, "frequency", freq);
    qdev_init(dev);
    sysbus_mmio_map(sysbus_from_qdev(dev), 0, base);

    for (i = 0; i < nr_timers; i++)
        sysbus_connect_irq(sysbus_from_qdev(dev), i, cpu_irqs[base_irq + i]);

    return dev;
}

/* APB UART */

static inline DeviceState
*grlib_apbuart_create(target_phys_addr_t  base,
                      CharDriverState    *serial,
                      qemu_irq            irq)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "grlib,apbuart");
    qdev_prop_set_ptr(dev, "chrdev", serial);
    qdev_init(dev);

    sysbus_mmio_map(sysbus_from_qdev(dev), 0, base);

    sysbus_connect_irq(sysbus_from_qdev(dev), 0, irq);

    return dev;
}

#endif /* ! _GRLIB_H_ */
