/*
 * QEMU Erc32 System Emulator
 *
 * Copyright (C) 2010-2011 AdaCore
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
#include "loader.h"
#include "elf.h"
#include "trace.h"

/* Default system clock.  */
#define SYSCLK (20 * 1000 * 1000)

/* ERC32 registers.  */
#define SYSTEM_CONTROL_REGISTER 0x00
#define SOFTWARE_RESET          0x04
#define POWER_DOWN              0x08
#define MEMORY_CONFIGURATION    0x10

/* Interrupt controller */
#define INTERRUPT_PENDING_REGISTER 0x48
#define INTERRUPT_MASK_REGISTER    0x4c
#define INTERRUPT_CLEAR_REGISTER   0x50
#define INTERRUPT_FORCE_REGISTER   0x54

/* Real-Time Clock and General-Purpose Timer */
#define RTC_COUNTER_REGISTER 0x80
#define RTC_SCALER_REGISTER  0x84
#define GPT_COUNTER_REGISTER 0x88
#define GPT_SCALER_REGISTER  0x8c

/* UART */
#define UART_A_DATA          0xe0
#define UART_B_DATA          0xe4
#define UART_STATUS_REGISTER 0xe8
#define UART_DR              (1 << 0)
#define UART_TS              (1 << 1)
#define UART_THE             (1 << 2)

#define PROM_FILENAME        "u-boot.bin"

#define MAX_PILS 16

typedef struct ResetData {
    CPUState *env;
    uint32_t  entry;            /* save kernel entry in case of reset */
} ResetData;

static void main_cpu_reset(void *opaque)
{
    ResetData *s   = (ResetData *)opaque;
    CPUState  *env = s->env;

    cpu_reset(env);

    env->halted = 0;
    env->pc     = s->entry;
    env->npc    = s->entry + 4;
}

typedef struct Erc32UartState {
    uint32_t         status;
    uint8_t          rcv;
    CharDriverState *chr;
    qemu_irq         irq;
} Erc32UartState;

struct Erc32TimerState {
    uint32_t scaler;
    uint32_t control;

    int id;

    /* FIXME: mask.  */

    QEMUBH *bh;
    struct ptimer_state *ptimer;
    qemu_irq irq;
};

struct Erc32IntState {
    uint32_t mask;
    uint32_t pending;
    uint32_t force;
    CPUState *env;
};

typedef struct Erc32IoState {
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

static void erc32_check_irqs(struct Erc32IntState *s)
{
    uint32_t       pend = (s->pending | s->force) & ~s->mask;
    CPUSPARCState *env  = s->env;

    trace_erc32_check_irqs(s->pending, s->force, s->mask, pend);

    env->pil_in = pend;

    if (env->pil_in && (env->interrupt_index == 0 ||
                        (env->interrupt_index & ~15) == TT_EXTINT)) {
        unsigned int i;

        for (i = 15; i > 0; i--) {
            if (env->pil_in & (1 << i)) {
                int old_interrupt = env->interrupt_index;

                env->interrupt_index = TT_EXTINT | i;
                if (old_interrupt != env->interrupt_index) {
                    trace_erc32_set_irq(i);
                    cpu_interrupt(env, CPU_INTERRUPT_HARD);
                }
                break;
            }
        }
    } else if (!env->pil_in && (env->interrupt_index & ~15) == TT_EXTINT) {
        trace_erc32_reset_irq(env->interrupt_index & 15);
        env->interrupt_index = 0;
        cpu_reset_interrupt(env, CPU_INTERRUPT_HARD);
    }
}

static void erc32_intctl_ack(void *irq_manager, int intno)
{
    struct Erc32IntState *intctl = (struct Erc32IntState *)irq_manager;
    uint32_t              mask;

    intno &= 15;
    mask = 1 << intno;

    trace_erc32_intctl_ack(intno);

    /* Clear registers.  */
    intctl->pending &= ~mask;
    intctl->force   &= ~mask;

    erc32_check_irqs(intctl);
}

static void erc32_set_irq(void *opaque, int irq, int level)
{
    struct Erc32IntState *s = opaque;

    if (level) {
        s->pending |= 1 << irq;
        erc32_check_irqs(s);
    }
}

static void erc32_uart_check_irq(struct Erc32UartState *s)
{
    if ((s->status & UART_DR) || !(s->status & UART_THE)) {
        qemu_irq_pulse(s->irq);
    }
}

static int erc32_uart_can_receive(void *opaque)
{
    Erc32UartState *s = opaque;

    return (s->status & UART_DR) ? 0 : 1;
}

static void erc32_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    Erc32UartState *s = opaque;
    s->rcv = *buf;
    s->status |= UART_DR;
    erc32_uart_check_irq(s);
}

