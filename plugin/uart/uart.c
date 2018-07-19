#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#include "bus_interface.h"
#define SERVER_PORT 8033

#define UART_BASE_ADDR 0x80001000
#define UART_REGS_SIZE 0x100
#define UART_IRQ_LINE  3

#define UART_CTRL_ADDR   0x0
#define UART_CTRL_INT_EN (1 << 0) /* Interrupt enabled */
#define UART_CTRL_DTR    (1 << 1) /* Data to read */
#define UART_DATA_ADDR   0x4

typedef struct UART {
    uint32_t ctrl;

    pthread_t thread;

    char            *buffer;
    int              len;
    int              current;
    pthread_mutex_t  mutex;
    Device          *dev;
} UART;

static void uart_lock(UART *uart)
{
    pthread_mutex_lock(&uart->mutex);
}

static void uart_unlock(UART *uart)
{
    pthread_mutex_unlock(&uart->mutex);
}

static int uart_data_to_read(UART *uart)
{
    return uart->buffer != NULL && uart->current < uart->len;
}

static char uart_pop(UART *uart)
{
    char ret;

    if (uart->buffer == NULL) {
        return 0;
    }

    uart_lock(uart);

    ret = uart->buffer[uart->current++];

    if (uart->current >= uart->len) {
        /* Flush */
        free(uart->buffer);
        uart->buffer  = NULL;
        uart->len     = 0;
        uart->current = 0;
    }

    uart_unlock(uart);

    return ret;
}

static void uart_add_to_buffer(UART *uart, const char *buffer, int length)
{
    uart_lock(uart);

    uart->buffer = realloc(uart->buffer, uart->len + length);

    if (uart->buffer == NULL) {
        abort();
    }

    memcpy(uart->buffer + uart->len, buffer, length);

    uart->len += length;

    uart_unlock(uart);
}

static void *uart_read_loop(void *opaque)
{
    UART *uart = (UART *)opaque;
    char buffer[256];
    int  read_length;
    int c_sock = -1;
    int serversock;
    struct sockaddr_in echoserver, echoclient;

    /* Create the TCP socket */
    serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serversock < 0) {
        perror("Failed to create socket");
    }
    /* Construct the server sockaddr_in structure */
    memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
    echoserver.sin_family = AF_INET;                  /* Internet/IP */
    echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   /* Incoming addr */
    echoserver.sin_port = htons(SERVER_PORT);         /* server port */

    /* Bind the server socket */
    if (bind(serversock, (struct sockaddr *) &echoserver,
             sizeof(echoserver)) < 0) {
        perror("Failed to bind the server socket");
    }
    /* Listen on the server socket */
    if (listen(serversock, 1) < 0) {
        perror("Failed to listen on server socket");
    }

    while (1) {
        /* Run until cancelled */
        unsigned int clientlen = sizeof(echoclient);
        /* Wait for client connection */
        c_sock = accept(serversock, (struct sockaddr *) &echoclient,
                        &clientlen);
        if (c_sock < 0) {
            perror("Failed to accept client connection");
        }

        fprintf(stdout, "Client connected: %s\n",
                inet_ntoa(echoclient.sin_addr));

        while (1) {
            read_length = read(c_sock, buffer, sizeof(buffer));

            if (read_length <= 0) {
                break;
            }

            uart_add_to_buffer(uart, buffer, read_length);

            if (uart->ctrl & UART_CTRL_INT_EN) {
                IRQ_pulse(uart->dev, UART_IRQ_LINE);
            }
        }
    }
    return NULL;
}

static uint32_t io_read(void *opaque, target_addr_t addr, uint32_t size)
{
    UART *uart = (UART *)opaque;

    addr -= UART_BASE_ADDR;

    switch (addr) {
    case UART_CTRL_ADDR:
        if (uart_data_to_read(uart)) {
            uart->ctrl |= UART_CTRL_DTR;
        } else {
            uart->ctrl &= ~UART_CTRL_DTR;
        }
        return uart->ctrl;
        break;

    case UART_DATA_ADDR:
        return uart_pop(uart);
        break;

    default:
        fprintf(stderr, "%s Unknown register 0x%08x\n", __func__, addr);
        return 0;
        break;
    }
}

static void io_write(void          *opaque,
                     target_addr_t  addr,
                     uint32_t       size,
                     uint32_t       val)
{
    UART *uart = (UART *)opaque;
    /* fprintf(stdout, "io_write addr:0x%x size:%d val:0x%lx\n", */
    /*         addr, size, (long unsigned int)val); */

    addr -= UART_BASE_ADDR;

    switch (addr) {
    case UART_CTRL_ADDR:
        val &= UART_CTRL_INT_EN; /* Keep the interrupt flag */
        uart->ctrl = (uart->ctrl & ~UART_CTRL_INT_EN) | val;
        break;

    case UART_DATA_ADDR:
        fprintf(stdout, "%c", (char)val);
        break;

    default:
        fprintf(stderr, "%s Unknown register 0x%08x\n", __func__, addr);
        break;
    }
}

static void device_reset(void *opaque)
{
    UART *uart = (UART *)opaque;

    fprintf(stdout, "reset\n");

    uart_lock(uart);

    uart->ctrl    = 0;
    uart->len     = 0;
    uart->current = 0;
    free(uart->buffer);
    uart->buffer = NULL;

    uart_unlock(uart);
}

static void device_init(void *opaque)
{
    fprintf(stdout, "init\n");
}

static void device_exit(void *opaque)
{
    fprintf(stdout, "exit\n");
}

int device_setup(Device *dev, int argc, const char *argv[])
{
    UART *uart = malloc(sizeof(*uart));

    uart->dev    = dev;
    uart->len    = 0;
    uart->buffer = NULL;
    pthread_mutex_init(&uart->mutex, NULL);

    printf("Start UART\n");
    if (set_device_info(dev,
                        uart,
                        0xfffffff /* Vendor Id */,
                        0xaaaaaaa /* Product Id   */,
                        "UART",
                        "Simple uart") < 0) {
        fprintf(stderr, "Failed to create device!\n");
        return 1;
    }

    if (register_callbacks(dev, io_read, io_write, device_reset,
                           device_init, device_exit) < 0) {
        fprintf(stderr, "UART: callbacks registration failed!\n");
        return 1;
    }

    if (register_io_memory(dev, UART_BASE_ADDR, UART_REGS_SIZE) < 0) {
        fprintf(stderr, "UART: I/O memory registration failed!\n");
        return 1;
    }

    pthread_create(&uart->thread, NULL, uart_read_loop, uart);

    return 0;
}
