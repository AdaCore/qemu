/*
 * QEMU HostFS
 *
 * Copyright (c) 2012 AdaCore
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
#include "qemu-common.h"
#include "qdev.h"
#include "sysemu.h"
#include "sysbus.h"
#include "osdep.h"
#include "trace.h"

#include "hostfs.h"

/* #define DEBUG_HOSTFS */

#define REG_NUMBER 6

typedef struct hostfs_Register_Definition {
    uint32_t offset;
    const char *name;
    const char *desc;
    uint32_t access;
    uint32_t reset;
} hostfs_Register_Definition;

#define ACC_RW      1           /* Read/Write */
#define ACC_RO      2           /* Read Only */
#define ACC_WO      3           /* Write Only */
#define ACC_w1c     4           /* Write 1 to clear */
#define ACC_UNKNOWN 4           /* Unknown register */

const hostfs_Register_Definition hostfs_registers_def[] = {
{0x000, "SYSCALL_ID", "Syscall ID",   ACC_RW, 0x00000000},
{0x004, "ARG1",       "1st argument", ACC_RW, 0x00000000},
{0x008, "ARG2",       "2nd argument", ACC_RW, 0x00000000},
{0x00c, "ARG3",       "3rd argument", ACC_RW, 0x00000000},
{0x010, "ARG4",       "4th argument", ACC_RW, 0x00000000},
{0x014, "ARG5",       "5th argument", ACC_RW, 0x00000000},
/* End Of Table */
{0x0, 0x0, 0x0, 0x0, 0x0}
};

/* Index of each register */

#define SYSCALL_ID (0x000 / 4)
#define ARG1       (0x004 / 4)
#define ARG2       (0x008 / 4)
#define ARG3       (0x00c / 4)
#define ARG4       (0x010 / 4)
#define ARG5       (0x014 / 4)

typedef struct hostfs_Register {
    const char *name;
    const char *desc;
    uint32_t    access;
    uint32_t    value;
} hostfs_Register;

typedef struct hostfs {
    SysBusDevice  busdev;

    MemoryRegion  io_area;

    hostfs_Register regs[REG_NUMBER];
} hostfs;

/* Syscall IDs */
#define SYSCALL_OPEN  1
#define SYSCALL_READ  2
#define SYSCALL_WRITE 3
#define SYSCALL_CLOSE 4

/* HostFS open flags */
#define HOSTFS_O_RDONLY (1 << 0)
#define HOSTFS_O_WRONLY (1 << 1)
#define HOSTFS_O_CREAT  (1 << 2)
#define HOSTFS_O_RDWR   (1 << 3)
#define HOSTFS_O_APPEND (1 << 4)
#define HOSTFS_O_TRUNC  (1 << 5)
#define HOSTFS_O_BINARY (1 << 6)

static uint32_t open_flags(uint32_t hostfs_flags)
{
    int ret = 0;

    if (hostfs_flags & HOSTFS_O_RDONLY) {
        ret |= O_RDONLY;
    }
    if (hostfs_flags & HOSTFS_O_WRONLY) {
        ret |= O_WRONLY;
    }
    if (hostfs_flags & HOSTFS_O_RDWR) {
        ret |= O_RDWR;
    }
    if (hostfs_flags & HOSTFS_O_CREAT) {
        ret |= O_CREAT;
    }
    if (hostfs_flags & HOSTFS_O_APPEND) {
        ret |= O_APPEND;
    }
    if (hostfs_flags & HOSTFS_O_TRUNC) {
        ret |= O_TRUNC;
    }
#ifdef _WIN32
    if (hostfs_flags & HOSTFS_O_BINARY) {
        ret |= O_BINARY;
    }
#endif
    trace_hostfs_open_flags(hostfs_flags, ret);
    return ret;
}

static uint32_t do_syscall(hostfs *hfs)
{
    uint32_t  ID   = hfs->regs[SYSCALL_ID].value;
    uint32_t  arg1 = hfs->regs[ARG1].value;
    uint32_t  arg2 = hfs->regs[ARG2].value;
    uint32_t  arg3 = hfs->regs[ARG3].value;
    uint32_t  arg4 = hfs->regs[ARG4].value;
    uint32_t  arg5 = hfs->regs[ARG5].value;
    uint32_t  ret  = 0;
    char     *buf  = NULL;
    target_phys_addr_t addr;
    target_phys_addr_t plen;

    trace_hostfs_do_syscall(ID, arg1, arg2, arg3, arg4, arg5);

    switch (ID) {
    case SYSCALL_OPEN:
        addr = arg1;
        plen = 1024; /* XXX: maximum length of filename string */
        arg2 = open_flags(arg2);

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, false /* is_write */);
        ret = open(buf, arg2, arg3);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        trace_hostfs_open(buf, arg2, arg3, ret);

        return ret;
        break;
    case SYSCALL_READ:
        addr = arg2;
        plen = arg3;

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, true /* is_write */);
        ret = read(arg1, buf, arg3);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        if ((int)ret == -1) {
            perror("hostfs");
        }

        trace_hostfs_read(arg1, buf, arg3, ret);
        return ret;
        break;
    case SYSCALL_WRITE:
        addr = arg2;
        plen = arg3;

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, false /* is_write */);
        ret = write(arg1, buf, arg3);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        if ((int)ret == -1) {
            perror("hostfs");
        }

        trace_hostfs_write(arg1, buf, arg3, ret);

        return ret;
        break;
    case SYSCALL_CLOSE:
        ret = close(arg1);
        trace_hostfs_close(arg1, ret);
        return ret;
        break;
    }

    return 0;
}

