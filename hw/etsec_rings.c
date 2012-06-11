#include "bswap.h"
/* For crc32 */
#include <zlib.h>
#include "net/checksum.h"

#include "etsec.h"
#include "etsec_registers.h"

//#define ETSEC_RING_DEBUG
//#define HEX_DUMP
//#define DEBUG_BD

#ifdef ETSEC_RING_DEBUG
#define RING_DEBUG(fmt, ...) printf ("%s:%s " fmt, __func__ , etsec->nic->nc.name, ## __VA_ARGS__)
#else
#define RING_DEBUG(...)
#endif  /* ETSEC_RING_DEBUG */

#define RING_DEBUG_A(fmt, ...) printf ("%s:%s " fmt, __func__ , etsec->nic->nc.name, ## __VA_ARGS__)

#ifdef DEBUG_BD

static void print_tx_bd_flags(uint16_t flags)
{
    printf("      Ready: %d\n", !!(flags & BD_TX_READY));
    printf("      PAD/CRC: %d\n", !!(flags & BD_TX_PADCRC));
    printf("      Wrap: %d\n", !!(flags & BD_WRAP));
    printf("      Interrupt: %d\n", !!(flags & BD_INTERRUPT));
    printf("      Last in frame: %d\n", !!(flags & BD_LAST));
    printf("      Tx CRC: %d\n", !!(flags & BD_TX_TC));
    printf("      User-defined preamble / defer: %d\n", !!(flags & BD_TX_PREDEF));
    printf("      Huge frame enable / Late collision: %d\n", !!(flags & BD_TX_HFELC));
    printf("      Control frame / Retransmission Limit: %d\n", !!(flags & BD_TX_CFRL));
    printf("      Retry count: %d\n", (flags >> BD_TX_RC_OFFSET) & BD_TX_RC_MASK);
    printf("      Underrun / TCP/IP off-load enable: %d\n", !!(flags & BD_TX_TOEUN));
    printf("      Truncation: %d\n", !!(flags & BD_TX_TR));
}

static void print_rx_bd_flags(uint16_t flags)
{
    printf("      Empty: %d\n", !!(flags & BD_RX_EMPTY));
    printf("      Receive software ownership: %d\n", !!(flags & BD_RX_RO1));
    printf("      Wrap: %d\n", !!(flags & BD_WRAP));
    printf("      Interrupt: %d\n", !!(flags & BD_INTERRUPT));
    printf("      Last in frame: %d\n", !!(flags & BD_LAST));
    printf("      First in frame: %d\n", !!(flags & BD_RX_FIRST));
    printf("      Miss: %d\n", !!(flags & BD_RX_MISS));
    printf("      Broadcast: %d\n", !!(flags & BD_RX_BROADCAST));
    printf("      Multicast: %d\n", !!(flags & BD_RX_MULTICAST));
    printf("      Rx frame length violation: %d\n", !!(flags & BD_RX_LG));
    printf("      Rx non-octet aligned frame: %d\n", !!(flags & BD_RX_NO));
    printf("      Short frame: %d\n", !!(flags & BD_RX_SH));
    printf("      Rx CRC Error: %d\n", !!(flags & BD_RX_CR));
    printf("      Overrun: %d\n", !!(flags & BD_RX_OV));
    printf("      Truncation: %d\n", !!(flags & BD_RX_TR));
}


static void print_bd(eTSEC_rxtx_bd bd, int mode, uint32_t index)
{
    printf("eTSEC %s Data Buffer Descriptor (%u)\n",
           mode == eTSEC_TRANSMIT ? "Transmit" : "Receive",
           index);
    printf("   Flags   : 0x%04x\n", bd.flags);
    if (mode == eTSEC_TRANSMIT) {
        print_tx_bd_flags(bd.flags);
    } else {
        print_rx_bd_flags(bd.flags);
    }
    printf("   Length  : 0x%04x\n", bd.length);
    printf("   Pointer : 0x%08x\n", bd.bufptr);
}

#endif  /* DEBUG_BD */

#ifdef HEX_DUMP

