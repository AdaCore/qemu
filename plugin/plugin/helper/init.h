#ifndef _INIT_H_
#define _INIT_H_

#include "qemu_plugin_interface.h"
#include "bus_interface.h"

extern QemuPlugin_Emulator *g_emulator;

QemuPlugin_State qemu_plugin_get_state(void);
void qemu_plugin_set_state(QemuPlugin_State state);

#ifdef ADA_PLUGIN

extern void plugin_enty(void);

#else

/* Defined in the plugin */
extern int device_setup(Device *dev, int argc, const char *argv[]);

#endif

#endif /* ! _INIT_H_ */
