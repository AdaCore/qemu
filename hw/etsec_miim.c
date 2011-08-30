/*
 * QEMU Freescale eTSEC Emulator
 *
 * Copyright (c) 2011 AdaCore
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

#include "etsec.h"
#include "etsec_registers.h"

#define MIIM_CONTROL    0
#define MIIM_STATUS     1
#define MIIM_PHY_ID_1   2
#define MIIM_PHY_ID_2   3
#define MIIM_EXT_STATUS 15

/* PHY Status Register */
#define MII_SR_EXTENDED_CAPS     0x0001    /* Extended register capabilities */
#define MII_SR_JABBER_DETECT     0x0002    /* Jabber Detected */
#define MII_SR_LINK_STATUS       0x0004    /* Link Status 1 = link */
#define MII_SR_AUTONEG_CAPS      0x0008    /* Auto Neg Capable */
#define MII_SR_REMOTE_FAULT      0x0010    /* Remote Fault Detect */
#define MII_SR_AUTONEG_COMPLETE  0x0020    /* Auto Neg Complete */
#define MII_SR_PREAMBLE_SUPPRESS 0x0040    /* Preamble may be suppressed */
#define MII_SR_EXTENDED_STATUS   0x0100    /* Ext. status info in Reg 0x0F */
#define MII_SR_100T2_HD_CAPS     0x0200    /* 100T2 Half Duplex Capable */
#define MII_SR_100T2_FD_CAPS     0x0400    /* 100T2 Full Duplex Capable */
#define MII_SR_10T_HD_CAPS       0x0800    /* 10T   Half Duplex Capable */
#define MII_SR_10T_FD_CAPS       0x1000    /* 10T   Full Duplex Capable */
#define MII_SR_100X_HD_CAPS      0x2000    /* 100X  Half Duplex Capable */
#define MII_SR_100X_FD_CAPS      0x4000    /* 100X  Full Duplex Capable */
#define MII_SR_100T4_CAPS        0x8000    /* 100T4 Capable */


static void miim_read_cycle(eTSEC *etsec)
{
    uint8_t  phy;
    uint8_t  addr;
    uint16_t value;

    phy  = (etsec->regs[MIIMADD].value >> 8) & 0x1F;
    addr = etsec->regs[MIIMADD].value & 0x1F;

    switch (addr) {
    case MIIM_CONTROL:
        value = etsec->phy_control;
        break;
    case MIIM_STATUS:
        value = etsec->phy_status;
        break;
    default:
        value = 0x0;
        break;
    };

    etsec->regs[MIIMSTAT].value = value;
}

static void miim_write_cycle(eTSEC *etsec)
{
    uint8_t  phy;
    uint8_t  addr;
    uint16_t value;

    phy   = (etsec->regs[MIIMADD].value >> 8) & 0x1F;
    addr  = etsec->regs[MIIMADD].value & 0x1F;
    value = etsec->regs[MIIMCON].value & 0xffff;

    switch (addr) {
    case MIIM_CONTROL:
        etsec->phy_control = value;
        break;
    default:
        break;
    };
}

void write_miim(eTSEC          *etsec,
               eTSEC_Register *reg,
               uint32_t        reg_index,
               uint32_t        value)
{

    switch (reg_index) {

    case MIIMCOM:
        /* Read and scan cycle */

        if (( ! (reg->value & MIIMCOM_READ)) && (value & MIIMCOM_READ)) {
            /* Read */
            miim_read_cycle(etsec);
        }
        reg->value = value;
        break;

    case MIIMCON:
        reg->value = value & 0xffff;
        miim_write_cycle(etsec);
        break;

    default:
        /* Default handling */
        switch (reg->access) {

        case ACC_RW:
        case ACC_WO:
            reg->value = value;
            break;

        case ACC_w1c:
            reg->value &= ~value;
            break;

        case ACC_RO:
        default:
            /* Read         Only or Unknown register */
            break;
        }
    }

}

void miim_link_status (eTSEC *etsec, VLANClientState *nc)
{
    /* Set link status */
    if (nc->link_down) {
        etsec->phy_status &= ~MII_SR_LINK_STATUS;
    } else {
        etsec->phy_status |= MII_SR_LINK_STATUS;
    }
}
