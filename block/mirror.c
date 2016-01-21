/*
 * Image mirroring
 *
 * Copyright Red Hat, Inc. 2012
 *
 * Authors:
 *  Paolo Bonzini  <pbonzini@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "trace.h"
#include "block/blockjob.h"
#include "block/block_int.h"
#include "sysemu/block-backend.h"
#include "qapi/qmp/qerror.h"
#include "qemu/ratelimit.h"
#include "qemu/bitmap.h"
#include "qemu/error-report.h"

#define SLICE_TIME    100000000ULL /* ns */
#define MAX_IN_FLIGHT 16
#define DEFAULT_MIRROR_BUF_SIZE   (10 << 20)

/* The mirroring buffer is a list of granularity-sized chunks.
 * Free chunks are organized in a list.
 */
typedef struct MirrorBuffer {
    QSIMPLEQ_ENTRY(MirrorBuffer) next;
} MirrorBuffer;

typedef struct MirrorBlockJob {
    BlockJob common;
    RateLimit limit;
    BlockDriverState *target;
    BlockDriverState *base;
    /* The name of the graph node to replace */
    char *replaces;
    /* The BDS to replace */
    BlockDriverState *to_replace;
    /* Used to block operations on the drive-mirror-replace target */
    Error *replace_blocker;
    bool is_none_mode;
    BlockdevOnError on_source_error, on_target_error;
    bool synced;
    bool should_complete;
    int64_t sector_num;
    int64_t granularity;
    size_t buf_size;
    int64_t bdev_length;
    unsigned long *cow_bitmap;
    BdrvDirtyBitmap *dirty_bitmap;
    HBitmapIter hbi;
    uint8_t *buf;
    QSIMPLEQ_HEAD(, MirrorBuffer) buf_free;
    int buf_free_count;

    unsigned long *in_flight_bitmap;
    int in_flight;
    int sectors_in_flight;
    int ret;
    bool unmap;
    bool waiting_for_io;
} MirrorBlockJob;

typedef struct MirrorOp {
    MirrorBlockJob *s;
    QEMUIOVector qiov;
    int64_t sector_num;
    int nb_sectors;
} MirrorOp;

static BlockErrorAction mirror_error_action(MirrorBlockJob *s, bool read,
                                            int error)
{
    s->synced = false;
    if (read) {
        return block_job_error_action(&s->common, s->common.bs,
                                      s->on_source_error, true, error);
    } else {
        return block_job_error_action(&s->common, s->target,
                                      s->on_target_error, false, error);
    }
}

static void mirror_iteration_done(MirrorOp *op, int ret)
{
    MirrorBlockJob *s = op->s;
    struct iovec *iov;
    int64_t chunk_num;
    int i, nb_chunks, sectors_per_chunk;

    trace_mirror_iteration_done(s, op->sector_num, op->nb_sectors, ret);

    s->in_flight--;
    s->sectors_in_flight -= op->nb_sectors;
    iov = op->qiov.iov;
    for (i = 0; i < op->qiov.niov; i++) {
        MirrorBuffer *buf = (MirrorBuffer *) iov[i].iov_base;
        QSIMPLEQ_INSERT_TAIL(&s->buf_free, buf, next);
        s->buf_free_count++;
    }

    sectors_per_chunk = s->granularity >> BDRV_SECTOR_BITS;
    chunk_num = op->sector_num / sectors_per_chunk;
    nb_chunks = op->nb_sectors / sectors_per_chunk;
    bitmap_clear(s->in_flight_bitmap, chunk_num, nb_chunks);
    if (ret >= 0) {
        if (s->cow_bitmap) {
            bitmap_set(s->cow_bitmap, chunk_num, nb_chunks);
        }
        s->common.offset += (uint64_t)op->nb_sectors * BDRV_SECTOR_SIZE;
    }

    qemu_iovec_destroy(&op->qiov);
    g_free(op);

    if (s->waiting_for_io) {
        qemu_coroutine_enter(s->common.co, NULL);
    }
}

static void mirror_write_complete(void *opaque, int ret)
{
    MirrorOp *op = opaque;
    MirrorBlockJob *s = op->s;
    if (ret < 0) {
        BlockErrorAction action;

        bdrv_set_dirty_bitmap(s->dirty_bitmap, op->sector_num, op->nb_sectors);
        action = mirror_error_action(s, false, -ret);
        if (action == BLOCK_ERROR_ACTION_REPORT && s->ret >= 0) {
            s->ret = ret;
        }
    }
    mirror_iteration_done(op, ret);
}

