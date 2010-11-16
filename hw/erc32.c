/*
 * QEMU Erc32 System Emulator
 *
 * Copyright (C) 2010 AdaCore
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

#define DEBUG_IO

#ifdef DEBUG_IO
#define DPRINTF(fmt, args...)  qemu_log("erc32: " fmt , ##args)
#else
#define DPRINTF(fmt, args...)
#endif

/* Default system clock.  */
#define SYSCLK (20 * 1000 * 1000)

/* ERC32 registers.  */
#define SYSCTR 0x00
#define SWRST  0x04
#define PDOWN  0x08
#define MCNFR  0x10

#define INTSHR 0x44
#define INTPDR 0x48
#define INTMKR 0x4c
#define INTCLR 0x50
#define INTFCR 0x54

#define RTCCR  0x80
#define RTCSR  0x84
#define GPTCR  0x88
#define GPTSR  0x8c

#define UARTAR 0xe0
#define UARTBR 0xe4
#define UARTSR 0xe8
#define  UART_DR  (1 << 0)
#define  UART_TS  (1 << 1)
#define  UART_THE (1 << 2)

/* Leon registers.  */
#define MCFG1 0x00
#define MCFG2 0x04
#define MCFG3 0x08
#define FAILSR 0x10

#define CCR   0x14
#define CCR_MASK 0x00e13fff
#define CCR_INIT 0xf7100000

#define TIMC1 0x40
#define TIMR1 0x44
#define TIMCTR1 0x48
#define WDG 0x4c
#define TIMC2 0x50
#define TIMR2 0x54
#define TIMCTR2 0x58
#define SCAC 0x60
#define SCAR 0x64

#define UAD1 0x70
#define UAS1 0x74
#define UAC1 0x78
#define UASCA1 0x7c
#define UAD2 0x80
#define UAS2 0x84
#define UAC2 0x88
#define UASCA2 0x8c

#define ITMP 0x90
#define ITP 0x94
#define ITF 0x98
#define ITC 0x9c

#define IODAT 0xa0
#define IODIR 0xa4
#define IOIT 0xa8

#define UAS_DR 0x01
#define UAS_TS 0x02
#define UAS_TH 0x04
#define UAS_BR 0x08
#define UAS_OV 0x10
#define UAS_PE 0x20
#define UAS_FE 0x40

#define UAC_RE 0x01
#define UAC_TE 0x02
#define UAC_RI 0x04
#define UAC_TI 0x08
#define UAC_PS 0x10
#define UAC_PE 0x20
#define UAC_FL 0x40
#define UAC_LB 0x80
#define UAC_EC 0x100

#define TIMCTR_EN 0x01
#define TIMCTR_RL 0x02
#define TIMCTR_LD 0x04

#define PROM_FILENAME        "u-boot.bin"

#define MAX_PILS 16

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;

    cpu_reset(env);
    env->halted = 0;
}

typedef struct Erc32UartState
{
    uint32_t sr;
    uint8_t rcv;
    CharDriverState *chr;
    qemu_irq irq;
} Erc32UartState;

struct Erc32TimerState
{
    uint32_t sr;
    uint32_t cr;

    /* FIXME: mask.  */

    QEMUBH *bh;
    struct ptimer_state *ptimer;
    qemu_irq irq;
};

struct Erc32IntState {
    uint32_t intmkr;
    uint32_t intpdr;
    uint32_t intfcr;
    CPUState *env;
};

typedef struct Erc32IoState
{
    struct Erc32IntState intctl;

    uint32_t ccr;
    uint32_t scar;
    uint32_t wdg;
    uint32_t iodata;
    uint32_t iodir;
    uint32_t ioit;
    struct Erc32TimerState rttimer;
    struct Erc32TimerState gptimer;
    Erc32UartState uarta;
    Erc32UartState uartb;
} Erc32IoState;

static struct Erc32IntState *erc32_intctl;

