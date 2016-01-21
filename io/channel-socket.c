/*
 * QEMU I/O channels sockets driver
 *
 * Copyright (c) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include "io/channel-socket.h"
#include "io/channel-watch.h"
#include "trace.h"

#define SOCKET_MAX_FDS 16

SocketAddress *
qio_channel_socket_get_local_address(QIOChannelSocket *ioc,
                                     Error **errp)
{
    return socket_sockaddr_to_address(&ioc->localAddr,
                                      ioc->localAddrLen,
                                      errp);
}

SocketAddress *
qio_channel_socket_get_remote_address(QIOChannelSocket *ioc,
                                      Error **errp)
{
    return socket_sockaddr_to_address(&ioc->remoteAddr,
                                      ioc->remoteAddrLen,
                                      errp);
}

QIOChannelSocket *
qio_channel_socket_new(void)
{
    QIOChannelSocket *sioc;
    QIOChannel *ioc;

    sioc = QIO_CHANNEL_SOCKET(object_new(TYPE_QIO_CHANNEL_SOCKET));
    sioc->fd = -1;

    ioc = QIO_CHANNEL(sioc);
    ioc->features |= (1 << QIO_CHANNEL_FEATURE_SHUTDOWN);

    trace_qio_channel_socket_new(sioc);

    return sioc;
}


static int
qio_channel_socket_set_fd(QIOChannelSocket *sioc,
                          int fd,
                          Error **errp)
{
    if (sioc->fd != -1) {
        error_setg(errp, "Socket is already open");
        return -1;
    }

    sioc->fd = fd;
    sioc->remoteAddrLen = sizeof(sioc->remoteAddr);
    sioc->localAddrLen = sizeof(sioc->localAddr);


    if (getpeername(fd, (struct sockaddr *)&sioc->remoteAddr,
                    &sioc->remoteAddrLen) < 0) {
        if (socket_error() == ENOTCONN) {
            memset(&sioc->remoteAddr, 0, sizeof(sioc->remoteAddr));
            sioc->remoteAddrLen = sizeof(sioc->remoteAddr);
        } else {
            error_setg_errno(errp, socket_error(),
                             "Unable to query remote socket address");
            goto error;
        }
    }

    if (getsockname(fd, (struct sockaddr *)&sioc->localAddr,
                    &sioc->localAddrLen) < 0) {
        error_setg_errno(errp, socket_error(),
                         "Unable to query local socket address");
        goto error;
    }

#ifndef WIN32
    if (sioc->localAddr.ss_family == AF_UNIX) {
        QIOChannel *ioc = QIO_CHANNEL(sioc);
        ioc->features |= (1 << QIO_CHANNEL_FEATURE_FD_PASS);
    }
#endif /* WIN32 */

    return 0;

 error:
    sioc->fd = -1; /* Let the caller close FD on failure */
    return -1;
}

QIOChannelSocket *
qio_channel_socket_new_fd(int fd,
                          Error **errp)
{
    QIOChannelSocket *ioc;

    ioc = qio_channel_socket_new();
    if (qio_channel_socket_set_fd(ioc, fd, errp) < 0) {
        object_unref(OBJECT(ioc));
        return NULL;
    }

    trace_qio_channel_socket_new_fd(ioc, fd);

    return ioc;
}


int qio_channel_socket_connect_sync(QIOChannelSocket *ioc,
                                    SocketAddress *addr,
                                    Error **errp)
{
    int fd;

    trace_qio_channel_socket_connect_sync(ioc, addr);
    fd = socket_connect(addr, errp, NULL, NULL);
    if (fd < 0) {
        trace_qio_channel_socket_connect_fail(ioc);
        return -1;
    }

    trace_qio_channel_socket_connect_complete(ioc, fd);
    if (qio_channel_socket_set_fd(ioc, fd, errp) < 0) {
        close(fd);
        return -1;
    }

    return 0;
}


static int qio_channel_socket_connect_worker(QIOTask *task,
                                             Error **errp,
                                             gpointer opaque)
{
    QIOChannelSocket *ioc = QIO_CHANNEL_SOCKET(qio_task_get_source(task));
    SocketAddress *addr = opaque;
    int ret;

    ret = qio_channel_socket_connect_sync(ioc,
                                          addr,
                                          errp);

    object_unref(OBJECT(ioc));
    return ret;
}