static void mirror_read_complete(void *opaque, int ret)
{
    MirrorOp *op = opaque;
    MirrorBlockJob *s = op->s;
    if (ret < 0) {
        BlockErrorAction action;

        bdrv_set_dirty_bitmap(s->dirty_bitmap, op->sector_num, op->nb_sectors);
        action = mirror_error_action(s, true, -ret);
        if (action == BLOCK_ERROR_ACTION_REPORT && s->ret >= 0) {
            s->ret = ret;
        }

        mirror_iteration_done(op, ret);
        return;
    }
    bdrv_aio_writev(s->target, op->sector_num, &op->qiov, op->nb_sectors,
                    mirror_write_complete, op);
}

static uint64_t coroutine_fn mirror_iteration(MirrorBlockJob *s)
{
    BlockDriverState *source = s->common.bs;
    int nb_sectors, sectors_per_chunk, nb_chunks, max_iov;
    int64_t end, sector_num, next_chunk, next_sector, hbitmap_next_sector;
    uint64_t delay_ns = 0;
    MirrorOp *op;
    int pnum;
    int64_t ret;

    max_iov = MIN(source->bl.max_iov, s->target->bl.max_iov);

    s->sector_num = hbitmap_iter_next(&s->hbi);
    if (s->sector_num < 0) {
        bdrv_dirty_iter_init(s->dirty_bitmap, &s->hbi);
        s->sector_num = hbitmap_iter_next(&s->hbi);
        trace_mirror_restart_iter(s, bdrv_get_dirty_count(s->dirty_bitmap));
        assert(s->sector_num >= 0);
    }

    hbitmap_next_sector = s->sector_num;
    sector_num = s->sector_num;
    sectors_per_chunk = s->granularity >> BDRV_SECTOR_BITS;
    end = s->bdev_length / BDRV_SECTOR_SIZE;

    /* Extend the QEMUIOVector to include all adjacent blocks that will
     * be copied in this operation.
     *
     * We have to do this if we have no backing file yet in the destination,
     * and the cluster size is very large.  Then we need to do COW ourselves.
     * The first time a cluster is copied, copy it entirely.  Note that,
     * because both the granularity and the cluster size are powers of two,
     * the number of sectors to copy cannot exceed one cluster.
     *
     * We also want to extend the QEMUIOVector to include more adjacent
     * dirty blocks if possible, to limit the number of I/O operations and
     * run efficiently even with a small granularity.
     */
    nb_chunks = 0;
    nb_sectors = 0;
    next_sector = sector_num;
    next_chunk = sector_num / sectors_per_chunk;

    /* Wait for I/O to this cluster (from a previous iteration) to be done.  */
    while (test_bit(next_chunk, s->in_flight_bitmap)) {
        trace_mirror_yield_in_flight(s, sector_num, s->in_flight);
        s->waiting_for_io = true;
        qemu_coroutine_yield();
        s->waiting_for_io = false;
    }

    do {
        int added_sectors, added_chunks;

        if (!bdrv_get_dirty(source, s->dirty_bitmap, next_sector) ||
            test_bit(next_chunk, s->in_flight_bitmap)) {
            assert(nb_sectors > 0);
            break;
        }

        added_sectors = sectors_per_chunk;
        if (s->cow_bitmap && !test_bit(next_chunk, s->cow_bitmap)) {
            bdrv_round_to_clusters(s->target,
                                   next_sector, added_sectors,
                                   &next_sector, &added_sectors);

            /* On the first iteration, the rounding may make us copy
             * sectors before the first dirty one.
             */
            if (next_sector < sector_num) {
                assert(nb_sectors == 0);
                sector_num = next_sector;
                next_chunk = next_sector / sectors_per_chunk;
            }
        }

        added_sectors = MIN(added_sectors, end - (sector_num + nb_sectors));
        added_chunks = (added_sectors + sectors_per_chunk - 1) / sectors_per_chunk;

        /* When doing COW, it may happen that there is not enough space for
         * a full cluster.  Wait if that is the case.
         */
        while (nb_chunks == 0 && s->buf_free_count < added_chunks) {
            trace_mirror_yield_buf_busy(s, nb_chunks, s->in_flight);
            s->waiting_for_io = true;
            qemu_coroutine_yield();
            s->waiting_for_io = false;
        }
        if (s->buf_free_count < nb_chunks + added_chunks) {
            trace_mirror_break_buf_busy(s, nb_chunks, s->in_flight);
            break;
        }
        if (max_iov < nb_chunks + added_chunks) {
            trace_mirror_break_iov_max(s, nb_chunks, added_chunks);
            break;
        }

        /* We have enough free space to copy these sectors.  */
        bitmap_set(s->in_flight_bitmap, next_chunk, added_chunks);

        nb_sectors += added_sectors;
        nb_chunks += added_chunks;
        next_sector += added_sectors;
        next_chunk += added_chunks;
        if (!s->synced && s->common.speed) {
            delay_ns = ratelimit_calculate_delay(&s->limit, added_sectors);
        }
    } while (delay_ns == 0 && next_sector < end);

    /* Allocate a MirrorOp that is used as an AIO callback.  */
    op = g_new(MirrorOp, 1);
    op->s = s;
    op->sector_num = sector_num;
    op->nb_sectors = nb_sectors;

    /* Now make a QEMUIOVector taking enough granularity-sized chunks
     * from s->buf_free.
     */
    qemu_iovec_init(&op->qiov, nb_chunks);
    next_sector = sector_num;
    while (nb_chunks-- > 0) {
        MirrorBuffer *buf = QSIMPLEQ_FIRST(&s->buf_free);
        size_t remaining = (nb_sectors * BDRV_SECTOR_SIZE) - op->qiov.size;

        QSIMPLEQ_REMOVE_HEAD(&s->buf_free, next);
        s->buf_free_count--;
        qemu_iovec_add(&op->qiov, buf, MIN(s->granularity, remaining));

        /* Advance the HBitmapIter in parallel, so that we do not examine
         * the same sector twice.
         */
        if (next_sector > hbitmap_next_sector
            && bdrv_get_dirty(source, s->dirty_bitmap, next_sector)) {
            hbitmap_next_sector = hbitmap_iter_next(&s->hbi);
        }

        next_sector += sectors_per_chunk;
    }

    bdrv_reset_dirty_bitmap(s->dirty_bitmap, sector_num, nb_sectors);

    /* Copy the dirty cluster.  */
    s->in_flight++;
    s->sectors_in_flight += nb_sectors;
    trace_mirror_one_iteration(s, sector_num, nb_sectors);

    ret = bdrv_get_block_status_above(source, NULL, sector_num,
                                      nb_sectors, &pnum);
    if (ret < 0 || pnum < nb_sectors ||
            (ret & BDRV_BLOCK_DATA && !(ret & BDRV_BLOCK_ZERO))) {
        bdrv_aio_readv(source, sector_num, &op->qiov, nb_sectors,
                       mirror_read_complete, op);
    } else if (ret & BDRV_BLOCK_ZERO) {
        bdrv_aio_write_zeroes(s->target, sector_num, op->nb_sectors,
                              s->unmap ? BDRV_REQ_MAY_UNMAP : 0,
                              mirror_write_complete, op);
    } else {
        assert(!(ret & BDRV_BLOCK_DATA));
        bdrv_aio_discard(s->target, sector_num, op->nb_sectors,
                         mirror_write_complete, op);
    }
    return delay_ns;
}

