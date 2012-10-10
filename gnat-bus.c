#include "qemu-common.h"
#include "qemu-option.h"
#include "qemu-timer.h"
#include "sysemu.h"
#include "hw/sysbus.h"
#include "cpus.h"
#include "qemu_socket.h"
#include "qemu-char.h"
#include "trace.h"
#include "gnat-bus.h"

typedef struct Event_Entry {
    uint64_t        expire_time;
    uint32_t        event_id;
    uint64_t        event;
    GnatBus_Device *qbdev;

    QLIST_ENTRY(Event_Entry) list;
} Event_Entry;

GnatBus_Master *g_qbmaster = NULL;

/* Socket communication tools */

static void dev_set_blocking(GnatBus_Device *qbdev)
{
    TCPCharDriver *s = qbdev->chr->opaque;

    /* Remove fd from the active list.  */
    qemu_set_fd_handler2(s->fd, NULL, NULL, NULL, NULL);

    socket_set_block(s->fd);

}

static void dev_set_nonblocking(GnatBus_Device *qbdev)
{
    TCPCharDriver *s = qbdev->chr->opaque;

    socket_set_nonblock(s->fd);

    /* Put fd in the active list.  */
    qemu_set_fd_handler2(s->fd, tcp_chr_read_poll,
                         tcp_chr_read, NULL, qbdev->chr);
}

int gnatbus_send(GnatBus_Device *qbdev, const uint8_t *buf, int len)
{
    if (qbdev->status != CHR_EVENT_OPENED) {
        return -1;
    }

    return qbdev->chr->chr_write(qbdev->chr, buf, len);
}

static int gnatbus_recv(GnatBus_Device *qbdev, uint8_t *buf, int len)
{
    int           read_size = 0;

    do {
        read_size = tcp_chr_recv(qbdev->chr, (char *)buf, len);
    } while (read_size == -1 && (errno == EINTR || errno == EAGAIN));

    return read_size;
}

static GnatBusPacket *gnatbus_receive_packet_sync(GnatBus_Device *qbdev)
{
    GnatBusPacket *packet = NULL;
    uint32_t       init_packet_size;
    uint32_t       remaining_data;
    int            read_size;

    if (qbdev->status != CHR_EVENT_OPENED) {
        return NULL;
    }

    init_packet_size = sizeof(GnatBusPacket);
    packet           = g_malloc(init_packet_size);

    trace_gnatbus_receive_packet_sync();

   /* Read first part of the packet to get its complete size */

    read_size = gnatbus_recv(qbdev, (uint8_t *)packet, init_packet_size);
    if (read_size != init_packet_size
        || packet->size < init_packet_size) {
        /* Not enough data */
        fprintf(stderr, "%s: Failed to read packet header (read_size:%d)\n",
                __func__, read_size);
        g_free(packet);
        return NULL;
    }

    /* Now we have the real size of this packet */
    packet = g_realloc(packet, packet->size);

    remaining_data = packet->size - init_packet_size;

    do {
        read_size = gnatbus_recv(qbdev,
                                 (uint8_t *)packet + packet->size - remaining_data,
                                 remaining_data);
        if (read_size <= 0) {
            break;
        }

        remaining_data -= read_size;

    } while (read_size > 0 && remaining_data > 0);

    if (remaining_data == 0) {
        return packet;
    } else {
        fprintf(stderr, "%s: Not enough data PacketSize:%u received:%u\n",
                __func__, packet->size, packet->size - remaining_data);
        g_free(packet);
        return NULL;
    }
}

static int freeze_nested = 0;

static void gnatbus_freeze_cpu(void)
{
    if (freeze_nested == 0) {
        cpu_disable_ticks();
    }
    freeze_nested++;
}

static void gnatbus_unfreeze_cpu(void)
{
    freeze_nested--;
    if (freeze_nested == 0) {
        cpu_enable_ticks();
    }
}

static uint32_t gen_request_id(void)
{
    static uint32_t id = 0;

    return id++;
}

GnatBusPacket_Response *send_and_wait_resp(GnatBus_Device        *qbdev,
                                           GnatBusPacket_Request *request)
{
    GnatBusPacket          *packet;
    GnatBusPacket_Response *resp;

    if (qbdev->status != CHR_EVENT_OPENED) {
        return NULL;
    }

    trace_gnatbus_send_and_wait_resp();

    request->id = gen_request_id();
    if (gnatbus_send(qbdev, (uint8_t *)request, request->parent.size) <= 0) {
        return NULL;
    }

    while (1) {
        packet = gnatbus_receive_packet_sync(qbdev);

        if (packet == NULL) {
            return NULL;
        }

        if (packet->type == GnatBus_Response) {
            /* We have the response */
            break;
        } else {
            /* This is nested communication triggered by our request */
            gnatbus_process_packet(qbdev, packet);
            free(packet);
        }
    }

    resp = (GnatBusPacket_Response *)packet;

    if (resp->id != request->id) {
        return NULL;
    }

    return resp;
}

