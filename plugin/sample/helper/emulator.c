#include <stdlib.h>
#include <stdio.h>

#include "qemu_plugin_interface.h"
#include "emulator.h"
#include "init.h"

QemuPlugin_Emulator *g_emulator;

#define STATE_ERROR_RETURN(expected_state, msg, return_value) \
    if (qemu_plugin_get_state() != expected_state) {          \
        fprintf(stderr, PLUGIN_NAME ": %s error: "            \
                msg " (plug-in state: %s)\n", __func__,       \
                state_to_string(qemu_plugin_get_state()));    \
        return return_value;                                  \
    }                                                         \

#define STATE_ERROR(pstate, expected_state, msg, return_value) \
    if (qemu_plugin_get_state() != expected_state) {           \
        fprintf(stderr, PLUGIN_NAME ": %s error: "             \
                msg " (plug-in state: %s)\n", __func__,        \
                state_to_string(qemu_plugin_get_state()));     \
    }                                                          \

#define PLUGIN_CHECK_RETURN(return_value)                         \
    if (g_emulator == NULL) {                                     \
        fprintf(stderr, PLUGIN_NAME ": %s error: "                \
                "Plug-in not properly initialized \n", __func__); \
        return return_value;                                      \
    }                                                             \

static const char *state_to_string(QemuPlugin_State state)
{
    switch (state){
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

/* Devices */

QemuPlugin_DeviceInfo *create_device(uint32_t            vendor_id,
                                     uint32_t            device_id,
                                     qemu_device_endian  endianness,
                                     const char         *name,
                                     const char         *desc)
{
    QemuPlugin_DeviceInfo *ret = NULL;
    int i = 0;

    if (name == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: "
                "device name missing\n", __func__);
        return NULL;
    }

    ret = calloc(sizeof(*ret), 1);
    if (ret == NULL) {
        return NULL;
    }

    ret->vendor_id = vendor_id;
    ret->device_id = device_id;
    ret->endianness = endianness;
    ret->nr_iomem  = 0;

    /* Copy device name */
    for (i = 0; i < NAME_LENGTH && name[i] != '\0'; i++) {
        ret->name[i] = name[i];
    }

    if (i == NAME_LENGTH && name[i] != '\0') {
        fprintf(stderr, PLUGIN_NAME ": %s warning: "
                "device name too long\n", __func__);
    }

    /* Set trailing null character */
    ret->name[i] = '\0';

    /* Copy device description */
    i = 0;
    if (desc != NULL) {
        for (; i < DESC_LENGTH && desc[i] != '\0'; i++) {
            ret->desc[i] = desc[i];
        }
    }

    if (i == DESC_LENGTH && desc[i] != '\0') {
        fprintf(stderr, PLUGIN_NAME ": %s warning: "
                "device description too long\n", __func__);
    }

    /* Set trailing null character */
    ret->name[i] = '\0';

    return ret;
}

uint32_t register_callbacks(QemuPlugin_DeviceInfo *dev,
                            io_read_fn            *io_read,
                            io_write_fn           *io_write,
                            reset_fn              *device_reset,
                            init_fn               *device_init,
                            exit_fn               *device_exit)
{
    if ( dev == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid device\n", __func__);
        return QP_ERROR;
    }

    dev->io_read       = io_read;
    dev->io_write      = io_write;
    dev->pdevice_reset = device_reset;
    dev->pdevice_init  = device_init;
    dev->pdevice_exit  = device_exit;

    return QP_NOERROR;
}

uint32_t register_io_memory(QemuPlugin_DeviceInfo *dev,
                            target_addr_t          base,
                            target_addr_t          size)
{
    if ( dev == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid device\n", __func__);
        return QP_ERROR;
    }

    if (dev->nr_iomem >= MAX_IOMEM) {
        fprintf(stderr, PLUGIN_NAME ": %s error: "
                "Maximum I/O memory number reached\n", __func__);
        return QP_ERROR;
    }

    dev->iomem[dev->nr_iomem].base = base;
    dev->iomem[dev->nr_iomem].size = size;
    dev->nr_iomem++;

    return QP_NOERROR;
}

uint32_t attach_device(QemuPlugin_DeviceInfo *dev)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_INITIALIZATION,
                       "devices must be attached during initialization", QP_ERROR);

    if (dev == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid device\n", __func__);
        return QP_ERROR;
    }

    g_emulator->attach_device(dev);

    return QP_NOERROR;
}

/* Time */

vm_time_t get_vm_time(void)
{
    PLUGIN_CHECK_RETURN(0);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", 0);

    return g_emulator->get_time(QP_VM_CLOCK);
}

host_time_t get_host_time(void)
{
    PLUGIN_CHECK_RETURN(0);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", 0);

    return g_emulator->get_time(QP_HOST_CLOCK);
}

uint32_t add_vm_event(vm_time_t expire, QemuPlugin_EventCallback event)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    if (event == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid event\n", __func__);
        return QP_ERROR;
    }

    return g_emulator->add_event(expire, QP_VM_CLOCK, event);
}

uint32_t add_host_event(host_time_t expire, QemuPlugin_EventCallback event)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    if (event == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid event\n", __func__);
        return QP_ERROR;
    }

    return g_emulator->add_event(expire, QP_HOST_CLOCK, event);
}

uint32_t remove_vm_event(QemuPlugin_EventCallback event)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    if (event == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid event\n", __func__);
        return QP_ERROR;
    }

    return g_emulator->remove_event(QP_VM_CLOCK, event);
}

uint32_t remove_host_event(QemuPlugin_EventCallback event)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    if (event == NULL) {
        fprintf(stderr, PLUGIN_NAME ": %s error: invalid event\n", __func__);
        return QP_ERROR;
    }

    return g_emulator->remove_event(QP_HOST_CLOCK, event);
}

/* IRQ */

uint32_t IRQ_raise(uint32_t line)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    return g_emulator->set_irq(line, 1);
}

uint32_t IRQ_lower(uint32_t line)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    return g_emulator->set_irq(line, 0);
}

uint32_t IRQ_pulse(uint32_t line)
{
    uint32_t ret;

    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    ret = g_emulator->set_irq(line, 1);

    if (ret != QP_NOERROR) {
        return ret;
    }

    return g_emulator->set_irq(line, 1);
}


/* DMA */

uint32_t dma_read(void *dest, target_addr_t addr, int size)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    return g_emulator->dma_read(dest, addr, size);
}

uint32_t dma_write(void *src, target_addr_t addr, int size)
{
    PLUGIN_CHECK_RETURN(QP_ERROR);
    STATE_ERROR_RETURN(QPS_RUNNING, "Operation not available", QP_ERROR);

    return g_emulator->dma_write(src, addr, size);
}