static void erc32_uart_event(void *opaque, int event)
{
    trace_erc32_uart_event(event);
}

static uint32_t erc32_uart_read_reg(struct Erc32UartState *s)
{
    if (s->status & UART_DR) {
        s->status &= ~UART_DR;
        erc32_uart_check_irq(s);
    }
    return s->rcv;
}

static void erc32_uart_init(CharDriverState       *chr,
                            struct Erc32UartState *s,
                            qemu_irq               irq)
{
    s->chr    = chr;
    s->irq    = irq;
    s->status = UART_TS | UART_THE;

    qemu_chr_add_handlers(s->chr, erc32_uart_can_receive, erc32_uart_receive,
                          erc32_uart_event, s);
}

static void erc32_timer_bh(void *opaque)
{
    struct Erc32TimerState *s = opaque;

    trace_erc32_timer_hit(s->id);
    qemu_set_irq(s->irq, 1);
}

static uint32_t erc32_timer_read_control(struct Erc32TimerState *s)
{
    return ptimer_get_count(s->ptimer);
}

static uint32_t erc32_timer_read_scaler(struct Erc32TimerState *s)
{
    return s->scaler;
}

static void erc32_timer_write_control(struct Erc32TimerState *s, uint32_t val)
{
    s->control = val;
    if (s->control == 0) {
        ptimer_stop(s->ptimer);
    } else {
        ptimer_set_limit(s->ptimer, s->control + (s->scaler == 0 ? 1 : 0), 1);
        ptimer_run(s->ptimer, 1);
    }
}

static void erc32_timer_write_scaler(struct Erc32TimerState *s, uint32_t val)
{
    s->scaler = val;

    if (s->scaler > 0) {
        ptimer_set_freq(s->ptimer, SYSCLK / (s->scaler + 1));
    } else {
        ptimer_set_freq(s->ptimer, SYSCLK);
    }
}

static void erc32_timer_init(struct Erc32TimerState *s, qemu_irq irq, int id)
{
    s->bh     = qemu_bh_new(erc32_timer_bh, s);
    s->ptimer = ptimer_init(s->bh);
    s->irq    = irq;
    s->id     = id;
}

static uint32_t erc32_io_readl(void *opaque, target_phys_addr_t addr)
{
    Erc32IoState *s = opaque;
    uint32_t      ret;

    switch (addr) {
    case INTERRUPT_PENDING_REGISTER:
        ret = s->intctl.pending;
        break;
    case INTERRUPT_MASK_REGISTER:
        ret = s->intctl.mask;
        break;
    case INTERRUPT_FORCE_REGISTER:
        ret = s->intctl.force;
        break;
    case INTERRUPT_CLEAR_REGISTER:
        ret = 0;
        break;

    case RTC_COUNTER_REGISTER:
        ret = erc32_timer_read_control(&s->rttimer);
        break;
    case RTC_SCALER_REGISTER:
        ret = erc32_timer_read_scaler(&s->rttimer);
        break;
    case GPT_COUNTER_REGISTER:
        ret = erc32_timer_read_control(&s->gptimer);
        break;
    case GPT_SCALER_REGISTER:
        ret = erc32_timer_read_scaler(&s->gptimer);
        break;

    case UART_A_DATA:
        ret = erc32_uart_read_reg(&s->uarta);
        break;
    case UART_B_DATA:
        ret = erc32_uart_read_reg(&s->uartb);
        break;
    case UART_STATUS_REGISTER:
        ret = s->uarta.status | (s->uartb.status << 16);
        break;

    default:
        trace_erc32_unknown_register("read", addr);
        ret = 0;
        break;
    }

    trace_erc32_readl(addr, ret);
    return ret;
}

