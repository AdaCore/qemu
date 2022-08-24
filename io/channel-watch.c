/*
 * QEMU I/O channels watch helper APIs
 *
 * Copyright (c) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "io/channel-watch.h"

typedef struct QIOChannelFDSource QIOChannelFDSource;
struct QIOChannelFDSource {
    GSource parent;
    GPollFD fd;
    QIOChannel *ioc;
    GIOCondition condition;
};


#ifdef CONFIG_WIN32
typedef struct QIOChannelSocketSource QIOChannelSocketSource;
struct QIOChannelSocketSource {
    GSource parent;
    GPollFD fd;
    QIOChannel *ioc;
    SOCKET socket;
    int revents;
    GIOCondition condition;
};

#endif


typedef struct QIOChannelFDPairSource QIOChannelFDPairSource;
struct QIOChannelFDPairSource {
    GSource parent;
    GPollFD fdread;
    GPollFD fdwrite;
    QIOChannel *ioc;
    GIOCondition condition;
};


static gboolean
qio_channel_fd_source_prepare(GSource *source G_GNUC_UNUSED,
                              gint *timeout)
{
    *timeout = -1;

    return FALSE;
}


static gboolean
qio_channel_fd_source_check(GSource *source)
{
    QIOChannelFDSource *ssource = (QIOChannelFDSource *)source;

    return ssource->fd.revents & ssource->condition;
}


static gboolean
qio_channel_fd_source_dispatch(GSource *source,
                               GSourceFunc callback,
                               gpointer user_data)
{
    QIOChannelFunc func = (QIOChannelFunc)callback;
    QIOChannelFDSource *ssource = (QIOChannelFDSource *)source;

    return (*func)(ssource->ioc,
                   ssource->fd.revents & ssource->condition,
                   user_data);
}


static void
qio_channel_fd_source_finalize(GSource *source)
{
    QIOChannelFDSource *ssource = (QIOChannelFDSource *)source;

    object_unref(OBJECT(ssource->ioc));
}


#ifdef CONFIG_WIN32
static gboolean
qio_channel_socket_source_prepare(GSource *source G_GNUC_UNUSED,
                                  gint *timeout)
{
    *timeout = -1;

    return FALSE;
}


/*
 * NB, this impl only works when the socket is in non-blocking
 * mode on Win32
 */
static gboolean
qio_channel_socket_source_check(GSource *source)
{
    QIOChannelSocketSource *ssource = (QIOChannelSocketSource *)source;
    WSANETWORKEVENTS ev;

    if (!ssource->condition) {
        return 0;
    }

    /* For now, we don't support G_IO_HUP checks.
       We want to avoid calling WSAEnumNetworkEvents for any GSource
       having just G_IO_HUP. It might hide events aimed to be retrieved by
       other GSources waiting inputs or outputs (ie with G_IO_IN or G_IO_OUT).
       The reason is that the Windows API is based on HANDLE but we often
       create several GSources for the same HANDLE. Thus, input events might
       be picked and cleared by the G_IO_HUP GSource.  */
    if (ssource->condition == G_IO_HUP) {
        return 0;
    }

    if (WSAEnumNetworkEvents(ssource->socket, ssource->ioc->event, &ev)) {
        return 0;
    }

    ssource->revents = 0;

    if (ev.lNetworkEvents & (FD_READ | FD_ACCEPT | FD_OOB)) {
        ssource->revents |= G_IO_IN;
    }

    if (ev.lNetworkEvents & (FD_WRITE | FD_CONNECT)) {
        ssource->revents |= G_IO_OUT;
    }

    return ssource->revents & ssource->condition;
}


static gboolean
qio_channel_socket_source_dispatch(GSource *source,
                                   GSourceFunc callback,
                                   gpointer user_data)
{
    QIOChannelFunc func = (QIOChannelFunc)callback;
    QIOChannelSocketSource *ssource = (QIOChannelSocketSource *)source;

    return (*func)(ssource->ioc, ssource->revents, user_data);
}


static void
qio_channel_socket_source_finalize(GSource *source)
{
    QIOChannelSocketSource *ssource = (QIOChannelSocketSource *)source;


    object_unref(OBJECT(ssource->ioc));
}


GSourceFuncs qio_channel_socket_source_funcs = {
    qio_channel_socket_source_prepare,
    qio_channel_socket_source_check,
    qio_channel_socket_source_dispatch,
    qio_channel_socket_source_finalize
};
#endif


static gboolean
qio_channel_fd_pair_source_prepare(GSource *source G_GNUC_UNUSED,
                                   gint *timeout)
{
    *timeout = -1;

    return FALSE;
}