static void mirror_free_init(MirrorBlockJob *s)
{
    int granularity = s->granularity;
    size_t buf_size = s->buf_size;
    uint8_t *buf = s->buf;

    assert(s->buf_free_count == 0);
    QSIMPLEQ_INIT(&s->buf_free);
    while (buf_size != 0) {
        MirrorBuffer *cur = (MirrorBuffer *)buf;
        QSIMPLEQ_INSERT_TAIL(&s->buf_free, cur, next);
        s->buf_free_count++;
        buf_size -= granularity;
        buf += granularity;
    }
}

static void mirror_drain(MirrorBlockJob *s)
{
    while (s->in_flight > 0) {
        s->waiting_for_io = true;
        qemu_coroutine_yield();
        s->waiting_for_io = false;
    }
}

typedef struct {
    int ret;
} MirrorExitData;

static void mirror_exit(BlockJob *job, void *opaque)
{
    MirrorBlockJob *s = container_of(job, MirrorBlockJob, common);
    MirrorExitData *data = opaque;
    AioContext *replace_aio_context = NULL;
    BlockDriverState *src = s->common.bs;

    /* Make sure that the source BDS doesn't go away before we called
     * block_job_completed(). */
    bdrv_ref(src);

    if (s->to_replace) {
        replace_aio_context = bdrv_get_aio_context(s->to_replace);
        aio_context_acquire(replace_aio_context);
    }

    if (s->should_complete && data->ret == 0) {
        BlockDriverState *to_replace = s->common.bs;
        if (s->to_replace) {
            to_replace = s->to_replace;
        }

        /* This was checked in mirror_start_job(), but meanwhile one of the
         * nodes could have been newly attached to a BlockBackend. */
        if (to_replace->blk && s->target->blk) {
            error_report("block job: Can't create node with two BlockBackends");
            data->ret = -EINVAL;
            goto out;
        }

        if (bdrv_get_flags(s->target) != bdrv_get_flags(to_replace)) {
            bdrv_reopen(s->target, bdrv_get_flags(to_replace), NULL);
        }
        bdrv_replace_in_backing_chain(to_replace, s->target);
    }

out:
    if (s->to_replace) {
        bdrv_op_unblock_all(s->to_replace, s->replace_blocker);
        error_free(s->replace_blocker);
        bdrv_unref(s->to_replace);
    }
    if (replace_aio_context) {
        aio_context_release(replace_aio_context);
    }
    g_free(s->replaces);
    bdrv_op_unblock_all(s->target, s->common.blocker);
    bdrv_unref(s->target);
    block_job_completed(&s->common, data->ret);
    g_free(data);
    bdrv_drained_end(src);
    bdrv_unref(src);
}