void qio_channel_socket_connect_async(QIOChannelSocket *ioc,
                                      SocketAddress *addr,
                                      QIOTaskFunc callback,
                                      gpointer opaque,
                                      GDestroyNotify destroy)
{
    QIOTask *task = qio_task_new(
        OBJECT(ioc), callback, opaque, destroy);
    SocketAddress *addrCopy;

    qapi_copy_SocketAddress(&addrCopy, addr);

    /* socket_connect() does a non-blocking connect(), but it
     * still blocks in DNS lookups, so we must use a thread */
    trace_qio_channel_socket_connect_async(ioc, addr);
    qio_task_run_in_thread(task,
                           qio_channel_socket_connect_worker,
                           addrCopy,
                           (GDestroyNotify)qapi_free_SocketAddress);
}


int qio_channel_socket_listen_sync(QIOChannelSocket *ioc,
                                   SocketAddress *addr,
                                   Error **errp)
{
    int fd;

    trace_qio_channel_socket_listen_sync(ioc, addr);
    fd = socket_listen(addr, errp);
    if (fd < 0) {
        trace_qio_channel_socket_listen_fail(ioc);
        return -1;
    }

    trace_qio_channel_socket_listen_complete(ioc, fd);
    if (qio_channel_socket_set_fd(ioc, fd, errp) < 0) {
        close(fd);
        return -1;
    }

    return 0;
}


static int qio_channel_socket_listen_worker(QIOTask *task,
                                            Error **errp,
                                            gpointer opaque)
{
    QIOChannelSocket *ioc = QIO_CHANNEL_SOCKET(qio_task_get_source(task));
    SocketAddress *addr = opaque;
    int ret;

    ret = qio_channel_socket_listen_sync(ioc,
                                         addr,
                                         errp);

    object_unref(OBJECT(ioc));
    return ret;
}


void qio_channel_socket_listen_async(QIOChannelSocket *ioc,
                                     SocketAddress *addr,
                                     QIOTaskFunc callback,
                                     gpointer opaque,
                                     GDestroyNotify destroy)
{
    QIOTask *task = qio_task_new(
        OBJECT(ioc), callback, opaque, destroy);
    SocketAddress *addrCopy;

    qapi_copy_SocketAddress(&addrCopy, addr);

    /* socket_listen() blocks in DNS lookups, so we must use a thread */
    trace_qio_channel_socket_listen_async(ioc, addr);
    qio_task_run_in_thread(task,
                           qio_channel_socket_listen_worker,
                           addrCopy,
                           (GDestroyNotify)qapi_free_SocketAddress);
}


int qio_channel_socket_dgram_sync(QIOChannelSocket *ioc,
                                  SocketAddress *localAddr,
                                  SocketAddress *remoteAddr,
                                  Error **errp)
{
    int fd;

    trace_qio_channel_socket_dgram_sync(ioc, localAddr, remoteAddr);
    fd = socket_dgram(localAddr, remoteAddr, errp);
    if (fd < 0) {
        trace_qio_channel_socket_dgram_fail(ioc);
        return -1;
    }

    trace_qio_channel_socket_dgram_complete(ioc, fd);
    if (qio_channel_socket_set_fd(ioc, fd, errp) < 0) {
        close(fd);
        return -1;
    }

    return 0;
}


struct QIOChannelSocketDGramWorkerData {
    SocketAddress *localAddr;
    SocketAddress *remoteAddr;
};


static void qio_channel_socket_dgram_worker_free(gpointer opaque)
{
    struct QIOChannelSocketDGramWorkerData *data = opaque;
    qapi_free_SocketAddress(data->localAddr);
    qapi_free_SocketAddress(data->remoteAddr);
    g_free(data);
}

