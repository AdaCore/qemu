#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include "qemu_plugin_interface.h"

/* Devices */

QemuPlugin_DeviceInfo *create_device(uint32_t            vendor_id,
                                     uint32_t            device_id,
                                     qemu_device_endian  endianness,
                                     const char         *name,
                                     const char         *desc);

uint32_t register_callbacks(QemuPlugin_DeviceInfo *dev,
                            io_read_fn            *io_read,
                            io_write_fn           *io_write,
                            reset_fn              *device_reset,
                            init_fn               *device_init,
                            exit_fn               *device_exit);

uint32_t register_io_memory(QemuPlugin_DeviceInfo *dev,
                            target_addr_t          base,
                            target_addr_t          size);

uint32_t attach_device(QemuPlugin_DeviceInfo *dev);

/* Time */

#define SECOND      (1000000000ULL)
#define MILLISECOND (1000000ULL)
#define NANOSECOND  (1000ULL)

typedef uint64_t vm_time_t;
typedef uint64_t host_time_t;

vm_time_t   get_vm_time(void);
host_time_t get_host_time(void);

uint32_t add_vm_event(vm_time_t expire, QemuPlugin_EventCallback event);
uint32_t add_host_event(host_time_t expire, QemuPlugin_EventCallback event);
uint32_t remove_vm_event(QemuPlugin_EventCallback event);
uint32_t remove_host_event(QemuPlugin_EventCallback event);

/* IRQ */

uint32_t IRQ_raise(uint32_t line);
uint32_t IRQ_lower(uint32_t line);
uint32_t IRQ_pulse(uint32_t line);

/* DMA */

uint32_t dma_read(void *dest, target_addr_t addr, int size);
uint32_t dma_write(void *src, target_addr_t addr, int size);

#endif /* ! _EMULATOR_H_ */
