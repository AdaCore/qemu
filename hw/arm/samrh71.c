/*
 * Arm Microchip samrh71
 *
 * Copyright (c) 2013-2022 AdaCore
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
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/timer.h"
#include "hw/i2c/i2c.h"
#include "net/net.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#include "hw/char/pl011.h"
#include "hw/misc/unimp.h"
#include "cpu.h"
#include "hw/arm/armv7m.h"
#include "hw/adacore/hostfs.h"
#include "hw/adacore/gnat-bus.h"

#define SYSCLK_FRQ     (100000000LL) /* 100 MHz */

#define FLASH_OFFSET   (0x10000000)
#define FLASH_SIZE     (0x20000)
#define FLASH_MIRROR_OFFSET (0)
#define ITCRAM_SIZE    (0x20000)
#define DTCRAM_OFFSET  (0x2000000)
#define DTCRAM_SIZE    (0x40000)
#define FLEXRAM_OFFSET (0x21000000)
#define FLEXRAM_SIZE   (0xc0000)

#define HOSTFS_OFFSET  (0x10020000)

static void sys_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
  /* nothing to do */
}

static uint64_t sys_read(void *opaque, hwaddr addr, unsigned size)
{
    uint32_t         ret = 0;

    switch (addr) {
    case 0x68: /* SR register */
        ret = 0xFFFFFFFF;
        break;
    case 0x24: /* CKGR_MCFR register */
        ret = 0xFFFFFFFF;
        break;
    default:
        break;
    }
    return ret;
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

static void do_sys_reset(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}


static void samrh71_init(MachineState *args)
{
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *dtcram = g_new(MemoryRegion, 1);
    /*
     * ITCRAM is not used in this emulation.
     * The choice is to boot from flash.
     * ITCRAM base address is a shadow
     * of flash in the real board.
     */
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *flash_mirror = g_new(MemoryRegion, 1);
    MemoryRegion *flexram = g_new(MemoryRegion, 1);
    MemoryRegion *sys_io = g_new(MemoryRegion, 1);
    DeviceState *nvic;
    const int num_irq = 128;
    qemu_irq *pic = g_new(qemu_irq, num_irq);
    int i;
    Clock *sysclk;

    if (strcmp(args->cpu_type, ARM_CPU_TYPE_NAME("cortex-m7")) != 0) {
        error_report("This board can only be used with CPU cortex-m7");
        exit(1);
    }


    /* This clock doesn't need migration because it is fixed-frequency */
    sysclk = clock_new(OBJECT(args), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);


    /* Flash programming is done via the SCU, so pretend it is ROM.  */
    memory_region_init_ram(flash, NULL, "samrh71.flash", FLASH_SIZE,
                           &error_abort);
    memory_region_set_readonly(flash, true);
    memory_region_add_subregion(system_memory, FLASH_OFFSET, flash);

    memory_region_init_alias(flash_mirror, NULL, "samrh71.flash_mirror",
                             flash, 0, FLASH_SIZE);
    memory_region_add_subregion(system_memory, FLASH_MIRROR_OFFSET,
                                flash_mirror);


    memory_region_init_ram(dtcram, NULL, "samrh71.dtcram", DTCRAM_SIZE,
                           &error_abort);
    memory_region_add_subregion(system_memory, DTCRAM_OFFSET, dtcram);

    memory_region_init_ram(flexram, NULL, "samrh71.flexram", FLEXRAM_SIZE,
                           &error_abort);
    memory_region_add_subregion(system_memory, FLEXRAM_OFFSET, flexram);

    memory_region_init_io(sys_io, NULL, &sys_ops, NULL,
                          "Primary System Control", 0xFF);
    memory_region_add_subregion(system_memory, 0x4000C000, sys_io);


    nvic = qdev_new(TYPE_ARMV7M);
    qdev_prop_set_uint32(nvic, "num-irq", num_irq);
    qdev_prop_set_string(nvic, "cpu-type", args->cpu_type);
    object_property_set_link(OBJECT(nvic), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    qdev_connect_clock_in(nvic, "cpuclk", sysclk);
    /* This will exit with an error if the user passed us a bad cpu_type */
    sysbus_realize_and_unref(SYS_BUS_DEVICE(nvic), &error_fatal);

    qdev_connect_gpio_out_named(nvic, "SYSRESETREQ", 0,
                                qemu_allocate_irq(&do_sys_reset, NULL, 0));

    for (i = 0; i < num_irq; i++) {
        pic[i] = qdev_get_gpio_in(nvic, i);
    }

    hostfs_create(HOSTFS_OFFSET, system_memory);
    /* Initialize the GnatBus Master */
    gnatbus_master_init(pic, num_irq);
    gnatbus_device_init();

    armv7m_load_kernel(ARM_CPU(first_cpu), args->kernel_filename, FLASH_SIZE);
}

static void samrh71_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "samrh71";
    mc->init = samrh71_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m7");
    /* We may want to implement devices instead of that. */
    mc->ignore_memory_transaction_failures = true;
}

static const TypeInfo samrh71_type = {
    .name = MACHINE_TYPE_NAME("samrh71"),
    .parent = TYPE_MACHINE,
    .class_init = samrh71_class_init,
};

static void samrh71_machine_init(void)
{
    type_register_static(&samrh71_type);
}

type_init(samrh71_machine_init)