static void coroutine_fn mirror_run(void *opaque)
{
    MirrorBlockJob *s = opaque;
    MirrorExitData *data;
    BlockDriverState *bs = s->common.bs;
    int64_t sector_num, end, length;
    uint64_t last_pause_ns;
    BlockDriverInfo bdi;
    char backing_filename[2]; /* we only need 2 characters because we are only
                                 checking for a NULL string */
    int ret = 0;
    int n;

    if (block_job_is_cancelled(&s->common)) {
        goto immediate_exit;
    }

    s->bdev_length = bdrv_getlength(bs);
    if (s->bdev_length < 0) {
        ret = s->bdev_length;
        goto immediate_exit;
    } else if (s->bdev_length == 0) {
        /* Report BLOCK_JOB_READY and wait for complete. */
        block_job_event_ready(&s->common);
        s->synced = true;
        while (!block_job_is_cancelled(&s->common) && !s->should_complete) {
            block_job_yield(&s->common);
        }
        s->common.cancelled = false;
        goto immediate_exit;
    }

    length = DIV_ROUND_UP(s->bdev_length, s->granularity);
    s->in_flight_bitmap = bitmap_new(length);

    /* If we have no backing file yet in the destination, we cannot let
     * the destination do COW.  Instead, we copy sectors around the
     * dirty data if needed.  We need a bitmap to do that.
     */
    bdrv_get_backing_filename(s->target, backing_filename,
                              sizeof(backing_filename));
    if (backing_filename[0] && !s->target->backing) {
        ret = bdrv_get_info(s->target, &bdi);
        if (ret < 0) {
            goto immediate_exit;
        }
        if (s->granularity < bdi.cluster_size) {
            s->buf_size = MAX(s->buf_size, bdi.cluster_size);
            s->cow_bitmap = bitmap_new(length);
        }
    }

    end = s->bdev_length / BDRV_SECTOR_SIZE;
    s->buf = qemu_try_blockalign(bs, s->buf_size);
    if (s->buf == NULL) {
        ret = -ENOMEM;
        goto immediate_exit;
    }

    mirror_free_init(s);

    last_pause_ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
    if (!s->is_none_mode) {
        /* First part, loop on the sectors and initialize the dirty bitmap.  */
        BlockDriverState *base = s->base;
        bool mark_all_dirty = s->base == NULL && !bdrv_has_zero_init(s->target);

        for (sector_num = 0; sector_num < end; ) {
            /* Just to make sure we are not exceeding int limit. */
            int nb_sectors = MIN(INT_MAX >> BDRV_SECTOR_BITS,
                                 end - sector_num);
            int64_t now = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);

            if (now - last_pause_ns > SLICE_TIME) {
                last_pause_ns = now;
                block_job_sleep_ns(&s->common, QEMU_CLOCK_REALTIME, 0);
            }

            if (block_job_is_cancelled(&s->common)) {
                goto immediate_exit;
            }

            ret = bdrv_is_allocated_above(bs, base, sector_num, nb_sectors, &n);

            if (ret < 0) {
                goto immediate_exit;
            }

            assert(n > 0);
            if (ret == 1 || mark_all_dirty) {
                bdrv_set_dirty_bitmap(s->dirty_bitmap, sector_num, n);
            }
            sector_num += n;
        }
    }

    bdrv_dirty_iter_init(s->dirty_bitmap, &s->hbi);
    for (;;) {
        uint64_t delay_ns = 0;
        int64_t cnt;
        bool should_complete;

        if (s->ret < 0) {
            ret = s->ret;
            goto immediate_exit;
        }

        cnt = bdrv_get_dirty_count(s->dirty_bitmap);
        /* s->common.offset contains the number of bytes already processed so
         * far, cnt is the number of dirty sectors remaining and
         * s->sectors_in_flight is the number of sectors currently being
         * processed; together those are the current total operation length */
        s->common.len = s->common.offset +
                        (cnt + s->sectors_in_flight) * BDRV_SECTOR_SIZE;

        /* Note that even when no rate limit is applied we need to yield
         * periodically with no pending I/O so that bdrv_drain_all() returns.
         * We do so every SLICE_TIME nanoseconds, or when there is an error,
         * or when the source is clean, whichever comes first.
         */
        if (qemu_clock_get_ns(QEMU_CLOCK_REALTIME) - last_pause_ns < SLICE_TIME &&
            s->common.iostatus == BLOCK_DEVICE_IO_STATUS_OK) {
            if (s->in_flight == MAX_IN_FLIGHT || s->buf_free_count == 0 ||
                (cnt == 0 && s->in_flight > 0)) {
                trace_mirror_yield(s, s->in_flight, s->buf_free_count, cnt);
                s->waiting_for_io = true;
                qemu_coroutine_yield();
                s->waiting_for_io = false;
                continue;
            } else if (cnt != 0) {
                delay_ns = mirror_iteration(s);
            }
        }

        should_complete = false;
        if (s->in_flight == 0 && cnt == 0) {
            trace_mirror_before_flush(s);
            ret = bdrv_flush(s->target);
            if (ret < 0) {
                if (mirror_error_action(s, false, -ret) ==
                    BLOCK_ERROR_ACTION_REPORT) {
                    goto immediate_exit;
                }
            } else {
                /* We're out of the streaming phase.  From now on, if the job
                 * is cancelled we will actually complete all pending I/O and
                 * report completion.  This way, block-job-cancel will leave
                 * the target in a consistent state.
                 */
                if (!s->synced) {
                    block_job_event_ready(&s->common);
                    s->synced = true;
                }

                should_complete = s->should_complete ||
                    block_job_is_cancelled(&s->common);
                cnt = bdrv_get_dirty_count(s->dirty_bitmap);
            }
        }

        if (cnt == 0 && should_complete) {
            /* The dirty bitmap is not updated while operations are pending.
             * If we're about to exit, wait for pending operations before
             * calling bdrv_get_dirty_count(bs), or we may exit while the
             * source has dirty data to copy!
             *
             * Note that I/O can be submitted by the guest while
             * mirror_populate runs.
             */
            trace_mirror_before_drain(s, cnt);
            bdrv_drain(bs);
            cnt = bdrv_get_dirty_count(s->dirty_bitmap);
        }

        ret = 0;
        trace_mirror_before_sleep(s, cnt, s->synced, delay_ns);
        if (!s->synced) {
            block_job_sleep_ns(&s->common, QEMU_CLOCK_REALTIME, delay_ns);
            if (block_job_is_cancelled(&s->common)) {
                break;
            }
        } else if (!should_complete) {
            delay_ns = (s->in_flight == 0 && cnt == 0 ? SLICE_TIME : 0);
            block_job_sleep_ns(&s->common, QEMU_CLOCK_REALTIME, delay_ns);
        } else if (cnt == 0) {
            /* The two disks are in sync.  Exit and report successful
             * completion.
             */
            assert(QLIST_EMPTY(&bs->tracked_requests));
            s->common.cancelled = false;
            break;
        }
        last_pause_ns = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
    }

