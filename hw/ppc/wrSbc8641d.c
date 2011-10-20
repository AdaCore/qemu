/*
 * QEMU PPC SMP Hardware System Emulator
 *
 * Copyright (c) 2011 AdaCore
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
#include "cpu.h"
#include "hw/hw.h"
#include "hw/block/fdc.h"
#include "net/net.h"
#include "sysemu/sysemu.h"
#include "hw/isa/isa.h"
#include "hw/pci/pci.h"
#include "hw/ppc/ppc.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "hw/ide.h"
#include "hw/loader.h"
#include "hw/timer/mc146818rtc.h"
#include "sysemu/blockdev.h"
#include "elf.h"
#include "hw/ppc/openpic.h"
#include "hw/net/fsl_etsec/etsec.h"
#include "exec/memory.h"
#include "hw/char/serial.h"

#include "hw/adacore/gnat-bus.h"
#include "qemu-traces.h"
#include "hw/adacore/hostfs.h"

//#define DEBUG_WRSBC8641D

#ifdef DEBUG_WRSBC8641D
#define DPRINTF(fmt, ...) do { \
    printf(fmt, ## __VA_ARGS__); \
} while(0)
#else
#define DPRINTF(fmt, ...) do { } while(0)
#endif

#define MAX_CPUS        2

#define BIOS_SIZE       (32 * 1024 * 1024)
#define BIOS_FILENAME   "ppc_rom.bin"
#define KERNEL_LOAD_ADDR        0x01000000
#define INITRD_LOAD_ADDR        0x01800000

/* Memory Mapping of Configuration Registers */
#define CCSBAR          0xf8000000
#define MCM_START       (CCSBAR + 0x01000)
#define MCM_SIZE        0x00001000
#define SERIAL_START    (CCSBAR + 0x04500)
#define ETSEC_START     (CCSBAR + 0x24000)
#define PIC_START       (CCSBAR + 0x40000)
#define GUR_START       (CCSBAR + 0xe0000)
#define GUR_SIZE        0x1000
#define PMR_START       (CCSBAR + 0xe1000)
#define PMR_SIZE        0xa9

#define PARAMS_ADDR     (CCSBAR + 0x80000)
#define PARAMS_SIZE     (0x01000)
#define QTRACE_START    (CCSBAR + 0x81000)
#define QTRACE_SIZE     (0x01000)
#define HOSTFS_START    (CCSBAR + 0x82000)

/* Board specific configuration registers */
#define PIXIS_START     0xf8100000
#define PIXIS_SIZE      0x19

#define	FREQ_33_MHZ	 33330000
#define	FREQ_40_MHZ	 40000000
#define	FREQ_50_MHZ	 50000000
#define	FREQ_66_MHZ	 66670000
#define	FREQ_83_MHZ	 83330000
#define	FREQ_100_MHZ	100000000
#define	FREQ_133_MHZ	133330000
#define	FREQ_166_MHZ	166660000
#define	FREQ_266_MHZ	266660000
#define	FREQ_200_MHZ	200000000
#define	FREQ_333_MHZ	333330000
#define	FREQ_533_MHZ	533330000
#define	FREQ_400_MHZ	400000000

uint32_t pixisSpdTable[8] =
{
    FREQ_33_MHZ,
    FREQ_40_MHZ,
    FREQ_50_MHZ,
    FREQ_66_MHZ,
    FREQ_83_MHZ,
    FREQ_100_MHZ,
    FREQ_133_MHZ,
    FREQ_166_MHZ
};
/* This defines the oscillator frequency value. Choose an index in the
 * pixisSpdTable array that corresponds to the wanted value */
#define OSC_FREQ_INDEX  5

/* This is one of the PLL frequency multipliers ratio. This one influences the
 * core's decrementer and will be used to calculate the timebase */
#define MPX_TO_OSC_RATIO     2

/* We need to keep track of the CPUs so that we can start/halt/reset them when
 * needed */
static PowerPCCPU *ppc_cpus[] = {NULL, NULL};

typedef struct ResetData {
    PowerPCCPU *cpu;
    uint32_t    entry;          /* save kernel entry in case of reset */
} ResetData;

