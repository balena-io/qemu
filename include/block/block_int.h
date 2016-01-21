/*
 * QEMU System Emulator block driver
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef BLOCK_INT_H
#define BLOCK_INT_H

#include "block/accounting.h"
#include "block/block.h"
#include "block/throttle-groups.h"
#include "qemu/option.h"
#include "qemu/queue.h"
#include "qemu/coroutine.h"
#include "qemu/timer.h"
#include "qapi-types.h"
#include "qemu/hbitmap.h"
#include "block/snapshot.h"
#include "qemu/main-loop.h"
#include "qemu/throttle.h"

#define BLOCK_FLAG_ENCRYPT          1
#define BLOCK_FLAG_COMPAT6          4
#define BLOCK_FLAG_LAZY_REFCOUNTS   8

#define BLOCK_OPT_SIZE              "size"
#define BLOCK_OPT_ENCRYPT           "encryption"
#define BLOCK_OPT_COMPAT6           "compat6"
#define BLOCK_OPT_BACKING_FILE      "backing_file"
#define BLOCK_OPT_BACKING_FMT       "backing_fmt"
#define BLOCK_OPT_CLUSTER_SIZE      "cluster_size"
#define BLOCK_OPT_TABLE_SIZE        "table_size"
#define BLOCK_OPT_PREALLOC          "preallocation"
#define BLOCK_OPT_SUBFMT            "subformat"
#define BLOCK_OPT_COMPAT_LEVEL      "compat"
#define BLOCK_OPT_LAZY_REFCOUNTS    "lazy_refcounts"
#define BLOCK_OPT_ADAPTER_TYPE      "adapter_type"
#define BLOCK_OPT_REDUNDANCY        "redundancy"
#define BLOCK_OPT_NOCOW             "nocow"
#define BLOCK_OPT_OBJECT_SIZE       "object_size"
#define BLOCK_OPT_REFCOUNT_BITS     "refcount_bits"

#define BLOCK_PROBE_BUF_SIZE        512

enum BdrvTrackedRequestType {
    BDRV_TRACKED_READ,
    BDRV_TRACKED_WRITE,
    BDRV_TRACKED_FLUSH,
    BDRV_TRACKED_IOCTL,
    BDRV_TRACKED_DISCARD,
};

typedef struct BdrvTrackedRequest {
    BlockDriverState *bs;
    int64_t offset;
    unsigned int bytes;
    enum BdrvTrackedRequestType type;

    bool serialising;
    int64_t overlap_offset;
    unsigned int overlap_bytes;

    QLIST_ENTRY(BdrvTrackedRequest) list;
    Coroutine *co; /* owner, used for deadlock detection */
    CoQueue wait_queue; /* coroutines blocked on this request */

    struct BdrvTrackedRequest *waiting_for;
} BdrvTrackedRequest;

struct BlockDriver {
    const char *format_name;
    int instance_size;

    /* set to true if the BlockDriver is a block filter */
    bool is_filter;
    /* for snapshots block filter like Quorum can implement the
     * following recursive callback.
     * It's purpose is to recurse on the filter children while calling
     * bdrv_recurse_is_first_non_filter on them.
     * For a sample implementation look in the future Quorum block filter.
     */
    bool (*bdrv_recurse_is_first_non_filter)(BlockDriverState *bs,
                                             BlockDriverState *candidate);

    int (*bdrv_probe)(const uint8_t *buf, int buf_size, const char *filename);
    int (*bdrv_probe_device)(const char *filename);

    /* Any driver implementing this callback is expected to be able to handle
     * NULL file names in its .bdrv_open() implementation */
    void (*bdrv_parse_filename)(const char *filename, QDict *options, Error **errp);
    /* Drivers not implementing bdrv_parse_filename nor bdrv_open should have
     * this field set to true, except ones that are defined only by their
     * child's bs.
     * An example of the last type will be the quorum block driver.
     */
    bool bdrv_needs_filename;

    /* Set if a driver can support backing files */
    bool supports_backing;

    /* For handling image reopen for split or non-split files */
    int (*bdrv_reopen_prepare)(BDRVReopenState *reopen_state,
                               BlockReopenQueue *queue, Error **errp);
    void (*bdrv_reopen_commit)(BDRVReopenState *reopen_state);
    void (*bdrv_reopen_abort)(BDRVReopenState *reopen_state);
    void (*bdrv_join_options)(QDict *options, QDict *old_options);