immediate_exit:
    if (s->in_flight > 0) {
        /* We get here only if something went wrong.  Either the job failed,
         * or it was cancelled prematurely so that we do not guarantee that
         * the target is a copy of the source.
         */
        assert(ret < 0 || (!s->synced && block_job_is_cancelled(&s->common)));
        mirror_drain(s);
    }

    assert(s->in_flight == 0);
    qemu_vfree(s->buf);
    g_free(s->cow_bitmap);
    g_free(s->in_flight_bitmap);
    bdrv_release_dirty_bitmap(bs, s->dirty_bitmap);
    if (s->target->blk) {
        blk_iostatus_disable(s->target->blk);
    }

    data = g_malloc(sizeof(*data));
    data->ret = ret;
    /* Before we switch to target in mirror_exit, make sure data doesn't
     * change. */
    bdrv_drained_begin(s->common.bs);
    block_job_defer_to_main_loop(&s->common, mirror_exit, data);
}

static void mirror_set_speed(BlockJob *job, int64_t speed, Error **errp)
{
    MirrorBlockJob *s = container_of(job, MirrorBlockJob, common);

    if (speed < 0) {
        error_setg(errp, QERR_INVALID_PARAMETER, "speed");
        return;
    }
    ratelimit_set_speed(&s->limit, speed / BDRV_SECTOR_SIZE, SLICE_TIME);
}