static void hex_dump(FILE *f, const uint8_t *buf, int size)
{
    int len, i, j, c;

    for (i = 0; i < size; i += 16) {
        len = size - i;
        if (len > 16) {
            len = 16;
        }
        fprintf(f, "%08x ", i);
        for (j = 0; j < 16; j++) {
            if (j < len)
                fprintf(f, " %02x", buf[i + j]);
            else
                fprintf(f, "   ");
        }
        fprintf(f, " ");
        for (j = 0; j < len; j++) {
            c = buf[i + j];
            if (c < ' ' || c > '~')
                c = '.';
            fprintf(f, "%c", c);
        }
        fprintf(f, "\n");
    }
}

#endif

static void read_buffer_descriptor(eTSEC              *etsec,
                                   target_phys_addr_t  addr,
                                   eTSEC_rxtx_bd      *bd)
{
    assert(bd != NULL);

    RING_DEBUG("READ Buffer Descriptor @ 0x" TARGET_FMT_plx"\n", addr);
    cpu_physical_memory_read(addr,
                             bd,
                             sizeof(eTSEC_rxtx_bd));

    if (etsec->regs[DMACTRL].value & DMACTRL_LE) {
        bd->flags  = le16_to_cpupu(&bd->flags);
        bd->length = le16_to_cpupu(&bd->length);
        bd->bufptr = le32_to_cpupu(&bd->bufptr);
    } else {
        bd->flags  = be16_to_cpupu(&bd->flags);
        bd->length = be16_to_cpupu(&bd->length);
        bd->bufptr = be32_to_cpupu(&bd->bufptr);
    }
}

static void write_buffer_descriptor(eTSEC              *etsec,
                                    target_phys_addr_t  addr,
                                    eTSEC_rxtx_bd      *bd)
{
    assert(bd != NULL);

    if (etsec->regs[DMACTRL].value & DMACTRL_LE) {
        cpu_to_le16wu(&bd->flags, bd->flags);
        cpu_to_le16wu(&bd->length, bd->length);
        cpu_to_le32wu(&bd->bufptr, bd->bufptr);
    } else {
        cpu_to_be16wu(&bd->flags, bd->flags);
        cpu_to_be16wu(&bd->length, bd->length);
        cpu_to_be32wu(&bd->bufptr, bd->bufptr);
    }

    RING_DEBUG("Write Buffer Descriptor @ 0x" TARGET_FMT_plx"\n", addr);
    cpu_physical_memory_write(addr,
                              bd,
                              sizeof(eTSEC_rxtx_bd));

}

static void ievent_set(eTSEC    *etsec,
                       uint32_t  flags)
{
    etsec->regs[IEVENT].value |= flags;

    if ((flags & IEVENT_TXB && etsec->regs[IMASK].value & IMASK_TXBEN)
        || (flags & IEVENT_TXF && etsec->regs[IMASK].value & IMASK_TXFEN)) {
        qemu_irq_raise(etsec->tx_irq);
        RING_DEBUG("%s Raise Tx IRQ\n", __func__);
    }

    if ((flags & IEVENT_RXB && etsec->regs[IMASK].value & IMASK_RXBEN)
        || (flags & IEVENT_RXF && etsec->regs[IMASK].value & IMASK_RXFEN)) {
        qemu_irq_pulse(etsec->rx_irq);
        RING_DEBUG("%s Raise Rx IRQ\n", __func__);
    }
}

static void tx_append_crc(eTSEC *etsec)
{
    /* Never add CRC in Qemu */

    /* uint32_t crc = (uint32_t)crc32(~0, etsec->tx_buffer, etsec->tx_buffer_len); */

    /* /\* Append 32bits CRC to tx buffer *\/ */

    /* etsec->tx_buffer_len += 4; */
    /* etsec->tx_buffer = g_realloc(etsec->tx_buffer, etsec->tx_buffer_len); */
    /* *((uint32_t*)(etsec->tx_buffer + etsec->tx_buffer_len - 4)) = crc; */
}

static void tx_padding_and_crc(eTSEC *etsec, uint32_t min_frame_len)
{
    int add = min_frame_len - etsec->tx_buffer_len;

    /* Padding */
    if (add > 0) {
        RING_DEBUG("pad:%u\n", add);
        etsec->tx_buffer = g_realloc(etsec->tx_buffer,
                                        etsec->tx_buffer_len + add);

        memset(etsec->tx_buffer + etsec->tx_buffer_len, 0x0, add);
        etsec->tx_buffer_len += add;
    }

    /* CRC */
    tx_append_crc(etsec);
}

