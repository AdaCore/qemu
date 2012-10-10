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
#include "hw.h"
#include "nvram.h"
#include "pc.h"
#include "fdc.h"
#include "net.h"
#include "sysemu.h"
#include "isa.h"
#include "pci.h"
#include "prep_pci.h"
#include "usb-ohci.h"
#include "ppc.h"
#include "boards.h"
#include "qemu-log.h"
#include "ide.h"
#include "loader.h"
#include "mc146818rtc.h"
#include "blockdev.h"
#include "elf.h"
#include "openpic.h"
#include "etsec.h"
#include "exec-memory.h"
#include "hostfs.h"

#include "qemu-plugin.h"
#include "gnat-bus.h"
#include "qemu-traces.h"

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
static CPUState *cpus[] = {NULL, NULL};

typedef struct ResetData {
    CPUState *env;
    uint32_t  entry;            /* save kernel entry in case of reset */
} ResetData;

static void write_bogus(void *opaque, target_phys_addr_t addr, uint32_t
        value)
{
    DPRINTF("%s: attempting to write to : 0x%08x <= %08x\n", addr, __func__,
            value);
}

static CPUWriteMemoryFunc * const write_bogus_fct[] = {
    &write_bogus,
    &write_bogus,
    &write_bogus,
};

static uint32_t read_bogus(void *opaque, target_phys_addr_t addr)
{
    DPRINTF("%s: attempting to read from 0x%08x\n", __func__, addr);
    return 0;
}

static CPUReadMemoryFunc * const cpu_read_bogus[] = {
    &read_bogus,
    &read_bogus,
    &read_bogus,
};

static void write_mcm(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    switch (addr & 0xFFF) {
    case 0x010:
        DPRINTF("%s: Writing to MCM PCR <= 0x%08x\n", __func__, value);
        if (value & (1 << 25)) {
            DPRINTF("%s: Enabling Core 1\n", __func__);
            cpus[1]->halted = 0;
        }
        break;
    default:
        DPRINTF("%s: Writing to unassigned : 0x%08x <= 0x%x\n", __func__, addr
                + MCM_START, value);
    }
}

