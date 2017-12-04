
#include "qemu/osdep.h"
#include "hw/misc/xlnx_crl.h"
#include "hw/sysbus.h"

#define IOPLL_CTRL_OFFSET         (0x020 >> 2)
#define RPLL_CTRL_OFFSET          (0x030 >> 2)
#define RPLL_FRAC_CFG_OFFSET      (0x038 >> 2)
#define PLL_STATUS_OFFSET         (0x040 >> 2)
#define IOPLL_TO_FPD_CTRL_OFFSET  (0x044 >> 2)
#define RPLL_TO_FPD_CTRL_OFFSET   (0x048 >> 2)
#define USB3_DUAL_REF_CTRL_OFFSET (0x04C >> 2)
#define GEM0_REF_CTRL_OFFSET      (0x050 >> 2)
#define GEM1_REF_CTRL_OFFSET      (0x054 >> 2)
#define GEM2_REF_CTRL_OFFSET      (0x058 >> 2)
#define GEM3_REF_CTRL_OFFSET      (0x05C >> 2)
#define USB0_BUS_REF_CTRL_OFFSET  (0x060 >> 2)
#define USB1_BUS_REF_CTRL_OFFSET  (0x064 >> 2)
#define QSPI_REF_CTRL_OFFSET      (0x068 >> 2)
#define SDIO0_REF_CTRL_OFFSET     (0x06C >> 2)
#define SDIO1_REF_CTRL_OFFSET     (0x070 >> 2)
#define UART0_REF_CTRL_OFFSET     (0x074 >> 2)
#define UART1_REF_CTRL_OFFSET     (0x078 >> 2)
#define SPI0_REF_CTRL_OFFSET      (0x07C >> 2)
#define SPI1_REF_CTRL_OFFSET      (0x080 >> 2)
#define CAN0_REF_CTRL_OFFSET      (0x084 >> 2)
#define CAN1_REF_CTRL_OFFSET      (0x088 >> 2)
#define CPU_R5_CTRL_OFFSET        (0x090 >> 2)
#define IOU_SWITCH_CTRL_OFFSET    (0x09C >> 2)
#define CSU_PLL_CTRL_OFFSET       (0x0A0 >> 2)
#define PCAP_CTRL_OFFSET          (0x0A4 >> 2)
#define LPD_SWITCH_CTRL_OFFSET    (0x0A8 >> 2)
#define LPD_LSBUS_CTRL_OFFSET     (0x0AC >> 2)
#define DBG_LPD_CTRL_OFFSET       (0x0B0 >> 2)
#define NAND_REF_CTRL_OFFSET      (0x0B4 >> 2)
#define LPD_DMA_REF_CTRL_OFFSET   (0x0B8 >> 2)
#define PL0_REF_CTRL_OFFSET       (0x0C0 >> 2)
#define PL1_REF_CTRL_OFFSET       (0x0C4 >> 2)
#define PL2_REF_CTRL_OFFSET       (0x0C8 >> 2)
#define PL3_REF_CTRL_OFFSET       (0x0CC >> 2)
#define PL0_THR_CTRL_OFFSET       (0x0D0 >> 2)
#define PL0_THR_CNT_OFFSET        (0x0D4 >> 2)
#define PL1_THR_CTRL_OFFSET       (0x0D8 >> 2)
#define PL1_THR_CNT_OFFSET        (0x0DC >> 2)
#define PL2_THR_CTRL_OFFSET       (0x0E0 >> 2)
#define PL2_THR_CNT_OFFSET        (0x0E4 >> 2)
#define PL3_THR_CTRL_OFFSET       (0x0E8 >> 2)
#define PL3_THR_CNT_OFFSET        (0x0FC >> 2)
#define GEM_TSU_REF_CTRL_OFFSET   (0x100 >> 2)
#define DLL_REF_CTRL_OFFSET       (0x104 >> 2)
#define PSSYSMON_REF_CTRL_OFFSET  (0x108 >> 2)
#define I2C0_REF_CTRL_OFFSET      (0x120 >> 2)
#define I2C1_REF_CTRL_OFFSET      (0x124 >> 2)
#define TIMESTAMP_REF_CTRL_OFFSET (0x128 >> 2)
#define SAFETY_CHK_OFFSET         (0x130 >> 2)
#define CLKMON_STATUS_OFFSET      (0x140 >> 2)
#define CLKMON_MASK_OFFSET        (0x144 >> 2)
#define CLKMON_ENABLE_OFFSET      (0x148 >> 2)
#define CLKMON_DISABLE_OFFSET     (0x14C >> 2)
#define CLKMON_TRIGGER_OFFSET     (0x150 >> 2)
#define CHKR0_CLKA_UPPER_OFFSET   (0x160 >> 2)
#define CHKR0_CLKA_LOWER_OFFSET   (0x164 >> 2)
#define CHKR0_CLKB_CNT_OFFSET     (0x168 >> 2)
#define CHKR0_CTRL_OFFSET         (0x16C >> 2)
#define CHKR1_CLKA_UPPER_OFFSET   (0x170 >> 2)
#define CHKR1_CLKA_LOWER_OFFSET   (0x174 >> 2)
#define CHKR1_CLKB_CNT_OFFSET     (0x178 >> 2)
#define CHKR1_CTRL_OFFSET         (0x17C >> 2)
#define CHKR2_CLKA_UPPER_OFFSET   (0x180 >> 2)
#define CHKR2_CLKA_LOWER_OFFSET   (0x184 >> 2)
#define CHKR2_CLKB_CNT_OFFSET     (0x188 >> 2)
#define CHKR2_CTRL_OFFSET         (0x18C >> 2)
#define CHKR3_CLKA_UPPER_OFFSET   (0x190 >> 2)
#define CHKR3_CLKA_LOWER_OFFSET   (0x194 >> 2)
#define CHKR3_CLKB_CNT_OFFSET     (0x198 >> 2)
#define CHKR3_CTRL_OFFSET         (0x19C >> 2)
#define CHKR4_CLKA_UPPER_OFFSET   (0x1A0 >> 2)
#define CHKR4_CLKA_LOWER_OFFSET   (0x1A4 >> 2)
#define CHKR4_CLKB_CNT_OFFSET     (0x1A8 >> 2)
#define CHKR4_CTRL_OFFSET         (0x1AC >> 2)
#define CHKR5_CLKA_UPPER_OFFSET   (0x1B0 >> 2)
#define CHKR5_CLKA_LOWER_OFFSET   (0x1B4 >> 2)
#define CHKR5_CLKB_CNT_OFFSET     (0x1B8 >> 2)
#define CHKR5_CTRL_OFFSET         (0x1BC >> 2)
#define CHKR6_CLKA_UPPER_OFFSET   (0x1C0 >> 2)
#define CHKR6_CLKA_LOWER_OFFSET   (0x1C4 >> 2)
#define CHKR6_CLKB_CNT_OFFSET     (0x1C8 >> 2)
#define CHKR6_CTRL_OFFSET         (0x1CC >> 2)
#define CHKR7_CLKA_UPPER_OFFSET   (0x1D0 >> 2)
#define CHKR7_CLKA_LOWER_OFFSET   (0x1D4 >> 2)
#define CHKR7_CLKB_CNT_OFFSET     (0x1D8 >> 2)
#define CHKR7_CTRL_OFFSET         (0x1DC >> 2)
#define BOOT_MODE_USER_OFFSET     (0x200 >> 2)
#define BOOT_MODE_POR_OFFSET      (0x204 >> 2)
#define RESET_CTRL_OFFSET         (0x218 >> 2)
#define BLOCKONLY_RST_OFFSET      (0x21C >> 2)
#define RESET_REASON_OFFSET       (0x220 >> 2)
#define RST_LPD_IOU0_OFFSET       (0x230 >> 2)
#define RST_LPD_IOU2_OFFSET       (0x238 >> 2)
#define RST_LPD_TOP_OFFSET        (0x23C >> 2)
#define RST_LPD_DBG_OFFSET        (0x240 >> 2)
#define BANK3_CTRL0_OFFSET        (0x270 >> 2)
#define BANK3_CTRL1_OFFSET        (0x274 >> 2)
#define BANK3_CTRL2_OFFSET        (0x278 >> 2)
#define BANK3_CTRL3_OFFSET        (0x27C >> 2)
#define BANK3_CTRL4_OFFSET        (0x280 >> 2)
#define BANK3_CTRL5_OFFSET        (0x284 >> 2)
#define BANK3_STATUS_OFFSET       (0x288 >> 2)

