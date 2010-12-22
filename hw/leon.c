/*
 * QEMU Leon2 System Emulator
 *
 * Copyright (c) 2009-2011 AdaCore
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
#include "qemu-timer.h"
#include "qemu-char.h"
#include "sysemu.h"
#include "boards.h"

//#define DEBUG_IO
//#define DEBUG_LEON
//#define DEBUG_LEON_TIMER

#ifdef DEBUG_LEON
#define DPRINTF(fmt, args...)  printf("Leon: " fmt , ##args)
#else
#define DPRINTF(fmt, args...)
#endif

#ifdef DEBUG_LEON_TIMER
#define DTIMER(fmt, args...)  printf("Leon-timer: " fmt , ##args)
#else
#define DTIMER(fmt, args...)
#endif

/* Default system clock.  */
#define CPU_CLK (50 * 1000 * 1000)

/* Leon registers.  */
#define MCFG1  0x00             /* Memory Configuration Register 1 */
#define MCFG2  0x04             /* Memory Configuration Register 2 */
#define MCFG3  0x08             /* Memory Configuration Register 3 */
#define FAILSR 0x10             /* Fail Status Register */

#define CCR      0x14           /* Cache Control Register */
#define CCR_MASK 0x00e13fff
#define CCR_INIT 0xf7100000

#define TIMC1   0x40            /* Timer 1 Counter Register */
#define TIMR1   0x44            /* Timer 1 Reload Register */
#define TIMCTR1 0x48            /* Timer 1 Control Register */
#define WDG     0x4c            /* Watchdog Register */
#define TIMC2   0x50            /* Timer 2 Counter Register */
#define TIMR2   0x54            /* Timer 2 Reload Register */
#define TIMCTR2 0x58            /* Timer 2 Control Register */
#define SCAC    0x60            /* Prescaler Counter Register */
#define SCAR    0x64            /* Prescaler Reload Register */

#define UAD1   0x70             /* UART 1 Data Register */
#define UAS1   0x74             /* UART 1 Status Register */
#define UAC1   0x78             /* UART 1 Control Register */
#define UASCA1 0x7c             /* UART 1 Scaler Register */
#define UAD2   0x80             /* UART 2 Data Register */
#define UAS2   0x84             /* UART 2 Status Register */
#define UAC2   0x88             /* UART 2 Control Register */
#define UASCA2 0x8c             /* UART 2 Scaler Register */

#define ITMP 0x90               /* Interrupt Mask and Priority Register */
#define ITP  0x94               /* Interrupt Pending Register */
#define ITF  0x98               /* Interrupt Force Register */
#define ITC  0x9c               /* Interrupt Clear Register */

#define IODAT 0xa0              /* I/O Port Data Register */
#define IODIR 0xa4              /* I/O Port Direction Register */
#define IOIT  0xa8              /* I/O Port Interrupt Register */

#define UAS_DR 0x01             /* Data ready */
#define UAS_TS 0x02             /* Transmitter shift register empty */
#define UAS_TH 0x04             /* Transmitter hold register empty */
#define UAS_BR 0x08             /* Break received */
#define UAS_OV 0x10             /* Overrun */
#define UAS_PE 0x20             /* Parity error */
#define UAS_FE 0x40             /* Framing error */

#define UAC_RE 0x001            /* Receiver enable */
#define UAC_TE 0x002            /* Transmitter enable */
#define UAC_RI 0x004            /* Receiver interrupt enable */
#define UAC_TI 0x008            /* Transmitter interrupt enable */
#define UAC_PS 0x010            /* Parity select */
#define UAC_PE 0x020            /* Parity enable */
#define UAC_FL 0x040            /* Flow control */
#define UAC_LB 0x080            /* Loop back */
#define UAC_EC 0x100            /* External Clock */

#define TIMCTR_EN   0x01        /* Enable counter */
#define TIMCTR_RL   0x02        /* Reload counter */
#define TIMCTR_LD   0x04        /* Load counter */
#define TIMCTR_MASK 0x07

#define PROM_FILENAME        "u-boot.bin"

#define MAX_PILS 16

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;

    cpu_reset(env);
    env->halted = 0;
}

