/*
 * QEMU HostFS
 *
 * Copyright (c) 2013-2020 AdaCore
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
#include "hw/qdev-core.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "cpu.h"
#include "system/system.h"
#include "system/runstate.h"
#include "trace.h"
#include "hw/adacore/hostfs.h"
#include "qapi/error.h"
#include "qemu/queue.h"
#include "migration/vmstate.h"
#include "qemu/log.h"

/* #define DEBUG_HOSTFS */

#define REG_NUMBER 6

typedef struct hostfs_Register_Definition {
    uint32_t offset;
    const char *name;
    const char *desc;
    uint64_t reset;
} hostfs_Register_Definition;

#define HOSTFS_REG_SIZE (8)

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
    {HOSTFS_SYSCALL_ID_OFFSET, "SYSCALL_ID", "Syscall ID", 0},
    {HOSTFS_ARG1_OFFSET, "ARG1", "1st argument", 0},
    {HOSTFS_ARG2_OFFSET, "ARG2", "2nd argument", 0},
    {HOSTFS_ARG3_OFFSET, "ARG3", "3rd argument", 0},
    {HOSTFS_ARG4_OFFSET, "ARG4", "4th argument", 0},
    {HOSTFS_ARG5_OFFSET, "ARG5", "5th argument", 0},
    /* End Of Table */
    {0x0, 0x0, 0x0, 0x0}
};

typedef struct hostfs_Register {
    const char *name;
    const char *desc;
    uint64_t value;
} hostfs_Register;

typedef struct hostfs {
    SysBusDevice  busdev;

    MemoryRegion  io_area;

    hostfs_Register regs[REG_NUMBER];

    QLIST_HEAD(, open_file) open_files_qlist;

    int32_t next_guest_fd;
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

typedef struct open_file {
    int32_t guest_fd;
    uint64_t host_fd;
    uint32_t flags;
    uint64_t mode;
    uint8_t filename[1024];
    off_t offset;
    QLIST_ENTRY(open_file) next;
} open_file;

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

static open_file* get_file_from_guest_fd(uint64_t fd, hostfs *hfs){
    open_file *current_file;

    /* This search could be faster by using for example a GHashTable structure,
        but there is no VMState to migrate it easily, and we shouldn't have too
        many opened files at the same time. */
    QLIST_FOREACH(current_file, &hfs->open_files_qlist, next)
    {
        if (current_file->guest_fd == fd)
        {
            return current_file;
        }
    }
    qemu_log_mask(LOG_GUEST_ERROR, "hostfs error, invalid guest file descriptor\n");

    return NULL;
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
    char temp_buffer[1024];
    hwaddr addr;
    hwaddr plen;
    open_file *current_file;

    trace_hostfs_do_syscall(ID, arg1, arg2, arg3, arg4, arg5);

    switch (ID) {
    case HOSTFS_SYSCALL_OPEN:
        addr = arg1;
        plen = 1024; /* XXX: maximum length of filename string */
        arg2 = open_flags(arg2);

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, false /* is_write */);
        memcpy(temp_buffer, buf, plen);
        ret = open(buf, arg2, arg3);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        if ((int)ret != -1)
        {
            int32_t current_fd = hfs->next_guest_fd;
            open_file *file = g_malloc(sizeof(open_file));

            file->flags = arg2;
            file->mode = arg3;
            file->host_fd = ret;

            if (current_fd < INT32_MAX)
            {
                hfs->next_guest_fd = current_fd + 1;
            }
            else
            {
                qemu_log_mask(LOG_GUEST_ERROR,
                              "Maximum number of guest file descriptors reached");
            }
            file->guest_fd = current_fd;
            ret = current_fd;

            memcpy(file->filename, temp_buffer, plen);

            QLIST_INSERT_HEAD(&hfs->open_files_qlist, file, next);
        }

        trace_hostfs_open(buf, arg2, arg3, ret);

        return ret;
        break;
    case HOSTFS_SYSCALL_READ:
        addr = arg2;
        plen = arg3;

        if (!plen) {
            return 0;
        }

        /* Get the host FD from the abstract guest FD */
        current_file = get_file_from_guest_fd(arg1, hfs);
        if (current_file == NULL){
            return -1;
        }
        arg1 = current_file->host_fd;

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, true /* is_write */);
        ret = read(arg1, buf, arg3);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        if ((int)ret == -1)
        {
            qemu_log_mask(LOG_GUEST_ERROR, "hostfs read failed with error: %s\n",
                          strerror(errno));
        }

        trace_hostfs_read(arg1, buf, arg3, ret);
        return ret;
        break;
    case HOSTFS_SYSCALL_WRITE:
        addr = arg2;
        plen = arg3;

        if (!plen) {
            return 0;
        }

        /* Get the host FD from the abstract guest FD */
        current_file = get_file_from_guest_fd(arg1, hfs);
        if (current_file == NULL){
            return -1;
        }
        arg1 = current_file->host_fd;

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, false /* is_write */);
        ret = write(arg1, buf, arg3);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        if ((int)ret == -1)
        {
            qemu_log_mask(LOG_GUEST_ERROR, "hostfs write failed with error: %s\n",
                          strerror(errno));
        }

        trace_hostfs_write(arg1, buf, arg3, ret);

        return ret;
        break;
    case HOSTFS_SYSCALL_CLOSE:
        /* Get the host FD from the abstract guest FD */
        current_file = get_file_from_guest_fd(arg1, hfs);
        if (current_file == NULL){
            return -1;
        }
        arg1 = current_file->host_fd;

        ret = close(arg1);

        if ((int)ret == -1)
        {
            qemu_log_mask(LOG_GUEST_ERROR, "hostfs close failed with error: %s\n",
                          strerror(errno));
        }

        trace_hostfs_close(arg1, ret);

        QLIST_REMOVE(current_file, next);
        g_free(current_file);

        return ret;
        break;
    case HOSTFS_SYSCALL_UNLINK:
        addr = arg1;
        plen = 1024; /* XXX: maximum length of filename string */

        /* Convert guest buffer to host buffer */
        buf = cpu_physical_memory_map(addr, &plen, false /* is_write */);
        ret = unlink(buf);
        cpu_physical_memory_unmap(buf, plen, false /* is_write */, plen);

        if ((int)ret == -1)
        {
            qemu_log_mask(LOG_GUEST_ERROR, "hostfs unlink failed with error: %s\n",
                          strerror(errno));
        }

        trace_hostfs_unlink(arg1, ret);
        return ret;
        break;
    case HOSTFS_SYSCALL_LSEEK:
        /* Get the host FD from the abstract guest FD */
        current_file = get_file_from_guest_fd(arg1, hfs);
        if (current_file == NULL){
            return -1;
        }
        arg1 = current_file->host_fd;

        ret = lseek((int)arg1, (int)arg2, (int)arg3);

        if ((off_t)ret == -1)
        {
            qemu_log_mask(LOG_GUEST_ERROR, "hostfs lseek failed with error: %s\n",
                          strerror(errno));
        }

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

    if (size == 8) {
        ret = reg->value;
    } else {
        /* 32bit targets will split the operation into two 4-size access.  */
        assert (size == 4);
#if TARGET_BIG_ENDIAN
        if (addr % HOSTFS_REG_SIZE) {
            ret = reg->value & 0xFFFFFFFF;
        } else {
            ret = reg->value >> 32;
        }
#else
        if (addr % HOSTFS_REG_SIZE) {
            ret = reg->value >> 32;
        } else {
            ret = reg->value & 0xFFFFFFFF;
        }
#endif
    }

