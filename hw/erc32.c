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
#include "qemu-plugin.h"
#include "gnat-bus.h"
#include "sysemu.h"
#include "boards.h"
#include "loader.h"
#include "elf.h"
#include "trace.h"
#include "exec-memory.h"
#include "ptimer.h"

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

#define FIFO_LENGTH 1024

typedef struct ResetData {
    CPUSPARCState *env;
    uint32_t  entry;            /* save kernel entry in case of reset */
    uint32_t  stack_pointer;
} ResetData;

static void main_cpu_reset(void *opaque)
{
    ResetData     *s   = (ResetData *)opaque;
    CPUSPARCState *env = s->env;

    cpu_state_reset(env);

    env->halted     = 0;
    env->pc         = s->entry;
    env->npc        = s->entry + 4;
    env->regbase[6] = s->stack_pointer;
}

typedef struct Erc32UartState {
    uint32_t         status;
    CharDriverState *chr;
    qemu_irq         irq;

    /* FIFO */
    char buffer[FIFO_LENGTH];
    int  len;
    int  current;

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
    uint32_t       mask;
    uint32_t       pending;
    uint32_t       force;
    CPUSPARCState *env;
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

static void erc32_intctl_ack(CPUSPARCState *env, void *irq_manager, int intno)
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

static int uart_data_to_read(Erc32UartState *s)
{
    return s->current < s->len;
}

static char uart_pop(Erc32UartState *s)
{
    char ret;

    if (s->len == 0) {
        return 0;
    }

    ret = s->buffer[s->current++];

    if (s->current >= s->len) {
        /* Flush */
        s->len     = 0;
        s->current = 0;
    }
    return ret;
}

static void uart_add_to_fifo(Erc32UartState *s,
                             const uint8_t *buffer,
                             int            length)
{
    if (s->len + length > FIFO_LENGTH) {
        abort();
    }
    memcpy(s->buffer + s->len, buffer, length);
    s->len += length;
}

static int erc32_uart_can_receive(void *opaque)
{
    Erc32UartState *s = opaque;

    return FIFO_LENGTH - s->len;
}

static void erc32_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    Erc32UartState *s = opaque;

    uart_add_to_fifo(s, buf, size);
    s->status |= UART_DR;
    erc32_uart_check_irq(s);
}

static void erc32_uart_event(void *opaque, int event)
{
    trace_erc32_uart_event(event);
}