    int (*bdrv_open)(BlockDriverState *bs, QDict *options, int flags,
                     Error **errp);
    int (*bdrv_file_open)(BlockDriverState *bs, QDict *options, int flags,
                          Error **errp);
    int (*bdrv_read)(BlockDriverState *bs, int64_t sector_num,
                     uint8_t *buf, int nb_sectors);
    int (*bdrv_write)(BlockDriverState *bs, int64_t sector_num,
                      const uint8_t *buf, int nb_sectors);
    void (*bdrv_close)(BlockDriverState *bs);
    int (*bdrv_create)(const char *filename, QemuOpts *opts, Error **errp);
    int (*bdrv_set_key)(BlockDriverState *bs, const char *key);
    int (*bdrv_make_empty)(BlockDriverState *bs);

    void (*bdrv_refresh_filename)(BlockDriverState *bs, QDict *options);

    /* aio */
    BlockAIOCB *(*bdrv_aio_readv)(BlockDriverState *bs,
        int64_t sector_num, QEMUIOVector *qiov, int nb_sectors,
        BlockCompletionFunc *cb, void *opaque);
    BlockAIOCB *(*bdrv_aio_writev)(BlockDriverState *bs,
        int64_t sector_num, QEMUIOVector *qiov, int nb_sectors,
        BlockCompletionFunc *cb, void *opaque);
    BlockAIOCB *(*bdrv_aio_flush)(BlockDriverState *bs,
        BlockCompletionFunc *cb, void *opaque);
    BlockAIOCB *(*bdrv_aio_discard)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors,
        BlockCompletionFunc *cb, void *opaque);

    int coroutine_fn (*bdrv_co_readv)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors, QEMUIOVector *qiov);
    int coroutine_fn (*bdrv_co_writev)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors, QEMUIOVector *qiov);
    /*
     * Efficiently zero a region of the disk image.  Typically an image format
     * would use a compact metadata representation to implement this.  This
     * function pointer may be NULL and .bdrv_co_writev() will be called
     * instead.
     */
    int coroutine_fn (*bdrv_co_write_zeroes)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors, BdrvRequestFlags flags);
    int coroutine_fn (*bdrv_co_discard)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors);
    int64_t coroutine_fn (*bdrv_co_get_block_status)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors, int *pnum);

    /*
     * Invalidate any cached meta-data.
     */
    void (*bdrv_invalidate_cache)(BlockDriverState *bs, Error **errp);

    /*
     * Flushes all data that was already written to the OS all the way down to
     * the disk (for example raw-posix calls fsync()).
     */
    int coroutine_fn (*bdrv_co_flush_to_disk)(BlockDriverState *bs);

    /*
     * Flushes all internal caches to the OS. The data may still sit in a
     * writeback cache of the host OS, but it will survive a crash of the qemu
     * process.
     */
    int coroutine_fn (*bdrv_co_flush_to_os)(BlockDriverState *bs);

    const char *protocol_name;
    int (*bdrv_truncate)(BlockDriverState *bs, int64_t offset);

    int64_t (*bdrv_getlength)(BlockDriverState *bs);
    bool has_variable_length;
    int64_t (*bdrv_get_allocated_file_size)(BlockDriverState *bs);

    int (*bdrv_write_compressed)(BlockDriverState *bs, int64_t sector_num,
                                 const uint8_t *buf, int nb_sectors);

    int (*bdrv_snapshot_create)(BlockDriverState *bs,
                                QEMUSnapshotInfo *sn_info);
    int (*bdrv_snapshot_goto)(BlockDriverState *bs,
                              const char *snapshot_id);
    int (*bdrv_snapshot_delete)(BlockDriverState *bs,
                                const char *snapshot_id,
                                const char *name,
                                Error **errp);
    int (*bdrv_snapshot_list)(BlockDriverState *bs,
                              QEMUSnapshotInfo **psn_info);
    int (*bdrv_snapshot_load_tmp)(BlockDriverState *bs,
                                  const char *snapshot_id,
                                  const char *name,
                                  Error **errp);
    int (*bdrv_get_info)(BlockDriverState *bs, BlockDriverInfo *bdi);
    ImageInfoSpecific *(*bdrv_get_specific_info)(BlockDriverState *bs);

    int (*bdrv_save_vmstate)(BlockDriverState *bs, QEMUIOVector *qiov,
                             int64_t pos);
    int (*bdrv_load_vmstate)(BlockDriverState *bs, uint8_t *buf,
                             int64_t pos, int size);

    int (*bdrv_change_backing_file)(BlockDriverState *bs,
        const char *backing_file, const char *backing_fmt);

    /* removable device specific */
    bool (*bdrv_is_inserted)(BlockDriverState *bs);
    int (*bdrv_media_changed)(BlockDriverState *bs);
    void (*bdrv_eject)(BlockDriverState *bs, bool eject_flag);
    void (*bdrv_lock_medium)(BlockDriverState *bs, bool locked);

    /* to control generic scsi devices */
    BlockAIOCB *(*bdrv_aio_ioctl)(BlockDriverState *bs,
        unsigned long int req, void *buf,
        BlockCompletionFunc *cb, void *opaque);

    /* List of options for creating images, terminated by name == NULL */
    QemuOptsList *create_opts;

    /*
     * Returns 0 for completed check, -errno for internal errors.
     * The check results are stored in result.
     */
    int (*bdrv_check)(BlockDriverState* bs, BdrvCheckResult *result,
        BdrvCheckMode fix);

    int (*bdrv_amend_options)(BlockDriverState *bs, QemuOpts *opts,
                              BlockDriverAmendStatusCB *status_cb,
                              void *cb_opaque);

    void (*bdrv_debug_event)(BlockDriverState *bs, BlkdebugEvent event);

    /* TODO Better pass a option string/QDict/QemuOpts to add any rule? */
    int (*bdrv_debug_breakpoint)(BlockDriverState *bs, const char *event,
        const char *tag);
    int (*bdrv_debug_remove_breakpoint)(BlockDriverState *bs,
        const char *tag);
    int (*bdrv_debug_resume)(BlockDriverState *bs, const char *tag);
    bool (*bdrv_debug_is_suspended)(BlockDriverState *bs, const char *tag);

    void (*bdrv_refresh_limits)(BlockDriverState *bs, Error **errp);

    /*
     * Returns 1 if newly created images are guaranteed to contain only
     * zeros, 0 otherwise.
     */
    int (*bdrv_has_zero_init)(BlockDriverState *bs);

    /* Remove fd handlers, timers, and other event loop callbacks so the event
     * loop is no longer in use.  Called with no in-flight requests and in
     * depth-first traversal order with parents before child nodes.
     */
    void (*bdrv_detach_aio_context)(BlockDriverState *bs);

    /* Add fd handlers, timers, and other event loop callbacks so I/O requests
     * can be processed again.  Called with no in-flight requests and in
     * depth-first traversal order with child nodes before parent nodes.
     */
    void (*bdrv_attach_aio_context)(BlockDriverState *bs,
                                    AioContext *new_context);

    /* io queue for linux-aio */
    void (*bdrv_io_plug)(BlockDriverState *bs);
    void (*bdrv_io_unplug)(BlockDriverState *bs);
    void (*bdrv_flush_io_queue)(BlockDriverState *bs);

    /**
     * Try to get @bs's logical and physical block size.
     * On success, store them in @bsz and return zero.
     * On failure, return negative errno.
     */
    int (*bdrv_probe_blocksizes)(BlockDriverState *bs, BlockSizes *bsz);
    /**
     * Try to get @bs's geometry (cyls, heads, sectors)
     * On success, store them in @geo and return 0.
     * On failure return -errno.
     * Only drivers that want to override guest geometry implement this
     * callback; see hd_geometry_guess().
     */
    int (*bdrv_probe_geometry)(BlockDriverState *bs, HDGeometry *geo);

    /**
     * Drain and stop any internal sources of requests in the driver, and
     * remain so until next I/O callback (e.g. bdrv_co_writev) is called.
     */
    void (*bdrv_drain)(BlockDriverState *bs);

    QLIST_ENTRY(BlockDriver) list;
};

