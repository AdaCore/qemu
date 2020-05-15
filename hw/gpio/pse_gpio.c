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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/gpio/pse_gpio.h"
#include "migration/vmstate.h"
#include "trace.h"

static void update_output_irq(PSEGPIOState *s)
{
    uint32_t any = 0;
    uint32_t type;
    bool     enabled;
    bool     rise_pending;
    bool     fall_pending;
    bool     low_pending;
    bool     high_pending;
    bool     pending;

    for (int i = 0; i < PSE_GPIO_PINS; i++) {
        enabled = (s->config[i] & PSE_GPIO_CONFIG_EN_INT) != 0;
        type = (s->config[i] & PSE_GPIO_CONFIG_TYPE_MASK) >> 5;

        rise_pending = extract32(s->rise_pending, i, 1);
        fall_pending = extract32(s->fall_pending, i, 1);
        high_pending = extract32(s->high_pending, i, 1);
        low_pending  = extract32(s->low_pending, i, 1);

        switch (type) {
        case 0: // High
            pending = high_pending;
            break;
        case 1: // Low
            pending = low_pending;
            break;
        case 2: // Rise
            pending = rise_pending;
            break;
        case 3: // Fall
            pending = fall_pending;
            break;
        case 4: // Rise or Fall
            pending = rise_pending || fall_pending;
            break;
        default :
            pending = false;
            break;
        }
        if (pending && enabled) {
            any = deposit32(any, i, 1, 1);
        }
    }

    trace_pse_gpio_update_output_irq(any);
    qemu_set_irq(s->irq, any != 0);
}

static void update_state(PSEGPIOState *s)
{
    size_t i;
    bool prev_ival, in, in_mask, output_en, input_en, actual_value, ival, oval,
        gpout, rise_pending, fall_pending, low_pending, high_pending;

    for (i = 0; i < PSE_GPIO_PINS; i++) {
        prev_ival = extract32(s->value, i, 1);
        gpout = extract32(s->gpout, i, 1);
        output_en = (s->config[i] & 1) != 0;
        input_en  = (s->config[i] & 2) != 0;
        rise_pending = extract32(s->rise_pending, i, 1);
        fall_pending = extract32(s->fall_pending, i, 1);
        high_pending = extract32(s->high_pending, i, 1);
        low_pending  = extract32(s->low_pending, i, 1);

        in        = extract32(s->in, i, 1);
        in_mask   = extract32(s->in_mask, i, 1);

        /* Output value (IOF not supported) */
        oval = output_en && gpout;

        /* Pin both driven externally and internally */
        if (output_en && in_mask) {
            qemu_log_mask(LOG_GUEST_ERROR, "GPIO pin %zu short circuited\n", i);
        }

        if (in_mask) {
            /* The pin is driven by external device */
            actual_value = in;
        } else if (output_en) {
            /* The pin is driven by internal circuit */
            actual_value = oval;
        } else {
            /* Floating? No pull register on this device. */
            actual_value = false;
        }

        qemu_set_irq(s->output[i], actual_value);

        /* Input value */
        ival = input_en && actual_value;

        /* Interrupts */
        s->high_pending = deposit32(s->high_pending, i, 1,
                                    ival || high_pending);

        s->low_pending = deposit32(s->low_pending,  i, 1,
                                   !ival || low_pending);

        s->rise_pending = deposit32(s->rise_pending, i, 1,
                                    (ival && !prev_ival) || rise_pending);

        s->fall_pending = deposit32(s->fall_pending, i, 1,
                                    (!ival && prev_ival) || fall_pending);

        /* Update value */
        s->value = deposit32(s->value, i, 1, ival);
    }
    trace_pse_gpio_output_state(s->value);
    update_output_irq(s);
}