static int gnatbus_receive_packet_async(GnatBus_Device *qbdev,
                                        const uint8_t  *buf,
                                        int             size)
{
    uint32_t  packet_size;
    char     *dst;

    if (qbdev->status != CHR_EVENT_OPENED) {
        return -1;
    }

    trace_gnatbus_receive_packet_async(size);

    packet_size = qbdev->curr_packet_size + size;

    if (packet_size < sizeof(GnatBusPacket)) {
        /* Not enough data */
        fprintf(stderr, "%s: Not enough data\n", __func__);
        return -1;
    }

    qbdev->curr_packet = g_realloc(qbdev->curr_packet, packet_size);

    /* Append data in the current packet */
    dst = (char *)qbdev->curr_packet + qbdev->curr_packet_size;
    memcpy(dst, buf, size);
    qbdev->curr_packet_size = packet_size;


    while (packet_size >= qbdev->curr_packet->size) {
        /* Packet complete */

        gnatbus_process_packet(qbdev, qbdev->curr_packet);

        if (packet_size > qbdev->curr_packet->size) {
            /* We have a chunk of the next packet  */
            packet_size -= qbdev->curr_packet->size;
            qbdev->curr_packet_size = packet_size;
            memmove(qbdev->curr_packet, (char *)qbdev->curr_packet + qbdev->curr_packet->size, packet_size);

            qbdev->curr_packet = g_realloc(qbdev->curr_packet, packet_size);

        } else {
            g_free(qbdev->curr_packet);
            qbdev->curr_packet = NULL;
            qbdev->curr_packet_size = 0;
            break;
        }
    }
    return size;
}

static int gnatbus_chr_can_receive(void *opaque)
{
    return 4096;
}

static void gnatbus_chr_receive(void *opaque, const uint8_t *buf, int size)
{
    gnatbus_receive_packet_async(opaque, buf, size);
}

static void gnatbus_chr_event(void *opaque, int event)
{
    GnatBus_Device         *qbdev = opaque;

    if (event == CHR_EVENT_CLOSED || event == CHR_EVENT_OPENED) {
        qbdev->status = event;
    }
}

/* Transmit Qemu events to devices */

void gnatbus_device_init(void)
{
    GnatBus_Device         *qbdev;
    GnatBusPacket_Init      init;
    GnatBusPacket_Response *resp;

    GnatBusPacket_Init_Init(&init);

    if (g_qbmaster == NULL) {
        /* No Devices */
        return;
    }

    trace_gnatbus_device_init();

    QLIST_FOREACH(qbdev, &g_qbmaster->devices_list, list) {
        dev_set_blocking(qbdev);

        resp = send_and_wait_resp(qbdev, (GnatBusPacket_Request *)&init);

        dev_set_nonblocking(qbdev);

        /* We don't really need to read the response */
        g_free(resp);
    }
}

static void gnatbus_device_exit(void)
{
    GnatBus_Device *qbdev;
    GnatBusPacket_Exit exit;

    GnatBusPacket_Exit_Init(&exit);

    if (g_qbmaster == NULL) {
        /* No Devices */
        return;
    }

    trace_gnatbus_device_exit();

    QLIST_FOREACH(qbdev, &g_qbmaster->devices_list, list) {
        gnatbus_send(qbdev, (uint8_t *)&exit, sizeof(exit));
        if (qbdev->chr) {
            qemu_chr_fe_close(qbdev->chr);
        }
    }
}

static void gnatbus_cpu_reset(void *null)
{
    GnatBus_Device         *qbdev;
    GnatBusPacket_Reset     reset;
    GnatBusPacket_Response *resp;

    GnatBusPacket_Reset_Init(&reset);

    if (g_qbmaster == NULL) {
        /* No Devices */
        return;
    }

    trace_gnatbus_device_reset();

    QLIST_FOREACH(qbdev, &g_qbmaster->devices_list, list) {
        dev_set_blocking(qbdev);

        resp = send_and_wait_resp(qbdev, (GnatBusPacket_Request *)&reset);

        dev_set_nonblocking(qbdev);

        /* We don't really need to read the response */
        g_free(resp);
    }
}

