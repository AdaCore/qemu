#ifndef _QEMU_PLUGIN_INTERFACE_H_
#define _QEMU_PLUGIN_INTERFACE_H_

#include <stdint.h>
#include <stdlib.h>

typedef uint32_t target_addr_t;

#define QEMU_PLUGIN_INTERFACE_VERSION   (2)

#define QP_ERROR   (-1)
#define QP_NOERROR ( 0)

uint32_t qemu_plugin_init(const char *args);

struct QemuPlugin_Emulator;
typedef struct QemuPlugin_Emulator QemuPlugin_Emulator;

/* Device interface */

typedef uint32_t io_read_fn(void *opaque, target_addr_t addr, uint32_t size);

typedef void     io_write_fn(void          *opaque,
                             target_addr_t  addr,
                             uint32_t       size,
                             uint32_t       val);

typedef void reset_fn(void *opaque);
typedef void init_fn(void *opaque);
typedef void exit_fn(void *opaque);

#define NAME_LENGTH 64
#define DESC_LENGTH 256
#define MAX_IOMEM   32

typedef struct QemuPlugin_DeviceInfo
{
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

    uint32_t nr_iomem;
    struct QemuPlugin_IOMemory
    {
        target_addr_t base;
        target_addr_t size;
    } iomem[MAX_IOMEM];
} QemuPlugin_DeviceInfo;

/* Emulator interface */

typedef enum QemuPlugin_ClockType {
    QP_VM_CLOCK,
    QP_HOST_CLOCK,
} QemuPlugin_ClockType;

typedef void (*EventCallback)(void     *opaque,
                              uint32_t  event_id,
                              uint64_t  expire_time);

struct QemuPlugin_Emulator
{
    uint64_t (*get_time)(QemuPlugin_ClockType clock);


    uint32_t (*add_event)(uint64_t              expire_time,
                          QemuPlugin_ClockType  clock,
                          uint32_t              event_id,
                          EventCallback         event,
                          void                 *opaque);

    uint32_t (*remove_event)(QemuPlugin_ClockType clock,
                             uint32_t             event_id);

    uint32_t (*set_irq)(uint32_t line, uint32_t level);

    uint32_t (*dma_read)(void          *dest,
                         target_addr_t  addr,
                         uint32_t       size);

    uint32_t (*dma_write)(void          *src,
                          target_addr_t  addr,
                          uint32_t       size);

    uint32_t (*attach_device)(QemuPlugin_DeviceInfo *dev);

    uint32_t version;
};

typedef enum QemuPlugin_State {
    QPS_LOADED,
    QPS_INITIALIZATION,
    QPS_RUNNING,
    QPS_ERROR,
    QPS_STOPED,
} QemuPlugin_State;

typedef void       (*QemuPLugin_InitFunction)(QemuPlugin_Emulator *emu,
                                              const char *args);

#endif /* ! _QEMU_PLUGIN_INTERFACE_H_ */
