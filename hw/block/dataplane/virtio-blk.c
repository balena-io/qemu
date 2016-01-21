/*
 * Dedicated thread for virtio-blk I/O processing
 *
 * Copyright 2012 IBM, Corp.
 * Copyright 2012 Red Hat, Inc. and/or its affiliates
 *
 * Authors:
 *   Stefan Hajnoczi <stefanha@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "trace.h"
#include "qemu/iov.h"
#include "qemu/thread.h"
#include "qemu/error-report.h"
#include "hw/virtio/virtio-access.h"
#include "hw/virtio/dataplane/vring.h"
#include "hw/virtio/dataplane/vring-accessors.h"
#include "sysemu/block-backend.h"
#include "hw/virtio/virtio-blk.h"
#include "virtio-blk.h"
#include "block/aio.h"
#include "hw/virtio/virtio-bus.h"
#include "qom/object_interfaces.h"

struct VirtIOBlockDataPlane {
    bool started;
    bool starting;
    bool stopping;
    bool disabled;

    VirtIOBlkConf *conf;

    VirtIODevice *vdev;
    Vring vring;                    /* virtqueue vring */
    EventNotifier *guest_notifier;  /* irq */
    QEMUBH *bh;                     /* bh for guest notification */

    /* Note that these EventNotifiers are assigned by value.  This is
     * fine as long as you do not call event_notifier_cleanup on them
     * (because you don't own the file descriptor or handle; you just
     * use it).
     */
    IOThread *iothread;
    AioContext *ctx;
    EventNotifier host_notifier;    /* doorbell */

    /* Operation blocker on BDS */
    Error *blocker;
    void (*saved_complete_request)(struct VirtIOBlockReq *req,
                                   unsigned char status);
};

/* Raise an interrupt to signal guest, if necessary */
static void notify_guest(VirtIOBlockDataPlane *s)
{
    if (!vring_should_notify(s->vdev, &s->vring)) {
        return;
    }

    event_notifier_set(s->guest_notifier);
}

static void notify_guest_bh(void *opaque)
{
    VirtIOBlockDataPlane *s = opaque;

    notify_guest(s);
}

static void complete_request_vring(VirtIOBlockReq *req, unsigned char status)
{
    VirtIOBlockDataPlane *s = req->dev->dataplane;
    stb_p(&req->in->status, status);

    vring_push(s->vdev, &req->dev->dataplane->vring, &req->elem, req->in_len);

    /* Suppress notification to guest by BH and its scheduled
     * flag because requests are completed as a batch after io
     * plug & unplug is introduced, and the BH can still be
     * executed in dataplane aio context even after it is
     * stopped, so needn't worry about notification loss with BH.
     */
    qemu_bh_schedule(s->bh);
}

static void handle_notify(EventNotifier *e)
{
    VirtIOBlockDataPlane *s = container_of(e, VirtIOBlockDataPlane,
                                           host_notifier);
    VirtIOBlock *vblk = VIRTIO_BLK(s->vdev);

    event_notifier_test_and_clear(&s->host_notifier);
    blk_io_plug(s->conf->conf.blk);
    for (;;) {
        MultiReqBuffer mrb = {};
        int ret;

        /* Disable guest->host notifies to avoid unnecessary vmexits */
        vring_disable_notification(s->vdev, &s->vring);

        for (;;) {
            VirtIOBlockReq *req = virtio_blk_alloc_request(vblk);

            ret = vring_pop(s->vdev, &s->vring, &req->elem);
            if (ret < 0) {
                virtio_blk_free_request(req);
                break; /* no more requests */
            }

            trace_virtio_blk_data_plane_process_request(s, req->elem.out_num,
                                                        req->elem.in_num,
                                                        req->elem.index);

            virtio_blk_handle_request(req, &mrb);
        }

        if (mrb.num_reqs) {
            virtio_blk_submit_multireq(s->conf->conf.blk, &mrb);
        }

        if (likely(ret == -EAGAIN)) { /* vring emptied */
            /* Re-enable guest->host notifies and stop processing the vring.
             * But if the guest has snuck in more descriptors, keep processing.
             */
            if (vring_enable_notification(s->vdev, &s->vring)) {
                break;
            }
        } else { /* fatal error */
            break;
        }
    }
    blk_io_unplug(s->conf->conf.blk);
}

