
#ifndef XLNXCRL_H
#define XLNXCRL_H

#include "hw/qdev-core.h"
#include "hw/qdev-clock.h"
#include "sysemu/sysemu.h"
#include "exec/memory.h"
#include "hw/sysbus.h"

#define XLNX_CRL_MAX_REGS (0x300)

typedef struct {
    SysBusDevice parent_obj;

    uint32_t regs[XLNX_CRL_MAX_REGS];
    MemoryRegion iomem;

    /* Clock references */
    Clock *ps_ref_clk;
    Clock *video_ref_clk;
    Clock *ps_alt_ref_clk;
    Clock *aux_ref_clk;
    Clock *gt_crx_ref_clk;

    /* External clock signals */
    Clock *ext_dpll_clk_to_lpd;
    /* Output clock signals */
    Clock *out_lpd_lsbus;
} XlnxCRL;

#define TYPE_XLNX_CRL "xlnx_crl"
#define XLNX_CRL(obj) \
    OBJECT_CHECK(XlnxCRL, (obj), TYPE_XLNX_CRL)

#endif /* XLNXCRL_H */
