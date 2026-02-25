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

#ifndef QEMU_PHY_H
#define QEMU_PHY_H

#include "qom/object.h"
#include "qemu/module.h"

#define TYPE_QEMU_PHY "qemu-phy"
#define QEMU_PHY_GET_CLASS(obj) \
        OBJECT_GET_CLASS(QEMUPhyClass, obj, TYPE_QEMU_PHY)
#define QEMU_PHY_CLASS(klass) \
        OBJECT_CLASS_CHECK(QEMUPhyClass, klass, TYPE_QEMU_PHY)
#define QEMU_PHY(obj) OBJECT_CHECK(QEMUPhy, (obj), TYPE_QEMU_PHY)

typedef struct QEMUPhy {
    /*< private >*/
    Object parent;
    /*< public >*/
    int link_down;
} QEMUPhy;

typedef struct QEMUPhyClass {
    /*< private >*/
    ObjectClass parent_class;
    /*< public >*/
    void (*phy_update_link)(QEMUPhy *s, int link_down);
    void (*phy_reset)(QEMUPhy *s);
    uint16_t (*phy_read)(QEMUPhy *s, unsigned int reg_num);
    void (*phy_write)(QEMUPhy *s, unsigned int reg_num, uint16_t val);
    int (*phy_loopback)(QEMUPhy *s);
} QEMUPhyClass;

/*
 * qemu_phy_update_link:
 *
 * Make the emulated PHY link state described by @link_down.
 */
void qemu_phy_update_link(QEMUPhy *s, int link_down);
/*
 * qemu_phy_reset:
 *
 * Reset the PHY registers.
 */
void qemu_phy_reset(QEMUPhy *s);
/*
 * qemu_phy_read:
 *
 * Returns register @reg_num.
 */
uint16_t qemu_phy_read(QEMUPhy *s, unsigned int reg_num);
/*
 * qemu_phy_write:
 *
 * Write @val in register @reg_num of the PHY @s.
 */
void qemu_phy_write(QEMUPhy *s, unsigned int reg_num, uint16_t val);
/*
 * qemu_phy_loopback:
 *
 * Returns true if the phy is in loopback mode, false otherwise.
 */
int qemu_phy_loopback(QEMUPhy *s);

#endif /* QEMU_PHY_H */