static int qio_channel_socket_dgram_worker(QIOTask *task,
                                           Error **errp,
                                           gpointer opaque)
{
    QIOChannelSocket *ioc = QIO_CHANNEL_SOCKET(qio_task_get_source(task));
    struct QIOChannelSocketDGramWorkerData *data = opaque;
    int ret;

    /* socket_dgram() blocks in DNS lookups, so we must use a thread */
    ret = qio_channel_socket_dgram_sync(ioc,
                                        data->localAddr,
                                        data->remoteAddr,
                                        errp);

    object_unref(OBJECT(ioc));
    return ret;
}


void qio_channel_socket_dgram_async(QIOChannelSocket *ioc,
                                    SocketAddress *localAddr,
                                    SocketAddress *remoteAddr,
                                    QIOTaskFunc callback,
                                    gpointer opaque,
                                    GDestroyNotify destroy)
{
    QIOTask *task = qio_task_new(
        OBJECT(ioc), callback, opaque, destroy);
    struct QIOChannelSocketDGramWorkerData *data = g_new0(
        struct QIOChannelSocketDGramWorkerData, 1);

    qapi_copy_SocketAddress(&data->localAddr, localAddr);
    qapi_copy_SocketAddress(&data->remoteAddr, remoteAddr);

    trace_qio_channel_socket_dgram_async(ioc, localAddr, remoteAddr);
    qio_task_run_in_thread(task,
                           qio_channel_socket_dgram_worker,
                           data,
                           qio_channel_socket_dgram_worker_free);
}


QIOChannelSocket *
qio_channel_socket_accept(QIOChannelSocket *ioc,
                          Error **errp)
{
    QIOChannelSocket *cioc;

    cioc = QIO_CHANNEL_SOCKET(object_new(TYPE_QIO_CHANNEL_SOCKET));
    cioc->fd = -1;
    cioc->remoteAddrLen = sizeof(ioc->remoteAddr);
    cioc->localAddrLen = sizeof(ioc->localAddr);

 retry:
    trace_qio_channel_socket_accept(ioc);
    cioc->fd = accept(ioc->fd, (struct sockaddr *)&cioc->remoteAddr,
                      &cioc->remoteAddrLen);
    if (cioc->fd < 0) {
        trace_qio_channel_socket_accept_fail(ioc);
        if (socket_error() == EINTR) {
            goto retry;
        }
        goto error;
    }

    if (getsockname(cioc->fd, (struct sockaddr *)&cioc->localAddr,
                    &cioc->localAddrLen) < 0) {
        error_setg_errno(errp, socket_error(),
                         "Unable to query local socket address");
        goto error;
    }

#ifndef WIN32
    if (cioc->localAddr.ss_family == AF_UNIX) {
        QIO_CHANNEL(cioc)->features |= (1 << QIO_CHANNEL_FEATURE_FD_PASS);
    }
#endif /* WIN32 */

    trace_qio_channel_socket_accept_complete(ioc, cioc, cioc->fd);
    return cioc;

 error:
    object_unref(OBJECT(cioc));
    return NULL;
}

static void qio_channel_socket_init(Object *obj)
{
    QIOChannelSocket *ioc = QIO_CHANNEL_SOCKET(obj);
    ioc->fd = -1;
}

static void qio_channel_socket_finalize(Object *obj)
{
    QIOChannelSocket *ioc = QIO_CHANNEL_SOCKET(obj);
    if (ioc->fd != -1) {
        close(ioc->fd);
        ioc->fd = -1;
    }
}


#ifndef WIN32
static void qio_channel_socket_copy_fds(struct msghdr *msg,
                                        int **fds, size_t *nfds)
{
    struct cmsghdr *cmsg;

    *nfds = 0;
    *fds = NULL;

    for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
        int fd_size, i;
        int gotfds;

        if (cmsg->cmsg_len < CMSG_LEN(sizeof(int)) ||
            cmsg->cmsg_level != SOL_SOCKET ||
            cmsg->cmsg_type != SCM_RIGHTS) {
            continue;
        }

        fd_size = cmsg->cmsg_len - CMSG_LEN(0);

        if (!fd_size) {
            continue;
        }

        gotfds = fd_size / sizeof(int);
        *fds = g_renew(int, *fds, *nfds + gotfds);
        memcpy(*fds + *nfds, CMSG_DATA(cmsg), fd_size);

        for (i = 0; i < gotfds; i++) {
            int fd = (*fds)[*nfds + i];
            if (fd < 0) {
                continue;
            }

            /* O_NONBLOCK is preserved across SCM_RIGHTS so reset it */
            qemu_set_block(fd);

#ifndef MSG_CMSG_CLOEXEC
            qemu_set_cloexec(fd);
#endif
        }
        *nfds += gotfds;
    }
}


