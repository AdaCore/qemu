/****************************************************************************
 *                                                                          *
 *                         B U S _ I N T E R F A C E                        *
 *                                                                          *
 *                                  B o d y                                 *
 *                                                                          *
 *                        Copyright (C) 2011, AdaCore                       *
 *                                                                          *
 * This program is free software;  you can redistribute it and/or modify it *
 * under terms of  the GNU General Public License as  published by the Free *
 * Softwareg Foundation;  either version 3,  or (at your option)  any later *
 * version. This progran is distributed in the hope that it will be useful, *
 * but  WITHOUT  ANY  WARRANTY;   without  even  the  implied  warranty  of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                          *
 * As a special exception under Section 7 of GPL version 3, you are granted *
 * additional permissions  described in the GCC  Runtime Library Exception, *
 * version 3.1, as published by the Free Software Foundation.               *
 *                                                                          *
 * You should have received a copy  of the GNU General Public License and a *
 * copy of the  GCC Runtime Library Exception along  with this program; see *
 * the  files  COPYING3  and  COPYING.RUNTIME  respectively.  If  not,  see *
 * <http://www.gnu.org/licenses/>.                                          *
 *                                                                          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "gnat-bus-interface.h"

#include "bus_interface.h"
#include "packet_handlers.h"

#define DEVICE_CHECK_RETURN(return_value)                        \
if (dev == NULL) {                                               \
    fprintf(stderr, "%s error: "                                 \
            "Device not properly initialized\n", __func__);      \
    return return_value;                                         \
 }                                                               \


/* Devices */

Device *allocate_device(void)
{
    return calloc(sizeof(Device), 1);
}

void cleanup_device(Device *dev)
{
    close(dev->fd);
}

int set_device_info(Device     *dev,
                    void       *opaque,
                    uint32_t    vendor_id,
                    uint32_t    device_id,
                    const char *name,
                    const char *desc)
{
    DEVICE_CHECK_RETURN(-1);

    if (name == NULL) {
        fprintf(stderr, "%s: %s error: device name missing\n",
                dev->register_packet.name, __func__);
        return -1;
    }

    GnatBusPacket_Register_Init(&dev->register_packet);

    dev->opaque = opaque;
    dev->register_packet.bus_version = GNATBUS_VERSION;
    dev->register_packet.vendor_id = vendor_id;
    dev->register_packet.device_id = device_id;

    /* Copy device name */
    strncpy(dev->register_packet.name, name, NAME_LENGTH - 1);

    /* Set trailing null character */
    dev->register_packet.name[NAME_LENGTH - 1] = '\0';

    /* Copy device description */
    strncpy(dev->register_packet.desc, desc, DESC_LENGTH - 1);

    /* Set trailing null character */
    dev->register_packet.desc[NAME_LENGTH - 1] = '\0';

    return 0;
}

int register_callbacks(Device      *dev,
                       io_read_fn  *io_read,
                       io_write_fn *io_write,
                       reset_fn    *device_reset,
                       init_fn     *device_init,
                       exit_fn     *device_exit)
{
    DEVICE_CHECK_RETURN(-1);

    dev->io_read      = io_read;
    dev->io_write     = io_write;
    dev->device_reset = device_reset;
    dev->device_init  = device_init;
    dev->device_exit  = device_exit;

    return 0;
}

int register_io_memory(Device *dev, target_addr_t base, target_addr_t size)
{
    DEVICE_CHECK_RETURN(-1);

    if (dev->register_packet.nr_iomem >= MAX_IOMEM) {
        fprintf(stderr, "%s: %s error: Maximum I/O memory number reached\n",
                dev->register_packet.name, __func__);
        return -1;
    }

    dev->register_packet.iomem[dev->register_packet.nr_iomem].base = base;
    dev->register_packet.iomem[dev->register_packet.nr_iomem].size = size;
    dev->register_packet.nr_iomem++;

    return 0;
}

#ifdef _WIN32
static void socket_cleanup(void)
{
    WSACleanup();
}
#endif

int socket_init(void)
{
#ifdef _WIN32
    WSADATA Data;
    int ret, err;

    ret = WSAStartup(MAKEWORD(2,2), &Data);
    if (ret != 0) {
        err = WSAGetLastError();
        fprintf(stderr, "WSAStartup: %d\n", err);
        return -1;
    }
    atexit(socket_cleanup);
#endif
    return 0;
}

void psocket_error(const char *msg)
{
#ifdef _WIN32
    int err;
    err = WSAGetLastError();
    fprintf(stderr, "%s: winsock error %d\n", msg, err);
#else
    perror(msg);
#endif
}

