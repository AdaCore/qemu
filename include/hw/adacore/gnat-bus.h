#ifndef _QEMU_BUS_H_
#define _QEMU_BUS_H_

#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "exec/memory.h"
#include "qemu/timer.h"

#include "gnat-bus-interface.h"
#include "qemu-common.h"

typedef QLIST_HEAD(GnatBus_Device_List, GnatBus_Device)
     GnatBus_Device_List;

typedef QLIST_HEAD(GnatBus_Events_List, Event_Entry)
     GnatBus_Events_List;

typedef struct GnatBus_Master {
    qemu_irq            *cpu_irqs;
    int                  nr_irq;
    char                *optarg;
    GnatBus_Device_List  devices_list;

    /* Events */
    GnatBus_Events_List  events_list;
    QEMUTimer           *timer;
    QEMUClockType        qclock;
    QEMUBH              *bh;

} GnatBus_Master;

struct GnatBus_Device;
typedef struct GnatBus_Device GnatBus_Device;

typedef struct GnatBus_IORegion {
    GnatBus_Device *qbdev;
    uint64_t        base;
    MemoryRegion    mr;
} GnatBus_IORegion;

struct GnatBus_Device {
    GnatBusPacket_Register info;
    int                    start_ok;

    GnatBus_Master *master;

    int is_pipe;
    /* A shutdown has been requested during a write or a read. */
    bool shutdown_requested;

    MemoryRegionOps  io_ops;
    GnatBus_IORegion io_region[MAX_IOMEM];

    /* Shared Memories */
    void *mmap_ptr[GNATBUS_MAX_SHARED_MEM];
    MemoryRegion shared_mr[GNATBUS_MAX_SHARED_MEM];

    /* Chardev */
    GnatBusPacket   *curr_packet;
    uint32_t         curr_packet_size;
    CharBackend      chr;
    int              status;

    QLIST_ENTRY(GnatBus_Device) list;
};

typedef struct GnatBus_SysBusDevice {
    SysBusDevice    busdev;     /* Must be the first field */
    GnatBus_Device *qbdev;
} GnatBus_SysBusDevice;

void gnatbus_shutdown_vm(void);

void gnatbus_save_optargs(const char *optarg);

void gnatbus_save_timeout_optargs(const char *optarg);

void gnatbus_master_init(qemu_irq *cpu_irqs, int nr_irq);

void gnatbus_device_init(void);

int gnatbus_process_packet(GnatBus_Device *qbdev,
                           GnatBusPacket  *packet);

GnatBusPacket_Response *send_and_wait_resp(GnatBus_Device        *qbdev,
                                           GnatBusPacket_Request *request);

int gnatbus_add_event(GnatBus_Device *qbdev,
                      uint64_t        expire_time,
                      uint32_t        event_id,
                      uint64_t        event);

int gnatbus_send(GnatBus_Device *qbdev, const uint8_t *buf, int len);

extern const MemoryRegionOps gnatbus_ops;

#endif /* ! _QEMU_BUS_H_ */
