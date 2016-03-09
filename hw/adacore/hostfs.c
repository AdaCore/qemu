/*
 * QEMU HostFS
 *
 * Copyright (c) 2013-2018 AdaCore
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
#include "hw/hw.h"
#include "qemu-common.h"
#include "hw/qdev.h"
#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "trace.h"
#include "hw/adacore/hostfs.h"
#include "hw/target_size.h"

/* #define DEBUG_HOSTFS */

#define REG_NUMBER 6

typedef struct hostfs_Register_Definition {
    uint32_t offset;
    const char *name;
    const char *desc;
    uint32_t access;
    uint64_t reset;
} hostfs_Register_Definition;

#define ACC_RW      1           /* Read/Write */
#define ACC_RO      2           /* Read Only */
#define ACC_WO      3           /* Write Only */
#define ACC_w1c     4           /* Write 1 to clear */
#define ACC_UNKNOWN 4           /* Unknown register */

#ifndef TARGET_IS_64BITS
#error "Unknown error"
#endif

#if TARGET_IS_64BITS
#define HOSTFS_REG_SIZE (8)
#else
#define HOSTFS_REG_SIZE (4)
#endif

/* Index of each register */
#define HOSTFS_SYSCALL_ID (0)
#define HOSTFS_ARG1       (1)
#define HOSTFS_ARG2       (2)
#define HOSTFS_ARG3       (3)
#define HOSTFS_ARG4       (4)
#define HOSTFS_ARG5       (5)
#define HOSTFS_REG_INDEX(n) (n / HOSTFS_REG_SIZE)

/* Offset of each register */
#define HOSTFS_SYSCALL_ID_OFFSET (HOSTFS_SYSCALL_ID * HOSTFS_REG_SIZE)
#define HOSTFS_ARG1_OFFSET       (HOSTFS_ARG1 * HOSTFS_REG_SIZE)
#define HOSTFS_ARG2_OFFSET       (HOSTFS_ARG2 * HOSTFS_REG_SIZE)
#define HOSTFS_ARG3_OFFSET       (HOSTFS_ARG3 * HOSTFS_REG_SIZE)
#define HOSTFS_ARG4_OFFSET       (HOSTFS_ARG4 * HOSTFS_REG_SIZE)
#define HOSTFS_ARG5_OFFSET       (HOSTFS_ARG5 * HOSTFS_REG_SIZE)

const hostfs_Register_Definition hostfs_registers_def[] = {
    {HOSTFS_SYSCALL_ID_OFFSET, "SYSCALL_ID", "Syscall ID", ACC_RW, 0},
    {HOSTFS_ARG1_OFFSET, "ARG1", "1st argument", ACC_RW, 0},
    {HOSTFS_ARG2_OFFSET, "ARG2", "2nd argument", ACC_RW, 0},
    {HOSTFS_ARG3_OFFSET, "ARG3", "3rd argument", ACC_RW, 0},
    {HOSTFS_ARG4_OFFSET, "ARG4", "4th argument", ACC_RW, 0},
    {HOSTFS_ARG5_OFFSET, "ARG5", "5th argument", ACC_RW, 0},
    /* End Of Table */
    {0x0, 0x0, 0x0, 0x0, 0x0}
};

typedef struct hostfs_Register {
    const char *name;
    const char *desc;
    uint32_t access;
    uint64_t value;
} hostfs_Register;

typedef struct hostfs {
    SysBusDevice  busdev;

    MemoryRegion  io_area;

    hostfs_Register regs[REG_NUMBER];
} hostfs;

#define TYPE_HOSTFS_DEVICE "hostfs"
#define QEMU_HOSTFS_DEVICE(obj) \
    OBJECT_CHECK(hostfs, (obj), TYPE_HOSTFS_DEVICE)

/* Syscall IDs */
#define HOSTFS_SYSCALL_OPEN     1
#define HOSTFS_SYSCALL_READ     2
#define HOSTFS_SYSCALL_WRITE    3
#define HOSTFS_SYSCALL_CLOSE    4
#define HOSTFS_SYSCALL_UNLINK   5
#define HOSTFS_SYSCALL_LSEEK    6
#define HOSTFS_SYSCALL_SHUTDOWN 7

/* HostFS open flags */
#define HOSTFS_O_RDONLY (1 << 0)
#define HOSTFS_O_WRONLY (1 << 1)
#define HOSTFS_O_CREAT  (1 << 2)
#define HOSTFS_O_RDWR   (1 << 3)
#define HOSTFS_O_APPEND (1 << 4)
#define HOSTFS_O_TRUNC  (1 << 5)
#define HOSTFS_O_BINARY (1 << 6)
#define HOSTFS_O_EXCL   (1 << 7)

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
    if (hostfs_flags & HOSTFS_O_EXCL) {
        ret |= O_EXCL;
    }
#ifdef _WIN32
    if (hostfs_flags & HOSTFS_O_BINARY) {
        ret |= O_BINARY;
    }
