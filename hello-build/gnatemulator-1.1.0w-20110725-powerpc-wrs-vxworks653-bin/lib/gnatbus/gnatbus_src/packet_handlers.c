/****************************************************************************
 *                                                                          *
 *                       P A C K E T _ H A N D L E R S                      *
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

#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "packet_handlers.h"

static void hex_dump(FILE *f, const uint8_t *buf, int size)
{
    int len, i, j, c;

    for (i = 0; i < size; i += 16) {
        len = size - i;
        if (len > 16) {
            len = 16;
        }
        fprintf(f, "%08x ", i);
        for (j = 0; j < 16; j++) {
            if (j < len)
                fprintf(f, " %02x", buf[i + j]);
            else
                fprintf(f, "   ");
        }
        fprintf(f, " ");
        for (j = 0; j < len; j++) {
            c = buf[i + j];
            if (c < ' ' || c > '~')
                c = '.';
            fprintf(f, "%c", c);
        }
        fprintf(f, "\n");
    }
}

static uint32_t gen_request_id(void)
{
    static uint32_t id = 0;

    return id++;
}

GnatBusPacket *receive_packet(Device *dev)
{
    GnatBusPacket *packet = NULL;
    uint32_t       init_packet_size;
    uint32_t       remaining_data;
    int            read_size;

    assert(dev != NULL);

    init_packet_size = sizeof(GnatBusPacket);
    packet = malloc(init_packet_size);

    /* Read first part of the packet to get its complete size */

    read_size = recv(dev->fd, (char *)packet, init_packet_size, 0);

    if (read_size != init_packet_size
        || packet->size < init_packet_size) {
        /* Not enough data */
        if (read_size < 0) {
            /* If read_size is less than zero we have an error */
#ifdef _WIN32
            fprintf(stderr, "%s: Receive failed: %d\n",
                    __func__, WSAGetLastError());
#else
            perror(__func__);
#endif
            fprintf(stderr, "%s: Failed to read packet header\n", __func__);
        }
        free(packet);
        return NULL;
    }

    /* Now we have the real size of this packet */
    packet = realloc(packet, packet->size);

    remaining_data = packet->size - init_packet_size;

    do {
        read_size = recv(dev->fd,
                         (char *)packet + packet->size - remaining_data,
                         remaining_data, 0);

        if (read_size > 0) {
            remaining_data -= read_size;
        }

    } while (read_size > 0 && remaining_data > 0);

    if (remaining_data == 0) {
        return packet;
    } else {
        if (read_size < 0) {
#ifdef _WIN32
            fprintf(stderr, "%s: Receive error: %d\n",
                    __func__, WSAGetLastError());
#else
            perror(__func__);
#endif
        }
        fprintf(stderr, "%s: Not enough data PacketSize:%u received:%u\n",
                __func__, packet->size, packet->size - remaining_data);
        hex_dump(stderr, (uint8_t *)packet, packet->size - remaining_data);
        free(packet);
        return NULL;
    }
}


GnatBusPacket_Response *send_and_wait_resp(Device                *dev,
                                           GnatBusPacket_Request *request)
{
    GnatBusPacket          *packet;
    GnatBusPacket_Response *resp;

    assert(dev != NULL);
    assert(request != NULL);

    request->id = gen_request_id();
    if (send_packet(dev, (GnatBusPacket *)request) < 0) {
        return NULL;
    }

    while (1) {
        packet = receive_packet(dev);

        if (packet == NULL) {
            return NULL;
        }

        if (packet->type == GnatBus_Response) {
            /* We have the response */
            break;
        } else {
            /* This is nested communication triggered by our request */
            process_packet(dev, packet);
            free(packet);
        }
    }

    resp = (GnatBusPacket_Response *)packet;

    if (resp->id != request->id) {
        /* Bad response id */
        free(packet);
        return NULL;
    }

    return resp;
}

int send_packet(Device *dev, GnatBusPacket *packet)
{
    assert(dev != NULL);
    assert(packet != NULL);

    if (send(dev->fd, (char *)packet, packet->size, 0) != packet->size) {
        return -1;
    } else {
        return 0;
    }
}