static void erc32_check_irqs(struct Erc32IntState *s)
{
    uint32_t pend = (s->intpdr | s->intfcr) & ~s->intmkr;
    uint32_t m;
    int i;
    int num = 0;
    CPUSPARCState *env = s->env;

    /* FIXME: bit 'it'.  */
    m = pend;
    if (m != 0) {
	for (i = 15; i != 0; i--)
	    if (m & (1 << i)) {
		num = i;
		break;
	    }
    }

#if 0
    qemu_log("Erc32 check interrupt: num=%d int_index=0x%02x psrpil=%d "
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

void erc32_intctl_ack(CPUSPARCState *env, int intno)
{
    uint32_t mask;

    intno &= 15;
    mask = 1 << intno;

    DPRINTF ("intctl ack %d\n", intno);

    /* Clear registers.  */
    erc32_intctl->intpdr &= ~mask;
    erc32_intctl->intfcr &= ~mask;

    erc32_check_irqs(erc32_intctl);
}

static void erc32_set_irq(void *opaque, int irq, int level)
{
    struct Erc32IntState *s = opaque;

    if (level) {
        DPRINTF("Raise CPU IRQ %d\n", irq);
	s->intpdr |= 1 << irq;
        erc32_check_irqs(s);
    } else {
        DPRINTF("Lower CPU IRQ %d\n", irq);
        s->intpdr &= ~(1 << irq);
	erc32_check_irqs(s);
    }
}

static void erc32_uart_check_irq(struct Erc32UartState *s)
{
    if ((s->sr & UART_DR) || !(s->sr & UART_THE))
	qemu_set_irq(s->irq, 1);
    else
	qemu_set_irq(s->irq, 0);
}

static int erc32_uart_can_receive(void *opaque)
{
    Erc32UartState *s = opaque;
    return (s->sr & UART_DR) ? 0 : 1;
}

static void erc32_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    Erc32UartState *s = opaque;
    s->rcv = *buf;
    s->sr |= UART_DR;
    erc32_uart_check_irq(s);
}

static void erc32_uart_event(void *opaque, int event)
{
#ifdef DEBUG_UART
    printf("erc32 uart: event %x\n", event);
#endif
}

static uint32_t erc32_uart_read_reg(struct Erc32UartState *s)
{
    if (s->sr & UART_DR) {
	s->sr &= ~UART_DR;
	erc32_uart_check_irq(s);
    }
    return s->rcv;
}

static void erc32_uart_init (CharDriverState *chr,
                             struct Erc32UartState *s, qemu_irq irq)
{
    s->chr = chr;
    s->irq = irq;
    s->sr = UART_TS | UART_THE;

    qemu_chr_add_handlers(s->chr, erc32_uart_can_receive, erc32_uart_receive,
                          erc32_uart_event, s);
}

static void erc32_timer_bh(void *opaque)
{
    struct Erc32TimerState *s = opaque;

    DPRINTF("timer bh\n");
    qemu_set_irq(s->irq, 1);
}

static uint32_t erc32_timer_read_cr (struct Erc32TimerState *s)
{
    return ptimer_get_count (s->ptimer);
}

static uint32_t erc32_timer_read_sr (struct Erc32TimerState *s)
{
    return s->sr;
}

static void erc32_timer_write_cr (struct Erc32TimerState *s, uint32_t val)
{
    s->cr = val;
    if (s->cr == 0)
        ptimer_stop(s->ptimer);
    else {
        ptimer_set_limit(s->ptimer, s->cr + (s->sr == 0 ? 1 : 0), 1);
        ptimer_run(s->ptimer, 0);
    }
}

static void erc32_timer_write_sr (struct Erc32TimerState *s, uint32_t val)
{
    s->sr = val;
    if (s->sr > 0)
        ptimer_set_freq(s->ptimer, SYSCLK / (s->sr + 1));
    else
        ptimer_set_freq(s->ptimer, SYSCLK);
}

static void erc32_timer_init (struct Erc32TimerState *s, qemu_irq irq)
{
    s->bh = qemu_bh_new(erc32_timer_bh, s);
    s->ptimer = ptimer_init(s->bh);
    s->irq = irq;
}

static uint32_t erc32_io_readl(void *opaque, target_phys_addr_t addr)
{
    Erc32IoState *s = opaque;
    uint32_t ret;

    switch (addr) {
#if 0
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
    case WDG:
	ret = s->wdg;
	break;
    case SCAR:
	ret = s->scar;
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

    case TIMR1:
	ret = s->timr1.rld;
	break;
    case TIMR2:
	ret = s->timr2.rld;
	break;
    case TIMCTR1:
	ret = s->timr1.ctr;
	break;
    case TIMCTR2:
	ret = s->timr2.ctr;
	break;
    case TIMC1:
	ret = leon_timer_read_counter (&s->timr1);
	break;
    case TIMC2:
	ret = leon_timer_read_counter (&s->timr2);
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
#endif

    case INTMKR:
	ret = s->intctl.intmkr;
	break;
    case INTFCR:
	ret = s->intctl.intfcr;
	break;

    case RTCCR:
        ret = erc32_timer_read_cr(&s->rttimer);
        break;
    case RTCSR:
        ret = erc32_timer_read_sr(&s->rttimer);
        break;
    case GPTCR:
        ret = erc32_timer_read_cr(&s->gptimer);
        break;
    case GPTSR:
        ret = erc32_timer_read_sr(&s->gptimer);
        break;

    case UARTAR:
        ret = erc32_uart_read_reg(&s->uarta);
        break;
    case UARTBR:
        ret = erc32_uart_read_reg(&s->uartb);
        break;
    case UARTSR:
        ret = s->uarta.sr | (s->uartb.sr << 16);
        break;

    default:
	printf ("erc32: read unknown register 0x%04x\n", (int)addr);
	ret = 0;
	break;
	}

    DPRINTF("read reg 0x%02x = %x\n", (unsigned)addr, ret);

    return ret;
}

static void erc32_io_writel(void *opaque, target_phys_addr_t addr,
			   uint32_t val)
{
    Erc32IoState *s = opaque;

    DPRINTF("write reg 0x%02x = %x\n", (unsigned)addr, val);

    switch (addr) {
#if 0
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
    case WDG:
	s->wdg = val & 0x00ffffff;
	break;
    case SCAR:
	s->scar = val & 0x3ff;
	s->timr1.scar = s->scar;
	s->timr2.scar = s->scar;
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

    case SCAC:
	break;
    case TIMR1:
	s->timr1.rld = val & 0x00ffffff;
	break;
    case TIMR2:
	s->timr2.rld = val & 0x00ffffff;
	break;
    case TIMC1:
	break;
    case TIMCTR1:
	leon_write_timctr (&s->timr1, val);
	break;
    case TIMCTR2:
	leon_write_timctr (&s->timr2, val);
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
#endif

    case INTMKR:
	s->intctl.intmkr = val & 0x7ffe;
	erc32_check_irqs(&s->intctl);
	break;
    case INTFCR:
	s->intctl.intfcr = val & 0xfffe;
	erc32_check_irqs(&s->intctl);
	break;

    case RTCCR:
        erc32_timer_write_cr(&s->rttimer, val);
        break;
    case RTCSR:
        erc32_timer_write_sr(&s->rttimer, val);
        break;
    case GPTCR:
        erc32_timer_write_cr(&s->gptimer, val);
        break;
    case GPTSR:
        erc32_timer_write_sr(&s->gptimer, val);
        break;

    case UARTAR:
        {
	    unsigned char c = val;
	    qemu_chr_write(s->uarta.chr, &c, 1);
	}
	break;

    default:
	printf ("erc32: write unknown register 0x%04x=%x\n", (int)addr, val);
    }
}

static CPUReadMemoryFunc *erc32_io_read[3] = {
    NULL,
    NULL,
    erc32_io_readl
};

static CPUWriteMemoryFunc *erc32_io_write[3] = {
    NULL,
    NULL,
    erc32_io_writel,
};

static void tsc695_hw_init(ram_addr_t ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{
    CPUState *env;
    ram_addr_t ram_offset, prom_offset;
    int ret;
    char *filename;
    qemu_irq *cpu_irqs;
    int bios_size;
    int aligned_bios_size;
    int erc32_io_memory;
    Erc32IoState *s;

    /* init CPU */
    if (!cpu_model)
	cpu_model = "ERC32";

    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "qemu: Unable to find Sparc CPU definition\n");
        exit(1);
    }

    cpu_sparc_set_intctl(env, intctl_erc32);
    cpu_sparc_set_id(env, 0);

    qemu_register_reset(main_cpu_reset, env);

    s = qemu_mallocz(sizeof(struct Erc32IoState));
    erc32_intctl = &s->intctl;
    erc32_intctl->env = env;
    erc32_intctl->intmkr = 0x7ffe;

    cpu_irqs = qemu_allocate_irqs(erc32_set_irq, erc32_intctl, MAX_PILS);

    /* allocate RAM */
    if ((uint64_t)ram_size > (256UL << 20)) {
        fprintf(stderr,
                "qemu: Too much memory for this machine: %d, maximum 256M\n",
                (unsigned int)(ram_size / (1024 * 1024)));
        exit(1);
    }
    ram_offset = qemu_ram_alloc(ram_size);
    cpu_register_physical_memory(0x02000000, ram_size, ram_offset);

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

    erc32_io_memory = cpu_register_io_memory(erc32_io_read, erc32_io_write, s);
    cpu_register_physical_memory(0x01f80000, 0x1000, erc32_io_memory);

    erc32_timer_init (&s->rttimer, cpu_irqs[13]);
    erc32_timer_init (&s->gptimer, cpu_irqs[12]);

    if (serial_hds[0])
	erc32_uart_init (serial_hds[0], &s->uarta, cpu_irqs[4]);
    if (serial_hds[1])
	erc32_uart_init (serial_hds[1], &s->uartb, cpu_irqs[5]);

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

QEMUMachine tsc695_machine = {
    .name = "tsc695",
    .desc = "ERC32 Atmel 695",
    .init = tsc695_hw_init,
    .use_scsi = 0,
};

static void erc32_machine_init(void)
{
    qemu_register_machine(&tsc695_machine);
}

machine_init(erc32_machine_init);
