
/* TARGET_WORDS_BIGENDIAN is poisoned so let's workaround that */
#define GNATBUS_BIG_ENDIAN (TARGET_WORDS_BIGENDIAN)

#include "qemu/osdep.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "qemu-common.h"
#include "qemu/option.h"
#include "sysemu/sysemu.h"
#include "sysemu/cpus.h"
#include "hw/sysbus.h"
#include "hw/adacore/qemu-plugin.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"

#include "hw/adacore/qemu_plugin_interface.h"

typedef QLIST_HEAD(QemuPlugin_SysBusDevice_List, QemuPlugin_SysBusDevice)
     QemuPlugin_SysBusDevice_List;

struct QemuPlugin_SysBusDevice;
typedef struct QemuPlugin_SysBusDevice QemuPlugin_SysBusDevice;

typedef struct QemuPlugin_IORegion {
    QemuPlugin_SysBusDevice *pdev;
    uint64_t                 base;
    MemoryRegion             mr;
} QemuPlugin_IORegion;

typedef struct QemuPlugin_SharedMemRegion {
    MemoryRegion mr;
} QemuPlugin_SharedMemRegion;

struct QemuPlugin_SysBusDevice {
    SysBusDevice busdev;

    MemoryRegionOps io_ops;

    QemuPlugin_DeviceInfo *info;

    QemuPlugin_IORegion io_region[MAX_IOMEM];
    QemuPlugin_SharedMemRegion shm_region[QEMU_PLUGIN_MAX_SHARED_MEM];

    QLIST_ENTRY(QemuPlugin_SysBusDevice) list;
};

typedef QLIST_HEAD(QemuPlugin_Events_List, QemuPlugin_Event)
     QemuPlugin_Events_List;

typedef struct QemuPlugin_Event {
    uint64_t              expire_time;
    uint32_t              id;
    EventCallback         event;
    QemuPlugin_ClockType  clock;
    void                 *opaque;

    QLIST_ENTRY(QemuPlugin_Event) list;
} QemuPlugin_Event;

typedef struct QemuPlugin_EventsMgt {
    QemuPlugin_Events_List  events_list;
    QEMUTimer              *timer;
    QEMUClockType          qclock;
    QEMUBH                 *bh;
} QemuPlugin_EventsMgt;

typedef struct QemuPlugin {
    char *plugin_optarg;

    QemuPlugin_Emulator emulator_interface;

    QemuPlugin_SysBusDevice_List devices_list;

    QemuPlugin_EventsMgt vm_events;
    QemuPlugin_EventsMgt host_events;

    qemu_irq *cpu_irqs;
    int       nr_irq;
} QemuPlugin;

static QemuPlugin *g_plugin;

static uint32_t plugin_set_irq(uint32_t line, uint32_t level)
{
    if (line > g_plugin->nr_irq) {
        fprintf(stderr, "QEMU plug-in error: Invalid IRQ line: %d\n", line);
        return QP_ERROR;
    }

    qemu_set_irq(g_plugin->cpu_irqs[line], level);
    return QP_NOERROR;
}

static uint64_t plugin_get_time(QemuPlugin_ClockType clock)
{
    return qemu_clock_get_ns((clock == QP_VM_CLOCK) ?
                             QEMU_CLOCK_VIRTUAL : QEMU_CLOCK_HOST);
}

static void plugin_timer_tick(void *opaque)
{

    QemuPlugin_EventsMgt *ev = opaque;

    if (ev->bh) {
        qemu_bh_schedule(ev->bh);
    }
}

static void plugin_timer_tick_bh(void *opaque)
{
    QemuPlugin_EventsMgt   *ev         = opaque;
    QemuPlugin_Events_List *event_list = &ev->events_list;
    QEMUTimer              *timer      = ev->timer;
    QemuPlugin_Event       *event      = NULL;
    uint64_t                now;

    if (QLIST_FIRST(event_list) == NULL) {
        /* No events */
        return;
    }

    now = qemu_clock_get_ns(ev->qclock);

    while (QLIST_FIRST(event_list) != NULL) {
        event = QLIST_FIRST(event_list);

        if (event->expire_time <= now) {
            QLIST_REMOVE(event, list);

            /* Run the callback */
            event->event(event->opaque, event->id, event->expire_time);
            g_free(event);
        } else {
            break;
        }
    }

    if (QLIST_EMPTY(event_list)) {
        timer_mod_ns(timer, (uint64_t)-1);
    } else {
        timer_mod_ns(timer, QLIST_FIRST(event_list)->expire_time);
    }
}

