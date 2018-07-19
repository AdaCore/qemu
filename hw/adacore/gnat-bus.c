#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/option.h"
#include "qemu/timer.h"
#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "sysemu/cpus.h"
#include "qemu/sockets.h"
#include "trace.h"
#include "hw/adacore/gnat-bus.h"

#ifdef _WIN32
#include "chardev/char-win.h"
#endif /* _WIN32 */

typedef struct Event_Entry {
    uint64_t        expire_time;
    uint32_t        event_id;
    uint64_t        event;
    GnatBus_Device *qbdev;

    QLIST_ENTRY(Event_Entry) list;
} Event_Entry;

GnatBus_Master *g_qbmaster = NULL;
/* Timeout before qemu crash when attempting to connect a GNATBus device. */
long timeout = 0;

/* Socket communication tools */

int gnatbus_send(GnatBus_Device *qbdev, const uint8_t *buf, int len)
{
    if (qbdev->status != CHR_EVENT_OPENED) {
        return -1;
    }

    return qemu_chr_fe_write_all(&qbdev->chr, buf, len);
}

#ifdef _WIN32
static int win_readfile(Chardev *chr,  uint8_t *buf, int len)
{
    WinChardev *s = WIN_CHARDEV(chr);
    int ret;
    DWORD size;

    ret = ReadFile(s->file, buf, len, &size, NULL);
    if (!ret) {
        if (GetLastError() == ERROR_MORE_DATA) {
            return size;
        }
        fprintf(stderr, "GNATbus Read Pipe failed (%lu)\n", GetLastError());
        return 0;
    }

    return size;
}
#endif

static int gnatbus_recv(GnatBus_Device *qbdev, uint8_t *buf, int len)
{
    int           read_size = 0;

    do {
#ifdef _WIN32
        if (qbdev->is_pipe) {
            read_size = win_readfile(qbdev->chr.chr, buf, len);
        } else
#endif
        {
            read_size = tcp_chr_recv(qbdev->chr.chr, (char *)buf, len);
        }
    } while (read_size == -1 && (errno == EINTR || errno == EAGAIN));

    return read_size;
}