static uint64_t hostfs_read(void *opaque, target_phys_addr_t addr,
                            unsigned size)
{
    hostfs          *hfs     = opaque;
    uint32_t        reg_index = addr / 4;
    hostfs_Register *reg       = NULL;
    uint32_t        ret       = 0x0;

    assert(reg_index < REG_NUMBER);

    reg = &hfs->regs[reg_index];

    switch (reg->access) {
    case ACC_WO:
        ret = 0x00000000;
        break;

    case ACC_RW:
    case ACC_w1c:
    case ACC_RO:
    default:
        ret = reg->value;
        break;
    }

#ifdef DEBUG_HOSTFS
    printf("Read  0x%08x @ 0x" TARGET_FMT_plx
           "                            : %s (%s)\n",
           ret, addr, reg->name, reg->desc);
#endif

    return ret;
}

static void
hostfs_write(void *opaque, target_phys_addr_t addr, uint64_t value,
            unsigned size)
{
    hostfs          *hfs     = opaque;
    uint32_t        reg_index = addr / 4;
    hostfs_Register *reg       = NULL;
    uint32_t        before    = 0x0;

    assert(reg_index < REG_NUMBER);

    reg = &hfs->regs[reg_index];
    before = reg->value;

    switch (reg_index) {
    case SYSCALL_ID:
        hfs->regs[SYSCALL_ID].value = value;
        hfs->regs[ARG1].value = do_syscall(hfs);
        hfs->regs[ARG2].value = 0;
        hfs->regs[ARG3].value = 0;
        hfs->regs[ARG4].value = 0;
        hfs->regs[ARG5].value = 0;
        break;

    default:
        /* Default handling */
        switch (reg->access) {

        case ACC_RW:
        case ACC_WO:
            reg->value = value;
            break;

        case ACC_w1c:
            reg->value &= ~value;
            break;

        case ACC_RO:
        default:
            /* Read Only or Unknown register */
            break;
        }
    }

#ifdef DEBUG_HOSTFS
    printf("Write 0x%08x @ 0x" TARGET_FMT_plx" val:0x%08x->0x%08x : %s (%s)\n",
           (unsigned int)value, addr, before, reg->value, reg->name,
           reg->desc);
#else
    (void)before;
#endif

}

static const MemoryRegionOps hostfs_ops = {
    .read = hostfs_read,
    .write = hostfs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void hostfs_reset(DeviceState *d)
{
    hostfs *hfs = container_of(d, hostfs, busdev.qdev);
    int i = 0;
    int reg_index = 0;

    /* Default value for all registers */
    for (i = 0; i < REG_NUMBER; i++) {
        hfs->regs[i].name   = "Reserved";
        hfs->regs[i].desc   = "";
        hfs->regs[i].access = ACC_UNKNOWN;
        hfs->regs[i].value  = 0x00000000;
    }

    /* Set-up known registers */
    for (i = 0; hostfs_registers_def[i].name != NULL; i++) {

        reg_index = hostfs_registers_def[i].offset / 4;

        hfs->regs[reg_index].name   = hostfs_registers_def[i].name;
        hfs->regs[reg_index].desc   = hostfs_registers_def[i].desc;
        hfs->regs[reg_index].access = hostfs_registers_def[i].access;
        hfs->regs[reg_index].value  = hostfs_registers_def[i].reset;
    }
}

static int hostfs_init(SysBusDevice *dev)
{
    hostfs *hfs = FROM_SYSBUS(typeof(*hfs), dev);

    memory_region_init_io(&hfs->io_area, &hostfs_ops, hfs, "hostfs",
                          REG_NUMBER * 4);

    sysbus_init_mmio(dev, &hfs->io_area);

    return 0;
}

static void hostfs_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = hostfs_init;
    dc->reset = hostfs_reset;
}

static TypeInfo hostfs_info = {
    .name          = "hostfs",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(hostfs),
    .class_init    = hostfs_class_init,
};

static void hostfs_register_types(void)
{
    type_register_static(&hostfs_info);
}

type_init(hostfs_register_types)

DeviceState *hostfs_create(target_phys_addr_t  base,
                           MemoryRegion       *mr)
{
    DeviceState *dev;
    SysBusDevice *sbdev;

    dev = qdev_create(NULL, "hostfs");

    if (qdev_init(dev)) {
        return NULL;
    }

    sbdev = sysbus_from_qdev(dev);
    sbdev->mmio[0].addr = base;
    memory_region_add_subregion(mr, base, sbdev->mmio[0].memory);

    return dev;
}