#ifdef DEBUG_HOSTFS
    if (size == 4) {
        printf("Read  0x%08lx @ 0lx" HWADDR_FMT_plx" %s (%s)\n",
               ret, addr, reg->name, reg->desc);
    } else {
        printf("Read  0x%016lx @ 0lx" HWADDR_FMT_plx" %s (%s)\n",
               ret, addr, reg->name, reg->desc);
    }
#endif

    return ret;
}

static void hostfs_write(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    hostfs *hfs = opaque;
    uint64_t reg_index = HOSTFS_REG_INDEX(addr);
    hostfs_Register *reg = NULL;
    uint64_t before = 0x0, n_value = 0x0;

    assert(reg_index < REG_NUMBER);

    reg = &hfs->regs[reg_index];
    before = reg->value;


    if (size == 8) {
        n_value = value;
    } else {
        /* 32bit targets will split the operation into two 4-size access.  */
        assert (size == 4);
#if TARGET_BIG_ENDIAN
        if (addr % HOSTFS_REG_SIZE) {
            n_value = before | value;
        } else {
            n_value = value << 32;
        }
#else
        if (addr % HOSTFS_REG_SIZE) {
            n_value = value << 32 | before;
        } else {
            n_value = value;
        }

#endif
    }

    switch (reg_index) {
    case HOSTFS_SYSCALL_ID:
        /*
         *  32bit targets second access: aborted to avoid calling
         *  do_syscall twice.
         */
        if (size == 4 && value == 0) {
            break;
        }
        hfs->regs[HOSTFS_SYSCALL_ID].value = value;
        hfs->regs[HOSTFS_ARG1].value = do_syscall(hfs);
        hfs->regs[HOSTFS_ARG2].value = 0;
        hfs->regs[HOSTFS_ARG3].value = 0;
        hfs->regs[HOSTFS_ARG4].value = 0;
        hfs->regs[HOSTFS_ARG5].value = 0;
        break;
    default:
        reg->value = n_value;

    }

#ifdef DEBUG_HOSTFS
    if (size == 4) {
        printf("Write 0x%08lx @ 0x" HWADDR_FMT_plx" val:0x%016lx->0x%016lx : %s (%s)\n",
               value, addr, before, reg->value, reg->name,
               reg->desc);
    } else {
        printf("Write 0x%016lx @ 0x" HWADDR_FMT_plx" val:0x%016lx->0x%016lx : %s (%s)\n",
               value, addr, before, reg->value, reg->name,
               reg->desc);
    }
#else
    (void)before;
#endif

}