static uint32_t plugin_add_event(uint64_t              expire_time,
                                 QemuPlugin_ClockType  clock,
                                 uint32_t              event_id,
                                 EventCallback         event,
                                 void                 *opaque)

{
    QemuPlugin_EventsMgt *ev = (clock == QP_VM_CLOCK)
                                 ? &g_plugin->vm_events
                                 : &g_plugin->host_events;

    QemuPlugin_Events_List *event_list = &ev->events_list;
    QEMUTimer              *timer      = ev->timer;

    QemuPlugin_Event *new  = NULL;
    QemuPlugin_Event *parc = NULL;

    new              = g_malloc0(sizeof(*new));
    new->event       = event;
    new->id          = event_id;
    new->expire_time = expire_time;
    new->clock       = clock;
    new->opaque      = opaque;

    if (QLIST_EMPTY(event_list)
        || QLIST_FIRST(event_list)->expire_time >= expire_time) {

        /* Head insertion */
        QLIST_INSERT_HEAD(event_list, new, list);
    } else {
        QLIST_FOREACH(parc, event_list, list) {
            if (parc->expire_time > expire_time) {
                QLIST_INSERT_BEFORE(parc, new, list);
                break;
            } else if (QLIST_NEXT(parc, list) == NULL) {
                /* Tail insertion */
                QLIST_INSERT_AFTER(parc, new, list);
                break;
            }
        }
    }

    timer_mod_ns(timer, QLIST_FIRST(event_list)->expire_time);

    return QP_NOERROR;
}

static uint32_t plugin_remove_event(QemuPlugin_ClockType clock,
                                    uint32_t             event_id)
{
    QemuPlugin_EventsMgt *ev = (clock == QP_VM_CLOCK)
                                 ? &g_plugin->vm_events
                                 : &g_plugin->host_events;
    QEMUTimer              *timer      = ev->timer;
    QemuPlugin_Events_List *event_list = &ev->events_list;

    QemuPlugin_Event *parc      = NULL;
    QemuPlugin_Event *next_parc = NULL;

    QLIST_FOREACH_SAFE(parc, event_list, list, next_parc) {
        if (parc->id == event_id) {
            QLIST_REMOVE(parc, list);
            break;
            return QP_NOERROR;
        }
    }

    if (QLIST_EMPTY(event_list)) {
        timer_mod_ns(timer, (uint64_t)-1);
    } else {
        timer_mod_ns(timer, QLIST_FIRST(event_list)->expire_time);
    }

    return QP_ERROR;
}


static inline void plugin_write(void *opaque,
                                uint64_t addr,
                                uint64_t value,
                                unsigned size)
{
    QemuPlugin_IORegion     *io_region = opaque;
    QemuPlugin_SysBusDevice *pdev    = io_region->pdev;

    if (pdev->info->io_write != NULL) {
        pdev->info->io_write(pdev->info->opaque,
                             io_region->base + addr, size, value);
    } else {
        fprintf(stderr, "%s: error: no write I/O callback\n", pdev->info->name);
    }
}

static inline uint64_t plugin_read(void *opaque,
                                   uint64_t addr,
                                   unsigned size)
{
    QemuPlugin_IORegion     *io_region = opaque;
    QemuPlugin_SysBusDevice *pdev    = io_region->pdev;

    if (pdev->info->io_read != NULL) {
        return pdev->info->io_read(pdev->info->opaque,
                                   addr + io_region->base, size);
    } else {
        fprintf(stderr, "%s: error: no read I/O callback\n", pdev->info->name);
        return 0;
    }
}