typedef struct BlockLimits {
    /* maximum number of sectors that can be discarded at once */
    int max_discard;

    /* optimal alignment for discard requests in sectors */
    int64_t discard_alignment;

    /* maximum number of sectors that can zeroized at once */
    int max_write_zeroes;

    /* optimal alignment for write zeroes requests in sectors */
    int64_t write_zeroes_alignment;

    /* optimal transfer length in sectors */
    int opt_transfer_length;

    /* maximal transfer length in sectors */
    int max_transfer_length;

    /* memory alignment so that no bounce buffer is needed */
    size_t min_mem_alignment;

    /* memory alignment for bounce buffer */
    size_t opt_mem_alignment;

    /* maximum number of iovec elements */
    int max_iov;
} BlockLimits;

typedef struct BdrvOpBlocker BdrvOpBlocker;

typedef struct BdrvAioNotifier {
    void (*attached_aio_context)(AioContext *new_context, void *opaque);
    void (*detach_aio_context)(void *opaque);

    void *opaque;

    QLIST_ENTRY(BdrvAioNotifier) list;
} BdrvAioNotifier;

struct BdrvChildRole {
    void (*inherit_options)(int *child_flags, QDict *child_options,
                            int parent_flags, QDict *parent_options);
};

extern const BdrvChildRole child_file;
extern const BdrvChildRole child_format;