static void resp_error(Device             *dev,
                       uint32_t        request_id,
                       uint32_t        error_code)
{
    GnatBusPacket_Error err;

    assert(dev != NULL);

    GnatBusPacket_Error_Init(&err);

    err.parent.id  = request_id;
    err.error_code = error_code;

    send_packet(dev, (GnatBusPacket *)&err);
}

static inline int process_read(Device             *dev,
                               GnatBusPacket_Read *read)
{
    GnatBusPacket_Data *resp;
    uint32_t            ret;

    assert(dev != NULL);
    assert(read != NULL);

    if (dev->io_read == NULL) {
        resp_error(dev, read->parent.id, 1);
        return -1;
    }

    ret = dev->io_read(dev->opaque, read->address, read->length);
    /* Allocate the packet + 4 bytes of data */
    resp = malloc(sizeof(GnatBusPacket_Data) + 4);

    if (resp == NULL) {
        resp_error(dev, read->parent.id, 1);
        return -1;
    }

    GnatBusPacket_Data_Init(resp);
    resp->parent.parent.size = sizeof(GnatBusPacket_Data) + 4;
    resp->parent.id = read->parent.id;
    resp->length = read->length;

    *((uint32_t *)resp->data) = ret;
    send_packet(dev, (GnatBusPacket *)resp);
    free(resp);
    return 0;
}

static inline int process_write(Device             *dev,
                                GnatBusPacket_Write *write)
{
    assert(dev != NULL);
    assert(write != NULL);

    if (write->length > 4) {
        resp_error(dev, write->parent.id, 1);
        return -1;
    }

    if (dev->io_write != NULL) {
        dev->io_write(dev->opaque,
                      write->address,
                      write->length,
                      *(uint32_t *)write->data);
        resp_error(dev, write->parent.id, 0);
    } else {
        resp_error(dev, write->parent.id, 1);
    }

    return 0;
}

static inline int process_triggerevent(Device                     *dev,
                                       GnatBusPacket_TriggerEvent *event)
{
    EventCallback cb;

    assert(dev != NULL);
    assert(event != NULL);

    cb = (EventCallback)event->event;

    if (cb != NULL) {
        cb(dev->opaque, event->event_id, event->expire_time);
        return 0;
    } else {
        return -1;
    }
}

static inline int process_request(Device                *dev,
                                  GnatBusPacket_Request *request)
{
    assert(dev != NULL);
    assert(request != NULL);

    switch (request->type) {
    case GnatBusRequest_Read:
        return process_read(dev, (GnatBusPacket_Read *)request);
        break;

    case GnatBusRequest_Write:
        return process_write(dev, (GnatBusPacket_Write *)request);
        break;

    case GnatBusRequest_Init:
        if (dev->device_init != NULL) {
            dev->device_init(dev->opaque);
            resp_error(dev, request->id, 0);
        } else {
            resp_error(dev, request->id, 1);
        }
        return 0;
        break;

    case GnatBusRequest_Reset:
        if (dev->device_reset != NULL) {
            dev->device_reset(dev->opaque);
            resp_error(dev, request->id, 0);
        } else {
            resp_error(dev, request->id, 1);
        }
        return 0;
        break;

    default:
        fprintf(stderr, "%s: Unknown request type (%d)\n",
                __func__, request->type);
        return -1;
        break;
    }
}

static inline int process_event(Device              *dev,
                                GnatBusPacket_Event *event)
{
    assert(dev != NULL);
    assert(event != NULL);

    switch (event->type) {
    case GnatBusEvent_Exit:
        if (dev->device_exit != NULL) {
            dev->device_exit(dev->opaque);
        }
        break;

    case GnatBusEvent_TriggerEvent:
        return process_triggerevent(dev, (GnatBusPacket_TriggerEvent *)event);
        break;

    default:
        fprintf(stderr, "%s: Unknown Event type (%d)\n",
                __func__, event->type);
        return -1;
        break;
    }

    return 0;
}

void process_packet(Device        *dev,
                    GnatBusPacket *packet)
{
    assert(dev != NULL);
    assert(packet != NULL);

    switch (packet->type) {
    case GnatBus_Event:
        process_event(dev, (GnatBusPacket_Event *)packet);
        break;
    case GnatBus_Request:
        process_request(dev, (GnatBusPacket_Request *)packet);
        break;
    default:
        fprintf(stderr, "%s: Unknown packet type (%d)\n",
                __func__, packet->type);
        break;
    }
}
