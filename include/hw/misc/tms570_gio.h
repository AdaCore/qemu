/*
 * TMS570 GIO QEMU Implementation
 *
 * Copyright (c) 2020 AdaCore
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

#ifndef TMS570_GIO_H
#define TMS570_GIO_H

#include "hw/sysbus.h"

#define TMS570_GIO_TOTAL_REGS (0x70)
#define TMS570_GIO_IRQ_COUNT  (2)
#define TMS570_GIO_IRQ_LOW    (0)
#define TMS570_GIO_IRQ_HIGH   (1)

#define MAX_PIN (16)
typedef struct TMS570GIOState {
    SysBusDevice parent_obj;

    /* IO memory. */
    MemoryRegion iomem;
    uint32_t regs[TMS570_GIO_TOTAL_REGS >> 2];

    /* IRQ rings.  */
    uint8_t high[MAX_PIN];
    uint8_t low[MAX_PIN];

    /* IRQs */
    qemu_irq irq[TMS570_GIO_IRQ_COUNT];
} TMS570GIOState;

#define TYPE_TMS570_GIO "tms570-gio"
#define TMS570_GIO(obj) OBJECT_CHECK(TMS570GIOState, (obj), TYPE_TMS570_GIO)

#endif /* TMS570_GIO_H */
