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
#include "qemu-plugin.h"
#include "sysemu.h"
#include "boards.h"
#include "loader.h"
#include "elf.h"
#include "trace.h"

/* Default system clock.  */
#define CPU_CLK (50 * 1000 * 1000)

/* Leon registers  */

#define MEMORY_CONFIGURATION_REGISTER_1 0x00
#define MEMORY_CONFIGURATION_REGISTER_2 0x04
#define MEMORY_CONFIGURATION_REGISTER_3 0x08
#define FAIL_STATUS_REGISTER            0x10

/* Cache Control register */

#define CACHE_CONTROL_REGISTER 0x14
#define CCR_MASK               0x00e13fff
#define CCR_INIT               0xf7100000

/* Cache Control register fields */

#define CACHE_STATE_MASK 0x3
#define CACHE_DISABLED   0x0
#define CACHE_FROZEN     0x1
#define CACHE_ENABLED    0x3

#define CACHE_CTRL_IF (1 <<  4) /* Instruction Cache Freeze On Interrupt */
#define CACHE_CTRL_DF (1 <<  5) /* Data Cache Freeze On Interrupt */
#define CACHE_CTRL_DP (1 << 14) /* Data Cache Flush Pending */
#define CACHE_CTRL_IP (1 << 15) /* Instruction Cache Flush Pending */
#define CACHE_CTRL_IB (1 << 16) /* Instruction Burst Fetch */
#define CACHE_CTRL_FI (1 << 21) /* Flush Instruction Cache (Write Only) */
#define CACHE_CTRL_FD (1 << 22) /* Flush Data Cache (Write Only) */

/* Timers registers */

#define TIMER_1_COUNTER_REGISTER   0x40
#define TIMER_1_RELOAD_REGISTER    0x44
#define TIMER_1_CONTROL_REGISTER   0x48
#define WATCHDOG_REGISTER          0x4c
#define TIMER_2_COUNTER_REGISTER   0x50
#define TIMER_2_RELOAD_REGISTER    0x54
#define TIMER_2_CONTROL_REGISTER   0x58
#define PRESCALER_COUNTER_REGISTER 0x60
#define PRESCALER_RELOAD_REGISTER  0x64

/* Timers registers fields */

#define TIMCTR_EN   0x01        /* Enable Counter */
#define TIMCTR_RL   0x02        /* Reload Counter */
#define TIMCTR_LD   0x04        /* Load Counter */
#define TIMCTR_MASK 0x07

/* Interrupt controller registers */

#define INTERRUPT_MASK_AND_PRIORITY_REGISTER 0x90
#define INTERRUPT_PENDING_REGISTER           0x94
#define INTERRUPT_FORCE_REGISTER             0x98
#define INTERRUPT_CLEAR_REGISTER             0x9c

/* IO registers */

#define IO_PORT_DATA_REGISTER      0xa0
#define IO_PORT_DIRECTION_REGISTER 0xa4
#define IO_PORT_INTERRUPT_REGISTER 0xa8

/* UART registers */

#define UART_1_DATA_REGISTER    0x70
#define UART_1_STATUS_REGISTER  0x74
#define UART_1_CONTROL_REGISTER 0x78
#define UART_1_SCALER_REGISTER  0x7c
#define UART_2_DATA_REGISTER    0x80
#define UART_2_STATUS_REGISTER  0x84
#define UART_2_CONTROL_REGISTER 0x88
#define UART_2_SCALER_REGISTER  0x8c

/* UART register fields */

#define UART_STATUS_DR 0x01     /* Data Ready */
#define UART_STATUS_TS 0x02     /* Transmitter Shift Register Empty */
#define UART_STATUS_TH 0x04     /* Transmitter Hold Register Empty */
#define UART_STATUS_BR 0x08     /* Break Received */
#define UART_STATUS_OV 0x10     /* Overrun */
#define UART_STATUS_PE 0x20     /* Parity Error */
#define UART_STATUS_FE 0x40     /* Framing Error */

#define UART_CONTROL_RE 0x001   /* Receiver Enable */
#define UART_CONTROL_TE 0x002   /* Transmitter Enable */
#define UART_CONTROL_RI 0x004   /* Receiver Interrupt Enable */
#define UART_CONTROL_TI 0x008   /* Transmitter Interrupt Enable */
#define UART_CONTROL_PS 0x010   /* Parity Select */
#define UART_CONTROL_PE 0x020   /* Parity Enable */
#define UART_CONTROL_FL 0x040   /* Flow Control */
#define UART_CONTROL_LB 0x080   /* Loop Back */
#define UART_CONTROL_EC 0x100   /* External Clock */


