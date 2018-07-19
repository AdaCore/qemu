
/* TARGET_WORDS_BIGENDIAN is poisoned so let's workaround that */
#define GNATBUS_BIG_ENDIAN (TARGET_WORDS_BIGENDIAN)

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "sysemu/cpus.h"
#include "hw/adacore/gnat-bus.h"
#include "trace.h"

static inline int gnatbus_process_setIRQ(GnatBus_Device       *qbdev,
                                         GnatBusPacket_SetIRQ *setIRQ)
{
    trace_gnatbus_setirq(setIRQ->line, setIRQ->level);

    if (setIRQ->line > qbdev->master->nr_irq) {
        fprintf(stderr, "%s: Invalid IRQ line: %d\n", __func__, setIRQ->line);
        return -1;
    }

    switch (setIRQ->level) {
    case 0:
        qemu_irq_lower(qbdev->master->cpu_irqs[setIRQ->line]);
        break;
    case 1:
        qemu_irq_raise(qbdev->master->cpu_irqs[setIRQ->line]);
        break;
    case 2:
        qemu_irq_pulse(qbdev->master->cpu_irqs[setIRQ->line]);
        break;
    default:
        fprintf(stderr, "%s: Unknown IRQ level (%d)\n",
                __func__, setIRQ->level);
        return -1;
        break;
    }
    return 0;
}

static inline
int gnatbus_process_registerevent(GnatBus_Device              *qbdev,
                                  GnatBusPacket_RegisterEvent *event)
{
    trace_gnatbus_registerevent(event->expire_time, event->event);

    gnatbus_add_event(qbdev, event->expire_time, event->event_id, event->event);
    return 0;
}

static inline int gnatbus_process_event(GnatBus_Device      *qbdev,
                                        GnatBusPacket_Event *event)
{
    switch (event->type) {
    case GnatBusEvent_SetIRQ:
        return gnatbus_process_setIRQ(qbdev, (GnatBusPacket_SetIRQ *)event);
        break;

    case GnatBusEvent_RegisterEvent:
        return gnatbus_process_registerevent
            (qbdev, (GnatBusPacket_RegisterEvent *)event);
        break;

    case GnatBusEvent_Shutdown:
        if (qemu_in_vcpu_thread()) {
            /* This basically means that we are waiting for a reply for
             * example a read or a write to a gnatbus register.. So we can't
             * process the pause_all_vcpus(..) yet or we will have the reply
             * packet in the io-thread.
             */
            qbdev->shutdown_requested = true;
        } else {
            gnatbus_shutdown_vm();
        }
        return 0;
        break;

    default:
        fprintf(stderr, "%s: Unknown event type (%d)\n",
                __func__, event->type);
        return -1;
        break;
    }
}

/* Response */

static void gnatbus_resp_error(GnatBus_Device *qbdev,
                               uint32_t        request_id,
                               uint32_t        error_code)
{
    GnatBusPacket_Error err;

    trace_gnatbus_resp_error(request_id, error_code);

    GnatBusPacket_Error_Init(&err);

    err.parent.id  = request_id;
    err.error_code = error_code;

    gnatbus_send(qbdev, (uint8_t *)&err, sizeof(err));
}

/* Request */

static inline int gnatbus_process_read(GnatBus_Device     *qbdev,
                                       GnatBusPacket_Read *read)
{
    GnatBusPacket_Data *resp;

    trace_gnatbus_process_read(read->address, read->length);

    /* Allocate packet + data */
    resp = g_malloc(sizeof(GnatBusPacket_Data) + read->length);

    GnatBusPacket_Data_Init(resp);
    resp->parent.parent.size = sizeof(GnatBusPacket_Data) + read->length;
    resp->parent.id          = read->parent.id;
    resp->length             = read->length;

    cpu_physical_memory_read(read->address, resp->data, read->length);

    gnatbus_send(qbdev, (uint8_t *)resp,
                 sizeof(GnatBusPacket_Data) + read->length);
    g_free(resp);
    return 0;
}

static inline int gnatbus_process_write(GnatBus_Device      *qbdev,
                                        GnatBusPacket_Write *write)
{
    trace_gnatbus_process_write(write->address, write->length);

    cpu_physical_memory_write(write->address, write->data, write->length);

    gnatbus_resp_error(qbdev, write->parent.id, 0);
    return 0;
}

extern const MemoryRegionOps gnatbus_io_ops;

#define TYPE_GNATBUS_DEVICE "gnatbus-device"
#define GNATBUS_DEVICE(obj) \
    OBJECT_CHECK(GnatBus_SysBusDevice, (obj), TYPE_GNATBUS_DEVICE)