typedef struct LeonUartState
{
    uint32_t uas;
    uint32_t uac;
    uint32_t uasca;
    uint8_t rcv;
    CharDriverState *chr;
    qemu_irq irq;
} LeonUartState;

struct LeonTimerState
{
    QEMUBH *bh;
    struct ptimer_state *ptimer;

    qemu_irq irq;

    int id;

    /* registers */
    uint32_t counter;
    uint32_t reload;
    uint32_t control;
};

struct LeonIntState {
    uint32_t itmp;
    uint32_t itp;
    uint32_t itf;
    CPUState *env;
};

typedef struct LeonIoState
{
    uint32_t mcfg[3];

    struct LeonIntState intctl;

    uint32_t ccr;
    uint32_t scar;
    uint32_t wdg;
    uint32_t iodata;
    uint32_t iodir;
    uint32_t ioit;
    struct LeonTimerState timr1;
    struct LeonTimerState timr2;
    LeonUartState uart1;
    LeonUartState uart2;
} LeonIoState;

static struct LeonIntState *leon_intctl;

static void leon_check_irqs(struct LeonIntState *s)
{
    uint32_t pend = (s->itp | s->itf) & s->itmp;
    uint32_t m;
    int i;
    int num = 0;
    CPUSPARCState *env = s->env;

    /* First level 1 */
    m = pend & (s->itmp >> 16);
    if (m != 0) {
	for (i = 15; i != 0; i--)
	    if (m & (1 << i)) {
		num = i;
		break;
	    }
    }
    /* Level 0 */
    if (num == 0) {
	m = pend & ~(s->itmp >> 16);
	if (m != 0) {
	    for (i = 15; i != 0; i--)
		if (m & (1 << i)) {
		    num = i;
		    break;
		}
	}
    }

#if 0
    qemu_log("Leon2 check interrupt: num=%d int_index=0x%02x psrpil=%d "
	     "pend=%04x itp=%04x, itmp=%04x\n",
	     num, env->interrupt_index, env->psrpil, pend, s->itp, s->itmp);
#endif

    if (num && (env->interrupt_index == 0 ||
		(env->interrupt_index & ~15) == TT_EXTINT)) {
        /* Either no interrupt in service or external interrupt pending.  */
	int old_interrupt = env->interrupt_index;

	env->interrupt_index = TT_EXTINT | num;
	if (old_interrupt != env->interrupt_index) {
	    DPRINTF("Set CPU IRQ %d\n", num);
	    cpu_interrupt(env, CPU_INTERRUPT_HARD);
	}
    } else if (!num && (env->interrupt_index & ~15) == TT_EXTINT) {
        /* Interrupt reset before being serviced by CPU.  */
        DPRINTF("Reset CPU IRQ %d\n", env->interrupt_index & 15);
        env->interrupt_index = 0;
        cpu_reset_interrupt(env, CPU_INTERRUPT_HARD);
    }
}

void leon2_intctl_ack(CPUSPARCState *env, int intno)
{
    uint32_t mask;

    intno &= 15;
    mask = 1 << intno;

    DPRINTF ("intctl ack %d\n", intno);

    /* Clear registers.  */
    leon_intctl->itp &= ~mask;
    leon_intctl->itf &= ~mask;

    leon_check_irqs(leon_intctl);
}

static void leon_set_irq(void *opaque, int irq, int level)
{
    struct LeonIntState *s = opaque;

    if (level) {
        DPRINTF("Raise CPU IRQ %d\n", irq);
	s->itp |= 1 << irq;
        leon_check_irqs(s);
    } else {
        DPRINTF("Lower CPU IRQ %d\n", irq);
        s->itp &= ~(1 << irq);
	leon_check_irqs(s);
    }
}

static void leon_uart_check_irq(struct LeonUartState *s)
{
    if (((s->uas & UAS_DR) && (s->uac & UAC_RI))
	|| (!(s->uas & UAS_TH) && (s->uac & UAC_TI)))
	qemu_set_irq(s->irq, 1);
    else
	qemu_set_irq(s->irq, 0);
}

