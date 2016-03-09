#include <stdio.h>

#include "init.h"
#include "bus_interface.h"
#include "qemu_plugin_interface.h"

QemuPlugin_State g_plugin_state = QPS_LOADED;

QemuPlugin_State qemu_plugin_get_state(void)
{
    return g_plugin_state;
}

void qemu_plugin_set_state(QemuPlugin_State state)
{
    g_plugin_state = state;
}


uint32_t __qemu_plugin_init(QemuPlugin_Emulator *emu, const char *args)
{
    Device   *dev;
    uint32_t  ret = QP_NOERROR;

    /* Check emulator structure */
    if (emu            == NULL || emu->get_time      == NULL ||
        emu->add_event == NULL || emu->remove_event  == NULL ||
        emu->set_irq   == NULL || emu->dma_read      == NULL ||
        emu->dma_write == NULL || emu->attach_device == NULL) {

        fprintf(stderr, "Plugin: initialisation failed: "
                "Invalid QemuPlugin_Emulator structure\n");
        return QP_ERROR;
    }

    /* Check version */
    if (emu->version != QEMU_PLUGIN_INTERFACE_VERSION) {
        fprintf(stderr, "Plugin: initialisation failed: "
                "interface version mismatch -> plug-in:%d qemu:%d\n",
                QEMU_PLUGIN_INTERFACE_VERSION, emu->version);
        return QP_ERROR;
    }

    /* Check args */
    if (args == NULL) {
        fprintf(stderr, "Plugin: initialisation failed: "
                "'args' string is NULL\n");
        return QP_ERROR;
    }

    /* save emulator structure */
    g_emulator = emu;

    qemu_plugin_set_state(QPS_INITIALIZATION);

#ifdef ADA_PLUGIN
    plugin_entry();
#else
    dev = allocate_device();
    if (dev == NULL) {
        fprintf(stderr, "Plugin: Failed to allocate device!\n");
        return QP_ERROR;
    }

    /* Call user initialisation */
    const char *argv[2] = {"main", args};
    ret = device_setup(dev, 2, argv);

    if (ret >= 0 && register_device(dev, 0) < 0) {
        fprintf(stderr, "Plugin: Failed to register device!\n");
        ret = QP_ERROR;
    }
#endif

    qemu_plugin_set_state((ret == QP_NOERROR) ? QPS_RUNNING : QPS_ERROR);

    return ret;
}
