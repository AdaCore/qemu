/*
 * mEMAC (T2080)
 *
 * Copyright (c) 2019-2021 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/net/phy/dp83867.h"

#define TYPE_MEMAC "memac"
#define MEMAC(obj) OBJECT_CHECK(MEMACState, (obj), TYPE_MEMAC)
#define MEMAC_R_MAX (0x800 >> 2)
#define MEMAC_MDIO_R_MAX (0x40 >> 2)

#define MEMAC_COMMAND_CONFIG    (0x008 >> 2)
#define MEMAC_COMMAND_CONFIG_SWR_SFT (12)
#define MEMAC_MAC_ADDR_0        (0x00C >> 2)
#define MEMAC_MAC_ADDR_1        (0x010 >> 2)
#define MEMAC_MAX_FRM           (0x014 >> 2)
#define MEMAC_RX_FIFO_SECTIONS  (0x01C >> 2)
#define MEMAC_TX_FIFO_SECTIONS  (0x020 >> 2)
#define MEMAC_HASHTABLE_CTRL    (0x02C >> 2)
#define MEMAC_IEVENT            (0x040 >> 2)
#define MEMAC_TX_LENGTH         (0x044 >> 2)
#define MEMAC_IMASK             (0x04C >> 2)
#define MEMAC_CL01_PAUSE_QUANTA (0x054 >> 2)
#define MEMAC_CL23_PAUSE_QUANTA (0x058 >> 2)
#define MEMAC_CL45_PAUSE_QUANTA (0x05C >> 2)
#define MEMAC_CL67_PAUSE_QUANTA (0x060 >> 2)
#define MEMAC_CL01_PAUSE_THRESH (0x064 >> 2)
#define MEMAC_CL23_PAUSE_THRESH (0x068 >> 2)
#define MEMAC_CL45_PAUSE_THRESH (0x06C >> 2)
#define MEMAC_CL67_PAUSE_THRESH (0x070 >> 2)
#define MEMAC_RX_PAUSE_STATUS   (0x074 >> 2)
#define MEMAC_MAC_ADDR_2        (0x80 >> 2)
#define MEMAC_MAC_ADDR_3        (0x84 >> 2)
#define MEMAC_MAC_ADDR_4        (0x88 >> 2)
#define MEMAC_MAC_ADDR_5        (0x8C >> 2)
#define MEMAC_MAC_ADDR_6        (0x90 >> 2)
#define MEMAC_MAC_ADDR_7        (0x94 >> 2)
#define MEMAC_MAC_ADDR_8        (0x98 >> 2)
#define MEMAC_MAC_ADDR_9        (0x9C >> 2)
#define MEMAC_MAC_ADDR_10       (0xA0 >> 2)
#define MEMAC_MAC_ADDR_11       (0xA4 >> 2)
#define MEMAC_MAC_ADDR_12       (0xA8 >> 2)
#define MEMAC_MAC_ADDR_13       (0xAC >> 2)
#define MEMAC_MAC_ADDR_14       (0xB0 >> 2)
#define MEMAC_MAC_ADDR_15       (0xB4 >> 2)
#define MEMAC_LPWAKE_TIMER      (0xB8 >> 2)
#define MEMAC_SLEEP_TIMER       (0xBC >> 2)
#define MEMAC_STATN_CONFIG      (0x0E0 >> 2)
#define MEMAC_REOCT_L           (0x100 >> 2)
#define MEMAC_REOCT_U           (0x104 >> 2)
#define MEMAC_ROCT_L            (0x108 >> 2)
#define MEMAC_ROCT_U            (0x10C >> 2)
#define MEMAC_RALN_L            (0x110 >> 2)
#define MEMAC_RALN_U            (0x114 >> 2)
#define MEMAC_RXPF_L            (0x118 >> 2)
#define MEMAC_RXPF_U            (0x11C >> 2)
#define MEMAC_RFRM_L            (0x120 >> 2)
#define MEMAC_RFRM_U            (0x124 >> 2)
#define MEMAC_RFCS_L            (0x128 >> 2)
#define MEMAC_RFCS_U            (0x12C >> 2)
#define MEMAC_RVLAN_L           (0x130 >> 2)
#define MEMAC_RVLAN_U           (0x134 >> 2)
#define MEMAC_RERR_L            (0x138 >> 2)
#define MEMAC_RERR_U            (0x13C >> 2)
#define MEMAC_RUCA_L            (0x140 >> 2)
#define MEMAC_RUCA_U            (0x144 >> 2)
#define MEMAC_RMCA_L            (0x148 >> 2)
#define MEMAC_RMCA_U            (0x14C >> 2)
#define MEMAC_RBCA_L            (0x150 >> 2)
#define MEMAC_RBCA_U            (0x154 >> 2)
#define MEMAC_RDRP_L            (0x158 >> 2)
#define MEMAC_RDRP_U            (0x15C >> 2)
#define MEMAC_RPKT_L            (0x160 >> 2)
#define MEMAC_RPKT_U            (0x164 >> 2)
#define MEMAC_RUND_L            (0x168 >> 2)
#define MEMAC_RUND_U            (0x16C >> 2)
#define MEMAC_R64_L             (0x170 >> 2)
#define MEMAC_R64_U             (0x174 >> 2)
#define MEMAC_R127_L            (0x178 >> 2)
#define MEMAC_R127_U            (0x17C >> 2)
#define MEMAC_R255_L            (0x180 >> 2)
#define MEMAC_R255_U            (0x184 >> 2)
#define MEMAC_R511_L            (0x188 >> 2)
#define MEMAC_R511_U            (0x18C >> 2)
#define MEMAC_R1023_L           (0x190 >> 2)
#define MEMAC_R1023_U           (0x194 >> 2)
#define MEMAC_R1518_L           (0x198 >> 2)
#define MEMAC_R1518_U           (0x19C >> 2)
#define MEMAC_R1519X_L          (0x1A0 >> 2)
#define MEMAC_R1519X_U          (0x1A4 >> 2)
#define MEMAC_ROVR_L            (0x1A8 >> 2)
#define MEMAC_ROVR_U            (0x1AC >> 2)
#define MEMAC_RJBR_L            (0x1B0 >> 2)
#define MEMAC_RJBR_U            (0x1B4 >> 2)
#define MEMAC_RFRG_L            (0x1B8 >> 2)
#define MEMAC_RFRG_U            (0x1BC >> 2)
#define MEMAC_RCNP_L            (0x1C0 >> 2)
#define MEMAC_RCNP_U            (0x1C4 >> 2)
#define MEMAC_RDRNTP_L          (0x1C8 >> 2)
#define MEMAC_RDRNTP_U          (0x1CC >> 2)
#define MEMAC_TEOCT_L           (0x200 >> 2)
#define MEMAC_TEOCT_U           (0x204 >> 2)
#define MEMAC_TOCT_L            (0x208 >> 2)
#define MEMAC_TOCT_U            (0x20C >> 2)
#define MEMAC_TXPF_L            (0x218 >> 2)
#define MEMAC_TXPF_U            (0x21C >> 2)
#define MEMAC_TFRM_L            (0x220 >> 2)
#define MEMAC_TFRM_U            (0x224 >> 2)
#define MEMAC_TFCS_L            (0x228 >> 2)
#define MEMAC_TFCS_U            (0x22C >> 2)
#define MEMAC_TVLAN_L           (0x230 >> 2)
#define MEMAC_TVLAN_U           (0x234 >> 2)
#define MEMAC_TERR_L            (0x238 >> 2)
#define MEMAC_TERR_U            (0x23C >> 2)
#define MEMAC_TUCA_L            (0x240 >> 2)
#define MEMAC_TUCA_U            (0x244 >> 2)
#define MEMAC_TMCA_L            (0x248 >> 2)
#define MEMAC_TMCA_U            (0x24C >> 2)
#define MEMAC_TBCA_L            (0x250 >> 2)
#define MEMAC_TBCA_U            (0x254 >> 2)
#define MEMAC_TPKT_L            (0x260 >> 2)
#define MEMAC_TPKT_U            (0x264 >> 2)
#define MEMAC_TUND_L            (0x268 >> 2)
#define MEMAC_TUND_U            (0x26C >> 2)
#define MEMAC_T64_L             (0x270 >> 2)
#define MEMAC_T64_U             (0x274 >> 2)
#define MEMAC_T127_L            (0x278 >> 2)
#define MEMAC_T127_U            (0x27C >> 2)
#define MEMAC_T255_L            (0x280 >> 2)
#define MEMAC_T255_U            (0x284 >> 2)
#define MEMAC_T511_L            (0x288 >> 2)
#define MEMAC_T511_U            (0x28C >> 2)
#define MEMAC_T1023_L           (0x290 >> 2)
#define MEMAC_T1023_U           (0x294 >> 2)
#define MEMAC_T1518_L           (0x298 >> 2)
#define MEMAC_T1518_U           (0x29C >> 2)
#define MEMAC_T1519X_L          (0x2A0 >> 2)
#define MEMAC_T1519X_U          (0x2A4 >> 2)
#define MEMAC_TCNP_L            (0x2C0 >> 2)
#define MEMAC_TCNP_U            (0x2C4 >> 2)
#define MEMAC_IF_MODE           (0x300 >> 2)
#define MEMAC_IF_STATUS         (0x304 >> 2)
#define MEMAC_HG_CONFIG         (0x340 >> 2)
#define MEMAC_HG_PAUSE_QUANTA   (0x350 >> 2)
#define MEMAC_HG_PAUSE_THRESH   (0x360 >> 2)
#define MEMAC_HGRX_PAUSE_STATUS (0x370 >> 2)
#define MEMAC_HG_FIFOS_STATUS   (0x374 >> 2)
#define MEMAC_RHM               (0x378 >> 2)
#define MEMAC_THM               (0x37C >> 2)

/* MDIO Registers */
#define MEMAC_MDIO_CFG          (0x030 >> 2)
#define MEMAC_MDIO_CFG_ENC45 (6)
#define MEMAC_MDIO_CTRL         (0x034 >> 2)
#define MEMAC_MDIO_CTRL_READ (15)
#define MEMAC_MDIO_DATA         (0x038 >> 2)
#define MEMAC_MDIO_ADDR         (0x03C >> 2)

