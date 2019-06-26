/*
 * Marvell PHY emulation
 *
 * Copyright (c) 2011 Xilinx, Inc.
 * Copyright (c) 2019 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MARVELL_PHY_H
#define MARVELL_PHY_H

#include "hw/net/phy/qemu-phy.h"

#define MARVELL_PHY_REGS_COUNT 32

typedef struct MarvellPhy {
    /*< private >*/
    QEMUPhy parent;
    /*< public >*/
    /* PHY registers backing store */
    uint16_t regs[MARVELL_PHY_REGS_COUNT];
    int phy_loop;
} MarvellPhy;

#define TYPE_MARVELL_PHY "marvell-phy"
#define MARVELL_PHY(obj) OBJECT_CHECK(MarvellPhy, (obj), TYPE_MARVELL_PHY)

#endif /* MARVELL_PHY_H */