static void mirror_iostatus_reset(BlockJob *job)
{
    MirrorBlockJob *s = container_of(job, MirrorBlockJob, common);

    if (s->target->blk) {
        blk_iostatus_reset(s->target->blk);
    }
}

static void mirror_complete(BlockJob *job, Error **errp)
{
    MirrorBlockJob *s = container_of(job, MirrorBlockJob, common);
    Error *local_err = NULL;
    int ret;

    ret = bdrv_open_backing_file(s->target, NULL, "backing", &local_err);
    if (ret < 0) {
        error_propagate(errp, local_err);
        return;
    }
    if (!s->synced) {
        error_setg(errp, QERR_BLOCK_JOB_NOT_READY, job->id);
        return;
    }

    /* check the target bs is not blocked and block all operations on it */
    if (s->replaces) {
        AioContext *replace_aio_context;

        s->to_replace = bdrv_find_node(s->replaces);
        if (!s->to_replace) {
            error_setg(errp, "Node name '%s' not found", s->replaces);
            return;
        }

        replace_aio_context = bdrv_get_aio_context(s->to_replace);
        aio_context_acquire(replace_aio_context);

        error_setg(&s->replace_blocker,
                   "block device is in use by block-job-complete");
        bdrv_op_block_all(s->to_replace, s->replace_blocker);
        bdrv_ref(s->to_replace);

        aio_context_release(replace_aio_context);
    }

    s->should_complete = true;
    block_job_enter(&s->common);
}

static const BlockJobDriver mirror_job_driver = {
    .instance_size = sizeof(MirrorBlockJob),
    .job_type      = BLOCK_JOB_TYPE_MIRROR,
    .set_speed     = mirror_set_speed,
    .iostatus_reset= mirror_iostatus_reset,
    .complete      = mirror_complete,
};

static const BlockJobDriver commit_active_job_driver = {
    .instance_size = sizeof(MirrorBlockJob),
    .job_type      = BLOCK_JOB_TYPE_COMMIT,
    .set_speed     = mirror_set_speed,
    .iostatus_reset
                   = mirror_iostatus_reset,
    .complete      = mirror_complete,
};