enum {
    XLNX_REF_GT = 7,
    XLNX_REF_AUX = 6,
    XLNX_REF_ALT = 5,
    XLNX_REF_VIDEO = 4,
    XLNX_REF_PS = 0
};

static void xlnx_crl_update_rpll(XlnxCRL *s)
{
    uint64_t rate_in = 0;
    uint64_t ctrl_reg = s->regs[RPLL_CTRL_OFFSET];
    uint32_t frac_reg = s->regs[RPLL_FRAC_CFG_OFFSET];

    if (extract32(ctrl_reg, 3, 1)) {
        /* Bypass */
        switch (extract32(ctrl_reg, 24, 3)) {
        case XLNX_REF_GT:
            rate_in = s->gt_crx_ref_clk;
            break;
        case XLNX_REF_AUX:
            rate_in = s->aux_ref_clk;
            break;
        case XLNX_REF_ALT:
            rate_in = s->ps_alt_ref_clk;
            break;
        case XLNX_REF_VIDEO:
            rate_in = s->video_ref_clk;
            break;
        case XLNX_REF_PS:
        default:
            /* PS_REF_CLK */
            rate_in = s->ps_ref_clk;
            break;
        }
    } else {
        if (!extract32(ctrl_reg, 0, 1)) {
            /* Not reset */
            switch (extract32(ctrl_reg, 24, 3)) {
            case XLNX_REF_GT:
                rate_in = s->gt_crx_ref_clk;
                break;
            case XLNX_REF_AUX:
                rate_in = s->aux_ref_clk;
                break;
            case XLNX_REF_ALT:
                rate_in = s->ps_alt_ref_clk;
                break;
            case XLNX_REF_VIDEO:
                rate_in = s->video_ref_clk;
                break;
            case XLNX_REF_PS:
            default:
                /* PS_REF_CLK */
                rate_in = s->ps_ref_clk;
                break;
            }

            /* Feedback divisor */
            rate_in *= extract32(ctrl_reg, 8, 7);

            if (extract32(frac_reg, 31, 1)) {
                /* Fractional mode */
                rate_in += (rate_in * extract32(frac_reg, 0, 16)) >> 16;
            }

            if (extract32(ctrl_reg, 16, 1)) {
                rate_in = rate_in >> 1;
            }
        }
    }

//    printf("rpll_update: reg: %" PRIX64 ", freq: %lu\n", ctrl_reg, rate_in);
    qemu_clk_update_rate(&s->int_rpll, rate_in);
}

