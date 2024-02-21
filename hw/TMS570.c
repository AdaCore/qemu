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

#include "sysbus.h"
#include "arm-misc.h"
#include "devices.h"
#include "sysemu.h"
#include "boards.h"
#include "exec-memory.h"
#include "blockdev.h"
#include "qemu-plugin.h"
#include "gnat-bus.h"

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
        /*
         * Update SSI_vec according to the SSIR value corresponding
         * of the highest priority; SSIR1 being the highest and SSIR4
         * the lowest.
         */
        int ssirn = __builtin_ctzll(sys_data->SSI_flag);
        sys_data->SSI_vec = (sys_data->SSIR[ssirn] << 8) + ssirn + 1;
        DPRINTF("%s: qemu_irq_raise(sys_data->SSIR_irq);"
                " sys_data->SSI_vec = 0x%x\n", __func__, sys_data->SSI_vec);
        qemu_irq_raise(sys_data->SSIR_irq);
    } else {
        sys_data->SSI_vec = 0;
        DPRINTF("%s: qemu_irq_lower(sys_data->SSIR_irq);\n", __func__);
        qemu_irq_lower(sys_data->SSIR_irq);
    }
}

static void sys_write(void *opaque, target_phys_addr_t addr,
                                uint64_t val, unsigned size)
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
            qemu_system_reset_request();

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

static uint64_t sys_read(void *opaque, target_phys_addr_t addr,
                           unsigned size)
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
        /* Clean SSI_flag cooresponding to the current SSIRn  */
        sys_data->SSI_flag &= ~(1 << ((ret & 0xFF) - 1));
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

static void tms570_init(ram_addr_t ram_size,
                        const char *boot_device,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *cpu_model)
{
    CPUARMState     *env      = NULL;
    MemoryRegion    *sysmem   = get_system_memory();
    MemoryRegion    *ram      = g_new(MemoryRegion, 1);
    MemoryRegion    *ram2     = g_new(MemoryRegion, 1);
    MemoryRegion    *vim_ram  = g_new(MemoryRegion, 1);
    MemoryRegion    *sys_io   = g_new(MemoryRegion, 1);
    DeviceState     *dev;
    tms570_sys_data *sys_data = g_new(tms570_sys_data, 1);;
    qemu_irq        *cpu_pic;
    int              n;
    qemu_irq         pic[64];

    if (!cpu_model) {
        cpu_model = "cortex-a9";
    }

    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }
    cpu_pic = arm_pic_init_cpu(env);

    qemu_register_reset(tms570_reset, sys_data);

    dev = sysbus_create_varargs("VIM", 0xFFFFFE00, cpu_pic[ARM_PIC_CPU_IRQ],
                                cpu_pic[ARM_PIC_CPU_FIQ], NULL);
    for (n = 0; n < 64; n++) {
        pic[n] = qdev_get_gpio_in(dev, n);
    }

    memory_region_init_ram(ram, "TMS570.ram", ram_size);
    vmstate_register_ram_global(ram);
    memory_region_add_subregion(sysmem, 0x08000000, ram);

    /* Put RAM instead of flash */
    memory_region_init_ram(ram2, "TMS570.ram2", ram_size);
    vmstate_register_ram_global(ram2);
    memory_region_add_subregion(sysmem, 0x0, ram2);

    /* VIM RAM */
    memory_region_init_ram(vim_ram, "VIM.ram", 0x1000);
    vmstate_register_ram_global(vim_ram);
    memory_region_add_subregion(sysmem, 0xFFF82000 , vim_ram);

    /* RTI */
    dev = qdev_create(NULL, "RTI");
    qdev_prop_set_uint32(dev, "freq", 180000000), /* 180 MHz */
    qdev_init_nofail(dev);
    sysbus_mmio_map(sysbus_from_qdev(dev), 0, 0xFFFFFC00);
    sysbus_connect_irq(sysbus_from_qdev(dev), 0, pic[2]); /* COMPARE0 */
    sysbus_connect_irq(sysbus_from_qdev(dev), 1, pic[3]); /* COMPARE1 */
    sysbus_connect_irq(sysbus_from_qdev(dev), 2, pic[4]); /* COMPARE2 */
    sysbus_connect_irq(sysbus_from_qdev(dev), 3, pic[5]); /* COMPARE3 */
    sysbus_connect_irq(sysbus_from_qdev(dev), 4, pic[6]); /* OVERFLOW0 */
    sysbus_connect_irq(sysbus_from_qdev(dev), 5, pic[7]); /* OVERFLOW1 */

    /* Primary System Control */
    memory_region_init_io(sys_io, &sys_ops, sys_data, "Primary System Control",
                          0xFF);
    memory_region_add_subregion(sysmem, 0xFFFFFF00, sys_io);
    sys_data->SSIR_irq = pic[21];

    if (serial_hds[0]) {
        dev = qdev_create(NULL, "SCI");
        qdev_prop_set_chr(dev, "chrdev", serial_hds[0]);
        qdev_init_nofail(dev);
        sysbus_mmio_map(sysbus_from_qdev(dev), 0, 0xFFF7E400);
        sysbus_connect_irq(sysbus_from_qdev(dev), 0, pic[13]);
        sysbus_connect_irq(sysbus_from_qdev(dev), 1, pic[27]);
    }

    if (serial_hds[1]) {
        dev = qdev_create(NULL, "SCI");
        qdev_prop_set_chr(dev, "chrdev", serial_hds[1]);
        qdev_init_nofail(dev);
        sysbus_mmio_map(sysbus_from_qdev(dev), 0, 0xFFF7E500);
        sysbus_connect_irq(sysbus_from_qdev(dev), 0, pic[13]);
        sysbus_connect_irq(sysbus_from_qdev(dev), 1, pic[27]);
    }

    /* Initialize plug-ins */
    plugin_init(pic, 64);
    plugin_device_init();

    /* Initialize the GnatBus Master */
    gnatbus_master_init(pic, 64);
    gnatbus_device_init();


    tms570_binfo.ram_size = ram_size;
    tms570_binfo.kernel_filename = kernel_filename;
    tms570_binfo.kernel_cmdline = kernel_cmdline;
    tms570_binfo.initrd_filename = initrd_filename;
    tms570_binfo.board_id = 0x0;
    arm_load_kernel(env, &tms570_binfo);
}

static QEMUMachine tms570_machine = {
    .name = "TMS570",
    .desc = "ARM Texas-Instrument TMS570",
    .init = tms570_init,
    .use_scsi = 1,
    .max_cpus = 1,
};

static void tms570_machine_init(void)
{
    qemu_register_machine(&tms570_machine);
}

machine_init(tms570_machine_init);
