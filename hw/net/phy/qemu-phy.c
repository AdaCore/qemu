/*
 * QEMU PHY
 *
 * Copyright (c) 2019 AdaCore
 *
 *  Developed by :
 *  Frederic Konrad   <frederic.konrad@adacore.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "hw/net/phy/qemu-phy.h"
#include "qemu/log.h"

#ifndef DEBUG_QEMU_PHY
#define DEBUG_QEMU_PHY 0
#endif

#define DPRINTF(fmt, ...) do {                                               \
    if (DEBUG_QEMU_PHY) {                                                    \
        qemu_log(TYPE_QEMU_PHY ": " fmt , ## __VA_ARGS__);                   \
    }                                                                        \
} while (0)

/*
 * qemu_phy_update_link:
 * Make the emulated PHY link state match the QEMU "interface" state.
 */
void qemu_phy_update_link(QEMUPhy *s, int link_down)
{
    QEMUPhyClass *klass = QEMU_PHY_GET_CLASS(s);

    DPRINTF("link %s\n", link_down ? "down" : "up");
    s->link_down = link_down;
    if (klass->phy_update_link) {
        klass->phy_update_link(s, link_down);
    }
}

void qemu_phy_reset(QEMUPhy *s)
{
    QEMUPhyClass *klass = QEMU_PHY_GET_CLASS(s);

    DPRINTF("reset\n");
    if (klass->phy_reset) {
        klass->phy_reset(s);
    }
    qemu_phy_update_link(s, s->link_down);
}

uint16_t qemu_phy_read(QEMUPhy *s, unsigned int reg_num)
{
    QEMUPhyClass *klass = QEMU_PHY_GET_CLASS(s);
    uint16_t ret = 0;

    if (klass->phy_read) {
        ret = klass->phy_read(s, reg_num);
    }

    DPRINTF("read register 0x%2.2x value: 0x%04x\n", reg_num, ret);
    return ret;
}

void qemu_phy_write(QEMUPhy *s, unsigned int reg_num, uint16_t val)
{
    QEMUPhyClass *klass = QEMU_PHY_GET_CLASS(s);

    DPRINTF("write register 0x%2.2x value: 0x%04x\n", reg_num, val);

    if (klass->phy_write) {
        klass->phy_write(s, reg_num, val);
    }
}

int qemu_phy_loopback(QEMUPhy *s)
{
    QEMUPhyClass *klass = QEMU_PHY_GET_CLASS(s);
    int ret = 0;

    if (klass->phy_loopback) {
        ret = klass->phy_loopback(s);
    }

    DPRINTF("device loopback mode: %s\n", ret ? "yes" : "no");
    return ret;
}

static const TypeInfo qemu_phy_info = {
    .name = TYPE_QEMU_PHY,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(QEMUPhy),
    .class_size = sizeof(QEMUPhyClass),
};

static void qemu_phy_register_types(void)
{
    type_register_static(&qemu_phy_info);
}

type_init(qemu_phy_register_types)