static uint32_t erc32_uart_read_reg(struct Erc32UartState *s)
{
    uint32_t ret = uart_pop(s);

    if (!uart_data_to_read(s)) {
        s->status &= ~UART_DR;
        erc32_uart_check_irq(s);
    }
    return ret;
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

static uint64_t erc32_io_read(void *opaque, target_phys_addr_t addr,
                              unsigned size)
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
    case UART_A_DATA + 3:       /* When only one byte read */
        ret = erc32_uart_read_reg(&s->uarta);
        break;
    case UART_B_DATA:
    case UART_B_DATA + 3:       /* When only one byte read */
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

static void erc32_io_write(void *opaque, target_phys_addr_t  addr,
                           uint64_t value, unsigned size)
{
    Erc32IoState *s = opaque;

    trace_erc32_writel(addr, value);

    switch (addr) {
    case INTERRUPT_PENDING_REGISTER:
        /* Read Only */
        break;
    case INTERRUPT_MASK_REGISTER:
        s->intctl.mask = value & 0x7ffe;
        erc32_check_irqs(&s->intctl);
        break;
    case INTERRUPT_FORCE_REGISTER:
        s->intctl.force = value & 0xfffe;
        erc32_check_irqs(&s->intctl);
        break;
    case INTERRUPT_CLEAR_REGISTER:
        s->intctl.pending &= ~(value & 0xfffe);
        erc32_check_irqs(&s->intctl);
        break;

    case RTC_COUNTER_REGISTER:
        erc32_timer_write_control(&s->rttimer, value);
        break;
    case RTC_SCALER_REGISTER:
        erc32_timer_write_scaler(&s->rttimer, value);
        break;
    case GPT_COUNTER_REGISTER:
        erc32_timer_write_control(&s->gptimer, value);
        break;
    case GPT_SCALER_REGISTER:
        erc32_timer_write_scaler(&s->gptimer, value);
        break;

    case UART_A_DATA:
    case UART_A_DATA + 3:       /* When only one byte write */
    {
        unsigned char c = value & 0xFF;
        qemu_chr_fe_write(s->uarta.chr, &c, 1);
    }
    break;
    case UART_B_DATA:
    case UART_B_DATA + 3:       /* When only one byte write */
    {
        unsigned char c = value & 0xFF;
        qemu_chr_fe_write(s->uartb.chr, &c, 1);
    }
    break;

    default:
        trace_erc32_unknown_register("write", addr);
    }
}

static const MemoryRegionOps erc32_io_ops = {
    .read  = erc32_io_read,
    .write = erc32_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void tsc695_hw_init(ram_addr_t  ram_size,
                           const char *boot_device,
                           const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           const char *cpu_model)
{
    CPUSPARCState *env;
    int            ret;
    char          *filename;
    qemu_irq      *cpu_irqs;
    int            bios_size;
    int            aligned_bios_size;
    Erc32IoState  *s;
    ResetData     *reset_info;
    MemoryRegion  *address_space_mem = get_system_memory();
    MemoryRegion  *ram               = g_new(MemoryRegion, 1);
    MemoryRegion  *prom              = g_new(MemoryRegion, 1);
    MemoryRegion  *iomem             = g_new(MemoryRegion, 1);

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
    reset_info        = g_malloc0(sizeof(ResetData));
    reset_info->env   = env;
    qemu_register_reset(main_cpu_reset, reset_info);

    s = g_malloc0(sizeof(struct Erc32IoState));
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
    memory_region_init_ram(ram, "erc32.ram", ram_size);
    memory_region_add_subregion(address_space_mem, 0x02000000, ram);

    reset_info->stack_pointer = 0x02000000 + ram_size;

    /* load boot prom */
    if (bios_name == NULL) {
        bios_name = PROM_FILENAME;
    }
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    bios_size = get_image_size(filename);
    if (bios_size > 0) {
        aligned_bios_size =
            (bios_size + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK;

        memory_region_init_ram(prom, "erc32.bios", aligned_bios_size);
        memory_region_set_readonly(prom, true);
        memory_region_add_subregion(address_space_mem, 0x00000000, prom);

        ret = load_image_targphys(filename, 0x00000000, bios_size);
        if (ret < 0 || ret > bios_size) {
            fprintf(stderr, "qemu: could not load prom '%s'\n", filename);
            exit(1);
        }
    } else if (kernel_filename == NULL) {
        fprintf(stderr, "Can't read bios image %s\n", filename);
        exit(1);
    }

    memory_region_init_io(iomem, &erc32_io_ops, s, "erc32_io", 0x1000);
    memory_region_add_subregion(address_space_mem, 0x01f80000, iomem);

    erc32_timer_init(&s->rttimer, cpu_irqs[13], 1 /* id */);
    erc32_timer_init(&s->gptimer, cpu_irqs[12], 2 /* id */);

    if (serial_hds[0]) {
        erc32_uart_init(serial_hds[0], &s->uarta, cpu_irqs[4]);
    }
    if (serial_hds[1]) {
        erc32_uart_init(serial_hds[1], &s->uartb, cpu_irqs[5]);
    }

    /* Initialize plug-ins */
    plugin_init(cpu_irqs, MAX_PILS);
    plugin_device_init();

    /* Initialize the GnatBus Master */
    gnatbus_master_init(cpu_irqs, MAX_PILS);
    gnatbus_device_init();

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