static int leon_uart_can_receive(void *opaque)
{
    LeonUartState *s = opaque;
    return (s->uas & UAS_DR) ? 0 : 1;
}

static void leon_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    LeonUartState *s = opaque;
    s->rcv = *buf;
    s->uas |= UAS_DR;
    leon_uart_check_irq(s);
}

static void leon_uart_event(void *opaque, int event)
{
#ifdef DEBUG_UART
    printf("Leon uart: event %x\n", event);
#endif
}

static uint32_t leon_uart_read_uad(struct LeonUartState *s)
{
    if (s->uas & UAS_DR) {
	s->uas &= ~UAS_DR;
	leon_uart_check_irq(s);
    }
    return s->rcv;
}

static void leon_uart_init (CharDriverState *chr,
			    struct LeonUartState *s, qemu_irq irq)
{
    s->chr = chr;
    s->uac = 0;
    s->irq = irq;
    s->uas = UAS_TS | UAS_TH;

    qemu_chr_add_handlers(s->chr, leon_uart_can_receive, leon_uart_receive,
                          leon_uart_event, s);
}


static void leon_timer_enable(struct LeonTimerState *s)
{
    ptimer_stop(s->ptimer);

    if (s->control & TIMCTR_LD) {
        /* reload */
        s->counter = s->reload;
    }

    if (!(s->control & TIMCTR_EN)) {
        /* Timer disabled */
        DTIMER("%s id:%d Timer disabled (control 0x%x)\n",
               __func__, s->id, s->control);
        return;
    }

    /* ptimer is triggered when the counter reach 0 but GPTimer is triggered at
       underflow. Set count + 1 to simulate the GPTimer behavior. */

    DTIMER("%s id:%d set count 0x%x and run\n",
           __func__, s->id, s->counter + 1);

    ptimer_set_count(s->ptimer, s->counter + 1);
    ptimer_run(s->ptimer, 1);
}

static void leon_timer_hit(void *opaque)
{
    struct LeonTimerState *s = opaque;

    DTIMER("%s id:%d\n", __func__, s->id);

    qemu_set_irq(s->irq, 1);

    if (s->control & TIMCTR_RL) {
        /* reload */
        s->control |= TIMCTR_LD;
        leon_timer_enable(s);
    }
}

static uint32_t leon_timer_io_read(void *opaque, target_phys_addr_t addr)
{
    LeonIoState *s = opaque;
    uint32_t ret;

    switch (addr) {
        case SCAC:
            break;
        case SCAR:
            ret = s->scar;
            break;

        case TIMC1:
            ret = ptimer_get_count (s->timr1.ptimer);
            break;

        case TIMC2:
            ret = ptimer_get_count (s->timr2.ptimer);
            break;

        case TIMR1:
            ret = s->timr1.reload;
            break;
        case TIMR2:
            ret = s->timr2.reload;
            break;


        case TIMCTR1:
            ret = s->timr1.control;
            break;
        case TIMCTR2:
            ret = s->timr2.control;
            break;

        case WDG:
            ret = s->wdg;
            break;


        default:
            DTIMER("read unknown register " TARGET_FMT_plx "\n", addr);
            return 0;
    }

    DTIMER("read reg 0x%02x = %x\n", (unsigned)addr, ret);

    return ret;
}

static void leon_timer_io_write(LeonIoState *s, target_phys_addr_t addr,
                                uint32_t val)
{
    DTIMER("write reg " TARGET_FMT_plx " = %x\n", addr, val);

    switch (addr) {
        case SCAC:
            break;
        case SCAR:
            DTIMER("Set scalar %d\n", val);
            s->scar = val & 0x3ff;
            val = CPU_CLK / (s->scar + 1);
            ptimer_set_freq(s->timr1.ptimer, val);
            ptimer_set_freq(s->timr2.ptimer, val);
            break;

        case TIMC1:
            s->timr1.counter = val & 0x00ffffff;
            leon_timer_enable(&s->timr1);
            break;
        case TIMC2:
            s->timr2.counter = val & 0x00ffffff;
            leon_timer_enable(&s->timr2);
            break;

        case TIMR1:
            s->timr1.reload = val & 0x00ffffff;
            break;
        case TIMR2:
            s->timr2.reload = val & 0x00ffffff;
            break;


        case TIMCTR1:
            s->timr1.control = val & TIMCTR_MASK;
            leon_timer_enable(&s->timr1);
            break;
        case TIMCTR2:
            s->timr2.control = val & TIMCTR_MASK;
            leon_timer_enable(&s->timr2);
            break;

        case WDG:
            s->wdg = val & 0x00ffffff;
            break;

        default:
            DTIMER("write unknown register " TARGET_FMT_plx "\n", addr);
            break;
    }
}

