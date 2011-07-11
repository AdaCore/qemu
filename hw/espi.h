#ifndef _ESPI_H_
#define _ESPI_H_

DeviceState *espi_create(target_phys_addr_t base,
                         qemu_irq           irq);

#endif /* ! _ESPI_H_ */
