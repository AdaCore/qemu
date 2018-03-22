/****************************************************************************
 *                                                                          *
 *                 Q E M U _ P L U G I N _ I N T E R F A C E                *
 *                                                                          *
 *                                  S p e c                                 *
 *                                                                          *
 *                     Copyright (C) 2012-2018, AdaCore                     *
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

#ifndef _QEMU_PLUGIN_INTERFACE_H_
#define _QEMU_PLUGIN_INTERFACE_H_

#include <stdint.h>
#include <stdlib.h>

#define QEMU_PLUGIN_INTERFACE_VERSION   (6)

#define QP_ERROR   (-1)
#define QP_NOERROR ( 0)

uint32_t qemu_plugin_init(const char *args);

struct QemuPlugin_Emulator;
typedef struct QemuPlugin_Emulator QemuPlugin_Emulator;

/* Device interface */

typedef uint64_t io_read_fn(void *opaque, uint64_t addr, uint64_t size);

typedef void io_write_fn(void *opaque,
                         uint64_t addr,
                         uint64_t size,
                         uint64_t val);

typedef void reset_fn(void *opaque);
typedef void init_fn(void *opaque);
typedef void exit_fn(void *opaque);

#define NAME_LENGTH 64
#define DESC_LENGTH 256
#define MAX_IOMEM   32
#define QEMU_PLUGIN_MAX_SHARED_MEM        1
#define QEMU_PLUGIN_SHARED_MEM_NAME_MAX 255

typedef enum DeviceEndianness {
    DeviceEndianness_NativeEndian = 0,
    DeviceEndianness_BigEndian    = 1,
    DeviceEndianness_LittleEndian = 2,
} DeviceEndianness;

typedef struct QemuPlugin_DeviceInfo {
    uint32_t version;

    uint32_t vendor_id;
    uint32_t device_id;
    char     name[NAME_LENGTH];
    char     desc[DESC_LENGTH];

    void *opaque;

    QemuPlugin_Emulator *emulator;

    io_read_fn  *io_read;
    io_write_fn *io_write;

    reset_fn *pdevice_reset;
    init_fn  *pdevice_init;
    exit_fn  *pdevice_exit;

    DeviceEndianness device_endianness;

    uint32_t nr_iomem;
    struct QemuPlugin_IOMemory {
        uint64_t base;
        uint64_t size;
    } iomem[MAX_IOMEM];

    uint32_t nr_shared_mem;
    struct QemuPlugin_SharedMemory {
        uint64_t base;
        uint64_t size;
        int shared_memory_fd;
        void *mmap_ptr;
        char *name;
    } shared_mem[QEMU_PLUGIN_MAX_SHARED_MEM];
} QemuPlugin_DeviceInfo;

/* Emulator interface */

typedef enum QemuPlugin_ClockType {
    QP_VM_CLOCK,
    QP_HOST_CLOCK,
} QemuPlugin_ClockType;

typedef void (*EventCallback)(void     *opaque,
                              uint32_t  event_id,
                              uint64_t  expire_time);

struct QemuPlugin_Emulator {
    uint64_t (*get_time)(QemuPlugin_ClockType clock);

    uint32_t (*add_event)(uint64_t              expire_time,
                          QemuPlugin_ClockType  clock,
                          uint32_t              event_id,
                          EventCallback         event,
                          void                 *opaque);

    uint32_t (*remove_event)(QemuPlugin_ClockType clock,
                             uint32_t             event_id);

    uint32_t (*set_irq)(uint32_t line, uint32_t level);

    uint32_t (*dma_read)(void *dest,
                         uint64_t addr,
                         uint64_t size);

    uint32_t (*dma_write)(void *src,
                          uint64_t addr,
                          uint64_t size);

    uint32_t (*attach_device)(QemuPlugin_DeviceInfo *dev);

    uint32_t (*target_endianness)(void);

    uint32_t (*shutdown_request)(void);

    uint32_t version;
};

typedef enum QemuPlugin_State {
    QPS_LOADED,
    QPS_INITIALIZATION,
    QPS_RUNNING,
    QPS_ERROR,
    QPS_STOPED,
} QemuPlugin_State;

typedef enum TargetEndianness {
    TargetEndianness_Unknown      = 0,
    TargetEndianness_BigEndian    = 1,
    TargetEndianness_LittleEndian = 2,
} TargetEndianness;

typedef void       (*QemuPLugin_InitFunction)(QemuPlugin_Emulator *emu,
                                              const char *args);

#endif /* ! _QEMU_PLUGIN_INTERFACE_H_ */
