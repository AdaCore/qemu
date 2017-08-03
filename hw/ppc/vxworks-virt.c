/*
 * vxworks-virt.c
 *
 * Copyright (c) 2017 AdaCore
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

/*
 * This platform is a virtual platform for running vxWorks.
 * It creates the device tree itself, and initialize the MMU as if the kernel
 * has been started by uboot.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/boards.h"
#include "sysemu/device_tree.h"
#include "sysemu/kvm.h"
#include "hw/pci/pci.h"
#include "hw/ppc/openpic.h"
#include "kvm_ppc.h"
#include "hw/ppc/openpic.h"
#include "hw/ppc/ppc.h"
#include "elf.h"
#include "hw/sysbus.h"
#include "hw/loader.h"
#include "sysemu/sysemu.h"
#include "qapi/error.h"
#include <libfdt.h>
#include "hw/adacore/hostfs.h"

#define EPAPR_MAGIC  (0x45504150)
#define DTC_LOAD_PAD (0x1800000)
#define DTC_PAD_MASK (0xFFFFF)
#define DTC_MAX_SIZE (8 * 1024 * 1024)

#define HOSTFS_START (0xf3082000)
#define PARAMS_ADDR  (0xf3080000)
#define PARAMS_SIZE  (0x01000)

#define QSP_PIC_MAX_IRQ (1024)

#define BOOKE_TIMER_FREQ (1000000000)

static void nowhere_write(void *opaque, hwaddr offset,
                          uint64_t value, unsigned size)
{
    printf("unhandled write: 0x%" PRIX64 " @0x%" HWADDR_PRIX "\n",
           value, offset);
    abort();
}

static uint64_t nowhere_read(void *opaque, hwaddr offset,
                             unsigned size)
{
    printf("unhandled read: @0x%" HWADDR_PRIX "\n", offset);
    abort();
    return 0;
}

static const MemoryRegionOps nowhere_ops = {
    .read = nowhere_read,
    .write = nowhere_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

struct epapr_data {
    hwaddr dtb_entry_ea;
    hwaddr boot_ima_sz;
    hwaddr entry;
};

/* No device tree provided we need to create one from scratch... */
static void *vxworks_virt_create_dtb(uint64_t ram_size,
                                     uint64_t load_addr, int *size)
{
    void *fdt;
    char cur_node[128];
    int i;
    int pic_phandle;
    uint8_t mac[6] = {0x00, 0xA0, 0x1E, 0x10, 0x20, 0X30};

    fdt = create_device_tree(size);
    if (fdt == NULL) {
        return NULL;
    }

    pic_phandle = qemu_fdt_alloc_phandle(fdt);

    /*
     * One important stuff here is that everything is done in reverse in the
     * device tree within a node.
     */
    qemu_fdt_setprop_cell(fdt, "/", "interrupt-parent", pic_phandle);
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 1);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 1);
    qemu_fdt_setprop_string(fdt, "/", "compatible", "simics,qsp-ppc");
    qemu_fdt_setprop_string(fdt, "/", "model", "QEMU QSP PPC");

    /* /soc */
    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 1);
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 1);
    qemu_fdt_setprop_string(fdt, "/soc", "compatible", "simple-bus");
    qemu_fdt_setprop_string(fdt, "/soc", "device_type", "soc");

    /* /soc/sysregs */
    sprintf(cur_node, "/soc/sysregs@%8.8x", 0xe0001000);
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0001000, 0x1000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "simics,qsp-sysregs");

    /* /soc/ethernet@* */
    for (i = 15; i >= 0; i--) {
        sprintf(cur_node, "/soc/ethernet@%8.8x", 0xe0040000 + i * 0x1000);
        mac[5] = 0x30 + i;
        qemu_fdt_add_subnode(fdt, cur_node);
        qemu_fdt_setprop(fdt, cur_node, "local-mac-address", mac, 6);
        qemu_fdt_setprop_cell(fdt, cur_node, "interrupts", 64 + i);
        qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0040000 + i * 0x1000,
                               0x1000);
        qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                                "simics,qsp-mac");
    }

    /* /soc/timer@* */
    for (i = 15; i >= 0; i--) {
        sprintf(cur_node, "/soc/timer@%8.8x", 0xe0020000 + i * 0x1000);
        qemu_fdt_add_subnode(fdt, cur_node);
        qemu_fdt_setprop_cell(fdt, cur_node, "clock-rate-max", 8000);
        qemu_fdt_setprop_cell(fdt, cur_node, "clock-rate-min", 10);
        qemu_fdt_setprop_cell(fdt, cur_node, "clock-frequency", 1000000);
        qemu_fdt_setprop_cell(fdt, cur_node, "interrupts", 32 + i);
        qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0020000 + i * 0x1000,
                               0x1000);
        qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                                "simics,qsp-timer");
    }

    /* /soc/serial@* */
    for (i = 15; i >= 0; i--) {
        sprintf(cur_node, "/soc/serial@%8.8x", 0xe0010000 + i * 0x1000);
        qemu_fdt_add_subnode(fdt, cur_node);
        qemu_fdt_setprop_cell(fdt, cur_node, "interrupts", 16 + i);
        qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0010000 + i * 0x1000,
                               0x1000);
        qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                                "simics,qsp-uart");
    }

    /* /soc/pic@* */
    qemu_fdt_add_subnode(fdt, "/soc/pic@e0000000");
    qemu_fdt_setprop_cell(fdt, "/soc/pic@e0000000", "phandle", pic_phandle);
    qemu_fdt_setprop_cell(fdt, "/soc/pic@e0000000", "linux,phandle",
                          pic_phandle);
    qemu_fdt_setprop_cell(fdt, "/soc/pic@e0000000", "#interrupt-cells", 1);
    qemu_fdt_setprop(fdt, "/soc/pic@e0000000", "interrupt-controller", NULL,
                     0);
    qemu_fdt_setprop_cells(fdt, "/soc/pic@e0000000", "reg", 0xe0000000,
                           0x1000);
    qemu_fdt_setprop_string(fdt, "/soc/pic@e0000000", "compatible",
                            "simics,qsp-pic");

    /* /booke-timer@0 */
    qemu_fdt_add_subnode(fdt, "/booke-timer@0");
    qemu_fdt_setprop_cell(fdt, "/booke-timer@0", "clock-frequency",
                          BOOKE_TIMER_FREQ);
    qemu_fdt_setprop_cells(fdt, "/booke-timer@0", "reg", 0, 0);
    qemu_fdt_setprop_string(fdt, "/booke-timer@0", "compatible",
                            "fsl,booke-timer");

    /* /chosen */
    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
                            "emac(0,0)host:vxWorks"
                            " h=10.10.0.1 e=10.10.0.2:ffffff00 g=10.10.0.1"
                            " u=vxworks pw=vxworks f=0x0 tn=qsp-ppc");

    /* /cpus */
    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 1);

    for (i = smp_cpus - 1; i >= 0; i--) {
        sprintf(cur_node, "/cpus/cpu@%d", i);

        qemu_fdt_add_subnode(fdt, cur_node);
        qemu_fdt_setprop_cell(fdt, cur_node, "d-cache-line-size", 64);
        qemu_fdt_setprop_cell(fdt, cur_node, "i-cache-line-size", 64);
        qemu_fdt_setprop_cell(fdt, cur_node, "d-cache-size", 32768);
        qemu_fdt_setprop_cell(fdt, cur_node, "i-cache-size", 32768);
        qemu_fdt_setprop_cell(fdt, cur_node, "timebase-frequency",
                              1000000000);
        qemu_fdt_setprop_cell(fdt, cur_node, "clock-frequency", 1000000000);
        qemu_fdt_setprop_cells(fdt, cur_node, "reg", i * 2, i * 2 + 1);
        qemu_fdt_setprop_string(fdt, cur_node, "device_type", "cpu");
    }

    return fdt;
}

