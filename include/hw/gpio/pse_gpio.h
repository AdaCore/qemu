/*
 * Microchip PolarFire SOC GPIO
 *
 * Copyright 2020 AdaCore
 *
 * Base on nrf51_gpio.c:
 *
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */
#ifndef PSE_GPIO_H
#define PSE_GPIO_H

#include "hw/sysbus.h"
#define TYPE_PSE_GPIO "pse_gpio"
#define PSE_GPIO(obj) OBJECT_CHECK(PSEGPIOState, (obj), TYPE_PSE_GPIO)

#define PSE_GPIO_PINS 32

#define PSE_GPIO_SIZE 0x1000

#define PSE_GPIO_CONFIG_EN_OUT    0b00000001
#define PSE_GPIO_CONFIG_EN_IN     0b00000010
#define PSE_GPIO_CONFIG_EN_OE_BUF 0b00000100
#define PSE_GPIO_CONFIG_EN_INT    0b00001000
#define PSE_GPIO_CONFIG_TYPE_MASK 0b11100000

typedef struct PSEGPIOState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    qemu_irq irq;
    qemu_irq output[PSE_GPIO_PINS];

    uint32_t config[PSE_GPIO_PINS];

    uint32_t value;             /* Actual value of the pin */
    uint32_t gpout;             /* Pin value requested by the user */
    uint32_t rise_pending;
    uint32_t fall_pending;
    uint32_t high_pending;
    uint32_t low_pending;

    uint32_t in;
    uint32_t in_mask;
} PSEGPIOState;

#endif
