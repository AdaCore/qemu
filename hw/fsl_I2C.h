#ifndef _FSL_I2C_H_
#define _FSL_I2C_H_

DeviceState *fsl_i2c_create(target_phys_addr_t base,
                            MemoryRegion       *mr,
                            qemu_irq           irq);


#endif /* ! _FSL_I2C_H_ */
