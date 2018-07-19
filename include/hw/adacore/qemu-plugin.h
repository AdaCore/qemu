#ifndef QEMU_PLUG_IN_H
#define QEMU_PLUG_IN_H

#include "qemu-common.h"
#include "hw/irq.h"

void plugin_init(qemu_irq *cpu_irqs, int nr_irq);

void plugin_save_optargs(const char *optarg);

void plugin_device_init(void);

#endif /* ! QEMU_PLUG_IN_H */
