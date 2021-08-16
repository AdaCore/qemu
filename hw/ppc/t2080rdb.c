/*
 * t2080rdb.c
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
#include "hw/char/serial.h"
#include "hw/qdev-properties.h"
#include "sysemu/reset.h"
#include "sysemu/runstate.h"
#include "hw/adacore/hostfs.h"

#define EPAPR_MAGIC  (0x45504150)
#define DTC_LOAD_PAD (0x1800000)
#define DTC_PAD_MASK (0xFFFFF)
#define DTC_MAX_SIZE (8 * 1024 * 1024)

#define HOSTFS_START (0xf3082000)
#define PARAMS_ADDR  (0xf3080000)
#define PARAMS_SIZE  (0x01000)

#define QSP_PIC_MAX_IRQ (1024)

#define BOOKE_TIMER_FREQ (6666666)

#define T2080RDB_CCSRBAR_BASE     0xFFE000000
#define T2080RDB_MPIC_BASE        0x00040000
#define T2080RDB_SERIAL0_BASE     0x0011C500
#define T2080RDB_SERIAL1_BASE     0x0011C600
#define T2080RDB_FMAN_BASE        0x00400000
#define T2080RDB_DEVICE_CONF_BASE 0x000E0000
#define T2080RDB_DEVICE_CONF_SIZE 0x00000E84
#define T2080RDB_SERIAL_IRQ (16 + 20)

#define T2080_MAX_CPUS 4

static void nowhere_write(void *opaque, hwaddr offset,
                          uint64_t value, unsigned size)
{
//    printf("unhandled write: 0x%" PRIX64 " @0x%" HWADDR_PRIX "\n",
//           value, offset);
//    abort();
}

static uint64_t nowhere_read(void *opaque, hwaddr offset,
                             unsigned size)
{
//    printf("unhandled read: @0x%" HWADDR_PRIX "\n", offset);
//    abort();
    return 0;
}

static const MemoryRegionOps nowhere_ops = {
    .read = nowhere_read,
    .write = nowhere_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

#define T2080RDB_DEVICE_CONF_RESET_OFFSET (0xB0 >> 2)

static void t2080rdb_device_conf_write(void *opaque, hwaddr offset,
                                       uint64_t value, unsigned size)
{
    switch (offset >> 2) {
    case T2080RDB_DEVICE_CONF_RESET_OFFSET:
        if (extract32(value, 1, 1)) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    default:
        printf("unhandled device conf write: @0x%" HWADDR_PRIX "\n", offset);
        abort();
        break;
    }
}

static uint64_t t2080rdb_device_conf_read(void *opaque, hwaddr offset,
                                          unsigned size)
{
    int64_t value = 0;

    switch (offset >> 2) {
    case T2080RDB_DEVICE_CONF_RESET_OFFSET:
        /* Will be 0 anyway.  */
        value = 0;
        break;
    default:
        printf("unhandled device conf write: @0x%" HWADDR_PRIX "\n", offset);
        abort();
    }
    return value;
}