const MemoryRegionOps plugin_ops = {
    .read = plugin_read,
    .write = plugin_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

#define TYPE_QEMU_PLUGIN_DEVICE "qemu-plugin-device"
#define QEMU_PLUGIN_DEVICE(obj) \
    OBJECT_CHECK(QemuPlugin_SysBusDevice, (obj), TYPE_QEMU_PLUGIN_DEVICE)

static int plugin_init_device(SysBusDevice *dev)
{
    QemuPlugin_SysBusDevice *pdev = QEMU_PLUGIN_DEVICE(dev);
    int i;

    /* Copy default io ops */
    memcpy(&pdev->io_ops, &plugin_ops, sizeof(MemoryRegionOps));

    for (i = 0; i < pdev->info->nr_iomem; i++) {

        if (pdev->info->device_endianness == DeviceEndianness_BigEndian) {
            pdev->io_ops.endianness = DEVICE_BIG_ENDIAN;
        } else if (pdev->info->device_endianness ==
                   DeviceEndianness_LittleEndian) {
            pdev->io_ops.endianness = DEVICE_LITTLE_ENDIAN;
        } else {
            pdev->io_ops.endianness = DEVICE_NATIVE_ENDIAN;
        }

        pdev->io_region[i].pdev = pdev;
        pdev->io_region[i].base = pdev->info->iomem[i].base;

        memory_region_transaction_begin();
        memory_region_init_io(&pdev->io_region[i].mr, OBJECT(pdev),
                              &pdev->io_ops, &pdev->io_region[i],
                              pdev->info->name, pdev->info->iomem[i].size);

        memory_region_add_subregion_overlap(get_system_memory(),
                                            pdev->info->iomem[i].base,
                                            &pdev->io_region[i].mr, 1);
        memory_region_transaction_commit();
    }

    for (i = 0; i < pdev->info->nr_shared_mem; i++) {
        struct QemuPlugin_SharedMemory *shm = &pdev->info->shared_mem[i];
        MemoryRegion *mr = &pdev->shm_region[i].mr;

        memory_region_transaction_begin();
        memory_region_init_ram_ptr(mr, OBJECT(pdev), shm->name,
                                   shm->size, shm->mmap_ptr);
        memory_region_add_subregion_overlap(get_system_memory(),
                                            shm->base, mr, 1);
        memory_region_transaction_commit();
    }

    return 0;
}

static uint32_t attach_device(QemuPlugin_DeviceInfo *devinfo)
{
    DeviceState *qdev;
    QemuPlugin_SysBusDevice *pdev;

    if (devinfo == NULL || devinfo->nr_iomem > MAX_IOMEM) {
        return QP_ERROR;
    }

    qdev = qdev_create(NULL, "qemu-plugin-device");
    pdev = (typeof(pdev))qdev;
    pdev->info = devinfo;

    qdev_init_nofail(qdev);

    QLIST_INSERT_HEAD(&g_plugin->devices_list, pdev, list);
    return QP_NOERROR;
}

static void plugin_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = plugin_init_device;
}

static TypeInfo plugin_info = {
    .name          = "qemu-plugin-device",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QemuPlugin_SysBusDevice),
    .class_init    = plugin_class_init,
};

static void plugin_register_types(void)
{
    type_register_static(&plugin_info);
}

type_init(plugin_register_types)

static uint32_t plugin_dma_write(void *src,
                                 uint64_t addr,
                                 uint64_t size)
{
    cpu_physical_memory_write(addr, src, size);
    return QP_NOERROR;
}

static uint32_t plugin_dma_read(void *dest,
                                uint64_t addr,
                                uint64_t size)
{
    cpu_physical_memory_read(addr, dest, size);
    return QP_NOERROR;
}

void plugin_device_init(void)
{
    QemuPlugin_SysBusDevice *pdev;

    if (g_plugin == NULL) {
        /* No plug-in */
        return;
    }

    QLIST_FOREACH(pdev, &g_plugin->devices_list, list) {
        if (pdev->info->pdevice_init != NULL) {
            pdev->info->pdevice_init(pdev->info->opaque);
        } else {
            fprintf(stderr, "%s: error: no init callback\n",
                    pdev->info->name);
        }
    }
}

static void plugin_device_exit(void)
{
    QemuPlugin_SysBusDevice *pdev;

    if (g_plugin == NULL) {
        /* No plug-in */
        return;
    }

    QLIST_FOREACH(pdev, &g_plugin->devices_list, list) {
        if (pdev->info->pdevice_exit != NULL) {
            pdev->info->pdevice_exit(pdev->info->opaque);
        } else {
            fprintf(stderr, "%s: error: no init callback\n",
                    pdev->info->name);
        }
    }
}

static void plugin_cpu_reset(void *null)
{
    QemuPlugin_SysBusDevice *pdev;

    if (g_plugin == NULL) {
        /* No plug-in */
        return;
    }

    QLIST_FOREACH(pdev, &g_plugin->devices_list, list) {
        if (pdev->info->pdevice_reset != NULL) {
            pdev->info->pdevice_reset(pdev->info->opaque);
        } else {
            fprintf(stderr, "%s: error: no reset callback\n",
                    pdev->info->name);
        }
    }
}