typedef struct {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion mdiomem;
    uint32_t reg[MEMAC_R_MAX];
    uint32_t mdio[MEMAC_MDIO_R_MAX];
    qemu_irq irq;

    QEMUPhy *phy;
} MEMACState;

#define DB_PRINT(...)

static void memac_reset(DeviceState *dev)
{
    MEMACState *s = MEMAC(dev);

    qemu_phy_reset(s->phy);

    memset(s->reg, 0, sizeof(s->reg));
    memset(s->mdio, 0, sizeof(s->mdio));

    s->reg[MEMAC_COMMAND_CONFIG] = 0x840;
    s->reg[MEMAC_MAX_FRM] = 0x600;
    s->mdio[MEMAC_MDIO_CFG] = 0x1448;
    qemu_phy_update_link(s->phy, 0);
}

static void memac_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    MEMACState *s = MEMAC(opaque);

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)offset, (unsigned)value);
    offset >>= 2;
    assert(offset < MEMAC_R_MAX);
    assert(size == 4);

    switch (offset) {
    case MEMAC_COMMAND_CONFIG:
        if (extract32(value, MEMAC_COMMAND_CONFIG_SWR_SFT, 1)) {
            memac_reset(DEVICE(s));
        } else {
            s->reg[offset] = value;
        }
        break;
    case MEMAC_MAX_FRM:
    case MEMAC_MAC_ADDR_0:
    case MEMAC_MAC_ADDR_1:
        s->reg[offset] = value;
        break;
    case MEMAC_HASHTABLE_CTRL:
        /* ignored for now */
        break;
    default:
        s->reg[offset] = value;
        break;
    }
}

