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

#include "hw/sysbus.h"
#include "hw/sparc/grlib.h"

/* Size of memory mapped registers */
#define APBPNP_REG_SIZE (4096 - 8)
#define AHBPNP_REG_SIZE 4096

#define GRLIB_AMBA_PNP(obj) \
    OBJECT_CHECK(AMBAPNP, (obj), TYPE_GRLIB_AMBA_PNP)

typedef struct AMBAPNP {
    SysBusDevice parent_obj;
    MemoryRegion ahb_iomem;
    MemoryRegion apb_iomem;
} AMBAPNP;

/* APB PNP */

static uint64_t grlib_apbpnp_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    uint64_t read_data;
    addr &= 0xfff;

    /* Unit registers */
    switch (addr & 0xffc) {
    case 0x00:
        read_data = 0x0400f000; /* Memory controller */
        break;
    case 0x04:
        read_data = 0x0000fff1;
        break;
    case 0x08:
        read_data = 0x0100c023; /* APBUART */
        break;
    case 0x0C:
        read_data = 0x0010fff1;
        break;
    case 0x10:
        read_data = 0x0100d040; /* IRQMP */
        break;
    case 0x14:
        read_data = 0x0020fff1;
        break;
    case 0x18:
        read_data = 0x01011006; /* GPTIMER */
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
    return read_data;
}

static const MemoryRegionOps grlib_apbpnp_ops = {
    .read       = grlib_apbpnp_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* AHB PNP */

static uint64_t grlib_ahbpnp_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    addr &= 0xffc;

    /* Unit registers */
    switch (addr) {
    case 0:
        return 0x01003000;      /* LEON3 */
    case 0x800:
        return 0x0400f000;      /* Memory controller  */
    case 0x810:
        return 0x0003e002;
    case 0x814:
        return 0x2000e002;
    case 0x818:
        return 0x4003c002;
    case 0x820:
        return 0x01006000;      /* APB bridge @ 0x80000000 */
    case 0x830:
        return 0x8000fff2;

    default:
        return 0;
    }
}

static const MemoryRegionOps grlib_ahbpnp_ops = {
    .read = grlib_ahbpnp_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void grlib_ambapnp_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    AMBAPNP *pnp = GRLIB_AMBA_PNP(obj);

    memory_region_init_io(&pnp->ahb_iomem, OBJECT(pnp), &grlib_ahbpnp_ops, pnp,
                          "ahbpnp", AHBPNP_REG_SIZE);
    sysbus_init_mmio(sbd, &pnp->ahb_iomem);

    memory_region_init_io(&pnp->apb_iomem, OBJECT(pnp), &grlib_apbpnp_ops, pnp,
                          "apbpnp", APBPNP_REG_SIZE);
    sysbus_init_mmio(sbd, &pnp->apb_iomem);
}

static const TypeInfo grlib_ambapnp_info = {
    .name          = TYPE_GRLIB_AMBA_PNP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AMBAPNP),
    .instance_init = grlib_ambapnp_init,
};

static void grlib_ambapnp_register_types(void)
{
    type_register_static(&grlib_ambapnp_info);
}

type_init(grlib_ambapnp_register_types)