static QemuPLugin_InitFunction plugin_load_init_func(const char *plugin_name,
                                                     const char *function_name)
{
    QemuPLugin_InitFunction ret;
#ifdef _WIN32
    HINSTANCE hInstPlugin;

    hInstPlugin = LoadLibrary(plugin_name);

    if (hInstPlugin == NULL) {
        fprintf(stderr, "error: cannot load plug-in '%s' error:%u\n",
                plugin_name, GetLastError());
        return NULL;
    }

    ret = (QemuPLugin_InitFunction)GetProcAddress(hInstPlugin,
                                                  function_name);
    if (ret == NULL) {
        fprintf(stderr, "error: cannot load plug-in init function '%s'"
                " error:%u\n", plugin_name, GetLastError());
        return NULL;
    }

    return ret;
#else
    void *handle;

    handle = dlopen(plugin_name, RTLD_LAZY | RTLD_LOCAL);

    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        return NULL;
    }

    ret = dlsym(handle, function_name);

    if (ret == NULL) {
        fprintf(stderr, "%s\n", dlerror());
    }

    return ret;
#endif
}

static uint32_t target_endianness(void)
{
#if defined(GNATBUS_BIG_ENDIAN)
    return TargetEndianness_BigEndian;
#else
    return TargetEndianness_LittleEndian;
#endif
}

static uint32_t shutdown_request(void)
{
    pause_all_vcpus();
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
    return 0;
}

void plugin_init(qemu_irq *cpu_irqs, int nr_irq)
{
    char **arg_list;
    char **tmp;
    char *value;

    QemuPLugin_InitFunction init;

    if (g_plugin == NULL || g_plugin->plugin_optarg == NULL) {
        return;
    }

    /* Initialize IRQ fields */
    g_plugin->nr_irq   = nr_irq;
    g_plugin->cpu_irqs = cpu_irqs;

    /* Initialize emulator-side callbacks */
    g_plugin->emulator_interface.version        = QEMU_PLUGIN_INTERFACE_VERSION;
    g_plugin->emulator_interface.get_time          = plugin_get_time;
    g_plugin->emulator_interface.add_event         = plugin_add_event;
    g_plugin->emulator_interface.remove_event      = plugin_remove_event;
    g_plugin->emulator_interface.set_irq           = plugin_set_irq;
    g_plugin->emulator_interface.dma_read          = plugin_dma_read;
    g_plugin->emulator_interface.dma_write         = plugin_dma_write;
    g_plugin->emulator_interface.attach_device     = attach_device;
    g_plugin->emulator_interface.target_endianness = target_endianness;
    g_plugin->emulator_interface.shutdown_request  = shutdown_request;

    /* Initialize devices list */
    QLIST_INIT(&g_plugin->devices_list);

    /* Register exit callback */
    atexit(plugin_device_exit);

    /* Initialize events lists */
    QLIST_INIT(&g_plugin->vm_events.events_list);
    QLIST_INIT(&g_plugin->host_events.events_list);

    /* Initialize timers */
    g_plugin->vm_events.timer   = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                               plugin_timer_tick,
                                               &g_plugin->vm_events);
    g_plugin->host_events.timer = timer_new_ns(QEMU_CLOCK_HOST,
                                               plugin_timer_tick,
                                               &g_plugin->host_events);


    /* Initialize bottom halves */
    g_plugin->vm_events.bh = qemu_bh_new(plugin_timer_tick_bh,
                                           &g_plugin->vm_events);
    g_plugin->host_events.bh = qemu_bh_new(plugin_timer_tick_bh,
                                           &g_plugin->host_events);
    g_plugin->vm_events.qclock   = QEMU_CLOCK_VIRTUAL;
    g_plugin->host_events.qclock = QEMU_CLOCK_HOST;

    /* Register CPU reset callback */
    qemu_register_reset(plugin_cpu_reset, NULL);
    arg_list = g_strsplit(g_plugin->plugin_optarg, ",", 0);
    tmp = arg_list;

    while (tmp && *tmp) {
        /* Find parameter name and value in the string.
         *  - name is the file to load
         *  - value is the option string for the plug-in
         */
        value = strchr(*tmp, '=');
        if (value) {
            value[0] = 0;
            value++;
        }

        /* Load the plug-in */
        init = plugin_load_init_func(*tmp, "__qemu_plugin_init");

        if (init != NULL) {
            /* Call plug-in initialization function */
            init(&g_plugin->emulator_interface, value);
        }
    }
}

void plugin_save_optargs(const char *optarg)
{
    g_plugin = g_malloc(sizeof(*g_plugin));
    g_plugin->plugin_optarg = g_strdup(optarg);
}