static void write_mcm(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    CPUState *cs = CPU(ppc_cpus[1]);

    switch (addr & 0xFFF) {
    case 0x010:
        DPRINTF("%s: Writing to MCM PCR <= 0x%08x\n", __func__, value);
        if (value & (1 << 25)) {
            if (cs != NULL) {
                DPRINTF("%s: Enabling Core 1\n", __func__);
                cs->halted = 0;
            } else {
                DPRINTF("%s: Core 1 is not installed on the board\n", __func__);
            }
        }
        break;
    default:
        DPRINTF("%s: Writing to unassigned : 0x%08x <= 0x%x\n", __func__, addr
                + MCM_START, value);
    }
}

static uint64_t read_mcm(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t ret;

    switch (addr & 0xFFF) {
    case 0x010:
        ret = (!(ppc_cpus[0] != NULL ? CPU(ppc_cpus[0])->halted : 0) << 24) |
              (!(ppc_cpus[1] != NULL ? CPU(ppc_cpus[1])->halted : 0) << 25);
        break;
    default:
        DPRINTF("%s: Reading from unassigned : 0x%08x\n", __func__, addr +
                MCM_START);
        return 0;
    }
    DPRINTF("%s: Reading from 0x%08x => 0x%08x\n", __func__, addr + MCM_START,
            ret);
    return ret;
}

