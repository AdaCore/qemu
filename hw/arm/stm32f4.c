
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/arm.h"
#include "hw/devices.h"
#include "qemu/timer.h"
#include "hw/i2c/i2c.h"
#include "net/net.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/char/pl011.h"
#include "hw/misc/unimp.h"
#include "cpu.h"
#include "hw/arm/armv7m.h"
#include "hw/adacore/hostfs.h"
#include "hw/adacore/gnat-bus.h"

#define FLASH_OFFSET        (0x00000000)
#define FLASH_MIRROR_OFFSET (0x08000000)
#define CCM_OFFSET          (0x10000000)
#define SRAM_OFFSET         (0x20000000)
#define PWR_OFFSET          (0x40007000)
#define UART_OFFSET         (0x40011000)
#define RCC_OFFSET          (0x40023800)

#define FLASH_SIZE (1024 * 1024)
#define CCM_SIZE   (64 * 1024)
#define SRAM_SIZE  (192 * 1024)

static void do_sys_reset(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

static void stm32_init(MachineState *args)
{
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *flash_mirror = g_new(MemoryRegion, 1);
    MemoryRegion *ccm = g_new(MemoryRegion, 1);
    DeviceState *nvic;
    const int num_irq = 128;
    qemu_irq *pic = g_new(qemu_irq, num_irq);
    int i;
    DeviceState  *dev;

    if (args->cpu_type == NULL) {
        args->cpu_type = "cortex-m4f";
    }

    system_clock_scale = 6;

    /* Flash programming is done via the SCU, so pretend it is ROM.  */
    memory_region_init_ram(flash, NULL, "stm32f4.flash", FLASH_SIZE,
                           &error_abort);
    memory_region_set_readonly(flash, true);
    memory_region_add_subregion(system_memory, FLASH_OFFSET, flash);

    memory_region_init_alias(flash_mirror, NULL, "stm32f4.flash_mirror",
                             flash, 0, FLASH_SIZE);
    memory_region_add_subregion(system_memory, FLASH_MIRROR_OFFSET,
                                flash_mirror);

    memory_region_init_ram(sram, NULL, "stm32f4.sram", SRAM_SIZE,
                           &error_abort);
    memory_region_add_subregion(system_memory, SRAM_OFFSET, sram);

    memory_region_init_ram(ccm, NULL, "stm32f4.ccm", CCM_SIZE,
                           &error_abort);
    memory_region_add_subregion(system_memory, CCM_OFFSET, ccm);

    nvic = qdev_create(NULL, TYPE_ARMV7M);
    qdev_prop_set_uint32(nvic, "num-irq", num_irq);
    qdev_prop_set_string(nvic, "cpu-type", args->cpu_type);
    object_property_set_link(OBJECT(nvic), OBJECT(get_system_memory()),
                             "memory", &error_abort);
    /* This will exit with an error if the user passed us a bad cpu_type */
    qdev_init_nofail(nvic);

    qdev_connect_gpio_out_named(nvic, "SYSRESETREQ", 0,
                                qemu_allocate_irq(&do_sys_reset, NULL, 0));

    for (i = 0; i < num_irq; i++) {
        pic[i] = qdev_get_gpio_in(nvic, i);
    }

    hostfs_create(0x80001000, system_memory);
    /* Initialize the GnatBus Master */
    gnatbus_master_init(pic, num_irq);
    gnatbus_device_init();

    dev = qdev_create(NULL, "stm32_PWR");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, PWR_OFFSET);

    dev = qdev_create(NULL, "stm32_RCC");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, RCC_OFFSET);

    /* UART */
    if (serial_hd(0)) {
        dev = qdev_create(NULL, "stm32_UART");
        qdev_prop_set_chr(dev, "chrdev", serial_hd(0));
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, UART_OFFSET);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(nvic, 7));
    }

    armv7m_load_kernel(ARM_CPU(first_cpu), args->kernel_filename, FLASH_SIZE);
}

static void stm32_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "stm32";
    mc->init = stm32_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m4f");
    /* We may want to implement devices instead of that. */
    mc->ignore_memory_transaction_failures = true;
}

static const TypeInfo stm32_type = {
    .name = MACHINE_TYPE_NAME("stm32"),
    .parent = TYPE_MACHINE,
    .class_init = stm32_class_init,
};

static void stm32f4_machine_init(void)
{
    type_register_static(&stm32_type);
}

type_init(stm32f4_machine_init)
