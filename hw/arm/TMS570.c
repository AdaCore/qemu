/*
 * Arm Texas-Instrument TMS570
 *
 * Copyright (c) 2013 AdaCore
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

#include "qemu/osdep.h"
#include "cpu.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/devices.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "exec/memory.h"
#include "sysemu/blockdev.h"
#include "exec/address-spaces.h"
#include "hw/adacore/gnat-bus.h"
#include "hw/adacore/hostfs.h"

/* #define DEBUG_TMS */

#ifdef DEBUG_TMS
#define DPRINTF(fmt, ...) do { printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

static struct arm_boot_info tms570_binfo;

typedef struct {
    uint32_t SSIR[4];
    uint32_t SSI_flag;
    uint32_t SSI_vec;
    qemu_irq SSIR_irq;
} tms570_sys_data;

static const uint32_t SSIR_key[4] = {0x7500, 0x8400, 0x9300, 0xA200};

static  void update_software_interrupt(tms570_sys_data *sys_data)
{

    if (sys_data->SSI_flag != 0) {
        sys_data->SSI_vec = __builtin_ctzll(sys_data->SSI_flag) + 1 ;
        DPRINTF("%s: qemu_irq_raise(sys_data->SSIR_irq);"
                " sys_data->SSI_vec = 0x%x\n", __func__, sys_data->SSI_vec);
        qemu_irq_raise(sys_data->SSIR_irq);
    } else {
        sys_data->SSI_vec = 0;
        DPRINTF("%s: qemu_irq_lower(sys_data->SSIR_irq);\n", __func__);
        qemu_irq_lower(sys_data->SSIR_irq);
    }
}

static void sys_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    tms570_sys_data *sys_data = (tms570_sys_data *)opaque;
    int              offset;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u val:0x%" PRIx64 "\n",
            __func__, addr, size, val);

    switch (addr) {
    case 0xB0 ... 0xBC: /* System Software Interrupt Request n */
        offset = (addr - 0xB0) / 4;

        if ((val & 0xFF00) == SSIR_key[offset]) {
            /* Keep only the SSDATA field */
            sys_data->SSIR[offset] = val & 0xFF;
            sys_data->SSI_flag |= 1 << offset;
            DPRINTF("%s: Trigger software interrupt %d\n", __func__, offset);
            update_software_interrupt(sys_data);
        }
        break;
    case 0xE0: /* System Exception Status Register */
        if (val & 0x8000 || !(val & 0x4000)) {
            DPRINTF("%s: qemu_system_reset_request()\n", __func__);
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    case 0xF8: /* System Software Interrupt Flag */
        sys_data->SSI_flag &= ~val;
        update_software_interrupt(sys_data);
        break;
    default:
        break;
    }
}

static uint64_t sys_read(void *opaque, hwaddr addr, unsigned size)
{
    tms570_sys_data *sys_data = (tms570_sys_data *)opaque;
    int              offset;
    uint32_t         ret;

    DPRINTF("%s: offset:0x"TARGET_FMT_plx" size:%u\n", __func__, addr, size);

    switch (addr) {
    case 0xB0 ... 0xBC: /* System Software Interrupt Request n */
        offset = (addr - 0xB0) / 4;
        return sys_data->SSIR[offset];
    case 0xF4: /* System Software Interrupt Vector */
        ret = sys_data->SSI_vec;
        sys_data->SSI_flag &= ~(1 << (ret - 1));
        update_software_interrupt(sys_data);
        return ret;
        break;
    case 0xF8: /* System Software Interrupt Flag */
        return sys_data->SSI_flag;
        break;
    default:
        break;
    }
    return 0;
}