#define PROM_FILENAME        "u-boot.bin"

#define MAX_PILS 16

#define FIFO_LENGTH 1024

typedef struct LeonUartState {
    uint32_t         status;
    uint32_t         control;
    uint32_t         scaler;
    CharDriverState *chr;
    qemu_irq         irq;

    /* FIFO */
    char buffer[FIFO_LENGTH];
    int  len;
    int  current;

} LeonUartState;

struct LeonTimerState {
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
        uint32_t lvl_mask;
        uint32_t pending;
        uint32_t force;
        CPUState *env;
};

typedef struct LeonIoState {
        uint32_t mcfg[3];

        struct LeonIntState intctl;

        uint32_t ccr;
        uint32_t scar;
        uint32_t wdg;
        uint32_t iodata;
        uint32_t iodir;
        uint32_t ioit;

        struct LeonTimerState timer1;
        struct LeonTimerState timer2;

        LeonUartState uart1;
        LeonUartState uart2;
} LeonIoState;


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

static void leon_check_irqs(struct LeonIntState *s)
{
    uint32_t       pend   = 0;
    uint32_t       level0 = 0;
    uint32_t       level1 = 0;
    CPUSPARCState *env    = s->env;

    pend = (s->pending | s->force) & (s->lvl_mask & 0xfffe);

    level0 = pend & ~(s->lvl_mask >> 16);
    level1 = pend &  (s->lvl_mask >> 16);

    trace_leon_check_irqs(s->pending, s->force,
                          s->lvl_mask, level1, level0);

    /* Trigger level1 interrupt first and level0 if there is no level1 */
    if (level1 != 0) {
        env->pil_in = level1;
    } else {
        env->pil_in = level0;
    }

    if (env->pil_in && (env->interrupt_index == 0 ||
                        (env->interrupt_index & ~15) == TT_EXTINT)) {
        unsigned int i;

        for (i = 15; i > 0; i--) {
            if (env->pil_in & (1 << i)) {
                int old_interrupt = env->interrupt_index;

                env->interrupt_index = TT_EXTINT | i;
                if (old_interrupt != env->interrupt_index) {
                    trace_leon_set_irq(i);
                    cpu_interrupt(env, CPU_INTERRUPT_HARD);
                }
                break;
            }
        }
    } else if (!env->pil_in && (env->interrupt_index & ~15) == TT_EXTINT) {
        trace_leon_reset_irq(env->interrupt_index & 15);
        env->interrupt_index = 0;
        cpu_reset_interrupt(env, CPU_INTERRUPT_HARD);
    }
}

static void leon2_intctl_ack(void *irq_manager, int intno)
{
    struct LeonIntState *intctl = (struct LeonIntState *)irq_manager;
    uint32_t             mask;
    uint32_t             state  = 0;

    intno &= 15;
    mask   = 1 << intno;

    trace_leon_intctl_ack(intno);

    /* Clear registers.  */
    intctl->pending &= ~mask;
    intctl->force   &= ~mask;

    /* Cache Control */
    if (intctl->env->cache_control & CACHE_CTRL_IF) {
        /* Instruction cache state */
        state = intctl->env->cache_control & CACHE_STATE_MASK;
        if (state == CACHE_ENABLED) {
            state = CACHE_FROZEN;
        }

        intctl->env->cache_control &= ~CACHE_STATE_MASK;
        intctl->env->cache_control |= state;
    }

    if (intctl->env->cache_control & CACHE_CTRL_DF) {
        /* Data cache state */
        state = (intctl->env->cache_control >> 2) & CACHE_STATE_MASK;
        if (state == CACHE_ENABLED) {
            state = CACHE_FROZEN;
        }

        intctl->env->cache_control &= ~(CACHE_STATE_MASK << 2);
        intctl->env->cache_control |= (state << 2);
    }

    leon_check_irqs(intctl);
}

static void leon_set_irq(void *opaque, int irq, int level)
{
    struct LeonIntState *s = opaque;

    if (level) {
        s->pending |= 1 << irq;
        leon_check_irqs(s);
    }
}