static const MemoryRegionOps hostfs_ops = {
    .read = hostfs_read,
    .write = hostfs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 8,
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
        hfs->regs[i].value  = 0x00000000;
    }

    /* Set-up known registers */
    for (i = 0; hostfs_registers_def[i].name != NULL; i++) {

        reg_index = HOSTFS_REG_INDEX(hostfs_registers_def[i].offset);

        hfs->regs[reg_index].name   = hostfs_registers_def[i].name;
        hfs->regs[reg_index].desc   = hostfs_registers_def[i].desc;
        hfs->regs[reg_index].value  = hostfs_registers_def[i].reset;
    }

    /* Free migration-related structures */
    open_file *current_file, *tmp;
    QLIST_FOREACH_SAFE(current_file, &hfs->open_files_qlist, next, tmp)
    {
        QLIST_REMOVE(current_file, next);
        g_free(current_file);
    }
}

static void hostfs_realize(DeviceState *dev, Error **errp)
{
    hostfs *hfs = QEMU_HOSTFS_DEVICE(dev);

    memory_region_init_io(&hfs->io_area, OBJECT(hfs),
                          &hostfs_ops, hfs, TYPE_HOSTFS_DEVICE, REG_NUMBER
                                                        * HOSTFS_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &hfs->io_area);
}

static int hostfs_pre_save(void *opaque){
    /* Save the offset associated to each FD */
    off_t ret_offset;
    hostfs *hfs = opaque;
    open_file *current_file;

    QLIST_FOREACH(current_file, &hfs->open_files_qlist, next)
    {
        /* Get the current offset */
        ret_offset = lseek(current_file->host_fd, 0, SEEK_CUR);

        if (ret_offset == -1){
            qemu_log_mask(LOG_GUEST_ERROR,
                          "hostfs unable to get the offset of file '%s' during"
                          "migration.\n"
                          "lseek failed with error: %s\n",
                          current_file->filename,
                          strerror(errno));
            return -1;
        }
        current_file->offset = (int64_t) ret_offset;
    }

    return 0;
}

static int hostfs_post_load(void *opaque, int version_id)
{
    hostfs *hfs = opaque;
    open_file *current_file;

    /* Open again files that were open when the migration was launched */
    QLIST_FOREACH(current_file, &hfs->open_files_qlist, next)
    {
        char *filename = current_file->filename;
        uint32_t flags = current_file->flags;
        uint64_t mode = current_file->mode;
        uint64_t ret = 0;
        off_t ret_offset;

        /* Files opened here should already exist, so the potential O_CREAT, O_EXCL
           and O_TRUNC flags have to be removed */
        flags &= ~(O_CREAT | O_EXCL | O_TRUNC);

        ret = open(filename, flags, mode);
        trace_hostfs_post_migration_open(filename, flags, mode, ret);

        if ((int)ret == -1)
        {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "hostfs unable to reopen file '%s' during migration.\n"
                          "open failed with error: %s\n",
                          filename,
                          strerror(errno));
            return -1;
        }

        current_file->host_fd = ret;

        /* Set the offset associated to the FD */
        ret_offset = lseek(current_file->host_fd, current_file->offset, SEEK_SET);

        if (ret_offset != current_file->offset)
        {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "hostfs unable to set correct offset for file '%s' during"
                          " migration.\nlseek failed with error: %s\n",
                          filename,
                          strerror(errno));
            return -1;
        }
    }

    return 0;
}

static const VMStateDescription vmstate_open_file = {
    .name = "open_file",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]){
        VMSTATE_INT32(guest_fd, open_file),
        VMSTATE_UINT64(host_fd, open_file),
        VMSTATE_UINT32(flags, open_file),
        VMSTATE_UINT64(mode, open_file),
        VMSTATE_BUFFER(filename, open_file),
        VMSTATE_INT64(offset, open_file),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_hostfs = {
    .name = "hostfs",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = hostfs_pre_save,
    .post_load = hostfs_post_load,
    .fields = (const VMStateField[]){
        VMSTATE_QLIST_V(open_files_qlist, hostfs, 1, vmstate_open_file, open_file, next),
        VMSTATE_INT32(next_guest_fd, hostfs),
        VMSTATE_END_OF_LIST()
    }
};

static void hostfs_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = hostfs_realize;
    device_class_set_legacy_reset(dc, hostfs_reset);
    dc->vmsd = &vmstate_hostfs;
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

    dev = qdev_new(TYPE_HOSTFS_DEVICE);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    sbdev = SYS_BUS_DEVICE(dev);
    sbdev->mmio[0].addr = base;
    memory_region_add_subregion(mr, base, sbdev->mmio[0].memory);

    hostfs *hfs = QEMU_HOSTFS_DEVICE(dev);
    QLIST_INIT(&hfs->open_files_qlist);

    /* Don't use low FD values to avoid conflicts */
    hfs->next_guest_fd = 32;
    return dev;
}