static void leon_timer_init(struct LeonTimerState *s, qemu_irq irq, int id)
{
    s->id      = id;
    s->counter = 0;
    s->reload  = 0;
    s->control = 0;
    s->irq     = irq;
    s->bh      = qemu_bh_new(leon_timer_hit, s);
    s->ptimer  = ptimer_init(s->bh);

    ptimer_set_freq(s->ptimer, CPU_CLK);

}

static uint32_t leon_io_readl(void *opaque, target_phys_addr_t addr)
{
    LeonIoState *s = opaque;
    uint32_t ret;

    switch (addr) {
        case MCFG1:
        case MCFG2:
        case MCFG3:
            ret = s->mcfg[(addr - MCFG1) >> 2];
            break;
        case FAILSR:
            ret = 0;
            break;
        case CCR:
            ret = s->ccr;
            break;
        case ITMP:
            ret = s->intctl.itmp;
            break;
        case ITP:
            ret = s->intctl.itp;
            break;
        case ITF:
            ret = s->intctl.itf;
            break;
        case ITC:
            ret = 0;
            break;
        case UAD1:
            ret = leon_uart_read_uad (&s->uart1);
            break;
        case UAC1:
            ret = s->uart1.uac;
            break;
        case UASCA1:
            ret = s->uart1.uasca;
            break;
        case UAS1:
            ret = s->uart1.uas;
            break;

        case TIMR1 ... SCAR:
            ret = leon_timer_io_read(s, addr);
            break;

        case IODAT:
            ret = s->iodata;
            break;
        case IODIR:
            ret = s->iodir;
            break;
        case IOIT:
            ret = s->ioit;
            break;
        default:
            printf ("Leon: read unknown register 0x%04x\n", (int)addr);
            ret = 0;
            break;
    }

    DPRINTF("read reg 0x%02x = %x\n", (unsigned)addr, ret);

    return ret;
}

static void leon_io_writel(void *opaque, target_phys_addr_t addr,
			   uint32_t val)
{
    LeonIoState *s = opaque;

    DPRINTF("write reg 0x%02x = %x\n", (unsigned)addr, val);

    switch (addr) {
        case MCFG1:
        case MCFG2:
        case MCFG3:
            s->mcfg[(addr - MCFG1) >> 2] = val;
            break;
        case FAILSR:
            break;
        case CCR:
            s->ccr = (val & CCR_MASK) | (s->ccr & ~CCR_MASK);
            break;
        case ITMP:
            s->intctl.itmp = val;
            break;
        case ITF:
            s->intctl.itf = val & 0xfffe;
            leon_check_irqs(&s->intctl);
            break;
        case UAC1:
            s->uart1.uac = val & 0x1ff;
            break;
        case UASCA1:
            s->uart1.uasca = val & 0x3ff;
            break;
        case UAD1:
        {
            unsigned char c = val;
            qemu_chr_write(s->uart1.chr, &c, 1);
        }
        break;

        case TIMR1 ... SCAR:
            leon_timer_io_write(s, addr, val);
            break;

        case IODAT:
            s->iodata = val & 0xffff;
            break;
        case IODIR:
            s->iodir = val & 0x3ffff;
            break;
        case IOIT:
            s->ioit = val;
            break;

        default:
            printf ("Leon: write unknown register 0x%04x=%x\n", (int)addr, val);
    }
}

static CPUReadMemoryFunc *leon_io_read[3] = {
    NULL,
    NULL,
    leon_io_readl
};

static CPUWriteMemoryFunc *leon_io_write[3] = {
    NULL,
    NULL,
    leon_io_writel,
};

