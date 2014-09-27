/*
 * QEMU GRLIB AMBA Plug&Play Emulator
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

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "trace.h"

#define APBPNP_REG_SIZE 4096     /* Size of memory mapped registers */

#define TYPE_GRLIB_APB_PNP "grlib,apbpnp"
#define GRLIB_APB_PNP(obj) \
    OBJECT_CHECK(APBPNP, (obj), TYPE_GRLIB_APB_PNP)

typedef struct APBPNP {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
} APBPNP;

static uint64_t grlib_apbpnp_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    uint64_t read_data;
    addr &= 0xfff;

    /* Unit registers */
    switch (addr & 0xffc) {
    case 0x00:
	read_data = 0x0400f000;	/* Memory controller */
	break;
    case 0x04:
	read_data = 0x0000fff1;
	break;
    case 0x08:
	read_data = 0x0100c023;	/* APBUART */
	break;
    case 0x0C:
	read_data = 0x0010fff1;
	break;
    case 0x10:
	read_data = 0x0100d040;	/* IRQMP */
	break;
    case 0x14:
	read_data = 0x0020fff1;
	break;
    case 0x18:
	read_data = 0x01011006;	/* GPTIMER */
	break;
    case 0x1C:
	read_data = 0x0030fff1;
	break;

    default:
        read_data = 0;
    }
    if (size == 1) {
	read_data >>= (24 - (addr & 3) * 8);
	read_data &= 0x0ff;
    }
    return (read_data);
}

static void grlib_apbpnp_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
}

static const MemoryRegionOps grlib_apbpnp_ops = {
    .write      = grlib_apbpnp_write,
    .read       = grlib_apbpnp_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void grlib_apbpnp_realize(DeviceState *dev, Error **errp)
{
    APBPNP *pnp = GRLIB_APB_PNP(dev);

    memory_region_init_io(&pnp->iomem, OBJECT(pnp), &grlib_apbpnp_ops, pnp,
                          "apbpnp", APBPNP_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &pnp->iomem);
}

static void grlib_apbpnp_reset(DeviceState *d)
{
}

static Property grlib_apbpnp_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void grlib_apbpnp_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = grlib_apbpnp_realize;
    dc->reset = grlib_apbpnp_reset;
    dc->props = grlib_apbpnp_properties;
}

static const TypeInfo grlib_apbpnp_info = {
    .name          = TYPE_GRLIB_APB_PNP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(APBPNP),
    .class_init    = grlib_apbpnp_class_init,
};

static void grlib_apbpnp_register_types(void)
{
    type_register_static(&grlib_apbpnp_info);
}

type_init(grlib_apbpnp_register_types)


/* AHB PNP */

#define AHBPNP_REG_SIZE 4096-8     /* Size of memory mapped registers */

#define TYPE_GRLIB_AHB_PNP "grlib,ahbpnp"
#define GRLIB_AHB_PNP(obj) \
    OBJECT_CHECK(AHBPNP, (obj), TYPE_GRLIB_AHB_PNP)

typedef struct AHBPNP {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
} AHBPNP;

static uint64_t grlib_ahbpnp_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    addr &= 0xffc;

    /* Unit registers */
    switch (addr) {
    case 0:
	return 0x01003000;	/* LEON3 */
    case 0x800:
	return 0x0400f000;	/* Memory controller  */
    case 0x810:
	return 0x0003e002;
    case 0x814:
	return 0x2000e002;
    case 0x818:
	return 0x4003c002;
    case 0x820:
	return 0x01006000;	/* APB bridge @ 0x80000000 */
    case 0x830:
	return 0x8000fff2;

    default:
        return 0;
    }
}

static void grlib_ahbpnp_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
}

static const MemoryRegionOps grlib_ahbpnp_ops = {
    .write      = grlib_ahbpnp_write,
    .read       = grlib_ahbpnp_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void grlib_ahbpnp_realize(DeviceState *dev, Error **errp)
{
    AHBPNP *pnp = GRLIB_AHB_PNP(dev);

    memory_region_init_io(&pnp->iomem, OBJECT(pnp), &grlib_ahbpnp_ops, pnp,
                          "ahbpnp", AHBPNP_REG_SIZE);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &pnp->iomem);
}

static void grlib_ahbpnp_reset(DeviceState *d)
{
}

static Property grlib_ahbpnp_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void grlib_ahbpnp_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = grlib_ahbpnp_realize;
    dc->reset = grlib_ahbpnp_reset;
    dc->props = grlib_ahbpnp_properties;
}

static const TypeInfo grlib_ahbpnp_info = {
    .name          = TYPE_GRLIB_AHB_PNP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AHBPNP),
    .class_init    = grlib_ahbpnp_class_init,
};

static void grlib_ahbpnp_register_types(void)
{
    type_register_static(&grlib_ahbpnp_info);
}

type_init(grlib_ahbpnp_register_types)