static void xlnx_crl_lpd_lsbus_update(XlnxCRL *s)
{
    QEMUClock *source = NULL;

    /* LPD_LSBUS_CTRL changed so we potentially changed SRCSEL */
    qemu_clk_unbind(&s->int_rpll, &s->out_lpd_lsbus);
    qemu_clk_unbind(&s->int_iopll, &s->out_lpd_lsbus);
    qemu_clk_unbind(&s->ext_dpll_clk_to_lpd, &s->out_lpd_lsbus);

    switch (extract32(s->regs[LPD_LSBUS_CTRL_OFFSET], 0, 3)) {
    case 0:
        source = &s->int_rpll;
        break;
    case 2:
        source = &s->int_iopll;
        break;
    case 3:
        source = &s->ext_dpll_clk_to_lpd;
        break;
    }

    assert(source);
    qemu_clk_bind(source, &s->out_lpd_lsbus);
    /* Will update the whole tree */
    qemu_clk_refresh(source);
}

static uint64_t lpd_lsbus_update_rate(void *opaque, uint64_t input_rate)
{
    XlnxCRL *s = XLNX_CRL(opaque);
    uint64_t ctrl_reg = s->regs[LPD_LSBUS_CTRL_OFFSET];
    uint64_t output_rate = 0;

    if (extract32(ctrl_reg, 24, 1)) {
        output_rate = extract32(ctrl_reg, 8, 6)
                    ? input_rate / extract32(ctrl_reg, 8, 6)
                    : 0;
    }

    return output_rate;
}

static uint64_t crl_apb_read(void *opaque, hwaddr addr, unsigned size)
{
    XlnxCRL *s = XLNX_CRL(opaque);
    uint64_t ret;

    switch (addr >> 2) {
    default:
        ret = s->regs[addr >> 2];
        break;
    }
//    printf("read @0x%" HWADDR_PRIX " -> %" PRIX64 "\n", addr, ret);
    return ret;
}