static void leon_uart_check_irq(struct LeonUartState *s)
{
    if (((s->status & UART_STATUS_DR) && (s->control & UART_CONTROL_RI))
        || (!(s->status & UART_STATUS_TH) && (s->control & UART_CONTROL_TI))) {
        qemu_irq_pulse(s->irq);
    }
}

static int uart_data_to_read(LeonUartState *s)
{
    return s->current < s->len;
}

static char uart_pop(LeonUartState *s)
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

static void uart_add_to_fifo(LeonUartState *s,
                             const uint8_t *buffer,
                             int            length)
{
    if (s->len + length > FIFO_LENGTH) {
        abort();
    }
    memcpy(s->buffer + s->len, buffer, length);
    s->len += length;
}

static int leon_uart_can_receive(void *opaque)
{
    LeonUartState *s = opaque;

    return FIFO_LENGTH - s->len;
}

static void leon_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    LeonUartState *s = opaque;

    uart_add_to_fifo(s, buf, size);
    s->status |= UART_STATUS_DR;
    leon_uart_check_irq(s);
}

static void leon_uart_event(void *opaque, int event)
{
    trace_leon_uart_event(event);
}

static uint32_t leon_uart_read_uad(struct LeonUartState *s)
{
    uint32_t ret = uart_pop(s);

    if (!uart_data_to_read(s)) {
        s->status &= ~UART_STATUS_DR;
        leon_uart_check_irq(s);
    }

    return ret;
}

static void leon_uart_init(CharDriverState *chr,
                           struct LeonUartState *s, qemu_irq irq)
{
    s->chr     = chr;
    s->control = 0;
    s->irq     = irq;
    s->status  = UART_STATUS_TS | UART_STATUS_TH;

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
        trace_leon_timer_disabled(s->id, s->control);
        return;
    }

    /* ptimer is triggered when the counter reach 0 but GPTimer is triggered at
       underflow. Set count + 1 to simulate the GPTimer behavior. */

    trace_leon_timer_enable(s->id, s->counter + 1);

    ptimer_set_count(s->ptimer, s->counter + 1);
    ptimer_run(s->ptimer, 1);
}

static void leon_timer_hit(void *opaque)
{
    struct LeonTimerState *s = opaque;

    trace_leon_timer_hit(s->id);

    qemu_irq_pulse(s->irq);

    if (s->control & TIMCTR_RL) {
        /* reload */
        s->control |= TIMCTR_LD;
        leon_timer_enable(s);
    }
}

static uint32_t leon_timer_io_read(void *opaque, target_phys_addr_t addr)
{
    LeonIoState *s = opaque;
    uint32_t     ret;

    switch (addr) {
    case PRESCALER_COUNTER_REGISTER:
        ret = 0;
        break;
    case PRESCALER_RELOAD_REGISTER:
        ret = s->scar;
        break;

    case TIMER_1_COUNTER_REGISTER:
        ret = ptimer_get_count(s->timer1.ptimer);
        break;

    case TIMER_2_COUNTER_REGISTER:
        ret = ptimer_get_count(s->timer2.ptimer);
        break;

    case TIMER_1_RELOAD_REGISTER:
        ret = s->timer1.reload;
        break;
    case TIMER_2_RELOAD_REGISTER:
        ret = s->timer2.reload;
        break;


    case TIMER_1_CONTROL_REGISTER:
        ret = s->timer1.control;
        break;
    case TIMER_2_CONTROL_REGISTER:
        ret = s->timer2.control;
        break;

    case WATCHDOG_REGISTER:
        ret = s->wdg;
        break;


    default:
        trace_leon_unknown_register("Timer:read", addr);
        return 0;
    }

    trace_leon_readl(addr, ret);
    return ret;
}

