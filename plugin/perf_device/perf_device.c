#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#include "bus_interface.h"

#define BASE_ADDR 0x80001000
#define REGS_SIZE 0x100
#define IRQ_LINE  3

static uint32_t io_read(void *opaque, target_addr_t addr, uint32_t size)
{
    return 42;
}

static void io_write(void          *opaque,
                     target_addr_t  addr,
                     uint32_t       size,
                     uint32_t       val)
{
    IRQ_pulse(opaque, IRQ_LINE);
}

static void event_callback(void *opaque, uint64_t expire_time)
{
    printf("%s\n", __func__);
    add_event(opaque, expire_time + 1 * SECOND, event_callback);
}


static void device_reset(void *opaque)
{
    uint32_t now = get_time(opaque);

    if (add_event(opaque, now + 1 * SECOND, event_callback) < 0) {
        fprintf(stderr, DEVICE_NAME ": Event registration failed!\n");
    }
}

static void device_init(void *opaque)
{
}

static void device_exit(void *opaque)
{
}

int device_setup(Device *dev, int argc, const char *argv[])
{
    printf("Setup '" DEVICE_NAME "'\n");
    if (set_device_info(dev,
                        dev /* opaque */,
                        0xfffffff /* Vendor Id */,
                        0xaaaaaaa /* Product Id */,
                        DEVICE_NAME,
                        "Performance measurement device") < 0) {
        fprintf(stderr, "Failed to create device!\n");
        return 1;
    }

    if (register_callbacks(dev, io_read, io_write, device_reset,
                           device_init, device_exit) < 0) {
        fprintf(stderr, DEVICE_NAME ": callbacks registration failed!\n");
        return 1;
    }

    if (register_io_memory(dev, BASE_ADDR, REGS_SIZE) < 0) {
        fprintf(stderr, DEVICE_NAME ": I/O memory registration failed!\n");
        return 1;
    }

    return 0;
}
