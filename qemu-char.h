#ifndef QEMU_CHAR_H
#define QEMU_CHAR_H

#include "qemu-common.h"
#include "qemu-queue.h"
#include "qemu-option.h"
#include "qemu-config.h"
#include "qobject.h"
#include "qstring.h"
#include "main-loop.h"

/* character device */

#define CHR_EVENT_BREAK   0 /* serial break char */
#define CHR_EVENT_FOCUS   1 /* focus to this terminal (modal input needed) */
#define CHR_EVENT_OPENED  2 /* new connection established */
#define CHR_EVENT_MUX_IN  3 /* mux-focus was set to this terminal */
#define CHR_EVENT_MUX_OUT 4 /* mux-focus will move on */
#define CHR_EVENT_CLOSED  5 /* connection closed */


#define CHR_IOCTL_SERIAL_SET_PARAMS   1
typedef struct {
    int speed;
    int parity;
    int data_bits;
    int stop_bits;
} QEMUSerialSetParams;

#define CHR_IOCTL_SERIAL_SET_BREAK    2

#define CHR_IOCTL_PP_READ_DATA        3
#define CHR_IOCTL_PP_WRITE_DATA       4
#define CHR_IOCTL_PP_READ_CONTROL     5
#define CHR_IOCTL_PP_WRITE_CONTROL    6
#define CHR_IOCTL_PP_READ_STATUS      7
#define CHR_IOCTL_PP_EPP_READ_ADDR    8
#define CHR_IOCTL_PP_EPP_READ         9
#define CHR_IOCTL_PP_EPP_WRITE_ADDR  10
#define CHR_IOCTL_PP_EPP_WRITE       11
#define CHR_IOCTL_PP_DATA_DIR        12

#define CHR_IOCTL_SERIAL_SET_TIOCM   13
#define CHR_IOCTL_SERIAL_GET_TIOCM   14

#define CHR_TIOCM_CTS	0x020
#define CHR_TIOCM_CAR	0x040
#define CHR_TIOCM_DSR	0x100
#define CHR_TIOCM_RI	0x080
#define CHR_TIOCM_DTR	0x002
#define CHR_TIOCM_RTS	0x004

typedef void IOEventHandler(void *opaque, int event);

struct CharDriverState {
    void (*init)(struct CharDriverState *s);
    int (*chr_write)(struct CharDriverState *s, const uint8_t *buf, int len);
    void (*chr_update_read_handler)(struct CharDriverState *s);
    int (*chr_ioctl)(struct CharDriverState *s, int cmd, void *arg);
    int (*get_msgfd)(struct CharDriverState *s);
    int (*chr_add_client)(struct CharDriverState *chr, int fd);
    IOEventHandler *chr_event;
    IOCanReadHandler *chr_can_read;
    IOReadHandler *chr_read;
    void *handler_opaque;
    void (*chr_close)(struct CharDriverState *chr);
    void (*chr_accept_input)(struct CharDriverState *chr);
    void (*chr_set_echo)(struct CharDriverState *chr, bool echo);
    void (*chr_guest_open)(struct CharDriverState *chr);
    void (*chr_guest_close)(struct CharDriverState *chr);
    void *opaque;
    QEMUBH *bh;
    char *label;
    char *filename;
    int opened;
    int avail_connections;
    QTAILQ_ENTRY(CharDriverState) next;

    /* Middle-end buffer */

    uint8_t  *me_buf;
    int      me_buf_head;
    int      me_buf_tail;
    QEMUBH   *me_bh;
    /*
     * me_buf is pointer to the buffer, me_buf_head and me_buf_tail are indexes
     * in this buffer.
     *
     * The buffer is allocated from me_buf to me_buf + me_buf_tail. However,
     * valid data are between me_buf_head and me_buf_tail.
     *
     *   me_buf            me_buf_head                            me_buf_tail
     *     |--------------------|=================================|
     *
     * A call to qemu_chr_me_write will append data to the buffer (if no
     * shortcut possible). The buffer is expended via g_realloc.
     *
     *   me_buf            me_buf_head                            me_buf_tail
     *     |--------------------|=================================>>>>>>>|
     *
     * A call to qemu_chr_me_try_flush will send as much data as possible from
     * me_buf_head to the front-end (no de-allocation).
     *
     *   me_buf            me_buf_head                            me_buf_tail
     *     |--------------------->>>>>>>>|===============================|
     *
     * If the buffer can't be entirely send, a bottom-half function will re-try
     * at the next loop until the buffer is empty (i.e. me_buf_head ==
     * me_buf_tail).
     *
     */
};

/**
 * @qemu_chr_new_from_opts:
 *
 * Create a new character backend from a QemuOpts list.
 *
 * @opts see qemu-config.c for a list of valid options
 * @init not sure..
 *
 * Returns: a new character backend
 */
CharDriverState *qemu_chr_new_from_opts(QemuOpts *opts,
                                    void (*init)(struct CharDriverState *s));

/**
 * @qemu_chr_new:
 *
 * Create a new character backend from a URI.
 *
 * @label the name of the backend
 * @filename the URI
 * @init not sure..
 *
 * Returns: a new character backend
 */
CharDriverState *qemu_chr_new(const char *label, const char *filename,
                              void (*init)(struct CharDriverState *s));

/**
 * @qemu_chr_delete:
 *
 * Destroy a character backend.
 */
void qemu_chr_delete(CharDriverState *chr);

/**
 * @qemu_chr_fe_set_echo:
 *
 * Ask the backend to override its normal echo setting.  This only really
 * applies to the stdio backend and is used by the QMP server such that you
 * can see what you type if you try to type QMP commands.
 *
 * @echo true to enable echo, false to disable echo
 */
void qemu_chr_fe_set_echo(struct CharDriverState *chr, bool echo);