static void process_tx_fcb(eTSEC        *etsec)
{
    uint8_t flags = (uint8_t)(*etsec->tx_buffer);
    /* L3 header offset from start of frame */
    uint8_t l3_header_offset = (uint8_t)*(etsec->tx_buffer + 3);
    /* L4 header offset from start of L3 header */
    uint8_t l4_header_offset = (uint8_t)*(etsec->tx_buffer + 2);
    /* L3 header */
    uint8_t *l3_header = etsec->tx_buffer + 8 + l3_header_offset;
    /* L4 header */
    uint8_t *l4_header = l3_header + l4_header_offset;

    /* uint16_t crc16; */

    /* if packet is IP4 and IP checksum is requested */
    if (flags & FCB_TX_IP && flags & FCB_TX_CIP) {
        /* do IP4 checksum (TODO This funtion does TCP/UDP checksum but not sure
         * if it also does IP4 checksum. */
        net_checksum_calculate(etsec->tx_buffer + 8,
                etsec->tx_buffer_len - 8);
    }
    /* TODO Check the correct usage of the PHCS field of the FCB in case the NPH
     * flag is on */

    /* if packet is IP4 and TCP or UDP */
    if (flags & FCB_TX_IP && flags & FCB_TX_TUP) {
        /* if UDP */
        if (flags & FCB_TX_UDP) {
            /* if checksum is requested */
            if (flags & FCB_TX_CTU) {
                /* do UDP checksum */
                //int add = pow2roundup(etsec->tx_buffer_len) -
                //    etsec->tx_buffer_len;
                //if (add > 0) {
                //    etsec->tx_buffer = g_realloc(etsec->tx_buffer,
                //            etsec->tx_buffer_len + add);
                //    memset(etsec->tx_buffer + etsec->tx_buffer_len, 0x0, add);
                //    etsec->tx_buffer_len += add;
                //}

                //l3_packet_len = etsec->tx_buffer_len - l3_header_offset;
                //crc16 = (uint16_t)crc32(~0, l3_header, l3_packet_len);
                //*((uint16_t*)(l4_header + 6)) = htons(crc16);

                net_checksum_calculate(etsec->tx_buffer + 8,
                        etsec->tx_buffer_len - 8);
            } else {
                /* set checksum field to 0 */
                l4_header[6] = 0;
                l4_header[7] = 0;
            }
        } else if (flags & FCB_TX_CTU) { /* if TCP and checksum is requested */
            /* do TCP checksum */
            net_checksum_calculate(etsec->tx_buffer + 8, etsec->tx_buffer_len -
                    8);
            //l4_header[31] = 0;
            //l4_header[32] = 0;
        }
    }
}