/* Context: QEMU global mutex held */
void virtio_blk_data_plane_create(VirtIODevice *vdev, VirtIOBlkConf *conf,
                                  VirtIOBlockDataPlane **dataplane,
                                  Error **errp)
{
    VirtIOBlockDataPlane *s;
    BusState *qbus = BUS(qdev_get_parent_bus(DEVICE(vdev)));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(qbus);

    *dataplane = NULL;

    if (!conf->iothread) {
        return;
    }

    /* Don't try if transport does not support notifiers. */
    if (!k->set_guest_notifiers || !k->set_host_notifier) {
        error_setg(errp,
                   "device is incompatible with dataplane "
                   "(transport does not support notifiers)");
        return;
    }

    /* If dataplane is (re-)enabled while the guest is running there could be
     * block jobs that can conflict.
     */
    if (blk_op_is_blocked(conf->conf.blk, BLOCK_OP_TYPE_DATAPLANE, errp)) {
        error_prepend(errp, "cannot start dataplane thread: ");
        return;
    }

    s = g_new0(VirtIOBlockDataPlane, 1);
    s->vdev = vdev;
    s->conf = conf;

    if (conf->iothread) {
        s->iothread = conf->iothread;
        object_ref(OBJECT(s->iothread));
    }
    s->ctx = iothread_get_aio_context(s->iothread);
    s->bh = aio_bh_new(s->ctx, notify_guest_bh, s);

    error_setg(&s->blocker, "block device is in use by data plane");
    blk_op_block_all(conf->conf.blk, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_RESIZE, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_DRIVE_DEL, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_BACKUP_SOURCE, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_CHANGE, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_COMMIT_SOURCE, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_COMMIT_TARGET, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_EJECT, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_EXTERNAL_SNAPSHOT, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_INTERNAL_SNAPSHOT, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_INTERNAL_SNAPSHOT_DELETE,
                   s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_MIRROR_SOURCE, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_STREAM, s->blocker);
    blk_op_unblock(conf->conf.blk, BLOCK_OP_TYPE_REPLACE, s->blocker);

    *dataplane = s;
}

/* Context: QEMU global mutex held */
void virtio_blk_data_plane_destroy(VirtIOBlockDataPlane *s)
{
    if (!s) {
        return;
    }

    virtio_blk_data_plane_stop(s);
    blk_op_unblock_all(s->conf->conf.blk, s->blocker);
    error_free(s->blocker);
    qemu_bh_delete(s->bh);
    object_unref(OBJECT(s->iothread));
    g_free(s);
}

/* Context: QEMU global mutex held */
void virtio_blk_data_plane_start(VirtIOBlockDataPlane *s)
{
    BusState *qbus = BUS(qdev_get_parent_bus(DEVICE(s->vdev)));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(qbus);
    VirtIOBlock *vblk = VIRTIO_BLK(s->vdev);
    VirtQueue *vq;
    int r;

    if (s->started || s->disabled) {
        return;
    }

    if (s->starting) {
        return;
    }

    s->starting = true;

    vq = virtio_get_queue(s->vdev, 0);
    if (!vring_setup(&s->vring, s->vdev, 0)) {
        goto fail_vring;
    }

    /* Set up guest notifier (irq) */
    r = k->set_guest_notifiers(qbus->parent, 1, true);
    if (r != 0) {
        fprintf(stderr, "virtio-blk failed to set guest notifier (%d), "
                "ensure -enable-kvm is set\n", r);
        goto fail_guest_notifiers;
    }
    s->guest_notifier = virtio_queue_get_guest_notifier(vq);

    /* Set up virtqueue notify */
    r = k->set_host_notifier(qbus->parent, 0, true);
    if (r != 0) {
        fprintf(stderr, "virtio-blk failed to set host notifier (%d)\n", r);
        goto fail_host_notifier;
    }
    s->host_notifier = *virtio_queue_get_host_notifier(vq);

    s->saved_complete_request = vblk->complete_request;
    vblk->complete_request = complete_request_vring;

    s->starting = false;
    s->started = true;
    trace_virtio_blk_data_plane_start(s);

    blk_set_aio_context(s->conf->conf.blk, s->ctx);

    /* Kick right away to begin processing requests already in vring */
    event_notifier_set(virtio_queue_get_host_notifier(vq));

    /* Get this show started by hooking up our callbacks */
    aio_context_acquire(s->ctx);
    aio_set_event_notifier(s->ctx, &s->host_notifier, true,
                           handle_notify);
    aio_context_release(s->ctx);
    return;

  fail_host_notifier:
    k->set_guest_notifiers(qbus->parent, 1, false);
  fail_guest_notifiers:
    vring_teardown(&s->vring, s->vdev, 0);
    s->disabled = true;
  fail_vring:
    s->starting = false;
}

/* Context: QEMU global mutex held */
void virtio_blk_data_plane_stop(VirtIOBlockDataPlane *s)
{
    BusState *qbus = BUS(qdev_get_parent_bus(DEVICE(s->vdev)));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(qbus);
    VirtIOBlock *vblk = VIRTIO_BLK(s->vdev);


    /* Better luck next time. */
    if (s->disabled) {
        s->disabled = false;
        return;
    }
    if (!s->started || s->stopping) {
        return;
    }
    s->stopping = true;
    vblk->complete_request = s->saved_complete_request;
    trace_virtio_blk_data_plane_stop(s);

    aio_context_acquire(s->ctx);

    /* Stop notifications for new requests from guest */
    aio_set_event_notifier(s->ctx, &s->host_notifier, true, NULL);

    /* Drain and switch bs back to the QEMU main loop */
    blk_set_aio_context(s->conf->conf.blk, qemu_get_aio_context());

    aio_context_release(s->ctx);

    /* Sync vring state back to virtqueue so that non-dataplane request
     * processing can continue when we disable the host notifier below.
     */
    vring_teardown(&s->vring, s->vdev, 0);

    k->set_host_notifier(qbus->parent, 0, false);

    /* Clean up guest notifier (irq) */
    k->set_guest_notifiers(qbus->parent, 1, false);

    s->started = false;
    s->stopping = false;
}