static uint64_t memac_read(void *opaque, hwaddr offset, unsigned size)
{
    MEMACState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < MEMAC_R_MAX);
    assert(size == 4);

    switch (offset) {
    case MEMAC_COMMAND_CONFIG:
    case MEMAC_MAX_FRM:
    case MEMAC_MAC_ADDR_0:
    case MEMAC_MAC_ADDR_1:
        val = s->reg[offset];
        break;
    default:
        val = s->reg[offset];
        break;
    }

    DB_PRINT(" offset:%x data:%08x\n", (unsigned)(offset << 2),
             (unsigned)val);
    return val;
}

static const MemoryRegionOps memac_ops = {
    .read = memac_read,
    .write = memac_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void memac_mdio_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    MEMACState *s = opaque;

    offset >>= 2;
    assert(offset < MEMAC_MDIO_R_MAX);
    assert(size == 4);

    switch (offset) {
    case MEMAC_MDIO_CTRL:
        if (extract32(value, MEMAC_MDIO_CTRL_READ, 1)) {
            /* Reading.. */
            uint32_t phy_addr;

            if (extract32(s->reg[MEMAC_MDIO_CFG], MEMAC_MDIO_CFG_ENC45, 1)) {
                /* This is Clause 45 the address is register MDIO_ADDR. */
                /* TODO: increment the address ?*/
                phy_addr = s->reg[MEMAC_MDIO_ADDR];
                abort();
            } else {
                /* This is Clause 22 the address is from the REG_ADDR field */
                phy_addr = extract32(value, 0, 5);
            }

            s->mdio[MEMAC_MDIO_DATA] = qemu_phy_read(s->phy, phy_addr);
        }
        break;
    case MEMAC_MDIO_CFG:
    case MEMAC_MDIO_DATA:
    case MEMAC_MDIO_ADDR:
        s->mdio[offset] = value;
        break;
    }
}