static void process_tx_bd(eTSEC         *etsec,
                          eTSEC_rxtx_bd *bd)
{
    uint8_t *tmp_buff = NULL;

    if (bd->length == 0) {
        /* ERROR */
        return;
    }

    if (etsec->tx_buffer_len == 0) {
        /* It's the first BD */
        etsec->first_bd = *bd;
    }

    /* TODO: if TxBD[TOE/UN] skip the Tx Frame Control Block*/

    /* Load this Data Buffer */
    etsec->tx_buffer = g_realloc(etsec->tx_buffer,
                                    etsec->tx_buffer_len + bd->length);
    tmp_buff = etsec->tx_buffer + etsec->tx_buffer_len;
    cpu_physical_memory_read(bd->bufptr, tmp_buff, bd->length);

    /* Update buffer length */
    etsec->tx_buffer_len += bd->length;


    if (etsec->tx_buffer_len != 0 && (bd->flags & BD_LAST)) {
        if (etsec->regs[MACCFG1].value & MACCFG1_TX_EN) {
            /* MAC Transmit enabled */

            /* Process offload Tx FCB */
            if (etsec->first_bd.flags & BD_TX_TOEUN)
                process_tx_fcb(etsec);

            if (etsec->first_bd.flags & BD_TX_PADCRC
                || etsec->regs[MACCFG2].value & MACCFG2_PADCRC) {

                /* Padding and CRC (Padding implies CRC) */
                tx_padding_and_crc(etsec, 64);

            } else if (etsec->first_bd.flags & BD_TX_TC
                       || etsec->regs[MACCFG2].value & MACCFG2_CRC_EN) {

                /* Only CRC */
                tx_append_crc(etsec);
            }

#if defined(HEX_DUMP)
            fprintf(stderr,"eTSEC Send packet size:%d\n", etsec->tx_buffer_len);
            hex_dump(stderr, etsec->tx_buffer, etsec->tx_buffer_len);
#endif  /* ETSEC_RING_DEBUG */

            if (etsec->first_bd.flags & BD_TX_TOEUN) {
                qemu_send_packet(&etsec->nic->nc,
                        etsec->tx_buffer + 8,
                        etsec->tx_buffer_len - 8);
            } else {
                qemu_send_packet(&etsec->nic->nc,
                        etsec->tx_buffer,
                        etsec->tx_buffer_len);
            }

        }

        etsec->tx_buffer_len = 0;

        if (bd->flags & BD_INTERRUPT) {
            ievent_set(etsec, IEVENT_TXF);
        }
    } else {
        if (bd->flags & BD_INTERRUPT) {
            ievent_set(etsec, IEVENT_TXB);
        }
    }

    /* Update DB flags */

    /* Clear Ready */
    bd->flags &= ~BD_TX_READY;

    /* Clear Defer */
    bd->flags &= ~BD_TX_PREDEF;

    /* Clear Late Collision */
    bd->flags &= ~BD_TX_HFELC;

    /* Clear Retransmission Limit */
    bd->flags &= ~BD_TX_CFRL;

    /* Clear Retry Count */
    bd->flags &= ~(BD_TX_RC_MASK << BD_TX_RC_OFFSET);

    /* Clear Underrun */
    bd->flags &= ~BD_TX_TOEUN;

    /* Clear Truncation */
    bd->flags &= ~BD_TX_TR;
}

void walk_tx_ring(eTSEC *etsec, int ring_nbr)
{
    target_phys_addr_t ring_base = 0;
    target_phys_addr_t bd_addr   = 0;
    eTSEC_rxtx_bd      bd;
    uint16_t           bd_flags;

    if ( ! (etsec->regs[MACCFG1].value & MACCFG1_TX_EN)) {
        RING_DEBUG("%s: MAC Transmit not enabled\n", __func__);
        return;
    }

    /* ring_base = (etsec->regs[TBASEH].value & 0xF) << 32; */
    ring_base += etsec->regs[TBASE0 + ring_nbr].value & ~0x7;
    bd_addr    = etsec->regs[TBPTR0 + ring_nbr].value & ~0x7;

    do {
        read_buffer_descriptor(etsec, bd_addr, &bd);

#ifdef DEBUG_BD
        print_bd(bd,
                 eTSEC_TRANSMIT,
                 (bd_addr - ring_base) / sizeof(eTSEC_rxtx_bd));

#endif  /* DEBUG_BD */

        /* Save flags before BD update */
        bd_flags = bd.flags;

        if (bd_flags & BD_TX_READY) {
            process_tx_bd(etsec, &bd);

            /* Write back BD after update */
            write_buffer_descriptor(etsec, bd_addr, &bd);
        }

        /* Wrap or next BD */
        if (bd_flags & BD_WRAP) {
            bd_addr = ring_base;
        } else {
            bd_addr += sizeof(eTSEC_rxtx_bd);
        }

    } while (bd_addr != ring_base); //bd_flags & BD_TX_READY);

    bd_addr = ring_base;

    /* Save the Buffer Descriptor Pointers to current bd */
    etsec->regs[TBPTR0 + ring_nbr].value = bd_addr;

    /* Set transmit halt THLTx */
    etsec->regs[TSTAT].value |= 1 << (31 - ring_nbr);
}

