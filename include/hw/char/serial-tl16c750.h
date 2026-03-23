/*
 * Device model for TI TL16C750 UART
 *
 * Copyright (c) 2026 Adacore
 * Written by Clément Chigot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_SERIAL_TL16C750_UART_H
#define HW_SERIAL_TL16C750_UART_H

#include "chardev/char-fe.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "qemu/fifo8.h"
#include "hw/char/serial.h"

#define TYPE_SERIAL_TL16C750 "serial.tl16c750"
OBJECT_DECLARE_SIMPLE_TYPE(SerialTl16c750State, SERIAL_TL16C750)

#define SERIAL_TL16C750_FIFO_SIZE 8

struct SerialTl16c750State {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    SerialState serial;
    CharBackend chr;
    MemoryRegion io;


    uint8_t r_mdr1;
    uint8_t r_ssr;
    uint8_t r_sysc;
    uint8_t r_syss;
};

#endif
