#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "emulator.h"

FILE *output;

uint64_t io_read(target_addr_t addr, uint8_t size)
{
    static uint64_t cnt = 0;
    fprintf(output, "io_read addr:0x%x size:%d\n", addr, size);
    fflush(output);
    return cnt++;
}

void io_write(target_addr_t addr, uint8_t size, uint64_t val)
{
    fprintf(output, "io_write addr:0x%x size:%d val:0x%lx\n",
            addr, size, (long unsigned int)val);
    fflush(output);
}

void device_reset(void)
{
    fprintf(output, "reset\n");
    fflush(output);
}

void vm_event_callback(void)
{
    vm_time_t vm_expire = get_vm_time() + 5 * SECOND;

    fprintf(output, "%s\n", __func__);
    fflush(output);

    IRQ_pulse(3);

    if (add_vm_event(vm_expire, vm_event_callback) < 0) {
        fprintf(stderr, PLUGIN_NAME ": vm event registration failed!\n");
        return;
    }
}

#define BUF_SIZE 256
void host_event_callback(void)
{
    uint8_t     buf[BUF_SIZE];
    int         i;
    host_time_t host_expire = get_host_time() + 5 * SECOND;

    fprintf(output, "%s\n", __func__);

    dma_read(buf, 0x40000000, BUF_SIZE);
    fprintf(output, "==== DUMP memory ====\n");
    for (i = 0; i < BUF_SIZE; i++) {
        fprintf(output, "%02x ", buf[i]);
        if ((i + 1) % 16 == 0) {
            fprintf(output, "\n");
        }
    }
    fprintf(output, "\n");

    if (add_host_event(host_expire, host_event_callback) < 0) {
        fprintf(stderr, PLUGIN_NAME ": host event registration failed!\n");
        return;
    }
    fflush(output);
}

void device_init(void)
{
    vm_time_t   vm_expire   = get_vm_time() + 2 * SECOND;
    host_time_t host_expire = get_host_time() + 2 * SECOND;

    fprintf(output, "init\n");
    fflush(output);

    if (add_vm_event(vm_expire, vm_event_callback) < 0) {
        fprintf(stderr, PLUGIN_NAME ": vm event registration failed!\n");
        return;
    }
    if (add_host_event(host_expire, host_event_callback) < 0) {
        fprintf(stderr, PLUGIN_NAME ": host event registration failed!\n");
        return;
    }
}

void device_exit(void)
{
    fprintf(output, "exit\n");
    fflush(output);
}

uint32_t qemu_plugin_init(const char *args)
{
    QemuPlugin_DeviceInfo *dev = create_device(0xf /* Vendor Id */,
                                               0xf /* Product Id */,
                                               QEMU_DEVICE_NATIVE_ENDIAN,
                                               PLUGIN_NAME,
                                               "This is just a dummy plug-in");

    printf("Start " PLUGIN_NAME " with args:%s\n", args);
    if (dev == NULL) {
        fprintf(stderr, "Failed to create device!\n");
        return QP_ERROR;
    }

    if (register_callbacks(dev, io_read, io_write, device_reset,
                           device_init, device_exit) < 0) {
        fprintf(stderr, PLUGIN_NAME ": callbacks registration failed!\n");
        return QP_ERROR;
    }

    if (register_io_memory(dev, 0x80001000, 256) < 0) {
        fprintf(stderr, PLUGIN_NAME ": I/O memory registration failed!\n");
        return QP_ERROR;
    }
    if (register_io_memory(dev, 0x80001200, 256) < 0) {
        fprintf(stderr, PLUGIN_NAME ": I/O memory registration failed!\n");
        return QP_ERROR;
    }
    if (register_io_memory(dev, 0x80001100, 256) < 0) {
        fprintf(stderr, PLUGIN_NAME ": I/O memory registration failed!\n");
        return QP_ERROR;
    }

    if (attach_device(dev) < 0) {
        fprintf(stderr, PLUGIN_NAME ": Failed to attach device!\n");
        exit(1);
    }

    if (strlen(args) == 0) {
        output = stdout;
        return QP_NOERROR;
    }

    int c_sock = -1;
    int serversock;
    struct sockaddr_in echoserver, echoclient;

    /* Create the TCP socket */
    if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Failed to create socket");
    }
    /* Construct the server sockaddr_in structure */
    memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
    echoserver.sin_family = AF_INET;                  /* Internet/IP */
    echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   /* Incoming addr */
    echoserver.sin_port = htons(atoi(args));       /* server port */

    /* Bind the server socket */
    if (bind(serversock, (struct sockaddr *) &echoserver,
             sizeof(echoserver)) < 0) {
        perror("Failed to bind the server socket");
    }
    /* Listen on the server socket */
    if (listen(serversock, 1) < 0) {
        perror("Failed to listen on server socket");
    }

    /* Run until cancelled */
    unsigned int clientlen = sizeof(echoclient);
    /* Wait for client connection */
    if ((c_sock =
         accept(serversock, (struct sockaddr *) &echoclient,
                &clientlen)) < 0) {
        perror("Failed to accept client connection");
    }

    output = fdopen(c_sock, "r+");
    fprintf(stdout, "Client connected: %s\n",
                inet_ntoa(echoclient.sin_addr));

    if (output == NULL) {
        output = stdout;
        return QP_NOERROR;
    }
    return QP_NOERROR;
}