static void fill_rx_bd(eTSEC          *etsec,
                       eTSEC_rxtx_bd  *bd,
                       const uint8_t **buf,
                       size_t         *size)
{
    uint16_t to_write = MIN(etsec->rx_fcb_size + *size - etsec->rx_padding,
                            etsec->regs[MRBLR].value);
    uint32_t bufptr   = bd->bufptr;
    uint8_t  padd[etsec->rx_padding];
    uint8_t  rem;

    RING_DEBUG("eTSEC fill Rx buffer @ 0x%08x size:%u(padding + crc:%u) + fcb:%u\n",
               bufptr, *size, etsec->rx_padding, etsec->rx_fcb_size);

    bd->length = 0;
    if (etsec->rx_fcb_size != 0) {
        cpu_physical_memory_write(bufptr, etsec->rx_fcb, etsec->rx_fcb_size);

        bufptr             += etsec->rx_fcb_size;
        bd->length         += etsec->rx_fcb_size;
        to_write           -= etsec->rx_fcb_size;
        etsec->rx_fcb_size  = 0;

    }

    if (to_write > 0) {
        cpu_physical_memory_write(bufptr, *buf, to_write);

        *buf   += to_write;
        bufptr += to_write;
        *size  -= to_write;

        bd->flags  &= ~BD_RX_EMPTY;
        bd->length += to_write;
    }

    if (*size == etsec->rx_padding) {
        /* The remaining bytes are for padding which is not actually allocated
           in the buffer */

        rem = MIN(etsec->regs[MRBLR].value - bd->length, etsec->rx_padding);

        if (rem > 0){
            memset(padd, 0x0, sizeof (padd));
            etsec->rx_padding -= rem;
            *size             -= rem;
            bd->length        += rem;
            cpu_physical_memory_write(bufptr, padd, rem);
        }
    }
}

static void rx_init_frame(eTSEC *etsec, const uint8_t *buf, size_t size)
{
    uint32_t fcb_size = 0;
    uint8_t  prsdep   = (etsec->regs[RCTRL].value >> RCTRL_PRSDEP_OFFSET)
        & RCTRL_PRSDEP_MASK;

    if (prsdep != 0) {
        /* Prepend FCB */
        fcb_size = 8 + 2;          /* FCB size + align */
        /* I can't find this 2 bytes alignement in fsl documentation but VxWorks
           expects them */

        etsec->rx_fcb_size = fcb_size;

        /* TODO: fill_FCB(etsec); */
        memset(etsec->rx_fcb, 0x0, sizeof(etsec->rx_fcb));

    } else {
        etsec->rx_fcb_size = 0;
    }

    if (etsec->rx_buffer != NULL) {
        g_free(etsec->rx_buffer);
    }

    /* Do not copy the frame for now */
    etsec->rx_buffer     = (uint8_t*)buf;
    etsec->rx_buffer_len = size;
    etsec->rx_padding    = 4;

    if (size < 60) {
        etsec->rx_padding += 60 - size;
    }

    etsec->rx_first_in_frame = 1;
    etsec->rx_remaining_data = etsec->rx_buffer_len;
    RING_DEBUG("%s: rx_buffer_len:%u rx_padding+crc:%u\n", __func__,
               etsec->rx_buffer_len, etsec->rx_padding);
}

void rx_ring_write(eTSEC *etsec, const uint8_t *buf, size_t size)
{
    int                ring_nbr       = 0; /* Always use ring0 (no filer) */

    if (etsec->rx_buffer_len != 0) {
        RING_DEBUG("%s: We can't receive now,"
                   " a buffer is already in the pipe\n", __func__);
        return;
    }

    if (etsec->regs[RSTAT].value & 1 << (23 - ring_nbr)) {
        RING_DEBUG("%s: The ring is halted\n", __func__);
        return;
    }

    if (etsec->regs[DMACTRL].value & DMACTRL_GRS) {
        RING_DEBUG("%s: Graceful receive stop\n", __func__);
        return;
    }

    if ( ! (etsec->regs[MACCFG1].value & MACCFG1_RX_EN)) {
        RING_DEBUG("%s: MAC Receive not enabled\n", __func__);
        return;
    }

    /* Don't drop short packets, just add padding (later) */

    rx_init_frame(etsec, buf, size);

    walk_rx_ring(etsec, ring_nbr);
}

