#ifndef PCI_HOST_APB_H
#define PCI_HOST_APB_H

#include "hw/sparc/sun4u_iommu.h"

#define MAX_IVEC 0x40

/* OBIO IVEC IRQs */
#define OBIO_HDD_IRQ         0x20
#define OBIO_NIC_IRQ         0x21
#define OBIO_LPT_IRQ         0x22
#define OBIO_FDD_IRQ         0x27
#define OBIO_KBD_IRQ         0x29
#define OBIO_MSE_IRQ         0x2a
#define OBIO_SER_IRQ         0x2b

#define TYPE_APB "pbm"

#define APB_DEVICE(obj) \
    OBJECT_CHECK(APBState, (obj), TYPE_APB)

typedef struct APBState {
    PCIHostState parent_obj;

    hwaddr special_base;
    hwaddr mem_base;
    MemoryRegion apb_config;
    MemoryRegion pci_config;
    MemoryRegion pci_mmio;
    MemoryRegion pci_ioport;
    uint64_t pci_irq_in;
    IOMMUState *iommu;
    PCIBridge *bridgeA;
    PCIBridge *bridgeB;
    uint32_t pci_control[16];
    uint32_t pci_irq_map[8];
    uint32_t pci_err_irq_map[4];
    uint32_t obio_irq_map[32];
    qemu_irq ivec_irqs[MAX_IVEC];
    unsigned int irq_request;
    uint32_t reset_control;
    unsigned int nr_resets;
} APBState;

typedef struct PBMPCIBridge {
    /*< private >*/
    PCIBridge parent_obj;
} PBMPCIBridge;

#define TYPE_PBM_PCI_BRIDGE "pbm-bridge"
#define PBM_PCI_BRIDGE(obj) \
    OBJECT_CHECK(PBMPCIBridge, (obj), TYPE_PBM_PCI_BRIDGE)

#endif
