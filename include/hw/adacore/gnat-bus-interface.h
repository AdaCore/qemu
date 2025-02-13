/****************************************************************************
 *                                                                          *
 *                    G N A T _ B U S _ I N T E R F A C E                   *
 *                                                                          *
 *                                  S p e c                                 *
 *                                                                          *
 *                     Copyright (C) 2011-2018, AdaCore                     *
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

#ifndef _GNAT_BUS_INTERFACE_H_
#define _GNAT_BUS_INTERFACE_H_

#include <stdint.h>

#define GNATBUS_VERSION 6

/* Packet types */

#define PACKED __attribute__ ((packed))

typedef enum PACKED GnatBusPacketType {
    GnatBus_Event = 0,
    GnatBus_Request,
    GnatBus_Response,
    MAX_PACKET_TYPE,
} GnatBusPacketType;

typedef enum PACKED GnatBusEventType {
    GnatBusEvent_Exit = 0,
    GnatBusEvent_SetIRQ,
    GnatBusEvent_RegisterEvent,
    GnatBusEvent_TriggerEvent,
    GnatBusEvent_Shutdown,
    MAX_EVENT_TYPE,
} GnatBusEventType;

typedef enum PACKED GnatBusRequestType {
    GnatBusRequest_Read = 0,
    GnatBusRequest_Write,
    GnatBusRequest_Register,
    GnatBusRequest_Init,
    GnatBusRequest_Reset,
    GnatBusRequest_GetTime,
} GnatBusRequestType;

typedef enum PACKED GnatBusResponseType {
    GnatBusResponse_Error = 0,
    GnatBusResponse_Data,
    GnatBusResponse_Time,
    GnatBusResponse_Endianness,
} GnatBusResponseType;

typedef enum PACKED TargetEndianness {
    TargetEndianness_Unknown      = 0,
    TargetEndianness_BigEndian    = 1,
    TargetEndianness_LittleEndian = 2,
} TargetEndianness;

typedef enum PACKED DeviceEndianness {
    DeviceEndianness_NativeEndian = 0,
    DeviceEndianness_BigEndian    = 1,
    DeviceEndianness_LittleEndian = 2,
} DeviceEndianness;

/* Base packet */
typedef struct PACKED GnatBusPacket {
    uint32_t          size;
    GnatBusPacketType type;
} GnatBusPacket;

#define GnatBusPacket_Init(packet, _type, _subtype)      \
do {                                                     \
    (packet)->parent.type = (_subtype);                  \
    (packet)->parent.parent.type = (_type);              \
    (packet)->parent.parent.size = sizeof(*(packet));    \
 } while (0);                                            \

/* Event base packet */

typedef struct PACKED GnatBusPacket_Event {
    GnatBusPacket    parent;
    GnatBusEventType type;
} GnatBusPacket_Event;

typedef struct PACKED GnatBusPacket_SimpleEvent {
    GnatBusPacket_Event parent;
} GnatBusPacket_SimpleEvent;

/* Simple Events (no specific fields in the event) */

typedef GnatBusPacket_SimpleEvent GnatBusPacket_Exit;

#define GnatBusPacket_Exit_Init(packet)                         \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_Exit)

typedef GnatBusPacket_SimpleEvent GnatBusPacket_Shutdown;

#define GnatBusPacket_Shutdown_Init(packet)                             \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_Shutdown)

/* Set IRQ */

typedef struct PACKED GnatBusPacket_SetIRQ {
    GnatBusPacket_Event parent;
    uint8_t             line;
    uint8_t             level;
} GnatBusPacket_SetIRQ;

#define GnatBusPacket_SetIRQ_Init(packet)                       \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_SetIRQ)

/* Register Event */

typedef struct PACKED GnatBusPacket_RegisterEvent {
    GnatBusPacket_Event parent;
    uint64_t            expire_time;
    uint64_t            event;
    uint32_t            event_id;
} GnatBusPacket_RegisterEvent;

#define GnatBusPacket_RegisterEvent_Init(packet)                        \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_RegisterEvent)

