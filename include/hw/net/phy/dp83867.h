/*
 * TI DP83867 PHY
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

#ifndef DP83867_PHY_H
#define DP83867_PHY_H

#include "hw/net/phy/qemu-phy.h"

#define DP83867_PHY_REGS_COUNT        32
#define DP83867_PHY_TOTAL_REGS_COUNT 0x200

typedef struct Dp83867Phy {
    /*< private >*/
    QEMUPhy parent;
    /*< public >*/
    uint16_t regs[DP83867_PHY_TOTAL_REGS_COUNT];
    int phy_loop;
    uint16_t current_extented_address;
} Dp83867Phy;

#define TYPE_DP83867_PHY "dp83867-phy"
#define DP83867_PHY(obj) OBJECT_CHECK(Dp83867Phy, (obj), TYPE_DP83867_PHY)

#endif /* DP83867_PHY_H */