static const MemoryRegionOps device_conf_ops = {
    .read = t2080rdb_device_conf_read,
    .write = t2080rdb_device_conf_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

struct epapr_data {
    hwaddr dtb_entry_ea;
    hwaddr boot_ima_sz;
    hwaddr entry;
};

/* No device tree provided we need to create one from scratch... */
static void *t2080rdb_create_dtb(MachineState *ms, uint64_t load_addr,
                                 int *size)
{
    void *fdt;
    const char *cur_node;
    int i;
    int pic_phandle;

    fdt = create_device_tree(size);
    if (fdt == NULL) {
        return NULL;
    }

    pic_phandle = qemu_fdt_alloc_phandle(fdt);

    /*
     * One important stuff here is that everything is done in reverse in the
     * device tree within a node.
     */
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_string(fdt, "/", "compatible", "fsl,t2080rdb");
    qemu_fdt_setprop_string(fdt, "/", "model", "QEMU T2080RDB");

    /* /soc */
    qemu_fdt_add_subnode(fdt, "/soc");
    const char soc_compat[] = "simple-bus\0fsl,immr";
    qemu_fdt_setprop(fdt, "/soc", "compatible", soc_compat,
                     sizeof(soc_compat));
    qemu_fdt_setprop_string(fdt, "/soc", "device_type", "soc");
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 1);
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 1);
    qemu_fdt_setprop_cells(fdt, "/soc", "ranges", 0x0, 0xf, 0xfe000000,
                           0x1000000);

    cur_node = "/soc/global-utilities@e0000";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop(fdt, cur_node, "fsl,liodn-bits", NULL, 0);
    qemu_fdt_setprop(fdt, cur_node, "fsl,has-rstcr", NULL, 0);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xe0000, 0xe00);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible", "fsl,qoriq-guts");

    /* /soc/serial@* */
    for (i = 1; i >= 0; i--) {
        const char serial_compat[] = "fsl,ns16550\0ns16550";
        char serial_node[] = "/soc/serial@00000000";

        sprintf(serial_node, "/soc/serial@%8.8x", 0x11c500 + i * 0x100);
        qemu_fdt_add_subnode(fdt, serial_node);
        qemu_fdt_setprop_cells(fdt, serial_node, "interrupts", 0x24,
                               pic_phandle, 0, 0);
        qemu_fdt_setprop_cells(fdt, serial_node, "reg", 0x11c500 + i * 0x100,
                               0x100);
        qemu_fdt_setprop(fdt, serial_node, "compatible", serial_compat,
                         sizeof(serial_compat));
    }

    cur_node = "/soc/pic@40000";
    qemu_fdt_add_subnode(fdt, cur_node);
    const char pic_compat[] = "fsl,mpic\0chrp,open-pic";
    qemu_fdt_setprop_cell(fdt, cur_node, "phandle", pic_phandle);
    qemu_fdt_setprop_cell(fdt, cur_node, "linux,phandle", pic_phandle);
    qemu_fdt_setprop_string(fdt, cur_node, "device_type", "open-pic");
    qemu_fdt_setprop(fdt, cur_node, "compatible", pic_compat,
                     sizeof(pic_compat));
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0x40000, 0x40000);
    qemu_fdt_setprop_cell(fdt, cur_node, "#interrupt-cells", 0x4);
    qemu_fdt_setprop_cell(fdt, cur_node, "#address-cells", 0x0);
    qemu_fdt_setprop(fdt, cur_node, "interrupt-controller", NULL, 0);

    cur_node = "/soc/l2-cache-controller";
    qemu_fdt_add_subnode(fdt, cur_node);
    qemu_fdt_setprop_cell(fdt, cur_node, "cache-size", 0x200000);
    qemu_fdt_setprop_cell(fdt, cur_node, "cache-line-size", 0x40);
    qemu_fdt_setprop_cells(fdt, cur_node, "reg", 0xc20000, 0x40000);
    qemu_fdt_setprop_string(fdt, cur_node, "compatible",
                            "fsl,corenet-l2-cache-controller");

    /* /chosen */
    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
                            "memac(0,0)host:vxWorks"
                            " h=10.10.0.1 e=10.10.0.2:ffffff00 g=10.10.0.1"
                            " u=vxworks pw=vxworks f=0x0");

    /* /booke-timer@0 */
    qemu_fdt_add_subnode(fdt, "/timer@0");
    /* Miss the clock field.. */
    qemu_fdt_setprop_string(fdt, "/timer@0", "compatible",
                            "fsl,booke-timer");
    qemu_fdt_setprop_cells(fdt, "/timer@0", "reg", 0, 0, 0, 0);

    /* /cpus */
    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 1);

    for (i = ms->smp.cpus - 1; i >= 0; i--) {
        char cpu_node[] = "/cpus/cpu@xx";
        sprintf(cpu_node, "/cpus/cpu@%d", i);

        qemu_fdt_add_subnode(fdt, cpu_node);
        qemu_fdt_setprop_cell(fdt, cpu_node, "d-cache-line-size", 0x40);
        qemu_fdt_setprop_cell(fdt, cpu_node, "i-cache-line-size", 0x40);
        qemu_fdt_setprop_cell(fdt, cpu_node, "d-cache-size", 0x8000);
        qemu_fdt_setprop_cell(fdt, cpu_node, "i-cache-size", 0x8000);
        qemu_fdt_setprop_cells(fdt, cpu_node, "reg", i * 2, i * 2 + 1);
        qemu_fdt_setprop_string(fdt, cpu_node, "device_type", "cpu");
    }

    return fdt;
}