/**
 * @qemu_chr_fe_open:
 *
 * Open a character backend.  This function call is an indication that the
 * front end is ready to begin doing I/O.
 */
void qemu_chr_fe_open(struct CharDriverState *chr);

/**
 * @qemu_chr_fe_close:
 *
 * Close a character backend.  This function call indicates that the front end
 * no longer is able to process I/O.  To process I/O again, the front end will
 * call @qemu_chr_fe_open.
 */
void qemu_chr_fe_close(struct CharDriverState *chr);

/**
 * @qemu_chr_fe_printf:
 *
 * Write to a character backend using a printf style interface.
 *
 * @fmt see #printf
 */
void qemu_chr_fe_printf(CharDriverState *s, const char *fmt, ...)
    GCC_FMT_ATTR(2, 3);

/**
 * @qemu_chr_fe_write:
 *
 * Write data to a character backend from the front end.  This function will
 * send data from the front end to the back end.
 *
 * @buf the data
 * @len the number of bytes to send
 *
 * Returns: the number of bytes consumed
 */
int qemu_chr_fe_write(CharDriverState *s, const uint8_t *buf, int len);

/**
 * @qemu_chr_fe_ioctl:
 *
 * Issue a device specific ioctl to a backend.
 *
 * @cmd see CHR_IOCTL_*
 * @arg the data associated with @cmd
 *
 * Returns: if @cmd is not supported by the backend, -ENOTSUP, otherwise the
 *          return value depends on the semantics of @cmd
 */
int qemu_chr_fe_ioctl(CharDriverState *s, int cmd, void *arg);

/**
 * @qemu_chr_fe_get_msgfd:
 *
 * For backends capable of fd passing, return the latest file descriptor passed
 * by a client.
 *
 * Returns: -1 if fd passing isn't supported or there is no pending file
 *          descriptor.  If a file descriptor is returned, subsequent calls to
 *          this function will return -1 until a client sends a new file
 *          descriptor.
 */
int qemu_chr_fe_get_msgfd(CharDriverState *s);

/**
 * @qemu_chr_be_can_write:
 *
 * Determine how much data the front end can currently accept.  This function
 * returns the number of bytes the front end can accept.  If it returns 0, the
 * front end cannot receive data at the moment.  The function must be polled
 * to determine when data can be received.
 *
 * Returns: the number of bytes the front end can receive via @qemu_chr_be_write
 */
int qemu_chr_be_can_write(CharDriverState *s);

/**
 * @qemu_chr_be_write:
 *
 * Write data from the back end to the front end.  Before issuing this call,
 * the caller should call @qemu_chr_be_can_write to determine how much data
 * the front end can currently accept.
 *
 * @buf a buffer to receive data from the front end
 * @len the number of bytes to receive from the front end
 */
void qemu_chr_be_write(CharDriverState *s, uint8_t *buf, int len);

/**
 * @qemu_chr_be_event:
 *
 * Send an event from the back end to the front end.
 *
 * @event the event to send
 */
void qemu_chr_be_event(CharDriverState *s, int event);

/**
 * @qemu_chr_me_write:
 *
 * Write data to the middle-end. This function handles a fifo buffer between
 * back and front-end, so that the back-end doesn't have to check if the
 * front-end is ready to receive data.
 *
 * @buf a buffer to receive data from the front end
 * @len the number of bytes to receive from the front end
 */
void qemu_chr_me_write(CharDriverState *s, uint8_t *buf, int len);

/**
 * @qemu_chr_me_try_flush:
 *
 * Internal function.
 *
 * This function tries to send the middle-end buffer to the front-end.
 */
void qemu_chr_me_try_flush(CharDriverState *chr);

/**
 * @qemu_chr_me_bh_func:
 *
 * Internal function.
 *
 * This bottom-half function will call qemu_chr_me_try_flush until the buffer is
 * empty.
 *
 * @opaque a CharDriverState
 */
void qemu_chr_me_bh_func(void *opaque);

void qemu_chr_add_handlers(CharDriverState *s,
                           IOCanReadHandler *fd_can_read,
                           IOReadHandler *fd_read,
                           IOEventHandler *fd_event,
                           void *opaque);

void qemu_chr_generic_open(CharDriverState *s);
void qemu_chr_accept_input(CharDriverState *s);
int qemu_chr_add_client(CharDriverState *s, int fd);
void qemu_chr_info_print(Monitor *mon, const QObject *ret_data);
void qemu_chr_info(Monitor *mon, QObject **ret_data);
CharDriverState *qemu_chr_find(const char *name);

QemuOpts *qemu_chr_parse_compat(const char *label, const char *filename);

/* add an eventfd to the qemu devices that are polled */
CharDriverState *qemu_chr_open_eventfd(int eventfd);

extern int term_escape_char;

/* memory chardev */
void qemu_chr_init_mem(CharDriverState *chr);
void qemu_chr_close_mem(CharDriverState *chr);
QString *qemu_chr_mem_to_qs(CharDriverState *chr);
size_t qemu_chr_mem_osize(const CharDriverState *chr);

CharDriverState *qemu_char_get_next_serial(void);

/* async I/O support */

ssize_t tcp_chr_recv(CharDriverState *chr, char *buf, size_t len);
void tcp_chr_read(void *opaque);
int tcp_chr_read_poll(void *opaque);

typedef struct {
    int fd, listen_fd;
    int connected;
    int max_size;
    int do_telnetopt;
    int do_nodelay;
    int is_unix;
    int msgfd;
} TCPCharDriver;

#ifdef _WIN32
typedef struct {
    int max_size;
    HANDLE hcom, hrecv, hsend;
    OVERLAPPED orecv, osend;
    BOOL fpipe;
    DWORD len;
} WinCharState;
#endif

#endif
