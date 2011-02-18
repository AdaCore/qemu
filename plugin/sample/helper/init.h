#ifndef _INIT_H_
#define _INIT_H_

#include "qemu_plugin_interface.h"

extern QemuPlugin_Emulator *g_emulator;

QemuPlugin_State qemu_plugin_get_state(void);
void qemu_plugin_set_state(QemuPlugin_State state);

#endif /* ! _INIT_H_ */
