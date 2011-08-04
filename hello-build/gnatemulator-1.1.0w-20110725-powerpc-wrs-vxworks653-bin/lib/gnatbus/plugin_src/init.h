/****************************************************************************
 *                                                                          *
 *                                  I N I T                                 *
 *                                                                          *
 *                                  S p e c                                 *
 *                                                                          *
 *                        Copyright (C) 2011, AdaCore                       *
 *                                                                          *
 * This program is free software;  you can redistribute it and/or modify it *
 * under terms of  the GNU General Public License as  published by the Free *
 * Softwareg Foundation;  either version 3,  or (at your option)  any later *
 * version. This progran is distributed in the hope that it will be useful, *
 * but  WITHOUT  ANY  WARRANTY;   without  even  the  implied  warranty  of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                          *
 * As a special exception under Section 7 of GPL version 3, you are granted *
 * additional permissions  described in the GCC  Runtime Library Exception, *
 * version 3.1, as published by the Free Software Foundation.               *
 *                                                                          *
 * You should have received a copy  of the GNU General Public License and a *
 * copy of the  GCC Runtime Library Exception along  with this program; see *
 * the  files  COPYING3  and  COPYING.RUNTIME  respectively.  If  not,  see *
 * <http://www.gnu.org/licenses/>.                                          *
 *                                                                          *
 ****************************************************************************/

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