static void gnatbus_set_blocking(GnatBus_Device *qbdev, int blocking)
{
#ifdef _WIN32
    if (qbdev->is_pipe) {
        /* It seems that PIPE_READMODE_MESSAGE requires that the whole
         * packet fits in the in/out buffer. So lets use
         * PIPE_READMODE_BYTE instead so we have no limits.
         */
        WinChardev *s_wpipe = WIN_CHARDEV(qbdev->chr.chr);
        DWORD dwMode = blocking ? PIPE_READMODE_BYTE | PIPE_WAIT
                                : PIPE_READMODE_BYTE | PIPE_NOWAIT;

        SetNamedPipeHandleState(s_wpipe->file, &dwMode, NULL, NULL);
    } else
#endif
    {
        SocketChardev *s_socket = SOCKET_CHARDEV(qbdev->chr.chr);

        assert(s_socket);
        qio_channel_set_blocking(s_socket->ioc, blocking, NULL);
        trace_gnatbus_set_blocking(blocking);
    }
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

    gnatbus_set_blocking(qbdev, true);

   /* Read first part of the packet to get its complete size */

    read_size = gnatbus_recv(qbdev, (uint8_t *)packet, init_packet_size);
    if (read_size != init_packet_size
        || packet->size < init_packet_size) {
        /* Not enough data */
        fprintf(stderr, "%s: Failed to read packet header (read_size:%d)\n",
                __func__, read_size);
        g_free(packet);
        packet = NULL;
        goto fail;
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


    if (remaining_data != 0) {
        fprintf(stderr, "%s: Not enough data PacketSize:%u received:%u\n",
                __func__, packet->size, packet->size - remaining_data);
        g_free(packet);
        packet = NULL;
    }

 fail:
    gnatbus_set_blocking(qbdev, false);
    return packet;
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

    /* We don't expect any response at this point.. If a shutdown has been
     * asked just proceed it.
     */
    if (qbdev->shutdown_requested) {
        gnatbus_shutdown_vm();
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
            memmove(qbdev->curr_packet,
                    (char *)qbdev->curr_packet + qbdev->curr_packet->size,
                    packet_size);

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

/* Transmit QEMU events to devices */

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

        resp = send_and_wait_resp(qbdev, (GnatBusPacket_Request *)&init);

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
        if (qemu_chr_fe_get_driver(&qbdev->chr)) {
            qemu_chr_fe_set_open(&qbdev->chr, 0);
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

        resp = send_and_wait_resp(qbdev, (GnatBusPacket_Request *)&reset);

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

    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

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
        timer_mod_ns(timer, (uint64_t)-1);
    } else {
        timer_mod_ns(timer, QLIST_FIRST(event_list)->expire_time);
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

    timer_mod_ns(timer, QLIST_FIRST(event_list)->expire_time);

    return 0;
}

/* I/O operations */

static inline void gnatbus_write(void     *opaque,
                                 hwaddr    addr,
                                 uint64_t  val,
                                 unsigned  size)
{
    GnatBus_IORegion    *io_region= opaque;
    GnatBusPacket_Write *write;
    GnatBusPacket_Error *resp;

    assert(size <= 8);

    if (io_region->qbdev->status != CHR_EVENT_OPENED) {
        return;
    }

    gnatbus_freeze_cpu();

    write = g_malloc(sizeof(GnatBusPacket_Write) + 8);

    GnatBusPacket_Write_Init(write);
    write->parent.parent.size = sizeof(GnatBusPacket_Write) + 8;

    write->address           = io_region->base + addr;
    write->length            = size;
    *(uint64_t *)write->data = val;


    trace_gnatbus_send_write(write->address, write->length);

    resp = (GnatBusPacket_Error *)send_and_wait_resp(io_region->qbdev,
                                                     (GnatBusPacket_Request *)write);

    g_free(write);
    /* We don't really need to read the response */
    g_free(resp);

    gnatbus_unfreeze_cpu();
}

static inline uint64_t gnatbus_read(void     *opaque,
                                    hwaddr    addr,
                                    unsigned  size)
{
    GnatBus_IORegion   *io_region = opaque;
    GnatBusPacket_Read  read;
    GnatBusPacket_Data *resp;
    uint64_t            ret     = 0;

    if (io_region->qbdev->status != CHR_EVENT_OPENED) {
        return 0;
    }

    assert(size <= 8);

    gnatbus_freeze_cpu();

    GnatBusPacket_Read_Init(&read);

    read.address = io_region->base + addr;
    read.length  = size;

    trace_gnatbus_send_read(read.address, read.length);

    resp = (GnatBusPacket_Data *)send_and_wait_resp(io_region->qbdev,
                                                    (GnatBusPacket_Request *)&read);

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

const MemoryRegionOps gnatbus_io_ops = {
    .read = gnatbus_read,
    .write = gnatbus_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

/* Initialization */

static int gnatbus_init(const char *optarg)
{
    static int       dev_cnt;
    GnatBus_Device  *qbdev   = NULL;
    char *buf;
    char *chardev_name;
    Chardev *chr;
    int              status  = 0;
    GnatBusPacket   *packet  = NULL;

    if (optarg[0] == '@') {
#ifdef _WIN32
        /* Windows named pipe */
        buf = g_strdup_printf("pipe_client:gnatbus\\%s", optarg + 1);
#else
        /* UNIX domain socket */
        buf = g_strdup_printf("unix:@/gnatbus/%s,timeout=%ld", optarg + 1,
                              timeout);
#endif /* _WIN32 */
    } else {
        buf = g_strdup_printf("tcp:%s", optarg);
    }

    chardev_name = g_strdup_printf("GnatBus_Chr_%d", dev_cnt++);

    /* Connect chardev */
    chr = qemu_chr_new(chardev_name, buf, NULL);

    /* Not needed anymore */
    g_free(buf);
    buf = NULL;
    g_free(chardev_name);
    chardev_name = NULL;

    if (chr == NULL) {
        fprintf(stderr, "%s: Chardev error\n", __func__);
        return -1;
    }

    qbdev         = g_malloc0(sizeof *qbdev);
    qbdev->master = g_qbmaster;
    qbdev->status = CHR_EVENT_OPENED;
    qbdev->shutdown_requested = false;

    if (optarg[0] == '@') {
        qbdev->is_pipe = 1;
    } else {
        qbdev->is_pipe = 0;
    }

    qemu_chr_fe_init(&qbdev->chr, chr, &error_fatal);

    qemu_chr_fe_set_handlers(&qbdev->chr,
                             gnatbus_chr_can_receive,
                             gnatbus_chr_receive,
                             gnatbus_chr_event,
                             NULL,
                             qbdev,
                             NULL,
                             true);

    packet = gnatbus_receive_packet_sync(qbdev);

    if (packet == NULL) {
        fprintf(stderr, "%s: Device initialization failure: "
                "Cannot read Register packet\n", __func__);
        return -1;
    }

    status = gnatbus_process_packet(qbdev, packet);
    g_free(packet);

    if (status != 0 || !qbdev->start_ok) {
        fprintf(stderr, "%s: Device initialization failure\n", __func__);
        return -1;
    }

    return 0;
}

void gnatbus_master_init(qemu_irq *cpu_irqs, int nr_irq)
{
    char **arg_list;
    char **tmp;

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
    g_qbmaster->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                     gnatbus_timer_tick,
                                     g_qbmaster);

    /* Initialize bottom halves */
    g_qbmaster->bh = qemu_bh_new(gnatbus_timer_tick_bh,
                                 g_qbmaster);

    arg_list = g_strsplit(g_qbmaster->optarg, ",", 0);
    tmp = arg_list;

    while (tmp && *tmp) {
        if (gnatbus_init(*tmp) < 0) {
            fprintf(stderr,
                    "error: GNATBus device '%s' initialisation failed\n",
                    *tmp);
            exit(1);
        }
        tmp++;
    }

    g_strfreev(arg_list);
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

void gnatbus_save_timeout_optargs(const char *optarg)
{
    timeout = strtol(optarg, NULL, 10);

    if (timeout <= 0) {
        fprintf(stderr, "gnatbus: wrong timeout parameter\n");
        exit(1);
    }
}

void gnatbus_shutdown_vm(void)
{
    pause_all_vcpus();
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
}