/*
 * The DTB need to be patched in some case because u-boot does that before
 * jumping in the vxworks kernel.
 */
static void t2080rdb_compute_dtb(MachineState *ms, uint64_t load_addr,
                                 int *size)
{
    void *fdt;
    Error *err = NULL;
    uint32_t acells, scells;
    int i;

    if (!ms->dtb) {
        /* Create the dtb */
        fdt = t2080rdb_create_dtb(ms, load_addr, size);
    } else {
        /* Load it */
        fdt = load_device_tree(ms->dtb, size);
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
                                    scells, ms->ram_size) < 0) {
        fprintf(stderr, "Can't set memory prop\n");
        exit(1);
    }

    /* Drop the CPUs nodes.  */
    for (i = T2080_MAX_CPUS - 1; i >= ms->smp.cpus; i--) {
        char node_name[14];

        snprintf(node_name, 13, "/cpus/cpu@%d", i);
        if (fdt_path_offset(fdt, node_name) >= 0) {
            qemu_fdt_nop_node(fdt, node_name);
        }
    }

    qemu_fdt_dumpdtb(fdt, *size);
    rom_add_blob_fixed("dtb", fdt, *size, load_addr);
    g_free(fdt);
}

static void t2080rdb_mmu_init(CPUPPCState *env)
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

static void t2080rdb_reset(void *opaque)
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

    t2080rdb_mmu_init(env);
}

static qemu_irq *t2080rdb_init_mpic(MachineState *ms, MemoryRegion *ccsr,
                                    qemu_irq **irqs)
{
    qemu_irq     *mpic;
    DeviceState  *dev = NULL;
    SysBusDevice *s;
    int           i, j, k;

    mpic = g_new(qemu_irq, 256);

    dev = qdev_new(TYPE_OPENPIC);
    qdev_prop_set_uint32(dev, "model", OPENPIC_MODEL_FSL_MPIC_20);
    qdev_prop_set_uint32(dev, "nb_cpus", 1);

    s = SYS_BUS_DEVICE(dev);
    sysbus_realize_and_unref(s, &error_fatal);

    k = 0;
    for (i = 0; i < ms->smp.cpus; i++) {
        for (j = 0; j < OPENPIC_OUTPUT_NB; j++) {
            sysbus_connect_irq(s, k++, irqs[i][j]);
        }
    }

    for (i = 0; i < 256; i++) {
        mpic[i] = qdev_get_gpio_in(dev, i);
    }

    memory_region_add_subregion(ccsr, T2080RDB_MPIC_BASE, s->mmio[0].memory);
    return mpic;
}