#endif
    trace_hostfs_open_flags(hostfs_flags, ret);
    return ret;
}

static uint64_t do_syscall(hostfs *hfs)
{
    uint64_t ID = hfs->regs[HOSTFS_SYSCALL_ID].value;
    uint64_t arg1 = hfs->regs[HOSTFS_ARG1].value;
    uint64_t arg2 = hfs->regs[HOSTFS_ARG2].value;
    uint64_t arg3 = hfs->regs[HOSTFS_ARG3].value;
    uint64_t arg4 = hfs->regs[HOSTFS_ARG4].value;
    uint64_t arg5 = hfs->regs[HOSTFS_ARG5].value;
    uint64_t ret = 0;
    char *buf = NULL;
    hwaddr addr;
    hwaddr plen;

    trace_hostfs_do_syscall(ID, arg1, arg2, arg3, arg4, arg5);

    switch (ID) {
    case HOSTFS_SYSCALL_OPEN:
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
    case HOSTFS_SYSCALL_READ:
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
    case HOSTFS_SYSCALL_WRITE:
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
    case HOSTFS_SYSCALL_CLOSE:
        ret = close(arg1);
        trace_hostfs_close(arg1, ret);
        return ret;
        break;
    case HOSTFS_SYSCALL_UNLINK:
        addr = arg1;
        plen = 1024; /* XXX: maximum length of filename string */

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, false /* is_write */);
        ret = unlink(buf);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        trace_hostfs_unlink(arg1, ret);
        return ret;
        break;
    case HOSTFS_SYSCALL_LSEEK:
        ret = lseek((int)arg1, (int)arg2, (int)arg3);
        trace_hostfs_lseek((int)arg1, (int)arg2, (int)arg3, ret);
        return ret;
        break;
    case HOSTFS_SYSCALL_SHUTDOWN:
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        return ret;
        break;
    }

    return 0;
}

static uint64_t hostfs_read(void *opaque, hwaddr addr, unsigned size)
{
    hostfs *hfs = opaque;
    uint64_t reg_index = HOSTFS_REG_INDEX(addr);
    hostfs_Register *reg = NULL;
    uint64_t ret = 0x0;

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

static void hostfs_write(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    hostfs *hfs = opaque;
    uint64_t reg_index = HOSTFS_REG_INDEX(addr);
    hostfs_Register *reg = NULL;
    uint64_t before = 0x0;

    assert(reg_index < REG_NUMBER);
    assert(!(addr % HOSTFS_REG_SIZE));

    reg = &hfs->regs[reg_index];
    before = reg->value;

    switch (reg_index) {
    case HOSTFS_SYSCALL_ID:
        hfs->regs[HOSTFS_SYSCALL_ID].value = value;
        hfs->regs[HOSTFS_ARG1].value = do_syscall(hfs);
        hfs->regs[HOSTFS_ARG2].value = 0;
        hfs->regs[HOSTFS_ARG3].value = 0;
        hfs->regs[HOSTFS_ARG4].value = 0;
        hfs->regs[HOSTFS_ARG5].value = 0;
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
    .valid = {
        .min_access_size = HOSTFS_REG_SIZE,
        .max_access_size = HOSTFS_REG_SIZE,
    },
    .impl = {
        .min_access_size = HOSTFS_REG_SIZE,
        .max_access_size = HOSTFS_REG_SIZE,
    },
};

static void hostfs_reset(DeviceState *d)
{
    hostfs *hfs = QEMU_HOSTFS_DEVICE(d);
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

        reg_index = HOSTFS_REG_INDEX(hostfs_registers_def[i].offset);

        hfs->regs[reg_index].name   = hostfs_registers_def[i].name;
        hfs->regs[reg_index].desc   = hostfs_registers_def[i].desc;
        hfs->regs[reg_index].access = hostfs_registers_def[i].access;
        hfs->regs[reg_index].value  = hostfs_registers_def[i].reset;
    }
}

static int hostfs_init(SysBusDevice *dev)
{
    hostfs *hfs = QEMU_HOSTFS_DEVICE(dev);

    memory_region_init_io(&hfs->io_area, OBJECT(hfs),
                          &hostfs_ops, hfs, TYPE_HOSTFS_DEVICE, REG_NUMBER
                                                        * HOSTFS_REG_SIZE);
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
    .name          = TYPE_HOSTFS_DEVICE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(hostfs),
    .class_init    = hostfs_class_init,
};

static void hostfs_register_types(void)
{
    type_register_static(&hostfs_info);
}

type_init(hostfs_register_types)

DeviceState *hostfs_create(hwaddr base, MemoryRegion *mr)
{
    DeviceState *dev;
    SysBusDevice *sbdev;

    dev = qdev_create(NULL, TYPE_HOSTFS_DEVICE);

    qdev_init_nofail(dev);

    sbdev = SYS_BUS_DEVICE(dev);
    sbdev->mmio[0].addr = base;
    memory_region_add_subregion(mr, base, sbdev->mmio[0].memory);

    return dev;
}