static void erc32_io_writel(void               *opaque,
                            target_phys_addr_t  addr,
                            uint32_t            val)
{
    Erc32IoState *s = opaque;

    trace_erc32_writel(addr, val);

    switch (addr) {
    case INTERRUPT_PENDING_REGISTER:
        /* Read Only */
        break;
    case INTERRUPT_MASK_REGISTER:
        s->intctl.mask = val & 0x7ffe;
        erc32_check_irqs(&s->intctl);
        break;
    case INTERRUPT_FORCE_REGISTER:
        s->intctl.force = val & 0xfffe;
        erc32_check_irqs(&s->intctl);
        break;
    case INTERRUPT_CLEAR_REGISTER:
        s->intctl.pending &= ~(val & 0xfffe);
        erc32_check_irqs(&s->intctl);
        break;

    case RTC_COUNTER_REGISTER:
        erc32_timer_write_control(&s->rttimer, val);
        break;
    case RTC_SCALER_REGISTER:
        erc32_timer_write_scaler(&s->rttimer, val);
        break;
    case GPT_COUNTER_REGISTER:
        erc32_timer_write_control(&s->gptimer, val);
        break;
    case GPT_SCALER_REGISTER:
        erc32_timer_write_scaler(&s->gptimer, val);
        break;

    case UART_A_DATA:
    {
        unsigned char c = val & 0xFF;
        qemu_chr_write(s->uarta.chr, &c, 1);
    }
    break;

    default:
        trace_erc32_unknown_register("write", addr);
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

static void tsc695_hw_init(ram_addr_t  ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{
    CPUState     *env;
    ram_addr_t    ram_offset, prom_offset;
    int           ret;
    char         *filename;
    qemu_irq     *cpu_irqs;
    int           bios_size;
    int           aligned_bios_size;
    int           erc32_io_memory;
    Erc32IoState *s;
    ResetData    *reset_info;

    /* init CPU */
    if (!cpu_model) {
        cpu_model = "ERC32";
    }

    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "qemu: Unable to find Sparc CPU definition\n");
        exit(1);
    }

    cpu_sparc_set_id(env, 0);

    /* Reset data */
    reset_info        = qemu_mallocz(sizeof(ResetData));
    reset_info->env   = env;
    qemu_register_reset(main_cpu_reset, reset_info);

    s = qemu_mallocz(sizeof(struct Erc32IoState));
    s->intctl.env = env;
    s->intctl.mask = 0x7ffe;

    env->irq_manager = &s->intctl;
    env->qemu_irq_ack = erc32_intctl_ack;
    cpu_irqs = qemu_allocate_irqs(erc32_set_irq, &s->intctl, MAX_PILS);

    /* allocate RAM */
    if ((uint64_t)ram_size > (256UL << 20)) {
        fprintf(stderr,
                "qemu: Too much memory for this machine: %d, maximum 256M\n",
                (unsigned int)(ram_size / (1024 * 1024)));
        exit(1);
    }
    ram_offset = qemu_ram_alloc(NULL, "Erc32.ram", ram_size);
    cpu_register_physical_memory(0x02000000, ram_size, ram_offset);

    /* load boot prom */
    if (bios_name == NULL) {
        bios_name = PROM_FILENAME;
    }
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    bios_size = get_image_size(filename);
    if (bios_size > 0) {
        aligned_bios_size =
            (bios_size + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK;
        prom_offset = qemu_ram_alloc(NULL, "Erc32.rom", aligned_bios_size);
        cpu_register_physical_memory(0x00000000, aligned_bios_size,
                                     prom_offset /* | IO_MEM_ROM */);
        ret = load_image_targphys(filename, 0x00000000, bios_size);
        if (ret < 0 || ret > bios_size) {
            fprintf(stderr, "qemu: could not load prom '%s'\n", filename);
            exit(1);
        }
    } else if (kernel_filename == NULL) {
        fprintf(stderr, "Can't read bios image %s\n", filename);
        exit(1);
    }

    erc32_io_memory = cpu_register_io_memory(erc32_io_read, erc32_io_write, s,
                                             DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(0x01f80000, 0x1000, erc32_io_memory);

    erc32_timer_init(&s->rttimer, cpu_irqs[13], 1 /* id */);
    erc32_timer_init(&s->gptimer, cpu_irqs[12], 2 /* id */);

    if (serial_hds[0]) {
        erc32_uart_init(serial_hds[0], &s->uarta, cpu_irqs[4]);
    }
    if (serial_hds[1]) {
        erc32_uart_init(serial_hds[1], &s->uartb, cpu_irqs[5]);
    }

    /* Can directly load an application. */
    if (kernel_filename != NULL) {
        long kernel_size;
        uint64_t entry;

        kernel_size = load_elf(kernel_filename, NULL, NULL, &entry, NULL, NULL,
                               1 /* big endian */, ELF_MACHINE, 0);
        if (kernel_size < 0) {
            fprintf(stderr, "qemu: could not load kernel '%s'\n",
                    kernel_filename);
            exit(1);
        }
        if (bios_size <= 0) {
            /* If there is no bios/monitor, start the application.  */
            env->pc = entry;
            env->npc = entry + 4;
            reset_info->entry = entry;
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
