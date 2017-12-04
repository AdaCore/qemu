
#ifndef XLNXCRL_H
#define XLNXCRL_H

#include "hw/qdev.h"
#include "sysemu/sysemu.h"
#include "qemu/qemu-clock.h"

#define XLNX_CRL_MAX_REGS (0x300)

typedef struct {
    DeviceState parent_obj;

    uint32_t regs[XLNX_CRL_MAX_REGS];
    MemoryRegion iomem;

    /* Clock references */
    uint32_t ps_ref_clk;
    uint32_t video_ref_clk;
    uint32_t ps_alt_ref_clk;
    uint32_t aux_ref_clk;
    uint32_t gt_crx_ref_clk;

    /* External clock signals */
    QEMUClock ext_dpll_clk_to_lpd;
    /* Internal clock signals */
    QEMUClock int_rpll;
    QEMUClock int_iopll;
    /* Output clock signals */
    QEMUClock out_lpd_lsbus;
} XlnxCRL;

#define TYPE_XLNX_CRL "xlnx_crl"
#define XLNX_CRL(obj) \
    OBJECT_CHECK(XlnxCRL, (obj), TYPE_XLNX_CRL)

#endif /* XLNXCRL_H */