/*
 * The DTB need to be patched in some case because u-boot does that before
 * jumping in the vxworks kernel.
 */
static void vxworks_virt_compute_dtb(char *filename, uint64_t ram_size,
                                     uint64_t load_addr, int *size)
{
    void *fdt;
    Error *err = NULL;
    uint32_t acells, scells;

    if (!filename) {
        /* Create the dtb */
        fdt = vxworks_virt_create_dtb(ram_size, load_addr, size);
    } else {
        /* Load it */
        fdt = load_device_tree(filename, size);
    }
    
    if (!fdt) {
        fprintf(stderr, "Can't load the dtb.\n");
        exit(1);
    }

    acells = qemu_fdt_getprop_cell(fdt, "/", "#address-cells",
                                   NULL, &error_fatal);
    scells = qemu_fdt_getprop_cell(fdt, "/", "#size-cells",
                                   NULL, &error_fatal);
    if (acells == 0 || scells == 0) {
        fprintf(stderr, "dtb file invalid"
                        " (#address-cells or #size-cells 0)\n");
        exit(1);
    }

    if (fdt_path_offset(fdt, "/memory") < 0) {
        qemu_fdt_add_subnode(fdt, "/memory");
    }

    if (!qemu_fdt_getprop(fdt, "/memory", "device_type", NULL, &err)) {
        qemu_fdt_setprop_string(fdt, "/memory", "device_type", "memory");
    }

    if(qemu_fdt_setprop_sized_cells(fdt, "/memory", "reg",
                                    acells, 0x00,
                                    scells, ram_size) < 0) {
        fprintf(stderr, "Can't set memory prop\n");
        exit(1);
    }

    qemu_fdt_dumpdtb(fdt, *size);
    rom_add_blob_fixed("dtb", fdt, *size, load_addr);

    g_free(fdt);
}

