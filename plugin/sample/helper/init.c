#include <stdio.h>

#include "init.h"
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
    uint32_t ret;

    /* Check emulator structure */
    if (emu            == NULL || emu->get_time == NULL      ||
        emu->add_event == NULL || emu->remove_event  == NULL ||
        emu->set_irq   == NULL || emu->dma_read == NULL      ||
        emu->dma_write == NULL || emu->attach_device == NULL) {

        fprintf(stderr, PLUGIN_NAME ": initialisation failed: "
                "Invalid QemuPlugin_Emulator structure\n");
        return QP_ERROR;
    }

    /* Check version */
    if (emu->version != QEMU_PLUGIN_INTERFACE_VERSION) {
        fprintf(stderr, PLUGIN_NAME ": initialisation failed: "
                "interface version mismatch -> plug-in:%d qemu:%d\n",
                QEMU_PLUGIN_INTERFACE_VERSION, emu->version);
        return QP_ERROR;
    }

    /* Check args */
    if (args == NULL) {
        fprintf(stderr, PLUGIN_NAME ": initialisation failed: "
                "'args' string is NULL\n");
        return QP_ERROR;
    }

    /* save emulator structure */
    g_emulator = emu;

    qemu_plugin_set_state(QPS_INITIALIZATION);

    /* Call user initialisation */
    ret = qemu_plugin_init(args);

    qemu_plugin_set_state((ret == QP_NOERROR) ? QPS_RUNNING : QPS_ERROR);

    return ret;
}