static void mirror_start_job(BlockDriverState *bs, BlockDriverState *target,
                             const char *replaces,
                             int64_t speed, uint32_t granularity,
                             int64_t buf_size,
                             BlockdevOnError on_source_error,
                             BlockdevOnError on_target_error,
                             bool unmap,
                             BlockCompletionFunc *cb,
                             void *opaque, Error **errp,
                             const BlockJobDriver *driver,
                             bool is_none_mode, BlockDriverState *base)
{
    MirrorBlockJob *s;
    BlockDriverState *replaced_bs;

    if (granularity == 0) {
        granularity = bdrv_get_default_bitmap_granularity(target);
    }

    assert ((granularity & (granularity - 1)) == 0);

    if ((on_source_error == BLOCKDEV_ON_ERROR_STOP ||
         on_source_error == BLOCKDEV_ON_ERROR_ENOSPC) &&
        (!bs->blk || !blk_iostatus_is_enabled(bs->blk))) {
        error_setg(errp, QERR_INVALID_PARAMETER, "on-source-error");
        return;
    }

    if (buf_size < 0) {
        error_setg(errp, "Invalid parameter 'buf-size'");
        return;
    }

    if (buf_size == 0) {
        buf_size = DEFAULT_MIRROR_BUF_SIZE;
    }

    /* We can't support this case as long as the block layer can't handle
     * multiple BlockBackends per BlockDriverState. */
    if (replaces) {
        replaced_bs = bdrv_lookup_bs(replaces, replaces, errp);
        if (replaced_bs == NULL) {
            return;
        }
    } else {
        replaced_bs = bs;
    }
    if (replaced_bs->blk && target->blk) {
        error_setg(errp, "Can't create node with two BlockBackends");
        return;
    }

    s = block_job_create(driver, bs, speed, cb, opaque, errp);
    if (!s) {
        return;
    }

    s->replaces = g_strdup(replaces);
    s->on_source_error = on_source_error;
    s->on_target_error = on_target_error;
    s->target = target;
    s->is_none_mode = is_none_mode;
    s->base = base;
    s->granularity = granularity;
    s->buf_size = ROUND_UP(buf_size, granularity);
    s->unmap = unmap;

    s->dirty_bitmap = bdrv_create_dirty_bitmap(bs, granularity, NULL, errp);
    if (!s->dirty_bitmap) {
        g_free(s->replaces);
        block_job_unref(&s->common);
        return;
    }

    bdrv_op_block_all(s->target, s->common.blocker);

    bdrv_set_enable_write_cache(s->target, true);
    if (s->target->blk) {
        blk_set_on_error(s->target->blk, on_target_error, on_target_error);
        blk_iostatus_enable(s->target->blk);
    }
    s->common.co = qemu_coroutine_create(mirror_run);
    trace_mirror_start(bs, s, s->common.co, opaque);
    qemu_coroutine_enter(s->common.co, s);
}

void mirror_start(BlockDriverState *bs, BlockDriverState *target,
                  const char *replaces,
                  int64_t speed, uint32_t granularity, int64_t buf_size,
                  MirrorSyncMode mode, BlockdevOnError on_source_error,
                  BlockdevOnError on_target_error,
                  bool unmap,
                  BlockCompletionFunc *cb,
                  void *opaque, Error **errp)
{
    bool is_none_mode;
    BlockDriverState *base;

    if (mode == MIRROR_SYNC_MODE_INCREMENTAL) {
        error_setg(errp, "Sync mode 'incremental' not supported");
        return;
    }
    is_none_mode = mode == MIRROR_SYNC_MODE_NONE;
    base = mode == MIRROR_SYNC_MODE_TOP ? backing_bs(bs) : NULL;
    mirror_start_job(bs, target, replaces,
                     speed, granularity, buf_size,
                     on_source_error, on_target_error, unmap, cb, opaque, errp,
                     &mirror_job_driver, is_none_mode, base);
}

void commit_active_start(BlockDriverState *bs, BlockDriverState *base,
                         int64_t speed,
                         BlockdevOnError on_error,
                         BlockCompletionFunc *cb,
                         void *opaque, Error **errp)
{
    int64_t length, base_length;
    int orig_base_flags;
    int ret;
    Error *local_err = NULL;

    orig_base_flags = bdrv_get_flags(base);

    if (bdrv_reopen(base, bs->open_flags, errp)) {
        return;
    }

    length = bdrv_getlength(bs);
    if (length < 0) {
        error_setg_errno(errp, -length,
                         "Unable to determine length of %s", bs->filename);
        goto error_restore_flags;
    }

    base_length = bdrv_getlength(base);
    if (base_length < 0) {
        error_setg_errno(errp, -base_length,
                         "Unable to determine length of %s", base->filename);
        goto error_restore_flags;
    }

    if (length > base_length) {
        ret = bdrv_truncate(base, length);
        if (ret < 0) {
            error_setg_errno(errp, -ret,
                            "Top image %s is larger than base image %s, and "
                             "resize of base image failed",
                             bs->filename, base->filename);
            goto error_restore_flags;
        }
    }

    bdrv_ref(base);
    mirror_start_job(bs, base, NULL, speed, 0, 0,
                     on_error, on_error, false, cb, opaque, &local_err,
                     &commit_active_job_driver, false, base);
    if (local_err) {
        error_propagate(errp, local_err);
        goto error_restore_flags;
    }

    return;

error_restore_flags:
    /* ignore error and errp for bdrv_reopen, because we want to propagate
     * the original error */
    bdrv_reopen(base, orig_base_flags, NULL);
    return;
}
