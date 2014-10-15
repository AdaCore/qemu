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

static uint32_t apbppmem[32*2];    /* 32-entry APB PP AREA */
static int apbppindex;

int grlib_apbpp_add(uint32_t id, uint32_t addr)
{
    apbppmem[apbppindex++] = id;
    apbppmem[apbppindex++] = addr;
    if(apbppindex >= (32*2)) apbppindex = 0; /* prevent overflow of area */
    return(apbppindex);
}

static uint64_t grlib_apbpnp_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    uint64_t read_data;
    addr &= 0xff;
    read_data = apbppmem[addr>>2];

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

static uint32_t ahbppmem[128*8];    /* 128-entry AHB PP AREA */
static int ahbmppindex;
static int ahbsppindex = 64*8;

int grlib_ahbmpp_add(uint32_t id)
{
    ahbppmem[ahbmppindex] = id;
    ahbmppindex += 8;
    if(ahbmppindex >= (64*8)) ahbmppindex = 0; /* prevent overflow of area */
    return(ahbmppindex);
}

int grlib_ahbspp_add(uint32_t id, uint32_t addr1, uint32_t addr2,
                     uint32_t addr3, uint32_t addr4)
{
    ahbppmem[ahbsppindex] = id;
    ahbsppindex += 4;
    ahbppmem[ahbsppindex++] = addr1;
    ahbppmem[ahbsppindex++] = addr2;
    ahbppmem[ahbsppindex++] = addr3;
    ahbppmem[ahbsppindex++] = addr4;
    if(ahbsppindex >= (128*8)) ahbsppindex = 64*8; /* prevent overflow of area */
    return(ahbsppindex);
}

static uint64_t grlib_ahbpnp_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    uint64_t read_data;

    addr &= 0xfff;
    read_data = ahbppmem[addr>>2];

    if (size == 1) {
        read_data >>= (24 - (addr & 3) * 8);
        read_data &= 0x0ff;
    }
    return read_data;
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