static void gnatbus_timer_tick(void *opaque)
{

    GnatBus_Master *bm = opaque;

    if (bm->bh) {
        qemu_bh_schedule(bm->bh);
    }
}

/* Event management */

static void gnatbus_timer_tick_bh(void *opaque)
{
    GnatBus_Master             *bm         = opaque;
    GnatBus_Events_List        *event_list = &bm->events_list;
    QEMUTimer                  *timer      = bm->timer;
    Event_Entry                *event      = NULL;
    uint64_t                    now;
    GnatBusPacket_TriggerEvent  trig;

    GnatBusPacket_TriggerEvent_Init(&trig);

    if (QLIST_FIRST(event_list) == NULL) {
        /* No events */
        return;
    }

    now = qemu_get_clock_ns(vm_clock);

    while (QLIST_FIRST(event_list) != NULL) {
        event = QLIST_FIRST(event_list);

        if (event->expire_time <= now) {
            QLIST_REMOVE(event, list);

            trig.event       = event->event;
            trig.event_id    = event->event_id;
            trig.expire_time = event->expire_time;

            trace_gnatbus_trigger_event(trig.event, trig.expire_time, now);
            gnatbus_send(event->qbdev, (uint8_t *)&trig, sizeof(trig));

            g_free(event);
        } else {
            break;
        }
    }

    if (QLIST_EMPTY(event_list)) {
        qemu_mod_timer(timer, (uint64_t)-1);
    } else {
        qemu_mod_timer(timer, QLIST_FIRST(event_list)->expire_time);
    }
}

int gnatbus_add_event(GnatBus_Device *qbdev,
                      uint64_t        expire_time,
                      uint32_t        event_id,
                      uint64_t        event)
{
    GnatBus_Master      *bm         = qbdev->master;
    GnatBus_Events_List *event_list = &bm->events_list;
    QEMUTimer           *timer      = bm->timer;

    Event_Entry *new  = NULL;
    Event_Entry *parc = NULL;

    new = g_malloc0(sizeof(*new));
    new->event       = event;
    new->event_id    = event_id;
    new->expire_time = expire_time;
    new->qbdev       = qbdev;

    if (QLIST_EMPTY(event_list)
        || QLIST_FIRST(event_list)->expire_time >= expire_time) {

        /* Head insertion */
        QLIST_INSERT_HEAD(event_list, new, list);
    } else {
        QLIST_FOREACH(parc, event_list, list) {
            if (parc->expire_time > expire_time) {
                QLIST_INSERT_BEFORE(parc, new, list);
                break;
            } else if (QLIST_NEXT(parc, list) == NULL) {
                /* Tail insertion */
                QLIST_INSERT_AFTER(parc, new, list);
                break;
            }
        }
    }

    qemu_mod_timer(timer, QLIST_FIRST(event_list)->expire_time);

    return 0;
}

/* I/O operations */

static inline void gnatbus_write_generic(void               *opaque,
                                         target_phys_addr_t  addr,
                                         uint64_t            val,
                                         unsigned            size)
{
    GnatBus_IOMEM_BaseAddr *io_base = opaque;
    GnatBusPacket_Write    *write;
    GnatBusPacket_Error    *resp;

    assert(size <= 4);

    if (io_base->qbdev->status != CHR_EVENT_OPENED) {
        return;
    }

    gnatbus_freeze_cpu();

    write = g_malloc(sizeof(GnatBusPacket_Write) + 4);

    GnatBusPacket_Write_Init(write);
    write->parent.parent.size = sizeof(GnatBusPacket_Write) + 4;

    write->address           = io_base->base + addr;
    write->length            = size;
    *(uint32_t *)write->data = val;


    trace_gnatbus_send_write(write->address, write->length);

    dev_set_blocking(io_base->qbdev);

    resp = (GnatBusPacket_Error *)send_and_wait_resp(io_base->qbdev,
                                                     (GnatBusPacket_Request *)write);

    dev_set_nonblocking(io_base->qbdev);

    /* We don't really need to read the response */
    g_free(resp);

    gnatbus_unfreeze_cpu();
}