static const MemoryRegionOps sys_ops = {
    .read = sys_read,
    .write = sys_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void tms570_reset(void *opaque)
{
    tms570_sys_data *sys_data = (tms570_sys_data *)opaque;

    sys_data->SSI_flag = 0;
    sys_data->SSI_vec  = 0;
    sys_data->SSIR[0]  = 0;
    sys_data->SSIR[1]  = 0;
    sys_data->SSIR[2]  = 0;
    sys_data->SSIR[3]  = 0;
    qemu_irq_lower(sys_data->SSIR_irq);
}

static void tms570_init(MachineState *args)
{
    Object *cpuobj;
    ARMCPU *cpu;
    MemoryRegion    *sysmem   = get_system_memory();
    MemoryRegion    *ram      = g_new(MemoryRegion, 1);
    MemoryRegion    *ram2     = g_new(MemoryRegion, 1);
    MemoryRegion    *vim_ram  = g_new(MemoryRegion, 1);
    MemoryRegion    *sys_io   = g_new(MemoryRegion, 1);
    DeviceState     *dev;
    tms570_sys_data *sys_data = g_new(tms570_sys_data, 1);;
    int              n;
    qemu_irq         pic[64];

    cpuobj = object_new(args->cpu_type);
    object_property_set_bool(cpuobj, true, "realized", &error_fatal);

    cpu = ARM_CPU(cpuobj);
    if (cpu == NULL) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    qemu_register_reset(tms570_reset, sys_data);

    dev = sysbus_create_varargs("VIM", 0xFFFFFE00,
                                qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ),
                                qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_FIQ),
                                NULL);
    for (n = 0; n < 64; n++) {
        pic[n] = qdev_get_gpio_in(dev, n);
    }

    memory_region_init_ram(ram, NULL, "TMS570.ram", ram_size,
                           &error_abort);
    memory_region_add_subregion(sysmem, 0x08000000, ram);

    /* Put RAM instead of flash */
    memory_region_init_ram(ram2, NULL, "TMS570.ram2", ram_size,
                           &error_abort);
    memory_region_add_subregion(sysmem, 0x0, ram2);

    /* VIM RAM */
    memory_region_init_ram(vim_ram, NULL, "VIM.ram", 0x1000,
                           &error_abort);
    memory_region_add_subregion(sysmem, 0xFFF82000 , vim_ram);

    /* RTI */
    dev = qdev_create(NULL, "RTI");
    qdev_prop_set_uint32(dev, "freq", 180000000), /* 180 MHz */
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xFFFFFC00);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, pic[2]); /* COMPARE0 */
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, pic[3]); /* COMPARE1 */
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 2, pic[4]); /* COMPARE2 */
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 3, pic[5]); /* COMPARE3 */
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 4, pic[6]); /* OVERFLOW0 */
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 5, pic[7]); /* OVERFLOW1 */

    /* Primary System Control */
    memory_region_init_io(sys_io, NULL, &sys_ops, sys_data,
                          "Primary System Control", 0xFF);
    memory_region_add_subregion(sysmem, 0xFFFFFF00, sys_io);
    sys_data->SSIR_irq = pic[21];

    if (serial_hd(0)) {
        dev = qdev_create(NULL, "SCI");
        qdev_prop_set_chr(dev, "chrdev", serial_hd(0));
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xFFF7E400);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, pic[13]);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, pic[27]);
    }

    if (serial_hd(1)) {
        dev = qdev_create(NULL, "SCI");
        qdev_prop_set_chr(dev, "chrdev", serial_hd(1));
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0xFFF7E500);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, pic[13]);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, pic[27]);
    }

    /* Initialize the GnatBus Master */
    gnatbus_master_init(pic, 64);
    gnatbus_device_init();

    /* HostFS */
    hostfs_create(0x80001000, sysmem);

    tms570_binfo.ram_size = ram_size;
    tms570_binfo.kernel_filename = args->kernel_filename;
    tms570_binfo.kernel_cmdline = args->kernel_cmdline;
    tms570_binfo.initrd_filename = args->initrd_filename;
    tms570_binfo.board_id = 0x0;
    arm_load_kernel(cpu, &tms570_binfo);
}

static void tms570_generic_machine_init(MachineClass *mc)
{
    mc->desc = "ARM Texas-Instrument TMS570";
    mc->max_cpus = 1;
    mc->init = tms570_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-r5f");
}

DEFINE_MACHINE("TMS570", tms570_generic_machine_init)