static void gnatbus_realize(DeviceState *dev, Error **errp)
{
    GnatBus_SysBusDevice *pdev  = GNATBUS_DEVICE(dev);
    GnatBus_Device       *qbdev = pdev->qbdev;
    int                   i;

    /* Copy default io ops */
    memcpy(&qbdev->io_ops, &gnatbus_io_ops, sizeof(MemoryRegionOps));

    if (qbdev->info.endianness == DeviceEndianness_BigEndian) {
        qbdev->io_ops.endianness = DEVICE_BIG_ENDIAN;
    } else if (qbdev->info.endianness == DeviceEndianness_LittleEndian) {
        qbdev->io_ops.endianness = DEVICE_LITTLE_ENDIAN;
    } else {
        qbdev->io_ops.endianness = DEVICE_NATIVE_ENDIAN;
    }

    for (i = 0; i < qbdev->info.nr_iomem; i++) {
        qbdev->io_region[i].qbdev = qbdev;
        qbdev->io_region[i].base  = qbdev->info.iomem[i].base;

        memory_region_transaction_begin();
        memory_region_init_io(&qbdev->io_region[i].mr, OBJECT(pdev),
                              &qbdev->io_ops, &qbdev->io_region[i],
                              qbdev->info.name, qbdev->info.iomem[i].size);
        memory_region_add_subregion_overlap(get_system_memory(),
                                            qbdev->info.iomem[i].base,
                                            &qbdev->io_region[i].mr, 1);
        memory_region_transaction_commit();
    }

#if defined(_WIN32) || defined(_WIN64)
    if (qbdev->info.nr_shared_mem) {
        /*
         * Windows target doesn't support SHM. We need to implement that
         * with Microsoft API.
         */
        fprintf(stderr, "shared memory isn't supported under windows,"
                        " ignoring..\n");
    }
#else /* Linux */
    int fd;

    for (i = 0; i < qbdev->info.nr_shared_mem; i++) {

        fd = shm_open(qbdev->info.shared_mem[i].name, O_RDWR,
                      S_IRUSR | S_IWUSR);
        if (fd < 0) {
            error_report("shm_open(\"%s\") failed",
                         qbdev->info.shared_mem[i].name);
            return;
        }

        qbdev->mmap_ptr[i] = mmap(NULL, qbdev->info.shared_mem[i].size,
                                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if (qbdev->mmap_ptr[i] == MAP_FAILED) {
            close(fd);
            error_report("mmap(%s) failed",
                         qbdev->info.shared_mem[i].name);
            return;
        }

        memory_region_transaction_begin();
        memory_region_init_ram_ptr(&qbdev->shared_mr[i],
                                   OBJECT(pdev),
                                   qbdev->info.shared_mem[i].name,
                                   qbdev->info.shared_mem[i].size,
                                   qbdev->mmap_ptr[i]);
        memory_region_add_subregion_overlap(get_system_memory(),
                                       qbdev->info.shared_mem[i].base,
                                       &qbdev->shared_mr[i], 1);
        memory_region_transaction_commit();
    }
#endif /* Linux */
}

static inline int gnatbus_process_register(GnatBus_Device         *qbdev,
                                           GnatBusPacket_Register *reg)
{
    /* printf("GnatBus: Received Register request from: '%s'\n", reg->name); */

    DeviceState              *qdev;
    GnatBus_SysBusDevice     *pdev;
    GnatBusPacket_Endianness  end;

    trace_gnatbus_process_register(reg->name);

    if (reg->bus_version != GNATBUS_VERSION) {
        fprintf(stderr, "%s: GNATBUS_VERSION mismatch qemu:%d device:%d\n",
                __func__, GNATBUS_VERSION, reg->bus_version);
        return -1;
    }

    qdev = qdev_create(NULL, "gnatbus-device");
    pdev = (typeof(pdev))qdev;
    pdev->qbdev = qbdev;

    memcpy(&qbdev->info, reg, sizeof(*reg));
    qdev_init_nofail(qdev);

    qbdev->start_ok = 1;
    QLIST_INSERT_HEAD(&qbdev->master->devices_list, qbdev, list);

    /* gnatbus_resp_error(qbdev, reg->parent.id, 0); */

    GnatBusPacket_Endianness_Init(&end);

    end.parent.id  = reg->parent.id;
#if defined(GNATBUS_BIG_ENDIAN)
    end.endianness = TargetEndianness_BigEndian;
#else
    end.endianness = TargetEndianness_LittleEndian;
#endif

    gnatbus_send(qbdev, (uint8_t *)&end, sizeof(end));

    return 0;
}

static void gnatbus_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = gnatbus_realize;
}

static TypeInfo gnatbus_device_info = {
    .name          = "gnatbus-device",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(GnatBus_SysBusDevice),
    .class_init    = gnatbus_device_class_init,
};

static void gnatbus_register_types(void)
{
    type_register_static(&gnatbus_device_info);
}

type_init(gnatbus_register_types)

static inline int gnatbus_process_gettime(GnatBus_Device        *qbdev,
                                          GnatBusPacket_GetTime *gettime)
{
    GnatBusPacket_Time time;

    GnatBusPacket_Time_Init(&time);

    time.parent.id = gettime->parent.id;
    time.time      = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    trace_gnatbus_process_gettime(time.time);

    gnatbus_send(qbdev, (uint8_t *)&time, sizeof(time));
    return 0;
}

static inline int gnatbus_process_request(GnatBus_Device        *qbdev,
                                          GnatBusPacket_Request *request)
{
    switch (request->type) {
    case GnatBusRequest_Register:
        return gnatbus_process_register(qbdev,
                                        (GnatBusPacket_Register *)request);
        break;

    case GnatBusRequest_Read:
        return gnatbus_process_read(qbdev, (GnatBusPacket_Read *)request);
        break;

    case GnatBusRequest_Write:
        return gnatbus_process_write(qbdev, (GnatBusPacket_Write *)request);
        break;

    case GnatBusRequest_GetTime:
        return gnatbus_process_gettime(qbdev, (GnatBusPacket_GetTime *)request);
        break;

    default:
        fprintf(stderr, "%s: Unknown request type (%d)\n",
                __func__, request->type);
        gnatbus_resp_error(qbdev, request->id, 1);
        return -1;
        break;
    }
}

int gnatbus_process_packet(GnatBus_Device *qbdev,
                           GnatBusPacket  *packet)
{
    switch (packet->type) {
    case GnatBus_Event:
        return gnatbus_process_event(qbdev, (GnatBusPacket_Event *)packet);
        break;
    case GnatBus_Request:
        return gnatbus_process_request(qbdev, (GnatBusPacket_Request *)packet);
        break;
    default:
        fprintf(stderr, "%s: Unknown packet type (%d)\n",
                __func__, packet->type);
        return -1;
        break;
    }
}