static void leon_timer_io_write(LeonIoState *s, target_phys_addr_t addr,
                                uint32_t val)
{
    trace_leon_writel(addr, val);

    switch (addr) {
    case PRESCALER_COUNTER_REGISTER:
        break;
    case PRESCALER_RELOAD_REGISTER:
        s->scar = val & 0x3ff;
        val = CPU_CLK / (s->scar + 1);
        ptimer_set_freq(s->timer1.ptimer, val);
        ptimer_set_freq(s->timer2.ptimer, val);
        break;

    case TIMER_1_COUNTER_REGISTER:
        s->timer1.counter = val & 0x00ffffff;
        leon_timer_enable(&s->timer1);
        break;
    case TIMER_2_COUNTER_REGISTER:
        s->timer2.counter = val & 0x00ffffff;
        leon_timer_enable(&s->timer2);
        break;

    case TIMER_1_RELOAD_REGISTER:
        s->timer1.reload = val & 0x00ffffff;
        break;
    case TIMER_2_RELOAD_REGISTER:
        s->timer2.reload = val & 0x00ffffff;
        break;


    case TIMER_1_CONTROL_REGISTER:
        s->timer1.control = val & TIMCTR_MASK;
        leon_timer_enable(&s->timer1);
        break;
    case TIMER_2_CONTROL_REGISTER:
        s->timer2.control = val & TIMCTR_MASK;
        leon_timer_enable(&s->timer2);
        break;

    case WATCHDOG_REGISTER:
        s->wdg = val & 0x00ffffff;
        break;

    default:
        trace_leon_unknown_register("Timer:write", addr);
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
    uint32_t     ret;

    switch (addr) {
    case MEMORY_CONFIGURATION_REGISTER_1:
    case MEMORY_CONFIGURATION_REGISTER_2:
    case MEMORY_CONFIGURATION_REGISTER_3:
        ret = s->mcfg[(addr - MEMORY_CONFIGURATION_REGISTER_1) >> 2];
        break;
    case FAIL_STATUS_REGISTER:
        ret = 0;
        break;
    case CACHE_CONTROL_REGISTER:
        ret = s->intctl.env->cache_control;
        break;

    case INTERRUPT_MASK_AND_PRIORITY_REGISTER:
        ret = s->intctl.lvl_mask;
        break;
    case INTERRUPT_PENDING_REGISTER:
        ret = s->intctl.pending;
        break;
    case INTERRUPT_FORCE_REGISTER:
        ret = s->intctl.force;
        break;
    case INTERRUPT_CLEAR_REGISTER:
        ret = 0;
        break;

    case UART_1_DATA_REGISTER:
        ret = leon_uart_read_uad(&s->uart1);
        break;
    case UART_1_CONTROL_REGISTER:
        ret = s->uart1.control;
        break;
    case UART_1_SCALER_REGISTER:
        ret = s->uart1.scaler;
        break;
    case UART_1_STATUS_REGISTER:
        ret = s->uart1.status;
        break;

    case TIMER_1_RELOAD_REGISTER ... PRESCALER_RELOAD_REGISTER:
        ret = leon_timer_io_read(s, addr);
        break;

    case IO_PORT_DATA_REGISTER:
        ret = s->iodata;
        break;
    case IO_PORT_DIRECTION_REGISTER:
        ret = s->iodir;
        break;
    case IO_PORT_INTERRUPT_REGISTER:
        ret = s->ioit;
        break;

    default:
        trace_leon_unknown_register("Leon:read", addr);
        ret = 0;
        break;
    }

    trace_leon_readl(addr, ret);

    return ret;
}

static void leon_io_writel(void *opaque, target_phys_addr_t addr,
                           uint32_t val)
{
    LeonIoState *s = opaque;

    trace_leon_writel(addr, val);

    switch (addr) {
    case MEMORY_CONFIGURATION_REGISTER_1:
    case MEMORY_CONFIGURATION_REGISTER_2:
    case MEMORY_CONFIGURATION_REGISTER_3:
        s->mcfg[(addr - MEMORY_CONFIGURATION_REGISTER_1) >> 2] = val;
        break;
    case FAIL_STATUS_REGISTER:
        break;
    case CACHE_CONTROL_REGISTER:
        /* These values must always be read as zeros */
        val &= ~CACHE_CTRL_FD;
        val &= ~CACHE_CTRL_FI;
        val &= ~CACHE_CTRL_IB;
        val &= ~CACHE_CTRL_IP;
        val &= ~CACHE_CTRL_DP;
        s->intctl.env->cache_control = val;
        break;

    case INTERRUPT_MASK_AND_PRIORITY_REGISTER:
        s->intctl.lvl_mask = val;
        break;
    case INTERRUPT_PENDING_REGISTER:
        /* Read Only */
        break;
    case INTERRUPT_FORCE_REGISTER:
        s->intctl.force = val & 0xfffe;
        leon_check_irqs(&s->intctl);
        break;
    case INTERRUPT_CLEAR_REGISTER:
        s->intctl.pending &= ~(val & 0xfffe);
        leon_check_irqs(&s->intctl);
        break;

    case UART_1_CONTROL_REGISTER:
        s->uart1.control = val & 0x1ff;
        break;
    case UART_1_SCALER_REGISTER:
        s->uart1.scaler = val & 0x3ff;
        break;
    case UART_1_DATA_REGISTER:
    {
        unsigned char c = val;
        qemu_chr_write(s->uart1.chr, &c, 1);
    }
    break;

    case TIMER_1_RELOAD_REGISTER ... PRESCALER_RELOAD_REGISTER:
        leon_timer_io_write(s, addr, val);
        break;

    case IO_PORT_DATA_REGISTER:
        s->iodata = val & 0xffff;
        break;
    case IO_PORT_DIRECTION_REGISTER:
        s->iodir = val & 0x3ffff;
        break;
    case IO_PORT_INTERRUPT_REGISTER:
        s->ioit = val;
        break;

    default:
        trace_leon_unknown_register("Leon:write", addr);
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

static void at697_hw_init(ram_addr_t  ram_size,
                          const char *boot_device,
                          const char *kernel_filename,
                          const char *kernel_cmdline,
                          const char *initrd_filename,
                          const char *cpu_model)
{
    CPUState    *env;
    ram_addr_t   ram_offset, prom_offset;
    ram_addr_t   ram2_size, ram2_offset;
    int          ret;
    char        *filename;
    qemu_irq    *cpu_irqs;
    int          bios_size;
    int          aligned_bios_size;
    int          leon_io_memory;
    LeonIoState *s;
    ResetData   *reset_info;

    /* init CPU */
    if (!cpu_model) {
        cpu_model = "LEON2";
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

    s = qemu_mallocz(sizeof(struct LeonIoState));
    s->ccr = CCR_INIT;
    s->intctl.env = env;;

    env->irq_manager = &s->intctl;
    env->qemu_irq_ack = leon2_intctl_ack;

    cpu_irqs = qemu_allocate_irqs(leon_set_irq, &s->intctl, MAX_PILS);

    /* allocate RAM */
    if ((uint64_t)ram_size > (1UL << 30)) {
        fprintf(stderr,
                "qemu: Too much memory for this machine: %d, maximum 1G\n",
                (unsigned int)(ram_size / (1024 * 1024)));
        exit(1);
    }
    ram_offset = qemu_ram_alloc(NULL, "leon.ram", ram_size);
    cpu_register_physical_memory(0x40000000, ram_size, ram_offset);

    /* Allocate RAM2.  */
    ram2_size = 8 << 20;
    ram2_offset = qemu_ram_alloc(NULL, "leon.ram2", ram2_size);
    cpu_register_physical_memory(0x20000000, ram2_size, ram2_offset);

    /* load boot prom */
    if (bios_name == NULL) {
        bios_name = PROM_FILENAME;
    }
    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    bios_size = get_image_size(filename);
    if (bios_size > 0) {
        aligned_bios_size =
            (bios_size + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK;
        prom_offset = qemu_ram_alloc(NULL, "leon.bios", aligned_bios_size);
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

    leon_io_memory = cpu_register_io_memory(leon_io_read, leon_io_write, s,
                                            DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(0x80000000, 0x1000, leon_io_memory);

    leon_timer_init(&s->timer1, cpu_irqs[8], 1 /* id */);
    leon_timer_init(&s->timer2, cpu_irqs[9], 2 /* id */);

    if (serial_hds[0]) {
        leon_uart_init(serial_hds[0], &s->uart1, cpu_irqs[3]);
    }
    if (serial_hds[1]) {
        leon_uart_init(serial_hds[1], &s->uart2, cpu_irqs[2]);
    }

    /* Initialize plug-ins */
    plugin_init(cpu_irqs, MAX_PILS);
    plugin_device_init();

    /* Can directly load an application. */
    if (kernel_filename != NULL) {
        long     kernel_size;
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

QEMUMachine at697_machine = {
    .name     = "at697",
    .desc     = "Leon-2 Atmel 697",
    .init     = at697_hw_init,
    .use_scsi = 0,
};

static void leon_machine_init(void)
{
    qemu_register_machine(&at697_machine);
}

machine_init(leon_machine_init);
