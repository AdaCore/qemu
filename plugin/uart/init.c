#include <stdio.h>

#include "bus_interface.h"
#include "uart.h"

int main(int argc, const char *argv[])
{
    Device *dev;

    if (argc < 2) {
        fprintf(stderr, "Port number needed, usage :%s port_number\n", argv[0]);
        return 1;
    }

    dev = allocate_device();
    if (dev == NULL) {
        fprintf(stderr, "UART: Failed to allocate the device!\n");
    }

    if (device_setup(dev, argc, argv) < 0) {
        fprintf(stderr, "UART: Failed to setup device!\n");
        return 1;
    }

    if (register_device(dev, atoi(argv[1])) < 0) {
        fprintf(stderr, "UART: Failed to register device!\n");
        return 1;
    }

    if (device_loop(dev) < 0) {
        fprintf(stderr, "UART: Device loop error!\n");
        return 1;
    }

    return 0;
}