static const MemoryRegionOps mcm_ops = {
    .read = read_mcm,
    .write = write_mcm,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* QEMUBH is used here to reset the board as the end of the translation block
 * might be executed after qemu_system_reset_request(..). So let the IO thread
 * kick off the CPU thread in order to execute the reset request.
 *
 * Code example:
 *
 * _exit:
 *   write 2 @GUR_BASE + 0xb0
 * loop:
 *   branch loop 
 */
static QEMUBH *board_reset_bh = NULL;
static void board_reset_cb(void *opaque)
{
    qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
}

static void write_gur(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    DPRINTF("write_gur : write to Global Utility Register : 0x%08x\n",
            addr +
            GUR_START);

    switch (addr & 0xfff) {
    case 0xb0: /* Reset Control Register - RSTCR */
        DPRINTF("%s: Writing RSTCR\n", __func__);
        if (value & 2) {
            qemu_bh_schedule(board_reset_bh);
        }
        break;
    default:
        DPRINTF("%s: writing non implemented register 0x%8x\n", __func__,
                GUR_START + addr);
    }
}

static uint64_t read_gur(void *opaque, hwaddr addr, unsigned size)
{
    /* DPRINTF("read_gur : reading from Global Utility Register : 0x%08x\n",
            addr +
            GUR_START); */
    uint64_t ret;
    CPUPPCState *env = &(ppc_cpus[0]->env);

    switch (addr & 0xfff) {
    case 0:
        /* PORPLL
         * Set (MPX bus CLK)/SYSCLK ratio to MPX_TO_OSC_RATIO, and (core
         * CLK)/(MPX bus CLK) to 2
         * The PPC decrementer always runs at (MPX bus CLK)/4 so only the first
         * ratio influences the timebase frequency to be used.
         * The timebase emulation frequency is calculated based on the frequency
         * reported by the PIXIS registers, and the MPX_TO_OSC_RATIO ratio. */
        ret = 0x00100000;
        ret |= MPX_TO_OSC_RATIO << 1;
        DPRINTF("%s: reading POR_PLL => 0x%08x\n", __func__, ret);
        break;
    case 0x0c: /* PORDEVSR[CORE1TE] */
        ret = 0;
        break;
    case 0xa0: /* Processor Version Register - PVR */
        DPRINTF("%s: Reading PVR register\n", __func__);
        ret = env->spr[SPR_PVR];
        break;
    case 0xa4: /* System Version Register - SVR */
        DPRINTF("%s: Reading SVR register\n", __func__);
        ret = env->spr[SPR_SVR];
        break;
    default:
        DPRINTF("%s: Reading non implemented register 0x%8x\n", __func__,
                GUR_START + addr);
        return 0;
    }
    return ret;
}

static const MemoryRegionOps gur_ops = {
    .read = read_gur,
    .write = write_gur,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void write_qtrace(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    switch (addr & 0xfff) {
    case 0x00:
        exec_trace_special(TRACE_SPECIAL_LOADADDR, value);
        break;
    default:
        DPRINTF("%s: writing non implemented register 0x%8x\n", __func__,
                QTRACE_START + addr);
    }
}

static const MemoryRegionOps qtrace_ops = {
    .write = write_qtrace,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void main_cpu_reset(void *opaque)
{
    ResetData    *s   = (ResetData *)opaque;
    PowerPCCPU *cpu = s->cpu;
    CPUState *cs = CPU(cpu);
    CPUPPCState *env = &cpu->env;

    cpu_reset(cs);
    env->nip = s->entry;
    cs->halted = 0;
    cpu->env.spr[SPR_MSSCR0] |= (0 << 5);
}

static void secondary_cpu_reset(void *opaque)
{
    PowerPCCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);

    cpu_reset(cs);

    /* Secondary CPU starts in halted state for now. Needs to change when
       implementing non-kernel boot. */
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
    cpu->env.spr[SPR_MSSCR0] |= (1 << 5);
}

static uint64_t read_pixis(void *opaque, hwaddr addr, unsigned size)
{
    /* DPRINTF("read_pixis : reading from PIXIS : "
            "0x%08x\n",
            addr +
            PIXIS_START); */
    uint64_t ret;
    switch (addr & 0xff) {
    case 0x10: /* PIXIS_VCTL */
        /* returning 1 makes VxWorks read the default oscillator frequency
         * (50MHz) instead of a configured value */
        DPRINTF("%s: Reading PIXIS_VCTL\n", __func__);
        ret = 0;
        break;
    case 0x7: /* PIXIS_OSC */
        /* an index corresponding to the oscillator frequency */
        DPRINTF("%s: Reading PIXIS_OSC which is set to %.2eHz\n", __func__,
                (double)pixisSpdTable[OSC_FREQ_INDEX]);
        ret = OSC_FREQ_INDEX;
        break;
    default:
        DPRINTF("%s: Reading unassigned at 0x%08x\n", __func__, addr +
                PIXIS_START);
        return 0;
    }
    DPRINTF("%s: Read => 0x%08x\n", __func__, ret);
    return ret;
}

static void write_pixis(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    /* DPRINTF("read_pixis : reading from PIXIS : "
            "0x%08x\n",
            addr +
            PIXIS_START); */
    switch (addr & 0xff) {
    case 0x04: /* PIXIS_RST : Reset the whole system */
        DPRINTF("%s: Write to PIXIS_RST => reset system\n", __func__);
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        break;
    default:
        DPRINTF("%s: Writing to unassigned PIXIS register at 0x%08x\n",
                __func__, addr + PIXIS_START);
    }
}

static const MemoryRegionOps pixis_ops = {
    .read = read_pixis,
    .write = write_pixis,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static qemu_irq *init_mpic(MemoryRegion *ccsr, qemu_irq **irqs)
{
    qemu_irq     *mpic;
    DeviceState  *dev = NULL;
    SysBusDevice *s;
    int           i, j, k;

    mpic = g_new(qemu_irq, 256);

    dev = qdev_create(NULL, TYPE_OPENPIC);
    qdev_prop_set_uint32(dev, "model", OPENPIC_MODEL_FSL_MPIC_20);
    qdev_prop_set_uint32(dev, "nb_cpus", smp_cpus);

    qdev_init_nofail(dev);
    s = SYS_BUS_DEVICE(dev);

    k = 0;
    for (i = 0; i < smp_cpus; i++) {
        for (j = 0; j < OPENPIC_OUTPUT_NB; j++) {
            sysbus_connect_irq(s, k++, irqs[i][j]);
        }
    }

    for (i = 0; i < 256; i++) {
        mpic[i] = qdev_get_gpio_in(dev, i);
    }

    memory_region_add_subregion(ccsr, PIC_START, s->mmio[0].memory);

    return mpic;
}

static void wrsbc8641d_init(MachineState *args)
{
    MemoryRegion  *ram  = g_new(MemoryRegion, 1);
    MemoryRegion  *bios = g_new(MemoryRegion, 1);
    MemoryRegion  *params = g_new(MemoryRegion, 1);
    char          *filename;
    int            linux_boot, i, bios_size;
    uint32_t       kernel_base, initrd_base;
    long           kernel_size, initrd_size;
    ResetData     *reset_info;
    uint64_t       elf_entry;

    linux_boot = (args->kernel_filename != NULL);

    /* Reset data */
    reset_info = g_malloc0(sizeof(ResetData));

    /* init CPUs */
    if (args->cpu_type == NULL) {
        args->cpu_type = "mpc8641d-powerpc-cpu";
    }

    for (i = 0; i < smp_cpus; i++) {
        ppc_cpus[i] = POWERPC_CPU(cpu_create(args->cpu_type));

        if (ppc_cpus[i] == NULL) {
            fprintf(stderr, "Unable to find PowerPC CPU definition\n");
            exit(1);
        }

        if (ppc_cpus[i]->env.flags & POWERPC_FLAG_RTC_CLK) {
            /* POWER / PowerPC 601 RTC clock frequency is 7.8125 MHz */
            cpu_ppc_tb_init(&ppc_cpus[i]->env, 7812500UL);
            printf("%s: Requesting RTC clock while ePIC timers are not"
                   "implemented yet. Aborting.\n",__func__);
            exit(-1);
        } else {
            /* Set time-base frequency
             * The timebase frequency corresponds directly to the core's
             * decrementer frequency. The decrementer always runs at (MPX bus
             * clk)/4 hence the formula used below :
             * SYSCLK * MPX_TO_OSC_RATIO / 4
             * SYSCLK is the frequency reported by the PIXIS registers. Its
             * value is taken from the pixisSpdTable array.
             * See MPC8641D ref man section 4.4.4.1 and e600 ref man section
             * 4.6.9 */
            uint32_t timebase = pixisSpdTable[OSC_FREQ_INDEX] * MPX_TO_OSC_RATIO
                / 4;
            cpu_ppc_tb_init(&ppc_cpus[i]->env, timebase);
        }
        if (i == 0) {
            reset_info->cpu = ppc_cpus[i];
            qemu_register_reset(main_cpu_reset, reset_info);
        } else {
            qemu_register_reset(secondary_cpu_reset, ppc_cpus[i]);
            CPU(ppc_cpus[i])->halted = 1;
        }
    }

    /* allocate RAM */
    memory_region_init_ram(ram, NULL, "mpc8641d.ram", ram_size, &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x0, ram);

    /* allocate and load BIOS */
    memory_region_init_ram(bios, NULL, "wrSbc8641d.bios", BIOS_SIZE,
                           &error_abort);
    memory_region_set_readonly(bios, true);
    memory_region_add_subregion(get_system_memory(), (uint32_t)(-BIOS_SIZE),
                                bios);

    if (bios_name != NULL && strcmp(bios_name, "-") == 0) {
        /* No bios.  */
        bios_size = BIOS_SIZE;
        filename = NULL;
    } else {
        if (bios_name == NULL)
            bios_name = BIOS_FILENAME;
        filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
        if (filename) {
            bios_size = get_image_size(filename);
        } else {
            bios_size = -1;
        }
    }
    if (bios_size > 0 && bios_size <= BIOS_SIZE) {
        hwaddr bios_addr;
        bios_size = (bios_size + 0xfff) & ~0xfff;
        bios_addr = (uint32_t)(-bios_size);
        if (filename) {
            bios_size = load_image_targphys(filename, bios_addr, bios_size);
        }
    }
    if (bios_size < 0 || bios_size > BIOS_SIZE) {
        hw_error("qemu: could not load bios for wrsbc8641d '%s'\n", bios_name);
    }
    if (filename) {
        g_free(filename);
    }

    if (linux_boot) {

        kernel_base = KERNEL_LOAD_ADDR;
        /* now we can load the kernel */
        kernel_size = load_elf(args->kernel_filename, NULL, NULL, NULL,
                               &elf_entry, NULL, NULL, 1, PPC_ELF_MACHINE, 0,
                               0);
        reset_info->entry = elf_entry;
        if (kernel_size < 0) {
            kernel_size = load_image_targphys(args->kernel_filename, kernel_base,
                                              ram_size - kernel_base);
        }
        if (kernel_size < 0) {
            hw_error("qemu: could not load kernel '%s'\n",
                     args->kernel_filename);
        }
        /* load initrd */
        if (args->initrd_filename) {
            initrd_base = INITRD_LOAD_ADDR;
            initrd_size = load_image_targphys(args->initrd_filename,
                                              initrd_base,
                                              ram_size - initrd_base);
            if (initrd_size < 0) {
                hw_error("qemu: could not load initial ram disk '%s'\n",
                          args->initrd_filename);
            }
        } else {
            initrd_base = 0;
            initrd_size = 0;
        }

        /* Set params.  */
        memory_region_init_ram(params, NULL, "wrSbc8641d.params", PARAMS_SIZE,
                               &error_abort);
        memory_region_add_subregion(get_system_memory(), PARAMS_ADDR, params);

        if (args->kernel_cmdline) {
            cpu_physical_memory_write(PARAMS_ADDR, args->kernel_cmdline,
                                      strlen (args->kernel_cmdline) + 1);
        } else {
            if (ppc_cpus[1] != NULL) {
                stb_phys(CPU(ppc_cpus[1])->as, PARAMS_ADDR, 0);
            }
        }

        /* Set read-only after writing command line */
        memory_region_set_readonly(params, true);

    } else {
        kernel_base = 0;
        kernel_size = 0;
        initrd_base = 0;
        initrd_size = 0;
    }

    qemu_irq ** openpic_irqs;
    openpic_irqs = g_malloc0(smp_cpus * sizeof(qemu_irq *));

    for (i = 0; i < smp_cpus; i++) {
        openpic_irqs[i] = g_malloc0(sizeof(qemu_irq) * OPENPIC_OUTPUT_NB);

        switch (PPC_INPUT((&ppc_cpus[i]->env))) {
            case PPC_FLAGS_INPUT_6xx:
                openpic_irqs[i][OPENPIC_OUTPUT_INT] =
                    ((qemu_irq *)ppc_cpus[i]->env.irq_inputs)[PPC6xx_INPUT_INT];
                openpic_irqs[i][OPENPIC_OUTPUT_CINT] =
                    ((qemu_irq *)ppc_cpus[i]->env.irq_inputs)[PPC6xx_INPUT_MCP];
                openpic_irqs[i][OPENPIC_OUTPUT_MCK] =
                    ((qemu_irq *)ppc_cpus[i]->env.irq_inputs)[PPC6xx_INPUT_MCP];
                /* Not connected ? */
                openpic_irqs[i][OPENPIC_OUTPUT_DEBUG] = NULL;
                /* Check this */
                openpic_irqs[i][OPENPIC_OUTPUT_RESET] =
                    ((qemu_irq *)ppc_cpus[i]->env.irq_inputs)[PPC6xx_INPUT_HRESET];
                break;
            default:
                hw_error("Bus model not supported on mac99 machine\n");
                exit(1);
        }
    }

    static struct {
        const MemoryRegionOps *ops;
        hwaddr                 start_addr;
        ram_addr_t             size;
        const char            *name;
    } const list[] = {
        {&mcm_ops, MCM_START, MCM_SIZE, "MCM"},
        {&gur_ops, GUR_START, GUR_SIZE, "GUR"},
        {&pixis_ops, PIXIS_START, PIXIS_SIZE, "PIXIS"},
        {&qtrace_ops, QTRACE_START, QTRACE_SIZE, "QTRACES"}
    };

    for (i = 0; i < sizeof(list)/sizeof(list[0]); i++) {
        MemoryRegion *iomem = g_new(MemoryRegion, 1);

        memory_region_init_io(iomem, NULL, list[i].ops, NULL, list[i].name,
                              list[i].size);
        memory_region_add_subregion(get_system_memory(), list[i].start_addr,
                                    iomem);
    }

    board_reset_bh = qemu_bh_new(board_reset_cb, NULL);

    /* MPIC */
    qemu_irq *pic;
    pic = init_mpic(get_system_memory(), openpic_irqs);

    int serial_irqs[] = {26,12};
    for (i = 0; i < 2; i++) {
        if (serial_hd(i)) {
            serial_mm_init(get_system_memory(), SERIAL_START + i * 0x100, 0,
                           pic[12+serial_irqs[i]], 9600, serial_hd(i),
                           DEVICE_BIG_ENDIAN);
        }
    }

    static const int etsec_irqs[4][3] = {
        {13, 14, 18},
        {19, 20, 24},
        {15, 16, 17},
        {21, 22, 23}
    };
    DPRINTF("%s: registering %d network eTSEC(s)\n", __func__, nb_nics);
    for (i = 0; i < nb_nics; i++) {
        etsec_create(ETSEC_START + 0x1000 * i, get_system_memory(),
                     &nd_table[i], pic[12+etsec_irqs[i][0]],
                     pic[12+etsec_irqs[i][1]], pic[12+etsec_irqs[i][2]]);
    }

    /* HostFS */
    hostfs_create(HOSTFS_START, get_system_memory());

    /* Initialize the GnatBus Master */
    gnatbus_master_init(pic, 96);
    gnatbus_device_init();
}

static void wrsbc8641d_generic_machine_init(MachineClass *mc)
{
    mc->desc     = "PowerPC wrsbc8641d SMP platform";
    mc->max_cpus = MAX_CPUS;
    mc->init     = wrsbc8641d_init;
}

DEFINE_MACHINE("wrsbc8641d_vxworks", wrsbc8641d_generic_machine_init)
