/*
 * QEMU Generic PCI Express Bridge Emulation
 *
 * Copyright (C) 2015 Alexander Graf <agraf@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

#ifndef HW_GPEX_H
#define HW_GPEX_H

#include "hw/sysbus.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie_host.h"
#include "qom/object.h"

#define TYPE_GPEX_HOST "gpex-pcihost"
typedef struct GPEXHost GPEXHost;
DECLARE_INSTANCE_CHECKER(GPEXHost, GPEX_HOST,
                         TYPE_GPEX_HOST)

#define TYPE_GPEX_ROOT_DEVICE "gpex-root"
typedef struct GPEXRootState GPEXRootState;
DECLARE_INSTANCE_CHECKER(GPEXRootState, GPEX_ROOT_DEVICE,
                         TYPE_GPEX_ROOT_DEVICE)

#define GPEX_NUM_IRQS 4

struct GPEXRootState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
};

struct GPEXHost {
    /*< private >*/
    PCIExpressHost parent_obj;
    /*< public >*/

    GPEXRootState gpex_root;

    MemoryRegion io_ioport;
    MemoryRegion io_mmio;
    qemu_irq irq[GPEX_NUM_IRQS];
    int irq_num[GPEX_NUM_IRQS];
};

int gpex_set_irq_num(GPEXHost *s, int index, int gsi);

#endif /* HW_GPEX_H */
