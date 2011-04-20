#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "qemu-common.h"
#include "qemu-option.h"
#include "sysemu.h"
#include "hw/sysbus.h"
#include "qemu-plugin.h"

#include "qemu_plugin_interface.h"

typedef QLIST_HEAD(QemuPlugin_SysBusDevice_List, QemuPlugin_SysBusDevice)
     QemuPlugin_SysBusDevice_List;

struct QemuPlugin_SysBusDevice;
typedef struct QemuPlugin_SysBusDevice QemuPlugin_SysBusDevice;

typedef struct QemuPlugin_IOMEM_BaseAddr {
    QemuPlugin_SysBusDevice *pdev;
    uint64_t                 base;
} QemuPlugin_IOMEM_BaseAddr;

struct QemuPlugin_SysBusDevice {
    SysBusDevice busdev;

    QemuPlugin_DeviceInfo *info;

    QemuPlugin_IOMEM_BaseAddr io_base[MAX_IOMEM];

    QLIST_ENTRY(QemuPlugin_SysBusDevice) list;
};

typedef QLIST_HEAD(QemuPlugin_Events_List, QemuPlugin_Event)
     QemuPlugin_Events_List;

typedef struct QemuPlugin_Event
{
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
    QEMUClock              *qclock;
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
        fprintf(stderr, "Qemu plug-in error: Invalid IRQ line: %d\n", line);
        return QP_ERROR;
    }

    qemu_set_irq(g_plugin->cpu_irqs[line], level);
    return QP_NOERROR;
}

static uint64_t plugin_get_time(QemuPlugin_ClockType clock)
{
    return qemu_get_clock((clock == QP_VM_CLOCK) ? vm_clock : host_clock);
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

    now = qemu_get_clock(ev->qclock);

    while (QLIST_FIRST(event_list) != NULL) {
        event = QLIST_FIRST(event_list);

        if (event->expire_time <= now) {
            QLIST_REMOVE(event, list);

            /* Run the callback */
            event->event(event->opaque, event->id, event->expire_time);
            qemu_free(event);
        } else {
            break;
        }
    }

    if (QLIST_EMPTY(event_list)) {
        qemu_mod_timer(timer, (uint64_t)-1);
    } else {
        qemu_mod_timer(timer, QLIST_FIRST(event_list)->expire_time);
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

    new              = qemu_mallocz(sizeof(*new));
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

    qemu_mod_timer(timer, QLIST_FIRST(event_list)->expire_time);

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
        qemu_mod_timer(timer, (uint64_t)-1);
    } else {
        qemu_mod_timer(timer, QLIST_FIRST(event_list)->expire_time);
    }

    return QP_ERROR;
}


static inline void plugin_write_generic(void               *opaque,
                                        target_phys_addr_t  addr,
                                        uint32_t            size,
                                        uint32_t            value)
{
    QemuPlugin_IOMEM_BaseAddr *io_base = opaque;
    QemuPlugin_SysBusDevice   *pdev    = io_base->pdev;

    if (pdev->info->io_write != NULL) {
        pdev->info->io_write(pdev->info->opaque,
                             io_base->base + addr, size, value);
    } else {
        fprintf(stderr, "%s: error: no write I/O callback\n", pdev->info->name);
    }
}

static void
plugin_writeb(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    plugin_write_generic(opaque, addr, 1, value);
}

static void
plugin_writew(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    plugin_write_generic(opaque, addr, 2, value);
}

static void
plugin_writel(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    plugin_write_generic(opaque, addr, 4, value);
}

static inline uint32_t plugin_read_generic(void *opaque,
                                           target_phys_addr_t addr,
                                           uint32_t size)
{
    QemuPlugin_IOMEM_BaseAddr *io_base = opaque;
    QemuPlugin_SysBusDevice   *pdev    = io_base->pdev;

    if (pdev->info->io_read != NULL) {
        return pdev->info->io_read(pdev->info->opaque,
                                   addr + io_base->base, size);
    } else {
        fprintf(stderr, "%s: error: no read I/O callback\n", pdev->info->name);
        return 0;
    }
}

static uint32_t plugin_readb(void *opaque, target_phys_addr_t addr)
{
    return plugin_read_generic(opaque, addr, 1);
}

static uint32_t plugin_readw(void *opaque, target_phys_addr_t addr)
{
    return plugin_read_generic(opaque, addr, 2);
}

static uint32_t plugin_readl(void *opaque, target_phys_addr_t addr)
{
    return plugin_read_generic(opaque, addr, 4);
}

static CPUReadMemoryFunc * const plugin_read[] = {
    plugin_readb, plugin_readw, plugin_readl,
};

static CPUWriteMemoryFunc * const plugin_write[] = {
    plugin_writeb, plugin_writew, plugin_writel,
};

