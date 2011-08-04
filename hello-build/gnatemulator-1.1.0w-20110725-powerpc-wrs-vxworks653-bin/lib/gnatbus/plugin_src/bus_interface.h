/****************************************************************************
 *                                                                          *
 *                         B U S _ I N T E R F A C E                        *
 *                                                                          *
 *                                  S p e c                                 *
 *                                                                          *
 *                        Copyright (C) 2011, AdaCore                       *
 *                                                                          *
 * This program is free software;  you can redistribute it and/or modify it *
 * under terms of  the GNU General Public License as  published by the Free *
 * Softwareg Foundation;  either version 3,  or (at your option)  any later *
 * version. This progran is distributed in the hope that it will be useful, *
 * but  WITHOUT  ANY  WARRANTY;   without  even  the  implied  warranty  of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                          *
 * As a special exception under Section 7 of GPL version 3, you are granted *
 * additional permissions  described in the GCC  Runtime Library Exception, *
 * version 3.1, as published by the Free Software Foundation.               *
 *                                                                          *
 * You should have received a copy  of the GNU General Public License and a *
 * copy of the  GCC Runtime Library Exception along  with this program; see *
 * the  files  COPYING3  and  COPYING.RUNTIME  respectively.  If  not,  see *
 * <http://www.gnu.org/licenses/>.                                          *
 *                                                                          *
 ****************************************************************************/

#ifndef _BUS_INTERFACE_H_
#define _BUS_INTERFACE_H_

#include "qemu_plugin_interface.h"

/* Devices */

typedef QemuPlugin_DeviceInfo Device;

Device *allocate_device(void);
void cleanup_device(Device *dev);

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

#endif /* ! _BUS_INTERFACE_H_ */