static inline uint64_t gnatbus_read_generic(void               *opaque,
                                            target_phys_addr_t  addr,
                                            unsigned            size)
{
    GnatBus_IOMEM_BaseAddr *io_base = opaque;
    GnatBusPacket_Read      read;
    GnatBusPacket_Data     *resp;
    uint64_t                ret     = 0;

    if (io_base->qbdev->status != CHR_EVENT_OPENED) {
        return 0;
    }

    assert(size <= 4);

    gnatbus_freeze_cpu();

    GnatBusPacket_Read_Init(&read);

    read.address = io_base->base + addr;
    read.length  = size;

    trace_gnatbus_send_read(read.address, read.length);

    dev_set_blocking(io_base->qbdev);

    resp = (GnatBusPacket_Data *)send_and_wait_resp(io_base->qbdev,
                                                    (GnatBusPacket_Request *)&read);

    dev_set_nonblocking(io_base->qbdev);

    if (resp == NULL
        || (GnatBusResponseType)resp->parent.type != GnatBusResponse_Data
        || resp->length != size) {
        /* bad response */
    } else {
        memcpy(&ret, resp->data, size);
    }

    g_free(resp);

    gnatbus_unfreeze_cpu();

    return ret;
}

const MemoryRegionOps gnatbus_ops = {
    .read = gnatbus_read_generic,
    .write = gnatbus_write_generic,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* Initialization */

static int gnatbus_init(const char *optarg)
{
    GnatBus_Device  *qbdev  = NULL;
    char             buf[512];
    CharDriverState *chr;
    int              status = 0;
    GnatBusPacket   *packet = NULL;

    status = snprintf(buf, sizeof(buf), "tcp:%s", optarg);
    if (status < 0 || status >= sizeof(buf)) {
        printf("%s: Invalid device address (hostname too long)\n", __func__);
        return -1;
    }

    /* Connect chardev */
    chr = qemu_chr_new("GnatBus_Chr", buf, NULL);

    if (chr == NULL) {
        printf("%s: Chardev error\n", __func__);
        return -1;
    }

    qbdev         = g_malloc0(sizeof *qbdev);
    qbdev->master = g_qbmaster;
    qbdev->chr    = chr;
    qbdev->status = CHR_EVENT_OPENED;

    qemu_chr_add_handlers(qbdev->chr,
                          gnatbus_chr_can_receive,
                          gnatbus_chr_receive,
                          gnatbus_chr_event,
                          qbdev);

    dev_set_blocking(qbdev);

    packet = gnatbus_receive_packet_sync(qbdev);

    if (packet == NULL) {
        printf("%s: Device initialization failure: Cannot read Register packet\n", __func__);
        dev_set_nonblocking(qbdev);
        return -1;
    }

    status = gnatbus_process_packet(qbdev, packet);
    g_free(packet);

    dev_set_nonblocking(qbdev);

    if (status != 0 || ! qbdev->start_ok) {
        printf("%s: Device initialization failure\n", __func__);
        return -1;
    }

    return 0;
}

void gnatbus_master_init(qemu_irq *cpu_irqs, int nr_irq)
{
    char        name[256];
    const char *optarg;

    if (g_qbmaster == NULL) {
        return;
    }

    /* Initialize IRQ fields */
    g_qbmaster->nr_irq   = nr_irq;
    g_qbmaster->cpu_irqs = cpu_irqs;

    /* Register CPU reset callback */
    qemu_register_reset(gnatbus_cpu_reset, NULL);

    /* Register exit callback */
    atexit(gnatbus_device_exit);

    /* Initialize events lists */
    QLIST_INIT(&g_qbmaster->events_list);

    /* Initialize timers */
    g_qbmaster->timer = qemu_new_timer_ns(vm_clock,
                                          gnatbus_timer_tick,
                                          g_qbmaster);

    /* Initialize bottom halves */
    g_qbmaster->bh = qemu_bh_new(gnatbus_timer_tick_bh,
                                  g_qbmaster);

    optarg = g_qbmaster->optarg;

    while (*optarg) {

        optarg = get_opt_name(name, sizeof(name), optarg, ',');

        if (*optarg != '\0') {
            optarg++;
        }

        if (gnatbus_init(name) < 0 ) {
            fprintf(stderr,
                    "error: GNATBus device '%s' initialisation failed\n", name);
            abort();
        }
    }
}

void gnatbus_save_optargs(const char *optarg)
{
    /* Allocate the Bus Mater */
    if (g_qbmaster == NULL) {
        g_qbmaster = g_malloc0(sizeof *g_qbmaster);
    }

    g_qbmaster = g_malloc(sizeof(*g_qbmaster));
    g_qbmaster->optarg = g_strdup(optarg);
    QLIST_INIT(&g_qbmaster->devices_list);
}