static uint64_t memac_mdio_read(void *opaque, hwaddr offset, unsigned size)
{
    MEMACState *s = opaque;
    uint32_t val = 0;

    offset >>= 2;
    assert(offset < MEMAC_MDIO_R_MAX);
    assert(size == 4);

    switch (offset) {
    default:
        val = s->mdio[offset];
        break;
    }

    return val;
}

static const MemoryRegionOps memac_mdio_ops = {
    .read = memac_mdio_read,
    .write = memac_mdio_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void memac_realize(DeviceState *dev, Error **errp)
{
    memac_reset(dev);
}

static void memac_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    MEMACState *s = MEMAC(obj);

    /* Map the IOs */
    memory_region_init_io(&s->iomem, obj, &memac_ops, obj, "memac",
                          MEMAC_R_MAX << 2);
    memory_region_init_io(&s->mdiomem, obj, &memac_mdio_ops, obj, "mdio",
                          MEMAC_MDIO_R_MAX << 2);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_mmio(sbd, &s->mdiomem);
    sysbus_init_irq(sbd, &s->irq);

    /* Map a phy.. */
    s->phy = QEMU_PHY(object_new(TYPE_DP83867_PHY));
}

static void memac_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = memac_realize;
    dc->reset = memac_reset;
}

static const TypeInfo memac_info = {
    .name          = TYPE_MEMAC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MEMACState),
    .instance_init = memac_init,
    .class_init    = memac_class_init,
};

static void memac_register_types(void)
{
    type_register_static(&memac_info);
}

type_init(memac_register_types)