/* Trigger Event */

typedef GnatBusPacket_RegisterEvent GnatBusPacket_TriggerEvent;

#define GnatBusPacket_TriggerEvent_Init(packet)                         \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_TriggerEvent)

/* Request/Response base packet */

typedef struct PACKED GnatBusPacket_Request {
    GnatBusPacket      parent;
    GnatBusRequestType type;
    uint32_t           id;
} GnatBusPacket_Request;

typedef struct GnatBusPacket_Request GnatBusPacket_Response;

/* Simple requests (no specific fields in the request) */

typedef struct PACKED GnatBusPacket_SimpleRequest {
    GnatBusPacket_Request parent;
} GnatBusPacket_SimpleRequest;

typedef GnatBusPacket_SimpleRequest GnatBusPacket_Init;
typedef GnatBusPacket_SimpleRequest GnatBusPacket_Reset;
typedef GnatBusPacket_SimpleRequest GnatBusPacket_GetTime;

#define GnatBusPacket_Init_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Init)

#define GnatBusPacket_Reset_Init(packet)                                \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Reset)

#define GnatBusPacket_GetTime_Init(packet)                              \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_GetTime)

/* Read request */

typedef struct PACKED GnatBusPacket_Read {
    GnatBusPacket_Request parent;
    uint64_t              address;
    uint32_t              length;
} GnatBusPacket_Read;

#define GnatBusPacket_Read_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Read)

/* Write request */

typedef struct PACKED GnatBusPacket_Write {
    GnatBusPacket_Request parent;
    uint64_t              address;
    uint32_t              length;
    uint8_t               data[];
} GnatBusPacket_Write;

#define GnatBusPacket_Write_Init(packet)                                \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Write)

/* Register device request */

#define NAME_LENGTH 64
#define DESC_LENGTH 256
#define MAX_IOMEM   32
#define GNATBUS_MAX_SHARED_MEM 1
#define GNATBUS_SHARED_MEM_NAME_MAX (255)

typedef struct PACKED GnatBusPacket_Register {
    GnatBusPacket_Request parent;
    uint32_t              bus_version;
    uint32_t              vendor_id;
    uint32_t              device_id;
    DeviceEndianness      endianness;
    char                  name[NAME_LENGTH];
    char                  desc[DESC_LENGTH];

    uint32_t nr_iomem;
    struct PACKED QemuPlugin_IOMemory {
        uint64_t base;
        uint64_t size;
    } iomem[MAX_IOMEM];

    uint32_t nr_shared_mem;
    struct PACKED SharedMemory {
      uint64_t base;
      uint64_t size;
      char name[GNATBUS_SHARED_MEM_NAME_MAX];
    } shared_mem[GNATBUS_MAX_SHARED_MEM];
} GnatBusPacket_Register;

#define GnatBusPacket_Register_Init(packet)                             \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Register)

/* Responses */

/* Error_code response */
typedef struct PACKED GnatBusPacket_Error {
    GnatBusPacket_Request parent;
    uint32_t              error_code;
} GnatBusPacket_Error;

#define GnatBusPacket_Error_Init(packet)                                \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Error)

/* Data response */

typedef struct PACKED GnatBusPacket_Data {
    GnatBusPacket_Response parent;
    uint32_t               length;
    uint8_t                data[];
} GnatBusPacket_Data;

#define GnatBusPacket_Data_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Data)

/* Time response */

typedef struct PACKED GnatBusPacket_Time {
    GnatBusPacket_Response parent;
    uint64_t               time;
} GnatBusPacket_Time;

#define GnatBusPacket_Time_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Time)

/* Endianness response */

typedef struct PACKED GnatBusPacket_Endianness {
    GnatBusPacket_Response parent;
    TargetEndianness       endianness;
} GnatBusPacket_Endianness;

#define GnatBusPacket_Endianness_Init(packet)                           \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Endianness)

#endif /* ! _GNAT_BUS_INTERFACE_H_ */
