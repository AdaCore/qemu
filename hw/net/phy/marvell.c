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

#include "qemu/osdep.h"
#include "hw/net/phy/marvell.h"
#include "qemu/log.h"

/* Marvell PHY definitions */
#define PHY_REG_CONTROL            0
#define PHY_REG_STATUS             1
#define PHY_REG_PHYID1             2
#define PHY_REG_PHYID2             3
#define PHY_REG_ANEGADV            4
#define PHY_REG_LINKPABIL          5
#define PHY_REG_ANEGEXP            6
#define PHY_REG_NEXTP              7
#define PHY_REG_LINKPNEXTP         8
#define PHY_REG_100BTCTRL          9
#define PHY_REG_1000BTSTAT        10
#define PHY_REG_EXTSTAT           15
#define PHY_REG_PHYSPCFC_CTL      16
#define PHY_REG_PHYSPCFC_ST       17
#define PHY_REG_INT_EN            18
#define PHY_REG_INT_ST            19
#define PHY_REG_EXT_PHYSPCFC_CTL  20
#define PHY_REG_RXERR             21
#define PHY_REG_EACD              22
#define PHY_REG_LED               24
#define PHY_REG_LED_OVRD          25
#define PHY_REG_EXT_PHYSPCFC_CTL2 26
#define PHY_REG_EXT_PHYSPCFC_ST   27
#define PHY_REG_CABLE_DIAG        28

#define PHY_REG_CONTROL_RST     0x8000
#define PHY_REG_CONTROL_LOOP    0x4000
#define PHY_REG_CONTROL_ANEG    0x1000

#define PHY_REG_STATUS_LINK     0x0004
#define PHY_REG_STATUS_ANEGCMPL 0x0020

#define PHY_REG_INT_ST_ANEGCMPL 0x0800
#define PHY_REG_INT_ST_LINKC    0x0400
#define PHY_REG_INT_ST_ENERGY   0x0010

#ifndef DEBUG_MARVELL_PHY
#define DEBUG_MARVELL_PHY 0
#endif

#define DPRINTF(fmt, ...) do {                                               \
    if (DEBUG_MARVELL_PHY) {                                                 \
        qemu_log(TYPE_MARVELL_PHY ": " fmt , ## __VA_ARGS__);                \
    }                                                                        \
} while (0)

static void marvell_phy_update_link(QEMUPhy *phy, int link_down)
{
    MarvellPhy *s = MARVELL_PHY(phy);

    /* Autonegotiation status mirrors link status.  */
    if (link_down) {
        s->regs[PHY_REG_STATUS] &= ~(PHY_REG_STATUS_ANEGCMPL |
                                         PHY_REG_STATUS_LINK);
        s->regs[PHY_REG_INT_ST] |= PHY_REG_INT_ST_LINKC;
    } else {
        s->regs[PHY_REG_STATUS] |= (PHY_REG_STATUS_ANEGCMPL |
                                         PHY_REG_STATUS_LINK);
        s->regs[PHY_REG_INT_ST] |= (PHY_REG_INT_ST_LINKC |
                                        PHY_REG_INT_ST_ANEGCMPL |
                                        PHY_REG_INT_ST_ENERGY);
    }
}

static void marvell_phy_reset(QEMUPhy *phy)
{
    MarvellPhy *s = MARVELL_PHY(phy);

    memset(&s->regs[0], 0, sizeof(s->regs));
    s->regs[PHY_REG_CONTROL] = 0x1140;
    s->regs[PHY_REG_STATUS] = 0x7969;
    s->regs[PHY_REG_PHYID1] = 0x0141;
    s->regs[PHY_REG_PHYID2] = 0x0CC2;
    s->regs[PHY_REG_ANEGADV] = 0x01E1;
    s->regs[PHY_REG_LINKPABIL] = 0xCDE1;
    s->regs[PHY_REG_ANEGEXP] = 0x000F;
    s->regs[PHY_REG_NEXTP] = 0x2001;
    s->regs[PHY_REG_LINKPNEXTP] = 0x40E6;
    s->regs[PHY_REG_100BTCTRL] = 0x0300;
    s->regs[PHY_REG_1000BTSTAT] = 0x7C00;
    s->regs[PHY_REG_EXTSTAT] = 0x3000;
    s->regs[PHY_REG_PHYSPCFC_CTL] = 0x0078;
    s->regs[PHY_REG_PHYSPCFC_ST] = 0x7C00;
    s->regs[PHY_REG_EXT_PHYSPCFC_CTL] = 0x0C60;
    s->regs[PHY_REG_LED] = 0x4100;
    s->regs[PHY_REG_EXT_PHYSPCFC_CTL2] = 0x000A;
    s->regs[PHY_REG_EXT_PHYSPCFC_ST] = 0x848B;
}

static uint16_t marvell_phy_read(QEMUPhy *phy, unsigned int reg_num)
{
    MarvellPhy *s = MARVELL_PHY(phy);

    assert(reg_num < MARVELL_PHY_REGS_COUNT);
    return s->regs[reg_num];
}

static void marvell_phy_write(QEMUPhy *phy, unsigned int reg_num,
                              uint16_t val)
{
    MarvellPhy *s = MARVELL_PHY(phy);

    assert(reg_num < MARVELL_PHY_REGS_COUNT);
    switch (reg_num) {
    case PHY_REG_CONTROL:
        if (val & PHY_REG_CONTROL_RST) {
            /* Phy reset */
            marvell_phy_reset(phy);
            val &= ~(PHY_REG_CONTROL_RST | PHY_REG_CONTROL_LOOP);
            s->phy_loop = 0;
        }
        if (val & PHY_REG_CONTROL_ANEG) {
            /* Complete autonegotiation immediately */
            val &= ~PHY_REG_CONTROL_ANEG;
            s->regs[PHY_REG_STATUS] |= PHY_REG_STATUS_ANEGCMPL;
        }
        if (val & PHY_REG_CONTROL_LOOP) {
            DPRINTF("PHY placed in loopback\n");
            s->phy_loop = 1;
        } else {
            s->phy_loop = 0;
        }
        break;
    }
    s->regs[reg_num] = val;
}

static int marvell_phy_loopback(QEMUPhy *phy)
{
    MarvellPhy *s = MARVELL_PHY(phy);

    return s->phy_loop;
}

static void marvell_phy_class_init(ObjectClass *oc, void *data)
{
    QEMUPhyClass *klass = QEMU_PHY_CLASS(oc);

    klass->phy_update_link = marvell_phy_update_link;
    klass->phy_reset = marvell_phy_reset;
    klass->phy_read = marvell_phy_read;
    klass->phy_write = marvell_phy_write;
    klass->phy_loopback = marvell_phy_loopback;
}

static const TypeInfo marvell_phy_info = {
    .name = TYPE_MARVELL_PHY,
    .parent = TYPE_QEMU_PHY,
    .instance_size = sizeof(MarvellPhy),
    .class_init = marvell_phy_class_init,
};

static void marvell_phy_register_types(void)
{
    type_register_static(&marvell_phy_info);
}

type_init(marvell_phy_register_types)