struct BdrvChild {
    BlockDriverState *bs;
    char *name;
    const BdrvChildRole *role;
    QLIST_ENTRY(BdrvChild) next;
    QLIST_ENTRY(BdrvChild) next_parent;
};

/*
 * Note: the function bdrv_append() copies and swaps contents of
 * BlockDriverStates, so if you add new fields to this struct, please
 * inspect bdrv_append() to determine if the new fields need to be
 * copied as well.
 */
struct BlockDriverState {
    int64_t total_sectors; /* if we are reading a disk image, give its
                              size in sectors */
    int read_only; /* if true, the media is read only */
    int open_flags; /* flags used to open the file, re-used for re-open */
    int encrypted; /* if true, the media is encrypted */
    int valid_key; /* if true, a valid encryption key has been set */
    int sg;        /* if true, the device is a /dev/sg* */
    int copy_on_read; /* if true, copy read backing sectors into image
                         note this is a reference count */
    bool probed;

    BlockDriver *drv; /* NULL means no media */
    void *opaque;

    BlockBackend *blk;          /* owning backend, if any */

    AioContext *aio_context; /* event loop used for fd handlers, timers, etc */
    /* long-running tasks intended to always use the same AioContext as this
     * BDS may register themselves in this list to be notified of changes
     * regarding this BDS's context */
    QLIST_HEAD(, BdrvAioNotifier) aio_notifiers;

    char filename[PATH_MAX];
    char backing_file[PATH_MAX]; /* if non zero, the image is a diff of
                                    this file image */
    char backing_format[16]; /* if non-zero and backing_file exists */

    QDict *full_open_options;
    char exact_filename[PATH_MAX];

    BdrvChild *backing;
    BdrvChild *file;

    NotifierList close_notifiers;

    /* Callback before write request is processed */
    NotifierWithReturnList before_write_notifiers;

    /* number of in-flight serialising requests */
    unsigned int serialising_in_flight;

    /* I/O throttling.
     * throttle_state tells us if this BDS has I/O limits configured.
     * io_limits_enabled tells us if they are currently being
     * enforced, but it can be temporarily set to false */
    CoQueue      throttled_reqs[2];
    bool         io_limits_enabled;
    /* The following fields are protected by the ThrottleGroup lock.
     * See the ThrottleGroup documentation for details. */
    ThrottleState *throttle_state;
    ThrottleTimers throttle_timers;
    unsigned       pending_reqs[2];
    QLIST_ENTRY(BlockDriverState) round_robin;

    /* Offset after the highest byte written to */
    uint64_t wr_highest_offset;

    /* I/O Limits */
    BlockLimits bl;

    /* Whether produces zeros when read beyond eof */
    bool zero_beyond_eof;

    /* Alignment requirement for offset/length of I/O requests */
    unsigned int request_alignment;

    /* do we need to tell the quest if we have a volatile write cache? */
    int enable_write_cache;

    /* the following member gives a name to every node on the bs graph. */
    char node_name[32];
    /* element of the list of named nodes building the graph */
    QTAILQ_ENTRY(BlockDriverState) node_list;
    /* element of the list of "drives" the guest sees */
    QTAILQ_ENTRY(BlockDriverState) device_list;
    QLIST_HEAD(, BdrvDirtyBitmap) dirty_bitmaps;
    int refcnt;

    QLIST_HEAD(, BdrvTrackedRequest) tracked_requests;

    /* operation blockers */
    QLIST_HEAD(, BdrvOpBlocker) op_blockers[BLOCK_OP_TYPE_MAX];

    /* long-running background operation */
    BlockJob *job;

    /* The node that this node inherited default options from (and a reopen on
     * which can affect this node by changing these defaults). This is always a
     * parent node of this node. */
    BlockDriverState *inherits_from;
    QLIST_HEAD(, BdrvChild) children;
    QLIST_HEAD(, BdrvChild) parents;

