/*
 * QEMU Ultrasparc APB PCI host
 *
 * Copyright (c) 2006 Fabrice Bellard
 * Copyright (c) 2012,2013 Artyom Tarasenko
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

/* XXX This file and most of its contents are somewhat misnamed.  The
   Ultrasparc PCI host is called the PCI Bus Module (PBM).  The APB is
   the secondary PCI bridge.  */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci-host/apb.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "qapi/error.h"
#include "qemu/log.h"

/* debug APB */
//#define DEBUG_APB

#ifdef DEBUG_APB
#define APB_DPRINTF(fmt, ...) \
do { printf("APB: " fmt , ## __VA_ARGS__); } while (0)
#else
#define APB_DPRINTF(fmt, ...)
#endif

/*
 * Chipset docs:
 * PBM: "UltraSPARC IIi User's Manual",
 * http://www.sun.com/processors/manuals/805-0087.pdf
 *
 * APB: "Advanced PCI Bridge (APB) User's Manual",
 * http://www.sun.com/processors/manuals/805-1251.pdf
 */

#define PBM_PCI_IMR_MASK    0x7fffffff
#define PBM_PCI_IMR_ENABLED 0x80000000

#define POR          (1U << 31)
#define SOFT_POR     (1U << 30)
#define SOFT_XIR     (1U << 29)
#define BTN_POR      (1U << 28)
#define BTN_XIR      (1U << 27)
#define RESET_MASK   0xf8000000
#define RESET_WCMASK 0x98000000
#define RESET_WMASK  0x60000000

#define NO_IRQ_REQUEST (MAX_IVEC + 1)

static inline void pbm_set_request(APBState *s, unsigned int irq_num)
{
    APB_DPRINTF("%s: request irq %d\n", __func__, irq_num);

    s->irq_request = irq_num;
    qemu_set_irq(s->ivec_irqs[irq_num], 1);
}

static inline void pbm_check_irqs(APBState *s)
{

    unsigned int i;

    /* Previous request is not acknowledged, resubmit */
    if (s->irq_request != NO_IRQ_REQUEST) {
        pbm_set_request(s, s->irq_request);
        return;
    }
    /* no request pending */
    if (s->pci_irq_in == 0ULL) {
        return;
    }
    for (i = 0; i < 32; i++) {
        if (s->pci_irq_in & (1ULL << i)) {
            if (s->pci_irq_map[i >> 2] & PBM_PCI_IMR_ENABLED) {
                pbm_set_request(s, i);
                return;
            }
        }
    }
    for (i = 32; i < 64; i++) {
        if (s->pci_irq_in & (1ULL << i)) {
            if (s->obio_irq_map[i - 32] & PBM_PCI_IMR_ENABLED) {
                pbm_set_request(s, i);
                break;
            }
        }
    }
}

static inline void pbm_clear_request(APBState *s, unsigned int irq_num)
{
    APB_DPRINTF("%s: clear request irq %d\n", __func__, irq_num);
    qemu_set_irq(s->ivec_irqs[irq_num], 0);
    s->irq_request = NO_IRQ_REQUEST;
}

static AddressSpace *pbm_pci_dma_iommu(PCIBus *bus, void *opaque, int devfn)
{
    IOMMUState *is = opaque;

    return &is->iommu_as;
}

static void apb_config_writel (void *opaque, hwaddr addr,
                               uint64_t val, unsigned size)
{
    APBState *s = opaque;

    APB_DPRINTF("%s: addr " TARGET_FMT_plx " val %" PRIx64 "\n", __func__, addr, val);

    switch (addr & 0xffff) {
    case 0x30 ... 0x4f: /* DMA error registers */
        /* XXX: not implemented yet */
        break;
    case 0xc00 ... 0xc3f: /* PCI interrupt control */
        if (addr & 4) {
            unsigned int ino = (addr & 0x3f) >> 3;
            s->pci_irq_map[ino] &= PBM_PCI_IMR_MASK;
            s->pci_irq_map[ino] |= val & ~PBM_PCI_IMR_MASK;
            if ((s->irq_request == ino) && !(val & ~PBM_PCI_IMR_MASK)) {
                pbm_clear_request(s, ino);
            }
            pbm_check_irqs(s);
        }
        break;
    case 0x1000 ... 0x107f: /* OBIO interrupt control */
        if (addr & 4) {
            unsigned int ino = ((addr & 0xff) >> 3);
            s->obio_irq_map[ino] &= PBM_PCI_IMR_MASK;
            s->obio_irq_map[ino] |= val & ~PBM_PCI_IMR_MASK;
            if ((s->irq_request == (ino | 0x20))
                 && !(val & ~PBM_PCI_IMR_MASK)) {
                pbm_clear_request(s, ino | 0x20);
            }
            pbm_check_irqs(s);
        }
        break;
    case 0x1400 ... 0x14ff: /* PCI interrupt clear */
        if (addr & 4) {
            unsigned int ino = (addr & 0xff) >> 5;
            if ((s->irq_request / 4)  == ino) {
                pbm_clear_request(s, s->irq_request);
                pbm_check_irqs(s);
            }
        }
        break;
    case 0x1800 ... 0x1860: /* OBIO interrupt clear */
        if (addr & 4) {
            unsigned int ino = ((addr & 0xff) >> 3) | 0x20;
            if (s->irq_request == ino) {
                pbm_clear_request(s, ino);
                pbm_check_irqs(s);
            }
        }
        break;
    case 0x2000 ... 0x202f: /* PCI control */
        s->pci_control[(addr & 0x3f) >> 2] = val;
        break;
    case 0xf020 ... 0xf027: /* Reset control */
        if (addr & 4) {
            val &= RESET_MASK;
            s->reset_control &= ~(val & RESET_WCMASK);
            s->reset_control |= val & RESET_WMASK;
            if (val & SOFT_POR) {
                s->nr_resets = 0;
                qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
            } else if (val & SOFT_XIR) {
                qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
            }
        }
        break;
    case 0x5000 ... 0x51cf: /* PIO/DMA diagnostics */
    case 0xa400 ... 0xa67f: /* IOMMU diagnostics */
    case 0xa800 ... 0xa80f: /* Interrupt diagnostics */
    case 0xf000 ... 0xf01f: /* FFB config, memory control */
        /* we don't care */
    default:
        break;
    }
}

static uint64_t apb_config_readl (void *opaque,
                                  hwaddr addr, unsigned size)
{
    APBState *s = opaque;
    uint32_t val;

    switch (addr & 0xffff) {
    case 0x30 ... 0x4f: /* DMA error registers */
        val = 0;
        /* XXX: not implemented yet */
        break;
    case 0xc00 ... 0xc3f: /* PCI interrupt control */
        if (addr & 4) {
            val = s->pci_irq_map[(addr & 0x3f) >> 3];
        } else {
            val = 0;
        }
        break;
    case 0x1000 ... 0x107f: /* OBIO interrupt control */
        if (addr & 4) {
            val = s->obio_irq_map[(addr & 0xff) >> 3];
        } else {
            val = 0;
        }
        break;
    case 0x1080 ... 0x108f: /* PCI bus error */
        if (addr & 4) {
            val = s->pci_err_irq_map[(addr & 0xf) >> 3];
        } else {
            val = 0;
        }
        break;
    case 0x2000 ... 0x202f: /* PCI control */
        val = s->pci_control[(addr & 0x3f) >> 2];
        break;
    case 0xf020 ... 0xf027: /* Reset control */
        if (addr & 4) {
            val = s->reset_control;
        } else {
            val = 0;
        }
        break;
    case 0x5000 ... 0x51cf: /* PIO/DMA diagnostics */
    case 0xa400 ... 0xa67f: /* IOMMU diagnostics */
    case 0xa800 ... 0xa80f: /* Interrupt diagnostics */
    case 0xf000 ... 0xf01f: /* FFB config, memory control */
        /* we don't care */
    default:
        val = 0;
        break;
    }
    APB_DPRINTF("%s: addr " TARGET_FMT_plx " -> %x\n", __func__, addr, val);

    return val;
}

static const MemoryRegionOps apb_config_ops = {
    .read = apb_config_readl,
    .write = apb_config_writel,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void apb_pci_config_write(void *opaque, hwaddr addr,
                                 uint64_t val, unsigned size)
{
    APBState *s = opaque;
    PCIHostState *phb = PCI_HOST_BRIDGE(s);

    APB_DPRINTF("%s: addr " TARGET_FMT_plx " val %" PRIx64 "\n", __func__, addr, val);
    pci_data_write(phb->bus, addr, val, size);
}

static uint64_t apb_pci_config_read(void *opaque, hwaddr addr,
                                    unsigned size)
{
    uint32_t ret;
    APBState *s = opaque;
    PCIHostState *phb = PCI_HOST_BRIDGE(s);

    ret = pci_data_read(phb->bus, addr, size);
    APB_DPRINTF("%s: addr " TARGET_FMT_plx " -> %x\n", __func__, addr, ret);
    return ret;
}

/* The APB host has an IRQ line for each IRQ line of each slot.  */
static int pci_apb_map_irq(PCIDevice *pci_dev, int irq_num)
{
    /* Return the irq as swizzled by the PBM */
    return irq_num;
}

static int pci_pbmA_map_irq(PCIDevice *pci_dev, int irq_num)
{
    /* The on-board devices have fixed (legacy) OBIO intnos */
    switch (PCI_SLOT(pci_dev->devfn)) {
    case 1:
        /* Onboard NIC */
        return OBIO_NIC_IRQ;
    case 3:
        /* Onboard IDE */
        return OBIO_HDD_IRQ;
    default:
        /* Normal intno, fall through */
        break;
    }

    return ((PCI_SLOT(pci_dev->devfn) << 2) + irq_num) & 0x1f;
}

static int pci_pbmB_map_irq(PCIDevice *pci_dev, int irq_num)
{
    return (0x10 + (PCI_SLOT(pci_dev->devfn) << 2) + irq_num) & 0x1f;
}

static void pci_apb_set_irq(void *opaque, int irq_num, int level)
{
    APBState *s = opaque;

    APB_DPRINTF("%s: set irq_in %d level %d\n", __func__, irq_num, level);
    /* PCI IRQ map onto the first 32 INO.  */
    if (irq_num < 32) {
        if (level) {
            s->pci_irq_in |= 1ULL << irq_num;
            if (s->pci_irq_map[irq_num >> 2] & PBM_PCI_IMR_ENABLED) {
                pbm_set_request(s, irq_num);
            }
        } else {
            s->pci_irq_in &= ~(1ULL << irq_num);
        }
    } else {
        /* OBIO IRQ map onto the next 32 INO.  */
        if (level) {
            APB_DPRINTF("%s: set irq %d level %d\n", __func__, irq_num, level);
            s->pci_irq_in |= 1ULL << irq_num;
            if ((s->irq_request == NO_IRQ_REQUEST)
                && (s->obio_irq_map[irq_num - 32] & PBM_PCI_IMR_ENABLED)) {
                pbm_set_request(s, irq_num);
            }
        } else {
            s->pci_irq_in &= ~(1ULL << irq_num);
        }
    }
}

static void apb_pci_bridge_realize(PCIDevice *dev, Error **errp)
{
    /*
     * command register:
     * According to PCI bridge spec, after reset
     *   bus master bit is off
     *   memory space enable bit is off
     * According to manual (805-1251.pdf).
     *   the reset value should be zero unless the boot pin is tied high
     *   (which is true) and thus it should be PCI_COMMAND_MEMORY.
     */
    PBMPCIBridge *br = PBM_PCI_BRIDGE(dev);

    pci_bridge_initfn(dev, TYPE_PCI_BUS);

    pci_set_word(dev->config + PCI_COMMAND, PCI_COMMAND_MEMORY);
    pci_set_word(dev->config + PCI_STATUS,
                 PCI_STATUS_FAST_BACK | PCI_STATUS_66MHZ |
                 PCI_STATUS_DEVSEL_MEDIUM);

    /* Allow 32-bit IO addresses */
    pci_set_word(dev->config + PCI_IO_BASE, PCI_IO_RANGE_TYPE_32);
    pci_set_word(dev->config + PCI_IO_LIMIT, PCI_IO_RANGE_TYPE_32);
    pci_set_word(dev->wmask + PCI_IO_BASE_UPPER16, 0xffff);
    pci_set_word(dev->wmask + PCI_IO_LIMIT_UPPER16, 0xffff);

    pci_bridge_update_mappings(PCI_BRIDGE(br));
}

static void pci_pbm_reset(DeviceState *d)
{
    APBState *s = APB_DEVICE(d);
    PCIDevice *pci_dev;
    unsigned int i;
    uint16_t cmd;

    for (i = 0; i < 8; i++) {
        s->pci_irq_map[i] &= PBM_PCI_IMR_MASK;
    }
    for (i = 0; i < 32; i++) {
        s->obio_irq_map[i] &= PBM_PCI_IMR_MASK;
    }

    s->irq_request = NO_IRQ_REQUEST;
    s->pci_irq_in = 0ULL;

    if (s->nr_resets++ == 0) {
        /* Power on reset */
        s->reset_control = POR;
    }

    /* As this is the busA PCI bridge which contains the on-board devices
     * attached to the ebus, ensure that we initially allow IO transactions
     * so that we get the early serial console until OpenBIOS can properly
     * configure the PCI bridge itself */
    pci_dev = PCI_DEVICE(s->bridgeA);
    cmd = pci_get_word(pci_dev->config + PCI_COMMAND);
    pci_set_word(pci_dev->config + PCI_COMMAND, cmd | PCI_COMMAND_IO);
    pci_bridge_update_mappings(PCI_BRIDGE(pci_dev));
}

static const MemoryRegionOps pci_config_ops = {
    .read = apb_pci_config_read,
    .write = apb_pci_config_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void pci_pbm_realize(DeviceState *dev, Error **errp)
{
    APBState *s = APB_DEVICE(dev);
    PCIHostState *phb = PCI_HOST_BRIDGE(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);
    PCIDevice *pci_dev;

    /* apb_config */
    sysbus_mmio_map(sbd, 0, s->special_base);
    /* PCI configuration space */
    sysbus_mmio_map(sbd, 1, s->special_base + 0x1000000ULL);
    /* pci_ioport */
    sysbus_mmio_map(sbd, 2, s->special_base + 0x2000000ULL);

    memory_region_init(&s->pci_mmio, OBJECT(s), "pci-mmio", 0x100000000ULL);
    memory_region_add_subregion(get_system_memory(), s->mem_base,
                                &s->pci_mmio);

    phb->bus = pci_register_bus(dev, "pci",
                                pci_apb_set_irq, pci_apb_map_irq, s,
                                &s->pci_mmio,
                                &s->pci_ioport,
                                0, 32, TYPE_PCI_BUS);

    pci_create_simple(phb->bus, 0, "pbm-pci");

    /* APB IOMMU */
    memory_region_add_subregion_overlap(&s->apb_config, 0x200,
                    sysbus_mmio_get_region(SYS_BUS_DEVICE(s->iommu), 0), 1);
    pci_setup_iommu(phb->bus, pbm_pci_dma_iommu, s->iommu);

    /* APB secondary busses */
    pci_dev = pci_create_multifunction(phb->bus, PCI_DEVFN(1, 0), true,
                                   TYPE_PBM_PCI_BRIDGE);
    s->bridgeB = PCI_BRIDGE(pci_dev);
    pci_bridge_map_irq(s->bridgeB, "pciB", pci_pbmB_map_irq);
    qdev_init_nofail(&pci_dev->qdev);

    pci_dev = pci_create_multifunction(phb->bus, PCI_DEVFN(1, 1), true,
                                   TYPE_PBM_PCI_BRIDGE);
    s->bridgeA = PCI_BRIDGE(pci_dev);
    pci_bridge_map_irq(s->bridgeA, "pciA", pci_pbmA_map_irq);
    qdev_init_nofail(&pci_dev->qdev);
}

static void pci_pbm_init(Object *obj)
{
    APBState *s = APB_DEVICE(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    unsigned int i;

    for (i = 0; i < 8; i++) {
        s->pci_irq_map[i] = (0x1f << 6) | (i << 2);
    }
    for (i = 0; i < 2; i++) {
        s->pci_err_irq_map[i] = (0x1f << 6) | 0x30;
    }
    for (i = 0; i < 32; i++) {
        s->obio_irq_map[i] = ((0x1f << 6) | 0x20) + i;
    }
    qdev_init_gpio_in_named(DEVICE(s), pci_apb_set_irq, "pbm-irq", MAX_IVEC);
    qdev_init_gpio_out_named(DEVICE(s), s->ivec_irqs, "ivec-irq", MAX_IVEC);
    s->irq_request = NO_IRQ_REQUEST;
    s->pci_irq_in = 0ULL;

    /* IOMMU */
    object_property_add_link(obj, "iommu", TYPE_SUN4U_IOMMU,
                             (Object **) &s->iommu,
                             qdev_prop_allow_set_link_before_realize,
                             0, NULL);

    /* apb_config */
    memory_region_init_io(&s->apb_config, OBJECT(s), &apb_config_ops, s,
                          "apb-config", 0x10000);
    /* at region 0 */
    sysbus_init_mmio(sbd, &s->apb_config);

    memory_region_init_io(&s->pci_config, OBJECT(s), &pci_config_ops, s,
                          "apb-pci-config", 0x1000000);
    /* at region 1 */
    sysbus_init_mmio(sbd, &s->pci_config);

    /* pci_ioport */
    memory_region_init(&s->pci_ioport, OBJECT(s), "apb-pci-ioport", 0x1000000);

    /* at region 2 */
    sysbus_init_mmio(sbd, &s->pci_ioport);
}

static void pbm_pci_host_realize(PCIDevice *d, Error **errp)
{
    pci_set_word(d->config + PCI_COMMAND,
                 PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
    pci_set_word(d->config + PCI_STATUS,
                 PCI_STATUS_FAST_BACK | PCI_STATUS_66MHZ |
                 PCI_STATUS_DEVSEL_MEDIUM);
}

static void pbm_pci_host_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->realize = pbm_pci_host_realize;
    k->vendor_id = PCI_VENDOR_ID_SUN;
    k->device_id = PCI_DEVICE_ID_SUN_SABRE;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    /*
     * PCI-facing part of the host bridge, not usable without the
     * host-facing part, which can't be device_add'ed, yet.
     */
    dc->user_creatable = false;
}

static const TypeInfo pbm_pci_host_info = {
    .name          = "pbm-pci",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = pbm_pci_host_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static Property pbm_pci_host_properties[] = {
    DEFINE_PROP_UINT64("special-base", APBState, special_base, 0),
    DEFINE_PROP_UINT64("mem-base", APBState, mem_base, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void pbm_host_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = pci_pbm_realize;
    dc->reset = pci_pbm_reset;
    dc->props = pbm_pci_host_properties;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo pbm_host_info = {
    .name          = TYPE_APB,
    .parent        = TYPE_PCI_HOST_BRIDGE,
    .instance_size = sizeof(APBState),
    .instance_init = pci_pbm_init,
    .class_init    = pbm_host_class_init,
};

static void pbm_pci_bridge_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = apb_pci_bridge_realize;
    k->exit = pci_bridge_exitfn;
    k->vendor_id = PCI_VENDOR_ID_SUN;
    k->device_id = PCI_DEVICE_ID_SUN_SIMBA;
    k->revision = 0x11;
    k->config_write = pci_bridge_write_config;
    k->is_bridge = 1;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->reset = pci_bridge_reset;
    dc->vmsd = &vmstate_pci_device;
}

static const TypeInfo pbm_pci_bridge_info = {
    .name          = TYPE_PBM_PCI_BRIDGE,
    .parent        = TYPE_PCI_BRIDGE,
    .class_init    = pbm_pci_bridge_class_init,
    .instance_size = sizeof(PBMPCIBridge),
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void pbm_register_types(void)
{
    type_register_static(&pbm_host_info);
    type_register_static(&pbm_pci_host_info);
    type_register_static(&pbm_pci_bridge_info);
}

type_init(pbm_register_types)
