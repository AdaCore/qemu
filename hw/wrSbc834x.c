/* PowerPC microcontroller family: MPC83xx (PowerQUICC II Pro)
 * The CPU core is an e300 (its architecture is based on MPC603e)
 */
/* PowerPC board: WindRiver SBC8349E/47E
 * http://www.takefive.com/windsurf/techpubs/hwTools/referenceDesigns/
       ERG-00328-001.pdf
 */

#include "wrSbc834x_mm.h"

#include "qemu-common.h"
#include "sysemu.h"
#include "kvm.h"
#include "kvm_ppc.h"
#include "hw.h"
#include "ppc.h"
#include "pc.h"
#include "isa.h"
#include "exec-memory.h"
#include "boards.h"
#include "loader.h"
#include "elf.h"
#include "ipic.h"
#include "etsec.h"

#include "qemu-plugin.h"
#include "gnat-bus.h"

#define e300_VECTOR_MASK  0x000fffff
#define e300_RESET_VECTOR 0x00000100

#define CPU_NAME "MPC83xx"
#define BOARD_NAME "wrsbc834x_vxworks"
#define BOARD_DESC "WindRiver SBC834x board"

#define CPU_MODEL "MPC8349E"
#define MAX_CPUS 1
#define MAX_ETSEC_CONTROLLERS 2

#define DTC_LOAD_PAD               0x500000
#define DTC_PAD_MASK               0xFFFFF

#define KERNEL_LOAD_ADDR 0
#define INITRD_LOAD_ADDR 0x01800000

/* #define DEBUG_MPC83XX */
#ifdef DEBUG_MPC83XX
    /* set it to 1 to print configuration changes */
    #define DEBUG_CONFIG            1
    /* set it to 1 to print read/write of registers */
    #define DEBUG_RW                1
    /* set it to 1 to print warnings on read/write of registers */
    #define DEBUG_RW_ERROR          1
    /* set it to 1 to print not implemented requests */
    #define DEBUG_NOT_IMPLEMENTED   1
#else
    #define DEBUG_CONFIG            0
    #define DEBUG_RW                0
    #define DEBUG_RW_ERROR          0
    #define DEBUG_NOT_IMPLEMENTED   0
#endif