static void t2080rdb_init(MachineState *machine)
{
    int i, bios_size, dtb_size;
    CPUPPCState *env = NULL, *firstenv;
    struct epapr_data *epapr = NULL;
    hwaddr loadaddr;
    PowerPCCPU *cpu = NULL;
    MemoryRegion *ram, *flash, *params, *iomem, *dcfg;
    DeviceState *dev;
    const char *kernel_cmdline;
    qemu_irq *mpic, **irqs = NULL;
    MemoryRegion *ccsr_space;

    irqs = g_malloc0(machine->smp.cpus * sizeof(qemu_irq *));
    irqs[0] = g_malloc0(machine->smp.cpus * sizeof(qemu_irq)
                        * OPENPIC_OUTPUT_NB);

    if (strcmp(machine->cpu_type, POWERPC_CPU_TYPE_NAME("e6500"))) {
        fprintf(stderr, "Only e6500 cpu is supported here\n");
        exit(1);
    }

    if (machine->smp.cpus > 1) {
        fprintf(stderr, "Only 1 cpu is supported\n");
        exit(1);
    }

    for (i = 0; i < machine->smp.cpus; i++) {
        qemu_irq *input;

        cpu = POWERPC_CPU(cpu_create(machine->cpu_type));

        if (cpu == NULL) {
            fprintf(stderr, "Unable to initialize CPU!\n");
            exit(1);
        }
        env = &cpu->env;

        if (!firstenv) {
            firstenv = env;
        }

        irqs[i] = irqs[0] + (i * OPENPIC_OUTPUT_NB);
        input = (qemu_irq *)env->irq_inputs;
        irqs[i][OPENPIC_OUTPUT_INT] = input[PPCE500_INPUT_INT];
        irqs[i][OPENPIC_OUTPUT_CINT] = input[PPCE500_INPUT_CINT];
        env->spr[SPR_BOOKE_PIR] = CPU(cpu)->cpu_index = i;
        env->mpic_iack = T2080RDB_MPIC_BASE + T2080RDB_CCSRBAR_BASE + 0xa0;

        ppc_booke_timers_init(cpu, BOOKE_TIMER_FREQ, PPC_TIMER_E500);
    }

    qemu_register_reset(t2080rdb_reset, cpu);

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

    /* HostFS */
    hostfs_create(HOSTFS_START, get_system_memory());

    /* Peripheral Here */

    /* Configuration, Control, and Status Registers */
    ccsr_space = g_malloc0(sizeof(*ccsr_space));
    memory_region_init(ccsr_space, NULL, "CCSRBAR", 0x1000000);
    memory_region_add_subregion(get_system_memory(), T2080RDB_CCSRBAR_BASE,
                                ccsr_space);

    /* DCFG */
    dcfg = g_new(MemoryRegion, 1);
    memory_region_init_io(dcfg, NULL, &device_conf_ops, NULL, "dcfg",
                          T2080RDB_DEVICE_CONF_SIZE);
    memory_region_add_subregion(ccsr_space, T2080RDB_DEVICE_CONF_BASE, dcfg);

    /* FMAN */
    dev = DEVICE(object_new("fman_v3"));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    iomem = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_add_subregion(ccsr_space, T2080RDB_FMAN_BASE, iomem);

    /* MPIC */
    mpic = t2080rdb_init_mpic(machine, ccsr_space, irqs);

    if (!mpic) {
        cpu_abort(CPU(env), "MPIC failed to initialize\n");
    }

    serial_mm_init(ccsr_space, T2080RDB_SERIAL0_BASE, 0,
                   mpic[T2080RDB_SERIAL_IRQ], 115200, serial_hd(0),
                   DEVICE_BIG_ENDIAN);
    serial_mm_init(ccsr_space, T2080RDB_SERIAL1_BASE, 0,
                   mpic[T2080RDB_SERIAL_IRQ], 115200, serial_hd(1),
                   DEVICE_BIG_ENDIAN);

    epapr = g_new(struct epapr_data, 1);
    if (machine->kernel_filename) {
        bios_size = load_elf(machine->kernel_filename, NULL, NULL, NULL,
                             &epapr->entry, &loadaddr, NULL, NULL,
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
    t2080rdb_compute_dtb(machine, epapr->dtb_entry_ea, &dtb_size);
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

static void t2080rdb_machine_init(MachineClass *mc)
{
    mc->desc = "t2080rdb";
    mc->init = t2080rdb_init;
    mc->max_cpus = T2080_MAX_CPUS;
    mc->default_ram_size = 128ULL * 1024 * 1024;
    mc->default_cpu_type = POWERPC_CPU_TYPE_NAME("e6500");
}

DEFINE_MACHINE("t2080rdb", t2080rdb_machine_init)
