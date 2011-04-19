#ifndef _QEMU_BUS_INTERFACE_H_
#define _QEMU_BUS_INTERFACE_H_

#include <stdint.h>

#define GNATBUS_VERSION 1

/* Packet types */

typedef enum GnatBusPacketType {
    GnatBus_Event = 0,
    GnatBus_Request,
    GnatBus_Response,
    MAX_PACKET_TYPE,
} GnatBusPacketType;

typedef enum GnatBusEventType {
    GnatBusEvent_Exit = 0,
    GnatBusEvent_SetIRQ,
    GnatBusEvent_RegisterEvent,
    GnatBusEvent_TriggerEvent,
    MAX_EVENT_TYPE,
} GnatBusEventType;

typedef enum GnatBusRequestType {
    GnatBusRequest_Read = 0,
    GnatBusRequest_Write,
    GnatBusRequest_Register,
    GnatBusRequest_Init,
    GnatBusRequest_Reset,
    GnatBusRequest_GetTime,
} GnatBusRequestType;

typedef enum GnatBusResponseType {
    GnatBusResponse_Error = 0,
    GnatBusResponse_Data,
    GnatBusResponse_Time,
} GnatBusResponseType;

/* Base packet */
typedef struct GnatBusPacket {
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

typedef struct GnatBusPacket_Event {
    GnatBusPacket    parent;
    GnatBusEventType type;
} GnatBusPacket_Event;

typedef struct GnatBusPacket_SimpleEvent {
    GnatBusPacket_Event parent;
} GnatBusPacket_SimpleEvent;

/* Simple Events (no specific fields in the event) */

typedef GnatBusPacket_SimpleEvent GnatBusPacket_Exit;

#define GnatBusPacket_Exit_Init(packet)                         \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_Exit)


/* Set IRQ */

typedef struct GnatBusPacket_SetIRQ {
    GnatBusPacket_Event parent;
    uint8_t          line;
    uint8_t          level;
} GnatBusPacket_SetIRQ;

#define GnatBusPacket_SetIRQ_Init(packet)                       \
GnatBusPacket_Init((packet), GnatBus_Event, GnatBusEvent_SetIRQ)

/* Register Event */

typedef struct GnatBusPacket_RegisterEvent {
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

typedef struct GnatBusPacket_Request {
    GnatBusPacket      parent;
    GnatBusRequestType type;
    uint32_t           id;
} GnatBusPacket_Request;

typedef struct GnatBusPacket_Request GnatBusPacket_Response;

/* Simple requests (no specific fields in the request) */

typedef struct GnatBusPacket_SimpleRequest {
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

typedef struct GnatBusPacket_Read {
    GnatBusPacket_Request parent;
    uint64_t              address;
    uint32_t              length;
} GnatBusPacket_Read;

#define GnatBusPacket_Read_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Read)

/* Write request */

typedef struct GnatBusPacket_Write {
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

typedef struct GnatBusPacket_Register {
    GnatBusPacket_Request parent;
    uint32_t              bus_version;
    uint32_t              vendor_id;
    uint32_t              device_id;
    char                  name[NAME_LENGTH];
    char                  desc[DESC_LENGTH];

    uint32_t nr_iomem;
    struct QemuPlugin_IOMemory {
        uint64_t base;
        uint64_t size;
    } iomem[MAX_IOMEM];

} GnatBusPacket_Register;

#define GnatBusPacket_Register_Init(packet)                             \
GnatBusPacket_Init((packet), GnatBus_Request, GnatBusRequest_Register)

/* Responses */

/* Error_code response */
typedef struct GnatBusPacket_Error {
    GnatBusPacket_Request parent;
    uint32_t              error_code;
} GnatBusPacket_Error;

#define GnatBusPacket_Error_Init(packet)                                \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Error)

/* Data response */

typedef struct GnatBusPacket_Data {
    GnatBusPacket_Request parent;
    uint32_t              length;
    uint8_t               data[];
} GnatBusPacket_Data;

#define GnatBusPacket_Data_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Data)

/* Time response */

typedef struct GnatBusPacket_Time {
    GnatBusPacket_Request parent;
    uint64_t              time;
} GnatBusPacket_Time;

#define GnatBusPacket_Time_Init(packet)                                 \
GnatBusPacket_Init((packet), GnatBus_Response, GnatBusResponse_Time)

#endif /* ! _QEMU_BUS_INTERFACE_H_ */