    QDict *options;
    QDict *explicit_options;
    BlockdevDetectZeroesOptions detect_zeroes;

    /* The error object in use for blocking operations on backing_hd */
    Error *backing_blocker;

    /* threshold limit for writes, in bytes. "High water mark". */
    uint64_t write_threshold_offset;
    NotifierWithReturn write_threshold_notifier;

    int quiesce_counter;
};

struct BlockBackendRootState {
    int open_flags;
    bool read_only;
    BlockdevDetectZeroesOptions detect_zeroes;

    char *throttle_group;
    ThrottleState *throttle_state;
};

static inline BlockDriverState *backing_bs(BlockDriverState *bs)
{
    return bs->backing ? bs->backing->bs : NULL;
}


/* Essential block drivers which must always be statically linked into qemu, and
 * which therefore can be accessed without using bdrv_find_format() */
extern BlockDriver bdrv_file;
extern BlockDriver bdrv_raw;
extern BlockDriver bdrv_qcow2;

extern QTAILQ_HEAD(BdrvStates, BlockDriverState) bdrv_states;

/**
 * bdrv_setup_io_funcs:
 *
 * Prepare a #BlockDriver for I/O request processing by populating
 * unimplemented coroutine and AIO interfaces with generic wrapper functions
 * that fall back to implemented interfaces.
 */
void bdrv_setup_io_funcs(BlockDriver *bdrv);

int get_tmp_filename(char *filename, int size);
BlockDriver *bdrv_probe_all(const uint8_t *buf, int buf_size,
                            const char *filename);

void bdrv_set_io_limits(BlockDriverState *bs,
                        ThrottleConfig *cfg);


/**
 * bdrv_add_before_write_notifier:
 *
 * Register a callback that is invoked before write requests are processed but
 * after any throttling or waiting for overlapping requests.
 */
void bdrv_add_before_write_notifier(BlockDriverState *bs,
                                    NotifierWithReturn *notifier);

/**
 * bdrv_detach_aio_context:
 *
 * May be called from .bdrv_detach_aio_context() to detach children from the
 * current #AioContext.  This is only needed by block drivers that manage their
 * own children.  Both ->file and ->backing are automatically handled and
 * block drivers should not call this function on them explicitly.
 */
void bdrv_detach_aio_context(BlockDriverState *bs);

/**
 * bdrv_attach_aio_context:
 *
 * May be called from .bdrv_attach_aio_context() to attach children to the new
 * #AioContext.  This is only needed by block drivers that manage their own
 * children.  Both ->file and ->backing are automatically handled and block
 * drivers should not call this function on them explicitly.
 */
void bdrv_attach_aio_context(BlockDriverState *bs,
                             AioContext *new_context);

/**
 * bdrv_add_aio_context_notifier:
 *
 * If a long-running job intends to be always run in the same AioContext as a
 * certain BDS, it may use this function to be notified of changes regarding the
 * association of the BDS to an AioContext.
 *
 * attached_aio_context() is called after the target BDS has been attached to a
 * new AioContext; detach_aio_context() is called before the target BDS is being
 * detached from its old AioContext.
 */
void bdrv_add_aio_context_notifier(BlockDriverState *bs,
        void (*attached_aio_context)(AioContext *new_context, void *opaque),
        void (*detach_aio_context)(void *opaque), void *opaque);

/**
 * bdrv_remove_aio_context_notifier:
 *
 * Unsubscribe of change notifications regarding the BDS's AioContext. The
 * parameters given here have to be the same as those given to
 * bdrv_add_aio_context_notifier().
 */
void bdrv_remove_aio_context_notifier(BlockDriverState *bs,
                                      void (*aio_context_attached)(AioContext *,
                                                                   void *),
                                      void (*aio_context_detached)(void *),
                                      void *opaque);

#ifdef _WIN32
int is_windows_drive(const char *filename);
#endif

/**
 * stream_start:
 * @bs: Block device to operate on.
 * @base: Block device that will become the new base, or %NULL to
 * flatten the whole backing file chain onto @bs.
 * @base_id: The file name that will be written to @bs as the new
 * backing file if the job completes.  Ignored if @base is %NULL.
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @on_error: The action to take upon error.
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @errp: Error object.
 *
 * Start a streaming operation on @bs.  Clusters that are unallocated
 * in @bs, but allocated in any image between @base and @bs (both
 * exclusive) will be written to @bs.  At the end of a successful
 * streaming job, the backing file of @bs will be changed to
 * @base_id in the written image and to @base in the live BlockDriverState.
 */