static int plugin_init_device(SysBusDevice *dev)
{
    QemuPlugin_SysBusDevice *pdev = FROM_SYSBUS(typeof(*pdev), dev);
    int ioindex;
    int i;


    for (i = 0; i < pdev->info->nr_iomem; i++) {

        pdev->io_base[i].pdev = pdev;
        pdev->io_base[i].base = pdev->info->iomem[i].base & TARGET_PAGE_MASK;

        ioindex = cpu_register_io_memory(plugin_read,
                                         plugin_write,
                                         &pdev->io_base[i],
                                         DEVICE_NATIVE_ENDIAN);

        if (ioindex <= 0) {
            return -1;
        }

        sysbus_init_mmio(dev, pdev->info->iomem[i].size, ioindex);
        sysbus_mmio_map(dev, i, pdev->info->iomem[i].base);
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

    sysbus_register_dev(devinfo->name, sizeof(*pdev), plugin_init_device);

    qdev = qdev_create(NULL, devinfo->name);
    pdev = (typeof(pdev))qdev;

    pdev->info = devinfo;

    if (qdev_init(qdev)) {
        return QP_ERROR;
    }

    QLIST_INSERT_HEAD(&g_plugin->devices_list, pdev, list);
    return QP_NOERROR;
}

static uint32_t plugin_dma_write(void          *src,
                             target_addr_t  addr,
                             int            size)
{
    cpu_physical_memory_write(addr, src, size);
    return QP_NOERROR;
}

static uint32_t plugin_dma_read(void          *dest,
                            target_addr_t  addr,
                            int            size)
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


void plugin_init(qemu_irq *cpu_irqs, int nr_irq)
{
    char        name[256];
    char        value[256];
    char        next_delim;
    char       *optarg_delim;
    char       *value_delim;
    const char *optarg;

    QemuPLugin_InitFunction init;

    if (g_plugin == NULL || g_plugin->plugin_optarg == NULL) {
        return;
    }

    optarg = g_plugin->plugin_optarg;

    /* Initialize IRQ fields */
    g_plugin->nr_irq   = nr_irq;
    g_plugin->cpu_irqs = cpu_irqs;

    /* Initialize emulator-side callbacks */
    g_plugin->emulator_interface.version       = QEMU_PLUGIN_INTERFACE_VERSION;
    g_plugin->emulator_interface.get_time      = plugin_get_time;
    g_plugin->emulator_interface.add_event     = plugin_add_event;
    g_plugin->emulator_interface.remove_event  = plugin_remove_event;
    g_plugin->emulator_interface.set_irq       = plugin_set_irq;
    g_plugin->emulator_interface.dma_read      = plugin_dma_read;
    g_plugin->emulator_interface.dma_write     = plugin_dma_write;
    g_plugin->emulator_interface.attach_device = attach_device;

    /* Initialize devices list */
    QLIST_INIT(&g_plugin->devices_list);

    /* Register exit callback */
    atexit(plugin_device_exit);

    /* Initialize events lists */
    QLIST_INIT(&g_plugin->vm_events.events_list);
    QLIST_INIT(&g_plugin->host_events.events_list);

    /* Initialize timers */
    g_plugin->vm_events.timer   = qemu_new_timer(vm_clock,
                                          plugin_timer_tick,
                                          &g_plugin->vm_events);
    g_plugin->host_events.timer = qemu_new_timer(host_clock,
                                          plugin_timer_tick,
                                          &g_plugin->host_events);


    /* Initialize bottom halves */
    g_plugin->vm_events.bh = qemu_bh_new(plugin_timer_tick_bh,
                                           &g_plugin->vm_events);
    g_plugin->host_events.bh = qemu_bh_new(plugin_timer_tick_bh,
                                           &g_plugin->host_events);
    g_plugin->vm_events.qclock   = vm_clock;
    g_plugin->host_events.qclock = host_clock;

    /* Register CPU reset callback */
    qemu_register_reset(plugin_cpu_reset, NULL);

    while (*optarg) {
        /* Find parameter name and value in the string.
         *  - name is the file to load
         *  - value is the option string for the plug-in
         */
        optarg_delim = strchr(optarg, ',');
        value_delim = strchr(optarg, '=');

        if (value_delim && (value_delim < optarg_delim || !optarg_delim)) {
            next_delim = '=';
        } else {
            next_delim = ',';
            value_delim = NULL;
        }

        optarg = get_opt_name(name, sizeof(name), optarg, next_delim);
        if (value_delim) {
            optarg = get_opt_value(value, sizeof(value), optarg + 1);
        } else {
            value[0] = '\0';
        }

        if (*optarg != '\0') {
            optarg++;
        }

        /* Load the plug-in */
        init = plugin_load_init_func(name, "__qemu_plugin_init");

        if (init != NULL) {
            /* Call plug-in initialization function */
            init(&g_plugin->emulator_interface, value);
        }
    }
}

void plugin_save_optargs(const char *optarg)
{
    g_plugin = qemu_malloc(sizeof(*g_plugin));
    g_plugin->plugin_optarg = qemu_strdup(optarg);
}