static void vxworks_virt_mmu_init(CPUPPCState *env)
{
    ppcmas_tlb_t *tlb = booke206_get_tlbm(env, 1, 0, 0);
    hwaddr size = 0;

    /*
     * Create a first TLB to boot the kernel so it must cover
     * the kernel and the dtb
     */
    size = ((struct epapr_data *)env->load_info)->boot_ima_sz;
    size = 63 - clz64(size >> 10) + 1;
    if (size & 1) {
        size++;
    }
    tlb->mas1 = 0xC0000000 | (size << MAS1_TSIZE_SHIFT);
    tlb->mas2 = 0x00000006;
    tlb->mas7_3 = 0x00000015;

    env->tlb_dirty = true;
}

static void vxworks_virt_reset(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;
    struct epapr_data *epapr = (struct epapr_data *) env->load_info;

    cpu_reset(cs);

    /* Set initial guest state. */
    cs->halted = 0;
    env->gpr[1] = 0;
    env->gpr[3] = epapr->dtb_entry_ea;
    env->gpr[4] = 0;
    env->gpr[5] = 0;
    env->gpr[6] = EPAPR_MAGIC;
    env->gpr[7] = epapr->boot_ima_sz;
    env->gpr[8] = 0;
    env->gpr[9] = 0;
    env->nip = epapr->entry;

    vxworks_virt_mmu_init(env);
}