int register_device(Device *dev, int port)
{
    GnatBusPacket_Error *resp = NULL;
    int error;

    if (dev == NULL) {
        fprintf(stderr, "%s: %s error: invalid device\n",
                dev->register_packet.name, __func__);
        return -1;
    }

    if (socket_init() < 0) {
        return -1;
    }

    int c_sock = -1;
    int serversock;
    struct sockaddr_in echoserver, echoclient;

    /* Create the TCP socket */
    serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (serversock == INVALID_SOCKET) {
#else
    if (serversock < 0) {
#endif
        psocket_error("Failed to create socket");
        return -1;
    }

    /* Construct the server sockaddr_in structure */
    memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
    echoserver.sin_family = AF_INET;                  /* Internet/IP */
    echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   /* Incoming addr */
    echoserver.sin_port = htons(port);                /* server port */

    /* Bind the server socket */
    if (bind(serversock, (struct sockaddr *) &echoserver,
             sizeof(echoserver)) < 0) {
        psocket_error("Failed to bind the server socket");
        close(serversock);
        return -1;
    }
    /* Listen on the server socket */
    if (listen(serversock, 1) < 0) {
        psocket_error("Failed to listen on server socket");
        close(serversock);
        return -1;
    }

    /* Run until cancelled */
    unsigned int clientlen = sizeof(echoclient);

    /* Wait for synchronous channel connection */
    c_sock = accept(serversock, (struct sockaddr *) &echoclient, &clientlen);
    close(serversock);
    if (c_sock < 0) {
        psocket_error("Failed to accept connection");
        return -1;
    }

    fprintf(stderr, "Bus Master connected: %s\n",
            inet_ntoa(echoclient.sin_addr));

    dev->fd = c_sock;

    /* Send Register packet */

    resp = (GnatBusPacket_Error *)
        send_and_wait_resp(dev, (GnatBusPacket_Request *)&dev->register_packet);

    if (resp == NULL
        || resp->parent.type != GnatBusResponse_Error
        || resp->error_code != 0) {
        error = -1;
        fprintf(stderr, "Error status in Bus Master response\n");
        close (dev->fd);
    } else {
        fprintf(stderr, "Receive response from Bus Master: Device init ok\n");
        error = 0;
    }

    free(resp);

    return error;
}

int device_loop(Device *dev)
{
    GnatBusPacket *packet = NULL;

    DEVICE_CHECK_RETURN(-1);

    while (1) {
        packet = receive_packet(dev);
        if (packet == NULL) {
            break;
        }
        process_packet(dev, packet);
        free(packet);
    }
    return 0;
}

/* IRQ */

static int set_IRQ(Device *dev, uint8_t line, uint8_t level)
{
    GnatBusPacket_SetIRQ setIRQ;

    DEVICE_CHECK_RETURN(-1);

    GnatBusPacket_SetIRQ_Init(&setIRQ);
    setIRQ.line  = line;
    setIRQ.level = level;

    send_packet(dev, (GnatBusPacket *)&setIRQ);

    return 0;
}

int IRQ_raise(Device *dev, uint8_t line)
{
    DEVICE_CHECK_RETURN(-1);

    return set_IRQ(dev, line, 1);
}

int IRQ_lower(Device *dev, uint8_t line)
{
    DEVICE_CHECK_RETURN(-1);

    return set_IRQ(dev, line, 0);
}

int IRQ_pulse(Device *dev, uint8_t line)
{
    DEVICE_CHECK_RETURN(-1);

    return set_IRQ(dev, line, 2);

}

/* DMA */

int dma_read(Device *dev, void *dest, target_addr_t addr, int size)
{
    DEVICE_CHECK_RETURN(-1);

    GnatBusPacket_Read    read;
    GnatBusPacket_Data   *resp;
    uint32_t              ret = 0;

    GnatBusPacket_Read_Init(&read);

    read.address = addr;
    read.length  = size;

    printf("%s: send Read request addr:0x%08x, size:%d\n",
           __func__, addr, size);

    resp = (GnatBusPacket_Data *)
        send_and_wait_resp(dev, (GnatBusPacket_Request *)&read);

    if (resp == NULL
        || resp->parent.type != GnatBusResponse_Data
        || resp->length != size) {
        /* bad response */
        ret = -1;
    } else {
        memcpy(dest, resp->data, size);
    }

    free(resp);
    return ret;
}

int dma_write(Device *dev, void *src, target_addr_t addr, int size)
{
    GnatBusPacket_Write  *write;
    GnatBusPacket_Error   *resp;
    uint32_t              ret = 0;

    DEVICE_CHECK_RETURN(-1);

    write = malloc(sizeof(GnatBusPacket_Write) + size);

    if (write == NULL) {
        abort();
    }

    GnatBusPacket_Write_Init(write);
    write->parent.parent.size += size;
    write->address = addr;
    write->length  = size;
    memcpy(write->data, src, size);

    printf("%s: send Write request addr:0x%08x, size:%d\n",
           __func__, addr, size);

    resp = (GnatBusPacket_Error *)
        send_and_wait_resp(dev, (GnatBusPacket_Request *)write);

    free(write);

    if (resp == NULL
        || resp->parent.type != GnatBusResponse_Error
        || resp->error_code != 0) {
        /* bad response */
        ret = -1;
    } else {
        ret = 0;
    }

    free(resp);
    return ret;
}

/* Time */

uint64_t get_time(Device *dev)
{
    GnatBusPacket_GetTime  gettime;
    GnatBusPacket_Time    *resp;
    uint64_t               ret = 0;

    DEVICE_CHECK_RETURN(-1);

    GnatBusPacket_GetTime_Init(&gettime);

    resp = (GnatBusPacket_Time *)
        send_and_wait_resp(dev, (GnatBusPacket_Request *)&gettime);

    if (resp == NULL
        || resp->parent.type != GnatBusResponse_Time) {
        /* bad response */
        ret = (uint64_t)-1;
    } else {
        ret = resp->time;
    }
    free(resp);

    return ret;
}

int add_event(Device        *dev,
              uint64_t       expire,
              uint32_t       event_id,
              EventCallback  event)
{
    GnatBusPacket_RegisterEvent reg;

    GnatBusPacket_RegisterEvent_Init(&reg);

    reg.expire_time = expire;
    reg.event_id    = event_id;
    reg.event       = 0;
    reg.event       = (uint64_t)event;

    send_packet(dev, (GnatBusPacket *)&reg);
    return 0;
}