#define eprintf(fmt, args...) { \
    fprintf(stderr, CPU_NAME ": " fmt "\n", ## args) ; \
    abort() ; \
}
#define not_implemented0(fmt, args...) \
    fprintf(stderr, CPU_NAME ": " fmt ": not implemented\n", ## args)
#define not_implemented_register0(offset) \
    do { \
        int i = 0; \
        do { \
            if (offset >= regdescs[i].saddr && offset <= regdescs[i].eaddr) { \
                fprintf(stderr, \
                   CPU_NAME ": register 0x%08x is not implemented - %s\n", \
                   offset, regdescs[i].descr); \
            } \
            i++; \
        } while (regdescs[i].descr); \
    } while (0)
#ifdef DEBUG_MPC83XX
    #define dprintf0(fmt, args...) fprintf(stderr, fmt, ## args)
    #define not_implemented(fmt, args...) \
        if (DEBUG_NOT_IMPLEMENTED) { \
            not_implemented0(fmt, ## args); \
        }
    #define not_implemented_register(offset) \
        if (DEBUG_NOT_IMPLEMENTED) { \
            not_implemented_register0(offset); \
        }
#else
    #define dprintf0(fmt, args...) \
        do {} while (0)
    #define not_implemented(fmt, args...) \
        if (DEBUG_NOT_IMPLEMENTED) { \
            static int once ; \
            if (!once) { \
                once = 1 ; \
                not_implemented0(fmt, ## args) ; \
            } \
        }
    #define not_implemented_register(offset) \
        if (DEBUG_NOT_IMPLEMENTED) { \
            static int once ; \
            if (!once) { \
                once = 1 ; \
                not_implemented_register0(offset) ; \
            } \
        }
#endif
#define dprintf(fmt, args...) dprintf0(CPU_NAME ": " fmt, ## args)

/* memory mapped registers (IMMR) */
#define IMMR_SIZE 0x40000
#define IMMR_BASE 0xff400000 /* TODO: address from device tree */
#define IMMRBAR_DEFAULT IMMR_BASE
/* timebase enable */
#define SPCR_OFFSET 0x110
#define SPCR_TBEN_BIT (1 << (31 - 9))
/* software reset */
#define RPR_OFFSET  0x918
#define RPR_RSTE_VALUE 0x52535445
#define RCR_OFFSET  0x91C
#define RCR_SWxR_BITS 3
#define RCER_OFFSET 0x920
#define RCER_CRE_BIT  1

/* memory mapped registers handled by other modules */
#define IPIC_OFFSET    0x0700
#define SERIAL0_OFFSET 0x4500
#define SERIAL1_OFFSET 0x4600
#define ETSEC1_BASE        0x24000


#define CCSBAR_BASE  0xff400000

#if 0
typedef struct mpc83xx_config {
    uint32_t ccsr_init_addr;
    const char *cpu_model;
} mpc83xx_config;
#endif

typedef struct ResetData {
    CPUArchState *env;
    uint32_t  entry;            /* save kernel entry in case of reset */
} ResetData;

static MemoryRegion *ccsr_space;
static uint64_t      ccsr_addr = IMMR_BASE;

#if 0
/* THE object */
typedef struct {

    /* internal storage */
    CPUState *cpu ;
    int immr_index ;
    target_phys_addr_t immr_base ;

    /* registers */
    uint32_t immrbar ;
    uint32_t spcr ;
    uint32_t rcer ;

} MPC83xxState ;
#endif

#if 0
static mpc83xx_config sbc834x_config = {
    .ccsr_init_addr = 0xff400000,
    .cpu_model      = CPU_MODEL,
};
#endif

static uint64_t mpc83xx_immr_read(void *state, target_phys_addr_t address,
                                  unsigned size) {
    CPUArchState *s     = state;
    int           offset;
    uint32_t      value = 0;

    offset = address & 0xfffff;
    if (DEBUG_RW) {
        dprintf("%d-bits read from register 0x%05X address 0x%x\n",
                size << 3, offset, address);
    }
    switch (offset) {
    case SPCR_OFFSET:
        value = s->spcr;
        break ;
    case RCER_OFFSET:
        value = s->rcer;
        break;
    default:
        not_implemented_register(offset);
    }

    return value;
}

static void mpc83xx_immr_write(void *state, target_phys_addr_t address,
                                uint64_t value, unsigned size) {
    CPUArchState *s = state;
    int           offset;
    uint64_t      old_value;

    offset = address & 0xfffff;
    if (DEBUG_RW) {
        dprintf("%d-bits write 0x%08X to register 0x%05X\n",
                size << 3, value, offset);
    }
    switch (offset) {
    case SPCR_OFFSET:
        old_value = s->spcr;
        s->spcr = value;
        if ((old_value ^ value) & SPCR_TBEN_BIT) {
            /* TBEN bit has changed */
            qemu_set_irq(s->irq_inputs[PPC6xx_INPUT_TBEN],
                         value & SPCR_TBEN_BIT);
        }
        break;
    case RPR_OFFSET:
        if (value == RPR_RSTE_VALUE) {
            /* disable reset protection */
            s->rcer = RCER_CRE_BIT;
        }
        break;
    case RCR_OFFSET:
        if (s->rcer && value & RCR_SWxR_BITS) {
            dprintf("software reset\n");
            qemu_system_reset_request();
        }
        break;
    case RCER_OFFSET:
        if (value & RCER_CRE_BIT) {
            /* enable reset protection */
            s->rcer = 0;
        }
        break;
    default:
        not_implemented_register(offset);
    }
}

static const MemoryRegionOps mpc83xx_ops = {
    .write = mpc83xx_immr_write,
    .read  = mpc83xx_immr_read,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void main_cpu_reset(void *opaque)
{
    ResetData *s   = (ResetData *) opaque;
    CPUArchState *cpu   = s->env;

    cpu_state_reset(cpu);

    cpu->nip = s->entry;
}

static void sbc834x_init(ram_addr_t  ram_size,
                         const char *boot_device,
                         const char *kernel_filename,
                         const char *kernel_cmdline,
                         const char *initrd_filename,
                         const char *cpu_model)
{

    CPUArchState *s;
    clk_setup_cb  clock;
#if 0
    RTCState     *rtc;
    ISADevice    *pit;
#endif
    ResetData    *reset_info;
    SerialState  *serial0;
    SerialState  *serial1;
    qemu_irq     *irqs;
    qemu_irq     *devirqs;
    MemoryRegion *ram     = g_new(MemoryRegion, 1);
    MemoryRegion *immr    = g_new(MemoryRegion, 1);
    int32_t       kernel_size;
    uint64_t      kernel_pentry, kernel_lowaddr;
    target_ulong  dt_base = 0;
    int           i;

    /* initialize CPU */
    if (cpu_model == NULL) {
        cpu_model = CPU_MODEL;
    }

    dprintf("initialization with CPU %s\n", cpu_model);

    /* initialize CPU */
    s = cpu_ppc_init(cpu_model);
    if (s == NULL) {
        eprintf("unable to find CPU %s\n", cpu_model);
    }

    /* TODO: check core inputs */
    if (PPC_INPUT(s) != PPC_FLAGS_INPUT_6xx) {
        eprintf("only 6xx bus is supported on " CPU_NAME " microcontroller");
    }

    /* set time-base frequency to 66 Mhz */
    /* should be controlled by M66EN to switch between 33 and 66 */
    clock = cpu_ppc_tb_init(s, 66000000);

    /* configure reset */
    reset_info = g_malloc0(sizeof(ResetData));
    reset_info->env   = s;
    qemu_register_reset(main_cpu_reset, reset_info);

    reset_info->entry = 0x0000010000UL;

    /* allocate RAM */
    memory_region_init_ram(ram, "sbc834x.ram", ram_size);
    memory_region_add_subregion(get_system_memory(), 0x0, ram);

    ccsr_addr = IMMR_BASE;

    /* Configuration, Control, and Status Registers */
    ccsr_space = g_new(MemoryRegion, 1);
    memory_region_init(ccsr_space, "CCSR_space", 0x100000);
    memory_region_add_subregion(get_system_memory(), ccsr_addr, ccsr_space);

    memory_region_init_io(immr, &mpc83xx_ops, s, "IMMR", 0x24000);
    memory_region_add_subregion(ccsr_space, 0, immr);

#if 0
    /* register ISA IO space */
    /* TODO: check ISA, 1 MB ? Why needed ? */
    isa_mmio_init(0xff500000, 0x100000);
#endif

    /* initialize interrupt controller */
    irqs = g_malloc0(sizeof(qemu_irq) * IPIC_OUTPUT_SIZE);
    /* TODO: change irqs pins to e300 ones */
    irqs[IPIC_OUTPUT_INT] = s->irq_inputs[PPC6xx_INPUT_INT];
    irqs[IPIC_OUTPUT_SMI] = s->irq_inputs[PPC6xx_INPUT_SMI];
    irqs[IPIC_OUTPUT_MCP] = s->irq_inputs[PPC6xx_INPUT_MCP];
    devirqs = ipic_init(ccsr_space, IPIC_OFFSET, irqs);
    if (devirqs == NULL) {
        eprintf("cannot initialize IPIC");
    }
#if 0
    /* initialize periodic interval timer */
    /* TODO: check PIT parameters */
    pit = pit_init(0x40, 65);

    /* initialize real time clock */
    /* TODO: check RTC parameters */
    rtc = rtc_init(0x70, devirqs[64], 2000);
#endif

    /* initialize serial lines */
    if (serial_hds[0] != NULL) {
        serial0 = serial_mm_init(ccsr_space, SERIAL0_OFFSET, 0, devirqs[9],
                                 9600, serial_hds[0], DEVICE_BIG_ENDIAN);
        if (serial0 == NULL) {
            eprintf("cannot initialize first serial line");
        }
    }
    if (serial_hds[1] != NULL) {
        serial1 = serial_mm_init(ccsr_space, SERIAL1_OFFSET, 0, devirqs[10],
                                 9600, serial_hds[1], DEVICE_BIG_ENDIAN);
        if (serial1 == NULL) {
            eprintf("cannot initialize second serial line");
        }
    }

    for (i = 0; i < nb_nics; i++) {
        etsec_create(ETSEC1_BASE + 0x1000 * i, ccsr_space,
                     &nd_table[i],
                     devirqs[32 + i * 3],
                     devirqs[33 + i * 3],
                     devirqs[34 + i * 3]);
    }

    if (kernel_filename == NULL) {
        eprintf("boot from device not currently supported");
    }

    /* load kernel */
    dprintf("load %s\n", kernel_filename);
    kernel_size = load_elf(kernel_filename, NULL, NULL, &kernel_pentry,
                            &kernel_lowaddr, NULL, 1, ELF_MACHINE, 0);
    if (kernel_size < 0) {
        eprintf("cannot load kernel '%s'", kernel_filename);
    }

    dt_base = (kernel_size + DTC_LOAD_PAD) & ~DTC_PAD_MASK;

    cpu_synchronize_state(s);

    /* Set initial guest state. */
    s->gpr[1] = kernel_lowaddr + 4 * 1024 * 1024; /* FIXME: sp? */
    s->gpr[3] = dt_base;
    s->nip = kernel_pentry;    /* FIXME: entry? */
    reset_info->entry = kernel_pentry;

#if 0
    /* load initrd */
    if (initrd_filename) {
        initrd_base = INITRD_LOAD_ADDR;
        initrd_size = load_image_targphys(initrd_filename, initrd_base,
                                          ram_size - initrd_base);
        if (initrd_size < 0) {
            eprintf("cannot load initial ram disk '%s'", initrd_filename);
        }
    } else {
        initrd_base = 0;
        initrd_size = 0;
    }
#endif

    /* Initialize plug-ins */
    plugin_init(devirqs, 16);
    plugin_device_init();

    /* Initialize the GnatBus Master */
    gnatbus_master_init(devirqs, 16);
    gnatbus_device_init();

    /* TODO: append command line parameters
     * see linux/arch/powerpc/kernel/prom.c (variable cmd_line) */

    /* start kernel */
    s->nip = kernel_pentry; /* instruction pointer */
    dprintf("start kernel at 0x%08X\n", s->nip);

    /* register functions to suspend/resume the current state */
    /* TODO: functions to save/load VM */
    /* register_savevm(DEVICE, 0, VERSION, mpc83xx_save, mpc83xx_load, s); */
}

static QEMUMachine sbc834x_machine = {
    .name = BOARD_NAME,
    .desc = BOARD_DESC,
    .init = sbc834x_init,
    .max_cpus = MAX_CPUS
};

static void sbc834x_machine_init(void)
{
    qemu_register_machine(&sbc834x_machine);
}

machine_init(sbc834x_machine_init);

