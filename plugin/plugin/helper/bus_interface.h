#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include "qemu_plugin_interface.h"

/* Devices */

typedef QemuPlugin_DeviceInfo Device;

Device *allocate_device(void);

int set_device_info(Device     *dev,
                    void       *opaque,
                    uint32_t    vendor_id,
                    uint32_t    device_id,
                    const char *name,
                    const char *desc);

int register_callbacks(Device      *dev,
                       io_read_fn  *io_read,
                       io_write_fn *io_write,
                       reset_fn    *device_reset,
                       init_fn     *device_init,
                       exit_fn     *device_exit);

int register_io_memory(Device        *dev,
                       target_addr_t  base,
                       target_addr_t  size);

int register_device(Device *dev,
                    int     port);

int start_device(Device *dev);

int device_loop(Device *dev);

/* Time */

#define SECOND      (1000000000ULL)
#define MILLISECOND (1000000ULL)
#define NANOSECOND  (1000ULL)

uint64_t get_time(Device *dev);

int add_event(Device        *dev,
              uint64_t       expire,
              uint32_t       event_id,
              EventCallback  event);

/* uint64_t get_vm_time(void); */
/* uint64_t get_host_time(void); */

/* int add_vm_event(uint64_t expire, QemuPlugin_EventCallback event); */
/* int add_host_event(uint64_t expire, QemuPlugin_EventCallback event); */
/* int remove_vm_event(QemuPlugin_EventCallback event); */
/* int remove_host_event(QemuPlugin_EventCallback event); */

/* IRQ */

int IRQ_raise(Device *dev, uint8_t line);
int IRQ_lower(Device *dev, uint8_t line);
int IRQ_pulse(Device *dev, uint8_t line);

/* DMA */

int dma_read(Device *dev, void *dest, target_addr_t addr, int size);
int dma_write(Device *dev, void *src, target_addr_t addr, int size);

#endif /* ! _EMULATOR_H_ */
