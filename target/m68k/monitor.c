/*
 * QEMU monitor for m68k
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "monitor/hmp-target.h"

static const MonitorDef monitor_defs[] = {
    { "d0", offsetof(CPUM68KState, dregs[0]) },
    { "d1", offsetof(CPUM68KState, dregs[1]) },
    { "d2", offsetof(CPUM68KState, dregs[2]) },
    { "d3", offsetof(CPUM68KState, dregs[3]) },
    { "d4", offsetof(CPUM68KState, dregs[4]) },
    { "d5", offsetof(CPUM68KState, dregs[5]) },
    { "d6", offsetof(CPUM68KState, dregs[6]) },
    { "d7", offsetof(CPUM68KState, dregs[7]) },
    { "a0", offsetof(CPUM68KState, aregs[0]) },
    { "a1", offsetof(CPUM68KState, aregs[1]) },
    { "a2", offsetof(CPUM68KState, aregs[2]) },
    { "a3", offsetof(CPUM68KState, aregs[3]) },
    { "a4", offsetof(CPUM68KState, aregs[4]) },
    { "a5", offsetof(CPUM68KState, aregs[5]) },
    { "a6", offsetof(CPUM68KState, aregs[6]) },
    { "a7", offsetof(CPUM68KState, aregs[7]) },
    { "pc", offsetof(CPUM68KState, pc) },
    { "sr", offsetof(CPUM68KState, sr) },
    { "ssp", offsetof(CPUM68KState, sp[0]) },
    { "usp", offsetof(CPUM68KState, sp[1]) },
    { "isp", offsetof(CPUM68KState, sp[2]) },
    { NULL },
};

const MonitorDef *target_monitor_defs(void)
{
    return monitor_defs;
}