static ssize_t qio_channel_socket_readv(QIOChannel *ioc,
                                        const struct iovec *iov,
                                        size_t niov,
                                        int **fds,
                                        size_t *nfds,
                                        Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    ssize_t ret;
    struct msghdr msg = { NULL, };
    char control[CMSG_SPACE(sizeof(int) * SOCKET_MAX_FDS)];
    int sflags = 0;

#ifdef MSG_CMSG_CLOEXEC
    sflags |= MSG_CMSG_CLOEXEC;
#endif

    msg.msg_iov = (struct iovec *)iov;
    msg.msg_iovlen = niov;
    if (fds && nfds) {
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
    }

 retry:
    ret = recvmsg(sioc->fd, &msg, sflags);
    if (ret < 0) {
        if (socket_error() == EAGAIN ||
            socket_error() == EWOULDBLOCK) {
            return QIO_CHANNEL_ERR_BLOCK;
        }
        if (socket_error() == EINTR) {
            goto retry;
        }

        error_setg_errno(errp, socket_error(),
                         "Unable to read from socket");
        return -1;
    }

    if (fds && nfds) {
        qio_channel_socket_copy_fds(&msg, fds, nfds);
    }

    return ret;
}

static ssize_t qio_channel_socket_writev(QIOChannel *ioc,
                                         const struct iovec *iov,
                                         size_t niov,
                                         int *fds,
                                         size_t nfds,
                                         Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    ssize_t ret;
    struct msghdr msg = { NULL, };
    char control[CMSG_SPACE(sizeof(int) * SOCKET_MAX_FDS)] = { 0 };
    size_t fdsize = sizeof(int) * nfds;
    struct cmsghdr *cmsg;

    msg.msg_iov = (struct iovec *)iov;
    msg.msg_iovlen = niov;

    if (nfds) {
        if (nfds > SOCKET_MAX_FDS) {
            error_setg_errno(errp, -EINVAL,
                             "Only %d FDs can be sent, got %zu",
                             SOCKET_MAX_FDS, nfds);
            return -1;
        }

        msg.msg_control = control;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(fdsize);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(cmsg), fds, fdsize);
    }

 retry:
    ret = sendmsg(sioc->fd, &msg, 0);
    if (ret <= 0) {
        if (socket_error() == EAGAIN ||
            socket_error() == EWOULDBLOCK) {
            return QIO_CHANNEL_ERR_BLOCK;
        }
        if (socket_error() == EINTR) {
            goto retry;
        }
        error_setg_errno(errp, socket_error(),
                         "Unable to write to socket");
        return -1;
    }
    return ret;
}
#else /* WIN32 */
static ssize_t qio_channel_socket_readv(QIOChannel *ioc,
                                        const struct iovec *iov,
                                        size_t niov,
                                        int **fds,
                                        size_t *nfds,
                                        Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    ssize_t done = 0;
    ssize_t i;

    for (i = 0; i < niov; i++) {
        ssize_t ret;
    retry:
        ret = recv(sioc->fd,
                   iov[i].iov_base,
                   iov[i].iov_len,
                   0);
        if (ret < 0) {
            if (socket_error() == EAGAIN) {
                if (done) {
                    return done;
                } else {
                    return QIO_CHANNEL_ERR_BLOCK;
                }
            } else if (socket_error() == EINTR) {
                goto retry;
            } else {
                error_setg_errno(errp, socket_error(),
                                 "Unable to write to socket");
                return -1;
            }
        }
        done += ret;
        if (ret < iov[i].iov_len) {
            return done;
        }
    }

    return done;
}