static void at697_hw_init(ram_addr_t ram_size,
			  const char *boot_device,
			  const char *kernel_filename,
			  const char *kernel_cmdline,
			  const char *initrd_filename,
                          const char *cpu_model)
{
    CPUState *env;
    ram_addr_t ram_offset, prom_offset;
    ram_addr_t ram2_size, ram2_offset;
    int ret;
    char *filename;
    qemu_irq *cpu_irqs;
    int bios_size;
    int aligned_bios_size;
    int leon_io_memory;
    LeonIoState *s;

    /* init CPU */
    if (!cpu_model)
	cpu_model = "LEON2";

    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "qemu: Unable to find Sparc CPU definition\n");
        exit(1);
    }

    cpu_sparc_set_intctl(env, intctl_leon2);
    cpu_sparc_set_id(env, 0);

    qemu_register_reset(main_cpu_reset, env);

    s = qemu_mallocz(sizeof(struct LeonIoState));
    leon_intctl = &s->intctl;
    leon_intctl->env = env;
    s->ccr = CCR_INIT;

    cpu_irqs = qemu_allocate_irqs(leon_set_irq, leon_intctl, MAX_PILS);

    /* allocate RAM */
    if ((uint64_t)ram_size > (1UL << 30)) {
        fprintf(stderr,
                "qemu: Too much memory for this machine: %d, maximum 1G\n",
                (unsigned int)(ram_size / (1024 * 1024)));
        exit(1);
    }
    ram_offset = qemu_ram_alloc(ram_size);
    cpu_register_physical_memory(0x40000000, ram_size, ram_offset);

    /* Allocate RAM2.  */
    ram2_size = 8 << 20;
    ram2_offset = qemu_ram_alloc(ram2_size);
    cpu_register_physical_memory(0x20000000, ram2_size, ram2_offset);

    /* load boot prom */
    if (bios_name == NULL)
        bios_name = PROM_FILENAME;
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    bios_size = get_image_size(filename);
    if (bios_size > 0) {
	aligned_bios_size =
	    (bios_size + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK;
	prom_offset = qemu_ram_alloc(aligned_bios_size);
	cpu_register_physical_memory(0x00000000, aligned_bios_size,
				     prom_offset /* | IO_MEM_ROM */);
        ret = load_image_targphys(filename, 0x00000000, bios_size);
	if (ret < 0 || ret > bios_size) {
	    fprintf(stderr, "qemu: could not load prom '%s'\n", filename);
	    exit(1);
	}
    }
    else if (kernel_filename == NULL) {
      fprintf(stderr,"Can't read bios image %s\n", filename);
      exit(1);
    }

    leon_io_memory = cpu_register_io_memory(leon_io_read, leon_io_write, s);
    cpu_register_physical_memory(0x80000000, 0x1000, leon_io_memory);

    leon_timer_init (&s->timr1, cpu_irqs[8], 1 /* id */);
    leon_timer_init (&s->timr2, cpu_irqs[9], 2 /* id */);

    if (serial_hds[0])
	leon_uart_init (serial_hds[0], &s->uart1, cpu_irqs[3]);
    if (serial_hds[1])
	leon_uart_init (serial_hds[1], &s->uart2, cpu_irqs[2]);

    /* Can directly load an application. */
    if (kernel_filename != NULL) {
	long kernel_size;
	uint64_t entry;

	kernel_size = load_elf(kernel_filename, 0, &entry, NULL, NULL);
	if (kernel_size < 0) {
	    fprintf(stderr, "qemu: could not load kernel '%s'\n",
		    kernel_filename);
	    exit(1);
	}
	if (bios_size <= 0) {
	    /* If there is no bios/monitor, start the application.  */
	    env->pc = entry;
	    env->npc = entry + 4;
	}
    }
}

QEMUMachine at697_machine = {
    .name = "at697",
    .desc = "Leon-2 Atmel 697",
    .init = at697_hw_init,
    .use_scsi = 0,
};

static void leon_machine_init(void)
{
    qemu_register_machine(&at697_machine);
}

machine_init(leon_machine_init);
