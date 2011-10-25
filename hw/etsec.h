#ifndef _ETSEC_H_
#define _ETSEC_H_

#include "qdev.h"
#include "sysbus.h"
#include "net.h"
#include "qemu-timer.h"

/* Buffer Descriptors */

typedef struct eTSEC_rxtx_bd {
    uint16_t flags;
    uint16_t length;
    uint32_t bufptr;
} eTSEC_rxtx_bd;

#define BD_WRAP       (1 << 13)
#define BD_INTERRUPT  (1 << 12)
#define BD_LAST       (1 << 11)

#define BD_TX_READY     (1 << 15)
#define BD_TX_PADCRC    (1 << 14)
#define BD_TX_TC        (1 << 10)
#define BD_TX_PREDEF    (1 << 9)
#define BD_TX_HFELC     (1 << 7)
#define BD_TX_CFRL      (1 << 6)
#define BD_TX_RC_MASK   0xF
#define BD_TX_RC_OFFSET 0x2
#define BD_TX_TOEUN     (1 << 1)
#define BD_TX_TR        (1 << 0)

#define BD_RX_EMPTY     (1 << 15)
#define BD_RX_RO1       (1 << 14)
#define BD_RX_FIRST     (1 << 10)
#define BD_RX_MISS      (1 << 8)
#define BD_RX_BROADCAST (1 << 7)
#define BD_RX_MULTICAST (1 << 6)
#define BD_RX_LG        (1 << 5)
#define BD_RX_NO        (1 << 4)
#define BD_RX_SH        (1 << 3)
#define BD_RX_CR        (1 << 2)
#define BD_RX_OV        (1 << 1)
#define BD_RX_TR        (1 << 0)

/* Tx FCB flags */
#define FCB_TX_VLN     (1 << 7)
#define FCB_TX_IP      (1 << 6)
#define FCB_TX_IP6     (1 << 5)
#define FCB_TX_TUP     (1 << 4)
#define FCB_TX_UDP     (1 << 3)
#define FCB_TX_CIP     (1 << 2)
#define FCB_TX_CTU     (1 << 1)
#define FCB_TX_NPH     (1 << 0)

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

/* eTSEC */

#define REG_NUMBER 1024

typedef struct eTSEC_Register {
    const char *name;
    const char *desc;
    uint32_t    access;
    uint32_t    value;
} eTSEC_Register;

typedef struct eTSEC {
    SysBusDevice  busdev;

    MemoryRegion  io_area;

    eTSEC_Register regs[REG_NUMBER];

    NICState *nic;
    NICConf   conf;

    /* Tx */

    uint8_t       *tx_buffer;
    uint32_t       tx_buffer_len;
    eTSEC_rxtx_bd  first_bd;

    /* Rx */

    uint8_t       *rx_buffer;
    uint32_t       rx_buffer_len;
    uint32_t       rx_remaining_data;
    uint8_t        rx_first_in_frame;
    uint8_t        rx_fcb_size;
    eTSEC_rxtx_bd  rx_first_bd;
    uint8_t        rx_fcb[10];
    uint32_t       rx_padding;

    /* IRQs */
    qemu_irq     tx_irq;
    qemu_irq     rx_irq;
    qemu_irq     err_irq;


    uint16_t phy_status;
    uint16_t phy_control;

    /* Polling */
    QEMUBH *bh;
    struct ptimer_state *ptimer;

} eTSEC;

#define eTSEC_TRANSMIT 1
#define eTSEC_RECEIVE  2

DeviceState *etsec_create(target_phys_addr_t  base,
                          MemoryRegion       *mr,
                          NICInfo            *nd,
                          qemu_irq            tx_irq,
                          qemu_irq            rx_irq,
                          qemu_irq            err_irq);

void walk_tx_ring(eTSEC *etsec, int ring_nbr);
void walk_rx_ring(eTSEC *etsec, int ring_nbr);
void rx_ring_write (eTSEC *etsec, const uint8_t *buf, size_t size);

void write_miim(eTSEC          *etsec,
                eTSEC_Register *reg,
                uint32_t        reg_index,
                uint32_t        value);

void miim_link_status (eTSEC *etsec, VLANClientState *nc);

#endif /* ! _ETSEC_H_ */
