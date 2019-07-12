/*
 * MC68901 QEMU Implementation
 *
 * Copyright (c) 2019-2020 AdaCore
 *
 *  Developed by :
 *  Frederic Konrad   <frederic.konrad@adacore.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MC68901_H
#define MC68901_H

#include "chardev/char-fe.h"
#include "hw/sysbus.h"

#define MC68901_TOTAL_REGS (0x30)
#define MC68901_TIMER_COUNT (4)

typedef enum MC68901TimerOutput {
    MC68901_TAO = 0,
    MC68901_TBO = 1,
    MC68901_TC0 = 2,
    MC68901_TD0 = 3,
} MC68901TimerOutput;

typedef struct MC68901State {
    SysBusDevice parent_obj;

    /* IO memory. */
    MemoryRegion iomem;
    uint8_t regs[MC68901_TOTAL_REGS >> 1];

    /* USART Implementation */
    CharBackend chr;
    /* Receiver */
    int buffer_full;
    uint8_t receive_buf;
    /* Transmiter */

    /* Timers */
    uint32_t timer_freq;
    qemu_irq txo[MC68901_TIMER_COUNT];
    struct MC68901Timer {
        uint8_t id;
        QEMUTimer qemu_timer;
    } timers[MC68901_TIMER_COUNT];
} MC68901State;

#define TYPE_MC68901 "mc68901"
#define MC68901(obj) OBJECT_CHECK(MC68901State, (obj), TYPE_MC68901)

#endif /* MC68901_H */