static gboolean
qio_channel_fd_pair_source_check(GSource *source)
{
    QIOChannelFDPairSource *ssource = (QIOChannelFDPairSource *)source;
    GIOCondition poll_condition = ssource->fdread.revents |
        ssource->fdwrite.revents;

    return poll_condition & ssource->condition;
}


static gboolean
qio_channel_fd_pair_source_dispatch(GSource *source,
                                    GSourceFunc callback,
                                    gpointer user_data)
{
    QIOChannelFunc func = (QIOChannelFunc)callback;
    QIOChannelFDPairSource *ssource = (QIOChannelFDPairSource *)source;
    GIOCondition poll_condition = ssource->fdread.revents |
        ssource->fdwrite.revents;

    return (*func)(ssource->ioc,
                   poll_condition & ssource->condition,
                   user_data);
}


static void
qio_channel_fd_pair_source_finalize(GSource *source)
{
    QIOChannelFDPairSource *ssource = (QIOChannelFDPairSource *)source;

    object_unref(OBJECT(ssource->ioc));
}


GSourceFuncs qio_channel_fd_source_funcs = {
    qio_channel_fd_source_prepare,
    qio_channel_fd_source_check,
    qio_channel_fd_source_dispatch,
    qio_channel_fd_source_finalize
};


GSourceFuncs qio_channel_fd_pair_source_funcs = {
    qio_channel_fd_pair_source_prepare,
    qio_channel_fd_pair_source_check,
    qio_channel_fd_pair_source_dispatch,
    qio_channel_fd_pair_source_finalize
};


GSource *qio_channel_create_fd_watch(QIOChannel *ioc,
                                     int fd,
                                     GIOCondition condition)
{
    GSource *source;
    QIOChannelFDSource *ssource;

    source = g_source_new(&qio_channel_fd_source_funcs,
                          sizeof(QIOChannelFDSource));
    ssource = (QIOChannelFDSource *)source;

    ssource->ioc = ioc;
    object_ref(OBJECT(ioc));

    ssource->condition = condition;

#ifdef CONFIG_WIN32
    ssource->fd.fd = (gint64)_get_osfhandle(fd);
#else
    ssource->fd.fd = fd;
#endif
    ssource->fd.events = condition;

    g_source_add_poll(source, &ssource->fd);

    return source;
}

#ifdef CONFIG_WIN32
GSource *qio_channel_create_socket_watch(QIOChannel *ioc,
                                         int socket,
                                         GIOCondition condition)
{
    GSource *source;
    QIOChannelSocketSource *ssource;

#ifdef WIN32
    int ev = 0;

    if (condition & G_IO_IN) {
        ev |= (FD_READ | FD_ACCEPT | FD_OOB);
    }
    if (condition & G_IO_OUT) {
        ev |= (FD_WRITE | FD_CONNECT);
    }

    WSAEventSelect(socket, ioc->event, ev);
#endif

    source = g_source_new(&qio_channel_socket_source_funcs,
                          sizeof(QIOChannelSocketSource));
    ssource = (QIOChannelSocketSource *)source;

    ssource->ioc = ioc;
    object_ref(OBJECT(ioc));

    ssource->condition = condition;
    ssource->socket = socket;
    ssource->revents = 0;

    ssource->fd.fd = (gintptr)ioc->event;
    ssource->fd.events = condition;

    g_source_add_poll(source, &ssource->fd);

    return source;
}
#else
GSource *qio_channel_create_socket_watch(QIOChannel *ioc,
                                         int socket,
                                         GIOCondition condition)
{
    return qio_channel_create_fd_watch(ioc, socket, condition);
}
#endif

GSource *qio_channel_create_fd_pair_watch(QIOChannel *ioc,
                                          int fdread,
                                          int fdwrite,
                                          GIOCondition condition)
{
    GSource *source;
    QIOChannelFDPairSource *ssource;

    source = g_source_new(&qio_channel_fd_pair_source_funcs,
                          sizeof(QIOChannelFDPairSource));
    ssource = (QIOChannelFDPairSource *)source;

    ssource->ioc = ioc;
    object_ref(OBJECT(ioc));

    ssource->condition = condition;

#ifdef CONFIG_WIN32
    ssource->fdread.fd = (gint64)_get_osfhandle(fdread);
    ssource->fdwrite.fd = (gint64)_get_osfhandle(fdwrite);
#else
    ssource->fdread.fd = fdread;
    ssource->fdwrite.fd = fdwrite;
#endif

    ssource->fdread.events = condition & G_IO_IN;
    ssource->fdwrite.events = condition & G_IO_OUT;

    g_source_add_poll(source, &ssource->fdread);
    g_source_add_poll(source, &ssource->fdwrite);

    return source;
}