static uint64_t pse_gpio_read(void *opaque, hwaddr offset, unsigned int size)
{
    PSEGPIOState *s = PSE_GPIO(opaque);
    uint64_t r = 0;

    switch (offset) {
    case 0 ... 0x7c: /* CONFIG_X */
        r = s->config[offset / 4];
        break;

    case 0x80: /* INTR */
        r = s->high_pending | s->low_pending |
            s->rise_pending | s->fall_pending;
        break;

    case 0x84: /* GPIN */
        r = s->value;
        break;

    case 0x88: /* GPOUT */
        r = s->gpout;
        break;

    case 0x8C: /* CONFIG_ALL */
    case 0xA0: /* CLEAR_BITS */
    case 0xA4: /* SET_BITS */

        /* Not implemented */

    default:
        r = 0;
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: bad read offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
    }

    trace_pse_gpio_read(offset, r);

    return r;
}

static void pse_gpio_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned int size)
{
    PSEGPIOState *s = PSE_GPIO(opaque);

    trace_pse_gpio_write(offset, value);

    switch (offset) {

    case 0 ... 0x7c: /* CONFIG_X */
        s->config[offset / 4] = value;
        break;

    case 0x80: /* INTR */
        /* Write 1 to clear */
        s->high_pending &= ~value;
        s->low_pending &= ~value;
        s->rise_pending &= ~value;
        s->fall_pending &= ~value;
        break;

    case 0x84: /* GPIN */
        break;

    case 0x88: /* GPOUT */
        s->gpout = value;
        break;

    case 0x8C: /* CONFIG_ALL */
    case 0xA0: /* CLEAR_BITS */
    case 0xA4: /* SET_BITS */

        /* Not implemented */

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: bad write offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
    }

    update_state(s);
}

static const MemoryRegionOps gpio_ops = {
    .read =  pse_gpio_read,
    .write = pse_gpio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

static void pse_gpio_set(void *opaque, int line, int value)
{
    PSEGPIOState *s = PSE_GPIO(opaque);

    trace_pse_gpio_set(line, value);

    assert(line >= 0 && line < PSE_GPIO_PINS);

    s->in_mask = deposit32(s->in_mask, line, 1, value >= 0);
    if (value >= 0) {
        s->in = deposit32(s->in, line, 1, value != 0);
    }

    update_state(s);
}

static void pse_gpio_reset(DeviceState *dev)
{
    PSEGPIOState *s = PSE_GPIO(dev);

    for (int i = 0; i < PSE_GPIO_PINS; i++) {
        s->config[i] = 0;
    }
    s->value = 0;
    s->gpout = 0;
}

static const VMStateDescription vmstate_pse_gpio = {
    .name = TYPE_PSE_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(config, PSEGPIOState, PSE_GPIO_PINS),
        VMSTATE_UINT32(value, PSEGPIOState),
        VMSTATE_UINT32(gpout, PSEGPIOState),
        VMSTATE_UINT32(rise_pending, PSEGPIOState),
        VMSTATE_UINT32(fall_pending, PSEGPIOState),
        VMSTATE_UINT32(high_pending, PSEGPIOState),
        VMSTATE_UINT32(low_pending,  PSEGPIOState),
        VMSTATE_UINT32(in, PSEGPIOState),
        VMSTATE_UINT32(in_mask,  PSEGPIOState),
        VMSTATE_END_OF_LIST()
    }
};

static void pse_gpio_init(Object *obj)
{
    PSEGPIOState *s = PSE_GPIO(obj);

    memory_region_init_io(&s->mmio, obj, &gpio_ops, s,
            TYPE_PSE_GPIO, PSE_GPIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    qdev_init_gpio_in(DEVICE(s), pse_gpio_set, PSE_GPIO_PINS);
    qdev_init_gpio_out(DEVICE(s), s->output, PSE_GPIO_PINS);
}

static void pse_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_pse_gpio;
    dc->reset = pse_gpio_reset;
    dc->desc = "PSE_GPIO";
}

static const TypeInfo pse_gpio_info = {
    .name = TYPE_PSE_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PSEGPIOState),
    .instance_init = pse_gpio_init,
    .class_init = pse_gpio_class_init
};

static void pse_gpio_register_types(void)
{
    type_register_static(&pse_gpio_info);
}

type_init(pse_gpio_register_types)