void stream_start(BlockDriverState *bs, BlockDriverState *base,
                  const char *base_id, int64_t speed, BlockdevOnError on_error,
                  BlockCompletionFunc *cb,
                  void *opaque, Error **errp);

/**
 * commit_start:
 * @bs: Active block device.
 * @top: Top block device to be committed.
 * @base: Block device that will be written into, and become the new top.
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @on_error: The action to take upon error.
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @backing_file_str: String to use as the backing file in @top's overlay
 * @errp: Error object.
 *
 */
void commit_start(BlockDriverState *bs, BlockDriverState *base,
                 BlockDriverState *top, int64_t speed,
                 BlockdevOnError on_error, BlockCompletionFunc *cb,
                 void *opaque, const char *backing_file_str, Error **errp);
/**
 * commit_active_start:
 * @bs: Active block device to be committed.
 * @base: Block device that will be written into, and become the new top.
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @on_error: The action to take upon error.
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @errp: Error object.
 *
 */
void commit_active_start(BlockDriverState *bs, BlockDriverState *base,
                         int64_t speed,
                         BlockdevOnError on_error,
                         BlockCompletionFunc *cb,
                         void *opaque, Error **errp);
/*
 * mirror_start:
 * @bs: Block device to operate on.
 * @target: Block device to write to.
 * @replaces: Block graph node name to replace once the mirror is done. Can
 *            only be used when full mirroring is selected.
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @granularity: The chosen granularity for the dirty bitmap.
 * @buf_size: The amount of data that can be in flight at one time.
 * @mode: Whether to collapse all images in the chain to the target.
 * @on_source_error: The action to take upon error reading from the source.
 * @on_target_error: The action to take upon error writing to the target.
 * @unmap: Whether to unmap target where source sectors only contain zeroes.
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @errp: Error object.
 *
 * Start a mirroring operation on @bs.  Clusters that are allocated
 * in @bs will be written to @bs until the job is cancelled or
 * manually completed.  At the end of a successful mirroring job,
 * @bs will be switched to read from @target.
 */
void mirror_start(BlockDriverState *bs, BlockDriverState *target,
                  const char *replaces,
                  int64_t speed, uint32_t granularity, int64_t buf_size,
                  MirrorSyncMode mode, BlockdevOnError on_source_error,
                  BlockdevOnError on_target_error,
                  bool unmap,
                  BlockCompletionFunc *cb,
                  void *opaque, Error **errp);

/*
 * backup_start:
 * @bs: Block device to operate on.
 * @target: Block device to write to.
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @sync_mode: What parts of the disk image should be copied to the destination.
 * @sync_bitmap: The dirty bitmap if sync_mode is MIRROR_SYNC_MODE_INCREMENTAL.
 * @on_source_error: The action to take upon error reading from the source.
 * @on_target_error: The action to take upon error writing to the target.
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @txn: Transaction that this job is part of (may be NULL).
 *
 * Start a backup operation on @bs.  Clusters in @bs are written to @target
 * until the job is cancelled or manually completed.
 */
void backup_start(BlockDriverState *bs, BlockDriverState *target,
                  int64_t speed, MirrorSyncMode sync_mode,
                  BdrvDirtyBitmap *sync_bitmap,
                  BlockdevOnError on_source_error,
                  BlockdevOnError on_target_error,
                  BlockCompletionFunc *cb, void *opaque,
                  BlockJobTxn *txn, Error **errp);

void blk_set_bs(BlockBackend *blk, BlockDriverState *bs);

void blk_dev_change_media_cb(BlockBackend *blk, bool load);
bool blk_dev_has_removable_media(BlockBackend *blk);
void blk_dev_eject_request(BlockBackend *blk, bool force);
bool blk_dev_is_tray_open(BlockBackend *blk);
bool blk_dev_is_medium_locked(BlockBackend *blk);
void blk_dev_resize_cb(BlockBackend *blk);

void bdrv_set_dirty(BlockDriverState *bs, int64_t cur_sector, int nr_sectors);
bool bdrv_requests_pending(BlockDriverState *bs);

void bdrv_clear_dirty_bitmap(BdrvDirtyBitmap *bitmap, HBitmap **out);
void bdrv_undo_clear_dirty_bitmap(BdrvDirtyBitmap *bitmap, HBitmap *in);

#endif /* BLOCK_INT_H */