static void vxworks_virt_init(MachineState *machine)
{
    int i, bios_size, dtb_size;
    CPUPPCState *env = NULL, *firstenv;
    struct epapr_data *epapr = NULL;
    hwaddr loadaddr;
    PowerPCCPU *cpu = NULL;
    MemoryRegion *ram, *flash, *params, *iomem;
    DeviceState *dev;
    const char *kernel_cmdline;
    qemu_irq *irqs = NULL;

    if (strcmp(machine->cpu_type, POWERPC_CPU_TYPE_NAME("e6500"))) {
        fprintf(stderr, "Only e6500 cpu is supported here\n");
        exit(1);
    }

    if (smp_cpus > 1) {
        fprintf(stderr, "Only 1 cpu is supported\n");
        exit(1);
    }

    for (i = 0; i < smp_cpus; i++) {
        cpu = POWERPC_CPU(cpu_create(machine->cpu_type));

        if (cpu == NULL) {
            fprintf(stderr, "Unable to initialize CPU!\n");
            exit(1);
        }
        env = &cpu->env;

        if (!firstenv) {
            firstenv = env;
        }
    }

    ppc_booke_timers_init(cpu, BOOKE_TIMER_FREQ, PPC_TIMER_E500);
    qemu_register_reset(vxworks_virt_reset, cpu);

    ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ram, NULL, "ram", machine->ram_size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0, ram);

    flash = g_new(MemoryRegion, 1);
    memory_region_init_ram(flash, NULL, "flash", 256ULL * 1024 * 1024,
                           &error_fatal);
    memory_region_set_readonly(flash, true);
    memory_region_add_subregion(get_system_memory(), 0xD0000000, flash);

    params = g_new(MemoryRegion, 1);
    memory_region_init_ram(params, NULL, "params", PARAMS_SIZE,
                           &error_fatal);
    memory_region_add_subregion(get_system_memory(), PARAMS_ADDR, params);

    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, NULL, &nowhere_ops, NULL, "nowhere", ~0ULL);
    memory_region_add_subregion_overlap(get_system_memory(), 0, iomem, -1);

    irqs = g_new(qemu_irq, QSP_PIC_MAX_IRQ);
    dev = qdev_create(NULL, "qsp-pic");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xe0000000);

    for (i = 0; i < QSP_PIC_MAX_IRQ; i++) {
        irqs[i] = qdev_get_gpio_in(dev, i);
    }

    for (i = 0; i < smp_cpus; i++) {
        /* XXX: Should take an other env each time.. */
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i,
                           env->irq_inputs[PPCE500_INPUT_INT]);
    }

    for (i = 0; i < 16; i++) {
        Chardev *chr = NULL;

        if (i < 4) chr = serial_hd(i);
        dev = qdev_create(NULL, "qsp-uart");
        qdev_prop_set_chr(dev, "chardev", chr);
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xe0010000 + i * 0x1000);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irqs[16 + i]);
    }

    for (i = 0; i < 16; i++) {
        dev = qdev_create(NULL, "qsp-timer");
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xe0020000 + i * 0x1000);
    }

    for (i = 0; i < 16; i++) {
        dev = qdev_create(NULL, "qsp-mac");
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xe0040000 + i * 0x1000);
    }

    dev = qdev_create(NULL, "qsp-sysreg");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xe0001000);

    /* HostFS */
    hostfs_create(HOSTFS_START, get_system_memory());

    epapr = g_new(struct epapr_data, 1);
    if (machine->kernel_filename) {
        bios_size = load_elf(machine->kernel_filename, NULL, NULL, NULL,
                             &epapr->entry, &loadaddr, NULL,
                             1, PPC_ELF_MACHINE, 0, 0);
        if (bios_size < 0) {
            bios_size = load_uimage(machine->kernel_filename,
                                      &epapr->entry, &loadaddr, NULL,
                                      NULL, NULL);
            if (bios_size < 0) {
                fprintf(stderr, "error\n");
                exit(1);
            }
        }
        epapr->boot_ima_sz = bios_size;
    }

    epapr->dtb_entry_ea = (loadaddr + epapr->boot_ima_sz + DTC_LOAD_PAD)
        & ~DTC_PAD_MASK;
    vxworks_virt_compute_dtb(machine->dtb, machine->ram_size,
                             epapr->dtb_entry_ea,
                             &dtb_size);
    if (dtb_size < 0) {
        fprintf(stderr, "error\n");
        exit(1);
    }
    epapr->boot_ima_sz = epapr->dtb_entry_ea + dtb_size;

    kernel_cmdline = machine->kernel_cmdline ? machine->kernel_cmdline : "";
    cpu_physical_memory_write(PARAMS_ADDR, kernel_cmdline,
                              strlen(kernel_cmdline) + 1);
    memory_region_set_readonly(params, true);
    
    env->load_info = epapr;
}

static void vxworks_virt_machine_init(MachineClass *mc)
{
    mc->desc = "vxWorks virtual machine";
    mc->init = vxworks_virt_init;
    mc->max_cpus = 16;
    mc->default_ram_size = 3228ULL * 1024 * 1024;
    mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("e6500");
}

DEFINE_MACHINE("vxworks-virt", vxworks_virt_machine_init)
