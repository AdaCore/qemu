/*
 * QEMU PPC PREP hardware System Emulator
 *
 * Copyright (c) 2003-2007 Jocelyn Mayer
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

//#define HARD_DEBUG_PPC_IO
//#define DEBUG_PPC_IO

/* SMP is not enabled, for now */
#define MAX_CPUS 1

#define MAX_IDE_BUS 2

#define BIOS_SIZE (32 * 1024 * 1024)
#define BIOS_FILENAME "ppc_rom.bin"
#define KERNEL_LOAD_ADDR 0x01000000
#define INITRD_LOAD_ADDR 0x01800000

#if defined (HARD_DEBUG_PPC_IO) && !defined (DEBUG_PPC_IO)
#define DEBUG_PPC_IO
#endif

#if defined (HARD_DEBUG_PPC_IO)
#define PPC_IO_DPRINTF(fmt, ...)                         \
do {                                                     \
    if (qemu_loglevel_mask(CPU_LOG_IOPORT)) {            \
        qemu_log("%s: " fmt, __func__ , ## __VA_ARGS__); \
    } else {                                             \
        printf("%s : " fmt, __func__ , ## __VA_ARGS__);  \
    }                                                    \
} while (0)
#elif defined (DEBUG_PPC_IO)
#define PPC_IO_DPRINTF(fmt, ...) \
qemu_log_mask(CPU_LOG_IOPORT, fmt, ## __VA_ARGS__)
#else
#define PPC_IO_DPRINTF(fmt, ...) do { } while (0)
#endif

/* PCI intack register */
/* Read-only register (?) */
/* PowerPC control and status registers */
#if 0 // Not used
static struct {
    /* IDs */
    uint32_t veni_devi;
    uint32_t revi;
    /* Control and status */
    uint32_t gcsr;
    uint32_t xcfr;
    uint32_t ct32;
    uint32_t mcsr;
    /* General purpose registers */
    uint32_t gprg[6];
    /* Exceptions */
    uint32_t feen;
    uint32_t fest;
    uint32_t fema;
    uint32_t fecl;
    uint32_t eeen;
    uint32_t eest;
    uint32_t eecl;
    uint32_t eeint;
    uint32_t eemck0;
    uint32_t eemck1;
    /* Error diagnostic */
} XCSR;

static void PPC_XCSR_writeb (void *opaque,
                             target_phys_addr_t addr, uint32_t value)
{
    printf("%s: 0x" TARGET_FMT_plx " => 0x%08" PRIx32 "\n", __func__, addr,
           value);
}

static void PPC_XCSR_writew (void *opaque,
                             target_phys_addr_t addr, uint32_t value)
{
    printf("%s: 0x" TARGET_FMT_plx " => 0x%08" PRIx32 "\n", __func__, addr,
           value);
}

static void PPC_XCSR_writel (void *opaque,
                             target_phys_addr_t addr, uint32_t value)
{
    printf("%s: 0x" TARGET_FMT_plx " => 0x%08" PRIx32 "\n", __func__, addr,
           value);
}

static uint32_t PPC_XCSR_readb (void *opaque, target_phys_addr_t addr)
{
    uint32_t retval = 0;

    printf("%s: 0x" TARGET_FMT_plx " <= %08" PRIx32 "\n", __func__, addr,
           retval);

    return retval;
}

static uint32_t PPC_XCSR_readw (void *opaque, target_phys_addr_t addr)
{
    uint32_t retval = 0;

    printf("%s: 0x" TARGET_FMT_plx " <= %08" PRIx32 "\n", __func__, addr,
           retval);

    return retval;
}

static uint32_t PPC_XCSR_readl (void *opaque, target_phys_addr_t addr)
{
    uint32_t retval = 0;

    printf("%s: 0x" TARGET_FMT_plx " <= %08" PRIx32 "\n", __func__, addr,
           retval);

    return retval;
}

static CPUWriteMemoryFunc * const PPC_XCSR_write[] = {
    &PPC_XCSR_writeb,
    &PPC_XCSR_writew,
    &PPC_XCSR_writel,
};

static CPUReadMemoryFunc * const PPC_XCSR_read[] = {
    &PPC_XCSR_readb,
    &PPC_XCSR_readw,
    &PPC_XCSR_readl,
};
#endif

/* Fake super-io ports for PREP platform (Intel 82378ZB) */
typedef struct sysctrl_t {
    qemu_irq reset_irq;
    M48t59State *nvram;
    uint8_t state;
    uint8_t syscontrol;
    uint8_t fake_io[2];
    int contiguous_map;
    int endian;
    qemu_irq irq14;
    qemu_irq irq15;
} sysctrl_t;

enum {
    STATE_HARDFILE = 0x01,
};

static sysctrl_t *sysctrl;

static inline target_phys_addr_t prep_IO_address(sysctrl_t *sysctrl,
                                                 target_phys_addr_t addr)
{
    if (sysctrl->contiguous_map == 0) {
        /* 64 KB contiguous space for IOs */
        addr &= 0xFFFF;
    } else {
        /* 8 MB non-contiguous space for IOs */
        addr = (addr & 0x1F) | ((addr & 0x007FFF000) >> 7);
    }

    return addr;
}

static void PPC_prep_io_writeb (void *opaque, target_phys_addr_t addr,
                                uint32_t value)
{
    sysctrl_t *sysctrl = opaque;

    addr = prep_IO_address(sysctrl, addr);
    cpu_outb(addr, value);
}

static uint32_t PPC_prep_io_readb (void *opaque, target_phys_addr_t addr)
{
    sysctrl_t *sysctrl = opaque;
    uint32_t ret;

    addr = prep_IO_address(sysctrl, addr);
    ret = cpu_inb(addr);

    return ret;
}

static void PPC_prep_io_writew (void *opaque, target_phys_addr_t addr,
                                uint32_t value)
{
    sysctrl_t *sysctrl = opaque;

    addr = prep_IO_address(sysctrl, addr);
    //PPC_IO_DPRINTF("0x" TARGET_FMT_plx " => 0x%08" PRIx32 "\n", addr, value);
    cpu_outw(addr, value);
}

static uint32_t PPC_prep_io_readw (void *opaque, target_phys_addr_t addr)
{
    sysctrl_t *sysctrl = opaque;
    uint32_t ret;

    addr = prep_IO_address(sysctrl, addr);
    ret = cpu_inw(addr);
    //PPC_IO_DPRINTF("0x" TARGET_FMT_plx " <= 0x%08" PRIx32 "\n", addr, ret);

    return ret;
}

static void PPC_prep_io_writel (void *opaque, target_phys_addr_t addr,
                                uint32_t value)
{
    sysctrl_t *sysctrl = opaque;

    addr = prep_IO_address(sysctrl, addr);
    //PPC_IO_DPRINTF("0x" TARGET_FMT_plx " => 0x%08" PRIx32 "\n", addr, value);
    cpu_outl(addr, value);
}

static uint32_t PPC_prep_io_readl (void *opaque, target_phys_addr_t addr)
{
    sysctrl_t *sysctrl = opaque;
    uint32_t ret;

    addr = prep_IO_address(sysctrl, addr);
    ret = cpu_inl(addr);
    //PPC_IO_DPRINTF("0x" TARGET_FMT_plx " <= 0x%08" PRIx32 "\n", addr, ret);

    return ret;
}

static CPUWriteMemoryFunc * const PPC_prep_io_write[] = {
    &PPC_prep_io_writeb,
    &PPC_prep_io_writew,
    &PPC_prep_io_writel,
};

static CPUReadMemoryFunc * const PPC_prep_io_read[] = {
    &PPC_prep_io_readb,
    &PPC_prep_io_readw,
    &PPC_prep_io_readl,
};

#define MEM_START 0xF8000000
#define MEM_SIZE  0x00004000

uint32_t * phony_mem;

static void my_write (void *opaque, target_phys_addr_t addr, uint32_t value)
{
    printf("my_write : attempting to write to : 0x%08x <= 0x%x\n", addr +
            MEM_START, value);
    uint32_t * mem = opaque;
    mem[addr] = value;
}
static uint32_t my_read (void *opaque, target_phys_addr_t addr)
{
    printf("my_read : attempting to read from : 0x%08x\n", addr + MEM_START);
    uint32_t * mem = opaque;
    return mem[addr];
}
static CPUWriteMemoryFunc * const my_cpu_write_fct[] = {
    &my_write,
    &my_write,
    &my_write,
};

static CPUReadMemoryFunc * const my_cpu_read_fct[] = {
    &my_read,
    &my_read,
    &my_read,
};

#define GUR_START       0xf80e0000
#define GUR_SIZE        0x30
static uint32_t my_read_gur (void *opaque, target_phys_addr_t addr)
{
    printf("my_read_gur : reading from Global Utility Register : 0x%08x\n",
            addr +
            GUR_START);
    return 0;
}

static CPUReadMemoryFunc * const my_cpu_read_fct_gur[] = {
    &my_read_gur,
    &my_read_gur,
    &my_read_gur,
};

#define PMR_START       0xf80e1000
#define PMR_SIZE        0xA9
static uint32_t my_read_pmr (void *opaque, target_phys_addr_t addr)
{
    printf("my_read_pmr : reading from Performance Management Register : "
            "0x%08x\n",
            addr +
            PMR_START);
    return 0;
}

static CPUReadMemoryFunc * const my_cpu_read_fct_pmr[] = {
    &my_read_pmr,
    &my_read_pmr,
    &my_read_pmr,
};

/* PowerPC PREP hardware initialisation */
static void ppc_simple_init (ram_addr_t ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{
    CPUState *env = NULL;
    char *filename;
    int linux_boot, i, bios_size;
    ram_addr_t ram_offset, bios_offset;
    uint32_t kernel_base, initrd_base;
    long kernel_size, initrd_size;
    int ppc_boot_device;

    sysctrl = qemu_mallocz(sizeof(sysctrl_t));

    linux_boot = (kernel_filename != NULL);

    /* init CPUs */
    if (cpu_model == NULL)
        cpu_model = "602";
    for (i = 0; i < smp_cpus; i++) {
        env = cpu_init(cpu_model);
        if (!env) {
            fprintf(stderr, "Unable to find PowerPC CPU definition\n");
            exit(1);
        }
        if (env->flags & POWERPC_FLAG_RTC_CLK) {
            /* POWER / PowerPC 601 RTC clock frequency is 7.8125 MHz */
            cpu_ppc_tb_init(env, 7812500UL);
        } else {
            /* Set time-base frequency to 100 Mhz */
            cpu_ppc_tb_init(env, 100UL * 1000UL * 1000UL);
        }
        qemu_register_reset((QEMUResetHandler*)&cpu_reset, env);
    }

    /* allocate RAM */
    ram_offset = qemu_ram_alloc(NULL, "ppc_simple.ram", ram_size);
    cpu_register_physical_memory(0, ram_size, ram_offset);

    /* allocate and load BIOS */
    bios_offset = qemu_ram_alloc(NULL, "ppc_simple.bios", BIOS_SIZE);
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
        qemu_free(filename);
    }

    if (linux_boot) {
        kernel_base = KERNEL_LOAD_ADDR;
        /* now we can load the kernel */
        kernel_size = load_elf(kernel_filename, NULL, NULL, NULL, NULL, NULL,
                               1, ELF_MACHINE, 0);
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
        ppc_boot_device = 'm';
    } else {
        kernel_base = 0;
        kernel_size = 0;
        initrd_base = 0;
        initrd_size = 0;
        ppc_boot_device = '\0';
        /* For now, OHW cannot boot from the network. */
        for (i = 0; boot_device[i] != '\0'; i++) {
            if (boot_device[i] >= 'a' && boot_device[i] <= 'f') {
                ppc_boot_device = boot_device[i];
                break;
            }
        }
    }
    isa_mem_base = 0xc0000000;

    qemu_irq ** openpic_irqs;
    openpic_irqs = qemu_mallocz(smp_cpus * sizeof(qemu_irq *));
    openpic_irqs[0] =
            qemu_mallocz(smp_cpus * sizeof(qemu_irq) * OPENPIC_OUTPUT_NB);
    for (i = 0; i < smp_cpus; i++) {
            /* Mac99 IRQ connection between OpenPIC outputs pins
             * and PowerPC input pins
             */
            switch (PPC_INPUT(env)) {
            case PPC_FLAGS_INPUT_6xx:
                openpic_irqs[i] = openpic_irqs[0] + (i * OPENPIC_OUTPUT_NB);
                openpic_irqs[i][OPENPIC_OUTPUT_INT] =
                    ((qemu_irq *)env->irq_inputs)[PPC6xx_INPUT_INT];
                openpic_irqs[i][OPENPIC_OUTPUT_CINT] =
                    ((qemu_irq *)env->irq_inputs)[PPC6xx_INPUT_INT];
                openpic_irqs[i][OPENPIC_OUTPUT_MCK] =
                    ((qemu_irq *)env->irq_inputs)[PPC6xx_INPUT_MCP];
                /* Not connected ? */
                openpic_irqs[i][OPENPIC_OUTPUT_DEBUG] = NULL;
                /* Check this */
                openpic_irqs[i][OPENPIC_OUTPUT_RESET] =
                    ((qemu_irq *)env->irq_inputs)[PPC6xx_INPUT_HRESET];
                break;
#if defined(TARGET_PPC64)
        case PPC_FLAGS_INPUT_970:
            openpic_irqs[i] = openpic_irqs[0] + (i * OPENPIC_OUTPUT_NB);
            openpic_irqs[i][OPENPIC_OUTPUT_INT] =
                ((qemu_irq *)env->irq_inputs)[PPC970_INPUT_INT];
            openpic_irqs[i][OPENPIC_OUTPUT_CINT] =
                ((qemu_irq *)env->irq_inputs)[PPC970_INPUT_INT];
            openpic_irqs[i][OPENPIC_OUTPUT_MCK] =
                ((qemu_irq *)env->irq_inputs)[PPC970_INPUT_MCP];
            /* Not connected ? */
            openpic_irqs[i][OPENPIC_OUTPUT_DEBUG] = NULL;
            /* Check this */
            openpic_irqs[i][OPENPIC_OUTPUT_RESET] =
                ((qemu_irq *)env->irq_inputs)[PPC970_INPUT_HRESET];
            break;
#endif /* defined(TARGET_PPC64) */
        default:
            hw_error("Bus model not supported on mac99 machine\n");
            exit(1);
        }
    }
    phony_mem = qemu_mallocz(MEM_SIZE);
    int io_mem = cpu_register_io_memory(my_cpu_read_fct, my_cpu_write_fct,
            phony_mem, DEVICE_BIG_ENDIAN);
    cpu_register_physical_memory(MEM_START, MEM_SIZE, io_mem);

    io_mem = cpu_register_io_memory(my_cpu_read_fct_gur, my_cpu_write_fct,
            NULL, DEVICE_BIG_ENDIAN);
    cpu_register_physical_memory(GUR_START, GUR_SIZE, io_mem);

    io_mem = cpu_register_io_memory(my_cpu_read_fct_pmr, my_cpu_write_fct,
            NULL, DEVICE_BIG_ENDIAN);
    cpu_register_physical_memory(PMR_START, PMR_SIZE, io_mem);

    int pic_mem_mapping = 0xF8040000;
    qemu_irq *pic;
    pic = mpic_init(pic_mem_mapping, smp_cpus, openpic_irqs, NULL);
    PPC_IO_DPRINTF("OpenPIC init : pic_mem_index = 0x%x\n", pic_mem_index);
    //cpu_register_physical_memory(0xF8040000, 256 * 1024, pic_mem_index);

    int serial1_mem_mapping = 0xF8004500;
    int serial2_mem_mapping = 0xF8004600;
    if (serial_hds[0]) {
        serial_mm_init(serial1_mem_mapping, 0, pic[26], 115200,
                serial_hds[0], 1, 1);
    }
    if (serial_hds[1]) {
        serial_mm_init(serial2_mem_mapping, 0, pic[12], 115200,
                serial_hds[0], 1, 1);
    }

}

static QEMUMachine simple_machine = {
    .name = "simple",
    .desc = "PowerPC simple platform",
    .init = ppc_simple_init,
    .max_cpus = MAX_CPUS,
};

static void simple_machine_init(void)
{
    qemu_register_machine(&simple_machine);
}

machine_init(simple_machine_init);