void walk_rx_ring(eTSEC *etsec, int ring_nbr)
{
    target_phys_addr_t  ring_base     = 0;
    target_phys_addr_t  bd_addr       = 0;
    target_phys_addr_t  start_bd_addr = 0;
    eTSEC_rxtx_bd       bd;
    uint16_t            bd_flags;
    size_t              remaining_data;
    const uint8_t      *buf;
    uint8_t            *tmp_buf;
    size_t              size;


    if (etsec->rx_buffer_len == 0) {
        /* No frame to send */
        RING_DEBUG("No frame to send\n");
        return;
    }

    remaining_data = etsec->rx_remaining_data + etsec->rx_padding;
    buf            = etsec->rx_buffer
        + (etsec->rx_buffer_len - etsec->rx_remaining_data);
    size           = etsec->rx_buffer_len + etsec->rx_padding;

    /* ring_base = (etsec->regs[RBASEH].value & 0xF) << 32; */
    ring_base     += etsec->regs[RBASE0 + ring_nbr].value & ~0x7;
    start_bd_addr  = bd_addr = etsec->regs[RBPTR0 + ring_nbr].value & ~0x7;

    do {
        read_buffer_descriptor(etsec, bd_addr, &bd);

#ifdef DEBUG_BD
        print_bd(bd,
                 eTSEC_RECEIVE,
                 (bd_addr - ring_base) / sizeof(eTSEC_rxtx_bd));

#endif  /* DEBUG_BD */

        /* Save flags before BD update */
        bd_flags = bd.flags;

        if (bd_flags & BD_RX_EMPTY) {
            fill_rx_bd(etsec, &bd, &buf, &remaining_data);

            if (etsec->rx_first_in_frame) {
                bd.flags |= BD_RX_FIRST;
                etsec->rx_first_in_frame = 0;
                etsec->rx_first_bd = bd;
            }

            /* Last in frame */
            if (remaining_data == 0) {

                /* Clear flags */

                bd.flags &= ~0x7ff;

                bd.flags |= BD_LAST;

                /* NOTE: non-octet aligned frame is impossible in qemu */

                if (size >= etsec->regs[MAXFRM].value) {
                    /* frame length violation */
                    printf("%s frame length violation: size:%d MAXFRM:%d\n",
                           __func__, size, etsec->regs[MAXFRM].value);

                    bd.flags |= BD_RX_LG;
                }

                if (size  < 64) {
                    /* Short frame */
                    printf("%s Short frame: %d\n", __func__, size);
                    bd.flags |= BD_RX_SH;
                }

                /* TODO: Broadcast and Multicast */

                if (bd.flags | BD_INTERRUPT) {
                    /* Set RXFx */
                    etsec->regs[RSTAT].value |= 1 << (7 - ring_nbr);

                    /* Set IEVENT */
                    ievent_set(etsec, IEVENT_RXF);
                }

            } else {
                if (bd.flags | BD_INTERRUPT) {
                    /* Set IEVENT */
                    ievent_set(etsec, IEVENT_RXB);
                }
            }

            /* Write back BD after update */
            write_buffer_descriptor(etsec, bd_addr, &bd);
        }

        /* Wrap or next BD */
        if (bd_flags & BD_WRAP) {
            bd_addr = ring_base;
        } else {
            bd_addr += sizeof(eTSEC_rxtx_bd);
        }
    } while (   remaining_data != 0
             && (bd_flags & BD_RX_EMPTY)
             && bd_addr != start_bd_addr);

    /* Reset ring ptr */
    etsec->regs[RBPTR0 + ring_nbr].value = bd_addr;

    /* The frame is too large to fit in the Rx ring */
    if (remaining_data > 0) {

        /* Set RSTAT[QHLTx] */
        etsec->regs[RSTAT].value |= 1 << (23 - ring_nbr);

        /* Save remaining data to send the end of the frame when the ring will
         * be restarted
         */
        etsec->rx_remaining_data = remaining_data;

        /* Copy the frame */
        tmp_buf = g_malloc(size);
        memcpy(tmp_buf, etsec->rx_buffer, size);
        etsec->rx_buffer = tmp_buf;

        RING_DEBUG("no empty RxBD available any more\n");
    } else {
        etsec->rx_buffer_len = 0;
        etsec->rx_buffer     = NULL;
    }

    RING_DEBUG("eTSEC End of ring_write: remaining_data:%u\n", remaining_data);
}
