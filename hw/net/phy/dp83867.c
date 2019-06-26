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
#include "qemu/osdep.h"
#include "hw/net/phy/dp83867.h"
#include "qemu/log.h"
#include "qemu/bitops.h"

#ifndef DEBUG_DP83867_PHY
#define DEBUG_DP83867_PHY 0
#endif

#define DPRINTF(fmt, ...) do {                                               \
    if (DEBUG_DP83867_PHY) {                                                 \
        qemu_log(TYPE_DP83867_PHY ": " fmt , ## __VA_ARGS__);                \
    }                                                                        \
} while (0)

#define BMCR_OFFSET      0x0000
#define BMCR_RESET_SHIFT    (15)
#define BMCR_LOOPBACK_SHIFT (14)
#define BMSR_OFFSET      0x0001
#define PHYIDR1_OFFSET   0x0002
#define PHYIDR2_OFFSET   0x0003
#define ANAR_OFFSET      0x0004
#define ANLPAR_OFFSET    0x0005
#define CFG1_OFFSET      0x0009
#define STS1_OFFSET      0x000A

/* Extented register support */
#define REGCR_OFFSET     0x000D
#define REGCR_FUNC_SHIFT    (14)
#define ADDAR_OFFSET     0x000E

/* Extented registers */
#define RGMIICTL_OFFSET  0x0032

static void dp83867_phy_update_link(QEMUPhy *phy, int link_down)
{
    Dp83867Phy *s = DP83867_PHY(phy);

    /* Autonegotiation status mirrors link status.  */
    if (link_down) {
        /* The link is down report that in the BMSR registers and the
         * STS1 register. */
        s->regs[BMSR_OFFSET] &= ~((1ULL << 2) | (1ULL << 5));
        s->regs[ANLPAR_OFFSET] = 0;
        s->regs[STS1_OFFSET] = 0;
    } else {
        s->regs[BMSR_OFFSET] |= (1ULL << 2) | (1ULL << 5);
        s->regs[ANLPAR_OFFSET] = (7ULL << 7);
        s->regs[STS1_OFFSET] = (3ULL << 12);
    }
}

static void dp83867_phy_reset(QEMUPhy *phy)
{
    Dp83867Phy *s = DP83867_PHY(phy);

    memset(&s->regs[0], 0, sizeof(s->regs));

    s->regs[BMCR_OFFSET] = 0x1120;
    s->regs[BMSR_OFFSET] = 0x7949;
    s->regs[PHYIDR1_OFFSET] = 0x2000;
    s->regs[PHYIDR2_OFFSET] = 0xA231;
    s->regs[ANAR_OFFSET] = 0x0181;
    s->regs[CFG1_OFFSET] = 0x0300;

    s->regs[RGMIICTL_OFFSET] = 0x00D0;
}

static uint16_t dp83867_phy_read(QEMUPhy *phy, unsigned int reg_num);
static void dp83867_phy_write(QEMUPhy *phy, unsigned int reg_num,
                              uint16_t val);

static void dp83867_phy_write_extented(Dp83867Phy *s, uint16_t val)
{
    if (s->current_extented_address < DP83867_PHY_REGS_COUNT) {
        /* Avoid deadlock here */
        if (s->current_extented_address != ADDAR_OFFSET) {
            dp83867_phy_write(&s->parent, s->current_extented_address, val);
        }
    } else {
        switch (s->current_extented_address) {
        default:
            DPRINTF("Unhandled extented register write @0x%4.4X\n",
                    s->current_extented_address);
            break;
        }
        s->regs[s->current_extented_address] = val;
    }
}

static uint16_t dp83867_phy_read_extented(Dp83867Phy *s)
{
    if (s->current_extented_address < DP83867_PHY_REGS_COUNT) {
        if (s->current_extented_address != ADDAR_OFFSET) {
            return dp83867_phy_read(&s->parent, s->current_extented_address);
        }
        return s->regs[ADDAR_OFFSET];
    } else {
        switch (s->current_extented_address) {
        default:
            break;
        }
        return s->regs[s->current_extented_address];
    }
}

static uint16_t dp83867_phy_read(QEMUPhy *phy, unsigned int reg_num)
{
    Dp83867Phy *s = DP83867_PHY(phy);

    switch (reg_num) {
    case ADDAR_OFFSET:
        switch (extract32(s->regs[REGCR_OFFSET], REGCR_FUNC_SHIFT, 2)) {
        case 0x00:
            /* Reading the address.. */
            return s->current_extented_address;
        default:
            /* Read Data no post increment */
            return dp83867_phy_read_extented(s);
        case 0x2:
          {
            uint16_t ret = dp83867_phy_read_extented(s);
            /* XXX: Maximum size ? */
            s->current_extented_address++;
            return ret;
          }
        }
    default:
        break;
    }

    return s->regs[reg_num];
}

static void dp83867_phy_write(QEMUPhy *phy, unsigned int reg_num,
                              uint16_t val)
{
    Dp83867Phy *s = DP83867_PHY(phy);

    switch (reg_num) {
    case BMCR_OFFSET:
        if (extract32(val, BMCR_RESET_SHIFT, 1)) {
            /* Keep the state of the link! */
            int link_down = extract32(s->regs[BMSR_OFFSET], 2, 1) == 0;

            dp83867_phy_reset(phy);
            dp83867_phy_update_link(phy, link_down);
            return;
        }
        break;
    case ADDAR_OFFSET:
        switch (extract32(s->regs[REGCR_OFFSET], REGCR_FUNC_SHIFT, 2)) {
        case 0x00:
            /* Storing the address.. */
            s->current_extented_address = val;
            break;
        case 0x01:
            /* Write Data no post increment */
            dp83867_phy_write_extented(s, val);
            break;
        default:
            /* Write Data and post increment */
            dp83867_phy_write_extented(s, val);

            /* XXX: Maximum size ? */
            s->current_extented_address++;
            break;
        }
        break;
    default:
        break;
    }
    s->regs[reg_num] = val;
}

static int dp83867_phy_loopback(QEMUPhy *phy)
{
    Dp83867Phy *s = DP83867_PHY(phy);

    return extract32(s->regs[BMCR_OFFSET], BMCR_LOOPBACK_SHIFT, 1);
}

static void dp83867_phy_class_init(ObjectClass *oc, void *data)
{
    QEMUPhyClass *klass = QEMU_PHY_CLASS(oc);

    klass->phy_update_link = dp83867_phy_update_link;
    klass->phy_reset = dp83867_phy_reset;
    klass->phy_read = dp83867_phy_read;
    klass->phy_write = dp83867_phy_write;
    klass->phy_loopback = dp83867_phy_loopback;
}

static const TypeInfo dp83867_phy_info = {
    .name = TYPE_DP83867_PHY,
    .parent = TYPE_QEMU_PHY,
    .instance_size = sizeof(Dp83867Phy),
    .class_init = dp83867_phy_class_init,
};

static void dp83867_phy_register_types(void)
{
    type_register_static(&dp83867_phy_info);
}

type_init(dp83867_phy_register_types)
