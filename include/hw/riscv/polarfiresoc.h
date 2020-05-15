/*
 * Microchip PolarFire SOC machine interface
 *
 * Copyright 2020 AdaCore
 *
 * Base on sifive_u.h:
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_POLARFIRE_H
#define HW_POLARFIRE_H

#include "hw/net/cadence_gem.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/sifive_cpu.h"
#include "hw/riscv/sifive_u_prci.h"
#include "hw/riscv/sifive_u_otp.h"
#include "hw/gpio/pse_gpio.h"

#define TYPE_RISCV_U_SOC "riscv.microchip.polarfire.soc"
#define RISCV_U_SOC(obj) \
    OBJECT_CHECK(PolarFireSoCState, (obj), TYPE_RISCV_U_SOC)

#define POLARFIRE_NUM_GPIO 3

typedef struct PolarFireSoCState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    CPUClusterState e_cluster;
    CPUClusterState u_cluster;
    RISCVHartArrayState e_cpus;
    RISCVHartArrayState u_cpus;
    DeviceState *plic;
    CadenceGEMState gem;

    PSEGPIOState gpio[POLARFIRE_NUM_GPIO];
} PolarFireSoCState;

typedef struct PolarFireState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    PolarFireSoCState soc;
    void *fdt;
    int fdt_size;
} PolarFireState;

enum {
    POLARFIRE_DEBUG,
    POLARFIRE_MROM,
    POLARFIRE_TEST,
    POLARFIRE_HOSTFS,
    POLARFIRE_CLINT,
    POLARFIRE_L2LIM,
    POLARFIRE_PLIC,
    POLARFIRE_MMUART0,
    POLARFIRE_MMUART1,
    POLARFIRE_MMUART2,
    POLARFIRE_MMUART3,
    POLARFIRE_MMUART4,
    POLARFIRE_GPIO,
    POLARFIRE_DRAM,
    POLARFIRE_GEM
};

enum {
    POLARFIRE_MMUART0_IRQ = 90,
    POLARFIRE_MMUART1_IRQ = 91,
    POLARFIRE_MMUART2_IRQ = 92,
    POLARFIRE_MMUART3_IRQ = 93,
    POLARFIRE_MMUART4_IRQ = 94,
    POLARFIRE_GPIO_IRQ    = 51,
    POLARFIRE_GEM_IRQ     = 0x35
};

enum {
    POLARFIRE_CLOCK_FREQ    = 1000000000,
    POLARFIRE_GEM_CLOCK_FREQ = 125000000
};

#define POLARFIRE_PLIC_HART_CONFIG "MS"
#define POLARFIRE_PLIC_NUM_SOURCES 185
#define POLARFIRE_PLIC_NUM_PRIORITIES 7
#define POLARFIRE_PLIC_PRIORITY_BASE 0x04
#define POLARFIRE_PLIC_PENDING_BASE 0x1000
#define POLARFIRE_PLIC_ENABLE_BASE 0x2000
#define POLARFIRE_PLIC_ENABLE_STRIDE 0x80
#define POLARFIRE_PLIC_CONTEXT_BASE 0x200000
#define POLARFIRE_PLIC_CONTEXT_STRIDE 0x1000

#if defined(TARGET_RISCV32)
#define POLARFIRE_CPU TYPE_RISCV_CPU_SIFIVE_U34
#elif defined(TARGET_RISCV64)
#define POLARFIRE_CPU TYPE_RISCV_CPU_SIFIVE_U54
#endif

#endif
