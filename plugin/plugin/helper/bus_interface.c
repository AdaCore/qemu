#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "qemu_plugin_interface.h"
#include "bus_interface.h"
#include "init.h"

#define DEVICE_CHECK_RETURN(return_value)                        \
if (dev == NULL) {                                               \
    fprintf(stderr, "%s error: "                                 \
            "Device not properly initialized\n", __func__);      \
    return return_value;                                         \
 }                                                               \

#define STATE_ERROR_RETURN(expected_state, msg, return_value)           \
if (qemu_plugin_get_state() != expected_state) {                        \
    fprintf(stderr, "%s: %s error: "                                    \
            msg " (plug-in state: %s)\n", dev->name, __func__,          \
            state_to_string(qemu_plugin_get_state()));                  \
    return return_value;                                                \
 }                                                                      \

#define STATE_ERROR(pstate, expected_state, msg, return_value) \
if (qemu_plugin_get_state() != expected_state) {               \
    fprintf(stderr, "%s: %s error: "                           \
            msg " (plug-in state: %s)\n", dev->name, __func__, \
            state_to_string(qemu_plugin_get_state()));         \
 }                                                             \

#define PLUGIN_CHECK_RETURN(return_value)                               \
if (g_emulator == NULL) {                                            \
    fprintf(stderr, "%s: %s error: "                                    \
            "Plug-in not properly initialized\n", dev->name, __func__); \
    return return_value;                                                \
 }                                                                      \

static const char *state_to_string(QemuPlugin_State state)
{
    switch (state) {
    case QPS_LOADED:
        return "Loaded";
        break;
    case QPS_INITIALIZATION:
        return "Initialization";
        break;
    case QPS_RUNNING:
        return "Running";
        break;
    case QPS_ERROR:
        return "Error";
        break;
    case QPS_STOPED:
        return "Stoped";
        break;
    default:
        return "Unknown state";
    }

}


QemuPlugin_Emulator *g_emulator = NULL;

/* Devices */

Device *allocate_device(void)
{
    return calloc(sizeof(Device), 1);
}

int set_device_info(Device     *dev,
                    void       *opaque,
                    uint32_t    vendor_id,
                    uint32_t    device_id,
                    const char *name,
                    const char *desc)
{
    DEVICE_CHECK_RETURN(-1);

    if (name == NULL) {
        fprintf(stderr, "%s: %s error: "
                "device name missing\n", dev->name, __func__);
        return -1;
    }

    dev->opaque    = opaque;
    dev->vendor_id = vendor_id;
    dev->device_id = device_id;
    dev->nr_iomem  = 0;

    /* Copy device name */
    strncpy(dev->name, name, NAME_LENGTH - 1);

    /* Set trailing null character */
    dev->name[NAME_LENGTH - 1] = '\0';

    /* Copy device description */
    strncpy(dev->desc, desc, DESC_LENGTH - 1);

    /* Set trailing null character */
    dev->desc[DESC_LENGTH - 1] = '\0';

    return 0;
}

int register_callbacks(Device      *dev,
                       io_read_fn  *io_read,
                       io_write_fn *io_write,
                       reset_fn    *device_reset,
                       init_fn     *device_init,
                       exit_fn     *device_exit)
{
    DEVICE_CHECK_RETURN(-1);

    dev->io_read       = io_read;
    dev->io_write      = io_write;
    dev->pdevice_reset = device_reset;
    dev->pdevice_init  = device_init;
    dev->pdevice_exit  = device_exit;

    return QP_NOERROR;
}

int register_io_memory(Device        *dev,
                       target_addr_t  base,
                       target_addr_t  size)
{
    DEVICE_CHECK_RETURN(-1);

    if (dev->nr_iomem >= MAX_IOMEM) {
        fprintf(stderr, "%s: %s error: "
                "Maximum I/O memory number reached\n", dev->name, __func__);
        return -1;
    }

    dev->iomem[dev->nr_iomem].base = base;
    dev->iomem[dev->nr_iomem].size = size;
    dev->nr_iomem++;

    return QP_NOERROR;
}