static uint32_t read_mcm(void *opaque, target_phys_addr_t addr)
{
    uint32_t ret;
    switch (addr & 0xFFF) {
    case 0x010:
        ret = (!cpus[0]->halted << 24) |
              (!cpus[1]->halted << 25);
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

static CPUWriteMemoryFunc * const write_mcm_fct [] = {
    &write_mcm,
    &write_mcm,
    &write_mcm,
};

static CPUReadMemoryFunc * const read_mcm_fct [] = {
    &read_mcm,
    &read_mcm,
    &read_mcm,
};

static void write_gur(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    DPRINTF("write_gur : write to Global Utility Register : 0x%08x\n",
            addr +
            GUR_START);

    switch (addr & 0xfff) {
    case 0xb0: /* Reset Control Register - RSTCR */
        DPRINTF("%s: Writing RSTCR\n", __func__);
        if (value & 2) {
            qemu_system_reset_request();
        }
        break;
    default:
        DPRINTF("%s: writing non implemented register 0x%8x\n", __func__,
                GUR_START + addr);
    }
}

static uint32_t read_gur(void *opaque, target_phys_addr_t addr)
{
    /* DPRINTF("read_gur : reading from Global Utility Register : 0x%08x\n",
            addr +
            GUR_START); */
    uint32_t ret;
    CPUState *env = cpu_single_env?cpu_single_env:first_cpu;
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

static CPUWriteMemoryFunc * const write_gur_fct[] = {
    &write_gur,
    &write_gur,
    &write_gur,
};

static CPUReadMemoryFunc * const read_gur_fct[] = {
    &read_gur,
    &read_gur,
    &read_gur,
};

static void write_qtrace(void *opaque, target_phys_addr_t addr, uint32_t value)
{
    switch (addr & 0xfff) {
    case 0x00:
        trace_special (TRACE_SPECIAL_LOADADDR, value);
        break;
    default:
        DPRINTF("%s: writing non implemented register 0x%8x\n", __func__,
                QTRACE_START + addr);
    }
}

static CPUWriteMemoryFunc * const write_qtrace_fct[] = {
    &write_bogus,
    &write_bogus,
    &write_qtrace,
};

static CPUReadMemoryFunc * const read_qtrace_fct[] = {
    &read_bogus,
    &read_bogus,
    &read_bogus,
};

static void main_cpu_reset(void *opaque)
{
    ResetData *s   = (ResetData *)opaque;
    CPUState *env = s->env;

    cpu_reset(env);
    env->nip = s->entry;
    env->halted = 0;
}

static void secondary_cpu_reset(void *opaque)
{
    CPUState *env = opaque;

    cpu_reset(env);
    env->halted = 1;
}

static uint32_t read_pixis(void *opaque, target_phys_addr_t addr)
{
    /* DPRINTF("read_pixis : reading from PIXIS : "
            "0x%08x\n",
            addr +
            PIXIS_START); */
    uint32_t ret;
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

static void write_pixis(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    /* DPRINTF("read_pixis : reading from PIXIS : "
            "0x%08x\n",
            addr +
            PIXIS_START); */
    switch (addr & 0xff) {
    case 0x04: /* PIXIS_RST : Reset the whole system */
        DPRINTF("%s: Write to PIXIS_RST => reset system\n", __func__);
        qemu_system_reset_request();
        break;
    default:
        DPRINTF("%s: Writing to unassigned PIXIS register at 0x%08x\n",
                __func__, addr + PIXIS_START);
    }
}

static CPUReadMemoryFunc * const read_pixis_fct[] = {
    &read_pixis,
    &read_pixis,
    &read_pixis,
};

static CPUWriteMemoryFunc * const write_pixis_fct[] = {
    &write_pixis,
    &write_pixis,
    &write_pixis,
};

static void wrsbc8641d_init(ram_addr_t ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{

    CPUState     **env = cpus;
    MemoryRegion  *ram;
    char          *filename;
    int            linux_boot, i, bios_size;
    ram_addr_t     bios_offset, params_offset;
    uint32_t       kernel_base, initrd_base;
    long           kernel_size, initrd_size;
    ResetData     *reset_info;
    uint64_t       elf_entry;

    linux_boot = (kernel_filename != NULL);

    /* Reset data */
    reset_info = g_malloc0(sizeof(ResetData));

    /* init CPUs */
    if (cpu_model == NULL)
        cpu_model = "MPC8641D";
    for (i = 0; i < smp_cpus; i++) {
        env[i] = cpu_init(cpu_model);
        if (!env[i]) {
            fprintf(stderr, "Unable to find PowerPC CPU definition\n");
            exit(1);
        }
        /* Set the processor ID */
        env[i]->spr[SPR_MSSCR0] |= (i << 5);
        if (env[i]->flags & POWERPC_FLAG_RTC_CLK) {
            /* POWER / PowerPC 601 RTC clock frequency is 7.8125 MHz */
            cpu_ppc_tb_init(env[i], 7812500UL);
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
            cpu_ppc_tb_init(env[i], timebase);
        }
        if (i == 0) {
            reset_info->env = env[i];
            qemu_register_reset(main_cpu_reset, reset_info);
        } else {
            qemu_register_reset(secondary_cpu_reset, env[i]);
            env[i]->halted = 1;
        }
    }

    /* allocate RAM */
    ram = g_malloc0(sizeof(*ram));
    memory_region_init_ram(ram, NULL, "mpc8641d.ram", ram_size);
    memory_region_add_subregion(get_system_memory(), 0x0, ram);

    /* allocate and load BIOS */
    bios_offset = qemu_ram_alloc(NULL, "mpc8641d.bios", BIOS_SIZE);
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
        target_phys_addr_t bios_addr;
        bios_size = (bios_size + 0xfff) & ~0xfff;
        bios_addr = (uint32_t)(-bios_size);
        cpu_register_physical_memory(bios_addr, bios_size,
                                     bios_offset | IO_MEM_ROM);
        if (filename) {
            bios_size = load_image_targphys(filename, bios_addr, bios_size);
        }
    }
    if (bios_size < 0 || bios_size > BIOS_SIZE) {
        hw_error("qemu: could not load PPC PREP bios '%s'\n", bios_name);
    }
    if (filename) {
        g_free(filename);
    }

    if (linux_boot) {

        kernel_base = KERNEL_LOAD_ADDR;
        /* now we can load the kernel */
        kernel_size = load_elf(kernel_filename, NULL, NULL, &elf_entry, NULL, NULL, 1, ELF_MACHINE, 0);
        reset_info->entry = elf_entry;
        if (kernel_size < 0) {
            kernel_size = load_image_targphys(kernel_filename, kernel_base,
                                              ram_size - kernel_base);
        }
        if (kernel_size < 0) {
            hw_error("qemu: could not load kernel '%s'\n", kernel_filename);
        }
        /* load initrd */
        if (initrd_filename) {
            initrd_base = INITRD_LOAD_ADDR;
            initrd_size = load_image_targphys(initrd_filename, initrd_base,
                                              ram_size - initrd_base);
            if (initrd_size < 0) {
                hw_error("qemu: could not load initial ram disk '%s'\n",
                          initrd_filename);
            }
        } else {
            initrd_base = 0;
            initrd_size = 0;
        }
        /* Set params.  */
        params_offset = qemu_ram_alloc(NULL, "mpc8641d.params", PARAMS_SIZE);
        cpu_register_physical_memory(PARAMS_ADDR, PARAMS_SIZE, params_offset);
        if (kernel_cmdline) {
            cpu_physical_memory_write(PARAMS_ADDR, kernel_cmdline,
                                      strlen (kernel_cmdline) + 1);
        } else {
            stb_phys(PARAMS_ADDR, 0);
        }

    } else {
        kernel_base = 0;
        kernel_size = 0;
        initrd_base = 0;
        initrd_size = 0;
    }

    qemu_irq ** openpic_irqs;
    openpic_irqs = g_malloc0(smp_cpus * sizeof(qemu_irq *));
    openpic_irqs[0] =
            g_malloc0(smp_cpus * sizeof(qemu_irq) * OPENPIC_OUTPUT_NB);

    for (i = 0; i < smp_cpus; i++) {
            switch (PPC_INPUT(env[i])) {
            case PPC_FLAGS_INPUT_6xx:
                openpic_irqs[i] = openpic_irqs[0] + (i * OPENPIC_OUTPUT_NB);
                openpic_irqs[i][OPENPIC_OUTPUT_INT] =
                    ((qemu_irq *)env[i]->irq_inputs)[PPC6xx_INPUT_INT];
                openpic_irqs[i][OPENPIC_OUTPUT_CINT] =
                    ((qemu_irq *)env[i]->irq_inputs)[PPC6xx_INPUT_MCP];
                openpic_irqs[i][OPENPIC_OUTPUT_MCK] =
                    ((qemu_irq *)env[i]->irq_inputs)[PPC6xx_INPUT_MCP];
                /* Not connected ? */
                openpic_irqs[i][OPENPIC_OUTPUT_DEBUG] = NULL;
                /* Check this */
                openpic_irqs[i][OPENPIC_OUTPUT_RESET] =
                    ((qemu_irq *)env[i]->irq_inputs)[PPC6xx_INPUT_HRESET];
                break;
            default:
                hw_error("Bus model not supported on mac99 machine\n");
                exit(1);
        }
    }

    static struct {
        CPUReadMemoryFunc * const *read;
        CPUWriteMemoryFunc * const *write;
        target_phys_addr_t start_addr;
        ram_addr_t size;
    } const list[] = {
        {read_mcm_fct, write_mcm_fct, MCM_START, MCM_SIZE},
        {read_gur_fct, write_gur_fct, GUR_START, GUR_SIZE},
        {read_pixis_fct, write_pixis_fct, PIXIS_START, PIXIS_SIZE},
        {read_qtrace_fct, write_qtrace_fct, QTRACE_START, QTRACE_SIZE}
    };

    int io_mem;
    for (i = 0; i < sizeof(list)/sizeof(list[0]); i++) {
        io_mem = cpu_register_io_memory(list[i].read, list[i].write, NULL,
                DEVICE_BIG_ENDIAN);
        cpu_register_physical_memory(list[i].start_addr, list[i].size, io_mem);
    }

    qemu_irq *pic;
    pic = mpic_init(get_system_memory(), PIC_START, smp_cpus,
                    openpic_irqs, NULL);

    int serial_irqs[] = {26,12};
    for (i = 0; i < 2; i++) {
        if (serial_hds[i]) {
            serial_mm_init(get_system_memory(), SERIAL_START + i * 0x100, 0,
                           pic[12+serial_irqs[i]], 9600, serial_hds[i], 1, 1);
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

    /* Initialize plug-ins */
    plugin_init(pic, 16);
    plugin_device_init();

    /* Initialize the GnatBus Master */
    gnatbus_master_init(pic, 16);
    gnatbus_device_init();
}

static QEMUMachine wrsbc8641d_machine = {
    .name = "wrsbc8641d_vxworks",
    .desc = "PowerPC wrsbc8641d SMP platform",
    .init = wrsbc8641d_init,
    .max_cpus = MAX_CPUS,
};

static void wrsbc8641d_machine_init(void)
{
    qemu_register_machine(&wrsbc8641d_machine);
}

machine_init(wrsbc8641d_machine_init);