static ssize_t qio_channel_socket_writev(QIOChannel *ioc,
                                         const struct iovec *iov,
                                         size_t niov,
                                         int *fds,
                                         size_t nfds,
                                         Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    ssize_t done = 0;
    ssize_t i;

    for (i = 0; i < niov; i++) {
        ssize_t ret;
    retry:
        ret = send(sioc->fd,
                   iov[i].iov_base,
                   iov[i].iov_len,
                   0);
        if (ret < 0) {
            if (socket_error() == EAGAIN) {
                if (done) {
                    return done;
                } else {
                    return QIO_CHANNEL_ERR_BLOCK;
                }
            } else if (socket_error() == EINTR) {
                goto retry;
            } else {
                error_setg_errno(errp, socket_error(),
                                 "Unable to write to socket");
                return -1;
            }
        }
        done += ret;
        if (ret < iov[i].iov_len) {
            return done;
        }
    }

    return done;
}
#endif /* WIN32 */

static int
qio_channel_socket_set_blocking(QIOChannel *ioc,
                                bool enabled,
                                Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);

    if (enabled) {
        qemu_set_block(sioc->fd);
    } else {
        qemu_set_nonblock(sioc->fd);
    }
    return 0;
}


static void
qio_channel_socket_set_delay(QIOChannel *ioc,
                             bool enabled)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    int v = enabled ? 0 : 1;

    qemu_setsockopt(sioc->fd,
                    IPPROTO_TCP, TCP_NODELAY,
                    &v, sizeof(v));
}


static void
qio_channel_socket_set_cork(QIOChannel *ioc,
                            bool enabled)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    int v = enabled ? 1 : 0;

    socket_set_cork(sioc->fd, v);
}


static int
qio_channel_socket_close(QIOChannel *ioc,
                         Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);

    if (closesocket(sioc->fd) < 0) {
        sioc->fd = -1;
        error_setg_errno(errp, socket_error(),
                         "Unable to close socket");
        return -1;
    }
    sioc->fd = -1;
    return 0;
}

static int
qio_channel_socket_shutdown(QIOChannel *ioc,
                            QIOChannelShutdown how,
                            Error **errp)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    int sockhow;

    switch (how) {
    case QIO_CHANNEL_SHUTDOWN_READ:
        sockhow = SHUT_RD;
        break;
    case QIO_CHANNEL_SHUTDOWN_WRITE:
        sockhow = SHUT_WR;
        break;
    case QIO_CHANNEL_SHUTDOWN_BOTH:
    default:
        sockhow = SHUT_RDWR;
        break;
    }

    if (shutdown(sioc->fd, sockhow) < 0) {
        error_setg_errno(errp, socket_error(),
                         "Unable to shutdown socket");
        return -1;
    }
    return 0;
}

static GSource *qio_channel_socket_create_watch(QIOChannel *ioc,
                                                GIOCondition condition)
{
    QIOChannelSocket *sioc = QIO_CHANNEL_SOCKET(ioc);
    return qio_channel_create_fd_watch(ioc,
                                       sioc->fd,
                                       condition);
}

static void qio_channel_socket_class_init(ObjectClass *klass,
                                          void *class_data G_GNUC_UNUSED)
{
    QIOChannelClass *ioc_klass = QIO_CHANNEL_CLASS(klass);

    ioc_klass->io_writev = qio_channel_socket_writev;
    ioc_klass->io_readv = qio_channel_socket_readv;
    ioc_klass->io_set_blocking = qio_channel_socket_set_blocking;
    ioc_klass->io_close = qio_channel_socket_close;
    ioc_klass->io_shutdown = qio_channel_socket_shutdown;
    ioc_klass->io_set_cork = qio_channel_socket_set_cork;
    ioc_klass->io_set_delay = qio_channel_socket_set_delay;
    ioc_klass->io_create_watch = qio_channel_socket_create_watch;
}

static const TypeInfo qio_channel_socket_info = {
    .parent = TYPE_QIO_CHANNEL,
    .name = TYPE_QIO_CHANNEL_SOCKET,
    .instance_size = sizeof(QIOChannelSocket),
    .instance_init = qio_channel_socket_init,
    .instance_finalize = qio_channel_socket_finalize,
    .class_init = qio_channel_socket_class_init,
};

static void qio_channel_socket_register_types(void)
{
    type_register_static(&qio_channel_socket_info);
}

type_init(qio_channel_socket_register_types);