int register_device(Device *dev,
                    int     port)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_INITIALIZATION,
                       "devices must be attached during initialization", -1);

    g_emulator->attach_device(dev);

    return QP_NOERROR;
}

int device_loop(Device *dev)
{
    /* Unreachable code in plug-in mode */
    abort();
}

/* Time */

uint64_t get_vm_time(Device *dev)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    return g_emulator->get_time(QP_VM_CLOCK);
}

/* uint64_t get_host_time(void) */
/* { */
/*     PLUGIN_CHECK_RETURN(-1); */
/*     STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1); */

/*     return g_emulator->get_time(QP_HOST_CLOCK); */
/* } */

int add_vm_event(Device        *dev,
                 uint64_t       expire,
                 uint32_t       event_id,
                 EventCallback  event)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    if (event == NULL) {
        fprintf(stderr, "%s: %s error: invalid event\n", dev->name, __func__);
        return -1;
    }

    return g_emulator->add_event(expire,
                                 QP_VM_CLOCK,
                                 event_id,
                                 event,
                                 dev->opaque);
}

/* int add_host_event(uint64_t expire, QemuPlugin_EventCallback event_id) */
/* { */
/*     PLUGIN_CHECK_RETURN(-1); */
/*     STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1); */

/*     if (event == NULL) { */
/*         fprintf(stderr, "%s: %s error: invalid event\n", */
/*                 dev->name, __func__); */
/*         return -1; */
/*     } */

/*     return g_emulator->add_event(expire, QP_HOST_CLOCK, event); */
/* } */

/* int remove_vm_event(Device *dev, uint32_t eventid) */
/* { */
/*     DEVICE_CHECK_RETURN(-1); */
/*     PLUGIN_CHECK_RETURN(-1); */
/*     STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1); */

/*     if (event == NULL) { */
/*     fprintf(stderr, "%s: %s error: invalid event\n", dev->name, __func__); */
/*         return -1; */
/*     } */

/*     return g_emulator->remove_event(QP_VM_CLOCK, event_id); */
/* } */

/* int remove_host_event(QemuPlugin_EventCallback event) */
/* { */
/*     PLUGIN_CHECK_RETURN(-1); */
/*     STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1); */

/*     if (event == NULL) { */
/*         fprintf(stderr, "%s: %s error: invalid event\n", */
/*                 dev->name, __func__); */
/*         return -1; */
/*     } */

/*     return g_emulator->remove_event(QP_HOST_CLOCK, event); */
/* } */

uint64_t get_time(Device *dev)
{
    DEVICE_CHECK_RETURN(-1);

    return get_vm_time(dev);
}

int add_event(Device        *dev,
              uint64_t       expire,
              uint32_t       event_id,
              EventCallback  event)
{
    DEVICE_CHECK_RETURN(-1);
    return add_vm_event(dev, expire, event_id, event);
}

/* IRQ */

int IRQ_raise(Device *dev, uint8_t line)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    return g_emulator->set_irq(line, 1);
}

int IRQ_lower(Device *dev, uint8_t line)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    return g_emulator->set_irq(line, 0);
}

int IRQ_pulse(Device *dev, uint8_t line)
{
    int ret;

    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    ret = g_emulator->set_irq(line, 1);

    if (ret != QP_NOERROR) {
        return ret;
    }

    return g_emulator->set_irq(line, 0);
}


/* DMA */

int dma_read(Device *dev, void *dest, target_addr_t addr, int size)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    return g_emulator->dma_read(dest, addr, size);
}

int dma_write(Device *dev, void *src, target_addr_t addr, int size)
{
    DEVICE_CHECK_RETURN(-1);
    PLUGIN_CHECK_RETURN(-1);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", -1);

    return g_emulator->dma_write(src, addr, size);
}