static void crl_apb_write(void *opaque, hwaddr addr, uint64_t val,
                          unsigned size)
{
    XlnxCRL *s = XLNX_CRL(opaque);

//    printf("write @0x%" HWADDR_PRIX " -> %" PRIX64 "\n", addr, val);
    switch (addr >> 2) {
    case RPLL_FRAC_CFG_OFFSET:
    case RPLL_CTRL_OFFSET:
        s->regs[addr >> 2] = val;
        xlnx_crl_update_rpll(s);
        break;
    case LPD_LSBUS_CTRL_OFFSET:
        s->regs[addr >> 2] = val;
        xlnx_crl_lpd_lsbus_update(s);
        break;
    case RESET_CTRL_OFFSET: /* RESET_CTRL */
      if (val & 0x10) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
      }
    default:
        s->regs[addr >> 2] = val;
      break;
    }
}

static const MemoryRegionOps xlnx_crl_io = {
    .read = crl_apb_read,
    .write = crl_apb_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static Property xlnx_crl_properties[] = {
    DEFINE_PROP_UINT32("ps_ref_clk", XlnxCRL, ps_ref_clk, 0x1fca055),
    DEFINE_PROP_UINT32("video_ref_clk", XlnxCRL, video_ref_clk, 0x00000000),
    DEFINE_PROP_UINT32("ps_alt_ref_clk", XlnxCRL, ps_alt_ref_clk, 0x00000000),
    DEFINE_PROP_UINT32("aux_ref_clk", XlnxCRL, aux_ref_clk, 0x00000000),
    DEFINE_PROP_UINT32("gt_crx_ref_clk", XlnxCRL, gt_crx_ref_clk, 0x00000000),
    DEFINE_PROP_END_OF_LIST()
};

static void xlnx_crl_realizefn(DeviceState *dev, Error **errp)
{
    XlnxCRL *s = XLNX_CRL(dev);

    xlnx_crl_update_rpll(s);
    xlnx_crl_lpd_lsbus_update(s);
//    qemu_clk_update_rate(&s->out, s->rate);
}

static void xlnx_crl_instance_init(Object *obj)
{
    XlnxCRL *s = XLNX_CRL(obj);

    object_initialize(&s->int_rpll, sizeof(s->int_rpll), TYPE_CLOCK);
    qemu_clk_device_add_clock(DEVICE(obj), &s->int_rpll, "int_rpll");
    object_initialize(&s->int_iopll, sizeof(s->int_iopll), TYPE_CLOCK);
    qemu_clk_device_add_clock(DEVICE(obj), &s->int_iopll, "int_iopll");
    object_initialize(&s->ext_dpll_clk_to_lpd,
                      sizeof(s->ext_dpll_clk_to_lpd), TYPE_CLOCK);
    qemu_clk_device_add_clock(DEVICE(obj), &s->ext_dpll_clk_to_lpd,
                              "ext_dpll_clk_to_lpd");
    object_initialize(&s->out_lpd_lsbus, sizeof(s->out_lpd_lsbus), TYPE_CLOCK);
    qemu_clk_device_add_clock(DEVICE(obj), &s->out_lpd_lsbus, "out_lpd_lsbus");
    qemu_clk_set_callback(&s->out_lpd_lsbus, lpd_lsbus_update_rate, obj);

    memory_region_init_io(&s->iomem, obj, &xlnx_crl_io, s,
                          TYPE_XLNX_CRL, XLNX_CRL_MAX_REGS);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);

    s->regs[IOPLL_CTRL_OFFSET] =         0x00012C09;
    s->regs[RPLL_CTRL_OFFSET]  =         0x00012C09;
    s->regs[PLL_STATUS_OFFSET] =         0x0000001B;
    s->regs[IOPLL_TO_FPD_CTRL_OFFSET] =  0x00000400;
    s->regs[RPLL_TO_FPD_CTRL_OFFSET] =   0x00000400;
    s->regs[USB3_DUAL_REF_CTRL_OFFSET] = 0x00052000;
    s->regs[GEM0_REF_CTRL_OFFSET]      = 0x00002500;
    s->regs[GEM1_REF_CTRL_OFFSET]      = 0x00002500;
    s->regs[GEM2_REF_CTRL_OFFSET]      = 0x00002500;
    s->regs[GEM3_REF_CTRL_OFFSET]      = 0x00002500;
    s->regs[USB0_BUS_REF_CTRL_OFFSET]  = 0x00052000;
    s->regs[USB1_BUS_REF_CTRL_OFFSET] =  0x00052000;
    s->regs[QSPI_REF_CTRL_OFFSET] =      0x01000800;
    s->regs[SDIO0_REF_CTRL_OFFSET] =     0x01000F00;
    s->regs[SDIO1_REF_CTRL_OFFSET] =     0x01000F00;
    s->regs[UART0_REF_CTRL_OFFSET] =     0x01001800;
    s->regs[UART1_REF_CTRL_OFFSET] =     0x01001800;
    s->regs[SPI0_REF_CTRL_OFFSET] =      0x01001800;
    s->regs[SPI1_REF_CTRL_OFFSET] =      0x01001800;
    s->regs[CAN0_REF_CTRL_OFFSET] =      0x01001800;
    s->regs[CAN1_REF_CTRL_OFFSET] =      0x01001800;
    s->regs[CPU_R5_CTRL_OFFSET] =        0x03000600;
    s->regs[IOU_SWITCH_CTRL_OFFSET] =    0x00001500;
    s->regs[CSU_PLL_CTRL_OFFSET] =       0x01001500;
    s->regs[PCAP_CTRL_OFFSET] =          0x00001500;
    s->regs[LPD_SWITCH_CTRL_OFFSET] =    0x01000500;
    s->regs[LPD_LSBUS_CTRL_OFFSET] =     0x01001800;
    s->regs[DBG_LPD_CTRL_OFFSET] =       0x01002000;
    s->regs[NAND_REF_CTRL_OFFSET] =      0x00052000;
    s->regs[LPD_DMA_REF_CTRL_OFFSET] =   0x00002000;
    s->regs[PL0_REF_CTRL_OFFSET] =       0x00052000;
    s->regs[PL1_REF_CTRL_OFFSET] =       0x00052000;
    s->regs[PL2_REF_CTRL_OFFSET] =       0x00052000;
    s->regs[PL3_REF_CTRL_OFFSET] =       0x00052000;
    s->regs[PL0_THR_CTRL_OFFSET] =       0x00000001;
    s->regs[PL1_THR_CTRL_OFFSET] =       0x00000001;
    s->regs[PL2_THR_CTRL_OFFSET] =       0x00000001;
    s->regs[PL3_THR_CTRL_OFFSET] =       0x00000001;
    s->regs[GEM_TSU_REF_CTRL_OFFSET] =   0x00051000;
    s->regs[DLL_REF_CTRL_OFFSET] =       0x00000000;
    s->regs[PSSYSMON_REF_CTRL_OFFSET] =  0x01001800;
    s->regs[I2C0_REF_CTRL_OFFSET] =      0x01000500;
    s->regs[I2C1_REF_CTRL_OFFSET] =      0x01000500;
    s->regs[TIMESTAMP_REF_CTRL_OFFSET] = 0x00001800;
    s->regs[CLKMON_MASK_OFFSET] =        0x0000FFFF;
    s->regs[RESET_CTRL_OFFSET] =         0x00000001;
    s->regs[RESET_REASON_OFFSET] =       0x00000001;
    s->regs[RST_LPD_IOU0_OFFSET] =       0x0000000F;
    s->regs[RST_LPD_IOU2_OFFSET] =       0x0017FFFF;
    s->regs[RST_LPD_TOP_OFFSET] =        0x00188FDF;
    s->regs[RST_LPD_DBG_OFFSET] =        0x00000033;
    s->regs[BANK3_CTRL0_OFFSET] =        0x000003FF;
    s->regs[BANK3_CTRL1_OFFSET] =        0x000003FF;
    s->regs[BANK3_CTRL2_OFFSET] =        0x000003FF;
    s->regs[BANK3_CTRL3_OFFSET] =        0x000003FF;
    s->regs[BANK3_CTRL4_OFFSET] =        0x000003FF;
}

static void xlnx_crl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = xlnx_crl_realizefn;
    dc->props = xlnx_crl_properties;
}

static const TypeInfo xlnx_crl_info = {
    .name          = TYPE_XLNX_CRL,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(XlnxCRL),
    .instance_init = xlnx_crl_instance_init,
    .class_init    = xlnx_crl_class_init,
};

static void xlnx_crl_register_types(void)
{
    type_register_static(&xlnx_crl_info);
}

type_init(xlnx_crl_register_types);
