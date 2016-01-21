/*
 * Throttle infrastructure tests
 *
 * Copyright Nodalink, EURL. 2013-2014
 * Copyright Igalia, S.L. 2015
 *
 * Authors:
 *  Benoît Canet     <benoit.canet@nodalink.com>
 *  Alberto Garcia   <berto@igalia.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#include <glib.h>
#include <math.h>
#include "block/aio.h"
#include "qemu/throttle.h"
#include "qemu/error-report.h"
#include "block/throttle-groups.h"

static AioContext     *ctx;
static LeakyBucket    bkt;
static ThrottleConfig cfg;
static ThrottleState  ts;
static ThrottleTimers tt;

/* useful function */
static bool double_cmp(double x, double y)
{
    return fabsl(x - y) < 1e-6;
}

/* tests for single bucket operations */
static void test_leak_bucket(void)
{
    /* set initial value */
    bkt.avg = 150;
    bkt.max = 15;
    bkt.level = 1.5;

    /* leak an op work of time */
    throttle_leak_bucket(&bkt, NANOSECONDS_PER_SECOND / 150);
    g_assert(bkt.avg == 150);
    g_assert(bkt.max == 15);
    g_assert(double_cmp(bkt.level, 0.5));

    /* leak again emptying the bucket */
    throttle_leak_bucket(&bkt, NANOSECONDS_PER_SECOND / 150);
    g_assert(bkt.avg == 150);
    g_assert(bkt.max == 15);
    g_assert(double_cmp(bkt.level, 0));

    /* check that the bucket level won't go lower */
    throttle_leak_bucket(&bkt, NANOSECONDS_PER_SECOND / 150);
    g_assert(bkt.avg == 150);
    g_assert(bkt.max == 15);
    g_assert(double_cmp(bkt.level, 0));
}

static void test_compute_wait(void)
{
    int64_t wait;
    int64_t result;

    /* no operation limit set */
    bkt.avg = 0;
    bkt.max = 15;
    bkt.level = 1.5;
    wait = throttle_compute_wait(&bkt);
    g_assert(!wait);

    /* zero delta */
    bkt.avg = 150;
    bkt.max = 15;
    bkt.level = 15;
    wait = throttle_compute_wait(&bkt);
    g_assert(!wait);

    /* below zero delta */
    bkt.avg = 150;
    bkt.max = 15;
    bkt.level = 9;
    wait = throttle_compute_wait(&bkt);
    g_assert(!wait);

    /* half an operation above max */
    bkt.avg = 150;
    bkt.max = 15;
    bkt.level = 15.5;
    wait = throttle_compute_wait(&bkt);
    /* time required to do half an operation */
    result = (int64_t)  NANOSECONDS_PER_SECOND / 150 / 2;
    g_assert(wait == result);
}

/* functions to test ThrottleState initialization/destroy methods */
static void read_timer_cb(void *opaque)
{
}

static void write_timer_cb(void *opaque)
{
}

static void test_init(void)
{
    int i;

    /* fill the structures with crap */
    memset(&ts, 1, sizeof(ts));
    memset(&tt, 1, sizeof(tt));

    /* init structures */
    throttle_init(&ts);
    throttle_timers_init(&tt, ctx, QEMU_CLOCK_VIRTUAL,
                         read_timer_cb, write_timer_cb, &ts);

    /* check initialized fields */
    g_assert(tt.clock_type == QEMU_CLOCK_VIRTUAL);
    g_assert(tt.timers[0]);
    g_assert(tt.timers[1]);

    /* check other fields where cleared */
    g_assert(!ts.previous_leak);
    g_assert(!ts.cfg.op_size);
    for (i = 0; i < BUCKETS_COUNT; i++) {
        g_assert(!ts.cfg.buckets[i].avg);
        g_assert(!ts.cfg.buckets[i].max);
        g_assert(!ts.cfg.buckets[i].level);
    }

    throttle_timers_destroy(&tt);
}

static void test_destroy(void)
{
    int i;
    throttle_init(&ts);
    throttle_timers_init(&tt, ctx, QEMU_CLOCK_VIRTUAL,
                         read_timer_cb, write_timer_cb, &ts);
    throttle_timers_destroy(&tt);
    for (i = 0; i < 2; i++) {
        g_assert(!tt.timers[i]);
    }
}

/* function to test throttle_config and throttle_get_config */
static void test_config_functions(void)
{
    int i;
    ThrottleConfig orig_cfg, final_cfg;

    orig_cfg.buckets[THROTTLE_BPS_TOTAL].avg = 153;
    orig_cfg.buckets[THROTTLE_BPS_READ].avg  = 56;
    orig_cfg.buckets[THROTTLE_BPS_WRITE].avg = 1;

    orig_cfg.buckets[THROTTLE_OPS_TOTAL].avg = 150;
    orig_cfg.buckets[THROTTLE_OPS_READ].avg  = 69;
    orig_cfg.buckets[THROTTLE_OPS_WRITE].avg = 23;

    orig_cfg.buckets[THROTTLE_BPS_TOTAL].max = 0; /* should be corrected */
    orig_cfg.buckets[THROTTLE_BPS_READ].max  = 1; /* should not be corrected */
    orig_cfg.buckets[THROTTLE_BPS_WRITE].max = 120;

    orig_cfg.buckets[THROTTLE_OPS_TOTAL].max = 150;
    orig_cfg.buckets[THROTTLE_OPS_READ].max  = 400;
    orig_cfg.buckets[THROTTLE_OPS_WRITE].max = 500;

    orig_cfg.buckets[THROTTLE_BPS_TOTAL].level = 45;
    orig_cfg.buckets[THROTTLE_BPS_READ].level  = 65;
    orig_cfg.buckets[THROTTLE_BPS_WRITE].level = 23;

    orig_cfg.buckets[THROTTLE_OPS_TOTAL].level = 1;
    orig_cfg.buckets[THROTTLE_OPS_READ].level  = 90;
    orig_cfg.buckets[THROTTLE_OPS_WRITE].level = 75;

    orig_cfg.op_size = 1;

    throttle_init(&ts);
    throttle_timers_init(&tt, ctx, QEMU_CLOCK_VIRTUAL,
                         read_timer_cb, write_timer_cb, &ts);
    /* structure reset by throttle_init previous_leak should be null */
    g_assert(!ts.previous_leak);
    throttle_config(&ts, &tt, &orig_cfg);

    /* has previous leak been initialized by throttle_config ? */
    g_assert(ts.previous_leak);

    /* get back the fixed configuration */
    throttle_get_config(&ts, &final_cfg);

    throttle_timers_destroy(&tt);

    g_assert(final_cfg.buckets[THROTTLE_BPS_TOTAL].avg == 153);
    g_assert(final_cfg.buckets[THROTTLE_BPS_READ].avg  == 56);
    g_assert(final_cfg.buckets[THROTTLE_BPS_WRITE].avg == 1);

    g_assert(final_cfg.buckets[THROTTLE_OPS_TOTAL].avg == 150);
    g_assert(final_cfg.buckets[THROTTLE_OPS_READ].avg  == 69);
    g_assert(final_cfg.buckets[THROTTLE_OPS_WRITE].avg == 23);

    g_assert(final_cfg.buckets[THROTTLE_BPS_TOTAL].max == 15.3);/* fixed */
    g_assert(final_cfg.buckets[THROTTLE_BPS_READ].max  == 1);   /* not fixed */
    g_assert(final_cfg.buckets[THROTTLE_BPS_WRITE].max == 120);

    g_assert(final_cfg.buckets[THROTTLE_OPS_TOTAL].max == 150);
    g_assert(final_cfg.buckets[THROTTLE_OPS_READ].max  == 400);
    g_assert(final_cfg.buckets[THROTTLE_OPS_WRITE].max == 500);

    g_assert(final_cfg.op_size == 1);

    /* check bucket have been cleared */
    for (i = 0; i < BUCKETS_COUNT; i++) {
        g_assert(!final_cfg.buckets[i].level);
    }
}

/* functions to test is throttle is enabled by a config */
static void set_cfg_value(bool is_max, int index, int value)
{
    if (is_max) {
        cfg.buckets[index].max = value;
    } else {
        cfg.buckets[index].avg = value;
    }
}

static void test_enabled(void)
{
    int i;

    memset(&cfg, 0, sizeof(cfg));
    g_assert(!throttle_enabled(&cfg));

    for (i = 0; i < BUCKETS_COUNT; i++) {
        memset(&cfg, 0, sizeof(cfg));
        set_cfg_value(false, i, 150);
        g_assert(throttle_enabled(&cfg));
    }

    for (i = 0; i < BUCKETS_COUNT; i++) {
        memset(&cfg, 0, sizeof(cfg));
        set_cfg_value(false, i, -150);
        g_assert(!throttle_enabled(&cfg));
    }
}

/* tests functions for throttle_conflicting */

static void test_conflicts_for_one_set(bool is_max,
                                       int total,
                                       int read,
                                       int write)
{
    memset(&cfg, 0, sizeof(cfg));
    g_assert(!throttle_conflicting(&cfg));

    set_cfg_value(is_max, total, 1);
    set_cfg_value(is_max, read,  1);
    g_assert(throttle_conflicting(&cfg));

    memset(&cfg, 0, sizeof(cfg));
    set_cfg_value(is_max, total, 1);
    set_cfg_value(is_max, write, 1);
    g_assert(throttle_conflicting(&cfg));

    memset(&cfg, 0, sizeof(cfg));
    set_cfg_value(is_max, total, 1);
    set_cfg_value(is_max, read,  1);
    set_cfg_value(is_max, write, 1);
    g_assert(throttle_conflicting(&cfg));

    memset(&cfg, 0, sizeof(cfg));
    set_cfg_value(is_max, total, 1);
    g_assert(!throttle_conflicting(&cfg));

    memset(&cfg, 0, sizeof(cfg));
    set_cfg_value(is_max, read,  1);
    set_cfg_value(is_max, write, 1);
    g_assert(!throttle_conflicting(&cfg));
}

static void test_conflicting_config(void)
{
    /* bps average conflicts */
    test_conflicts_for_one_set(false,
                               THROTTLE_BPS_TOTAL,
                               THROTTLE_BPS_READ,
                               THROTTLE_BPS_WRITE);

    /* ops average conflicts */
    test_conflicts_for_one_set(false,
                               THROTTLE_OPS_TOTAL,
                               THROTTLE_OPS_READ,
                               THROTTLE_OPS_WRITE);

    /* bps average conflicts */
    test_conflicts_for_one_set(true,
                               THROTTLE_BPS_TOTAL,
                               THROTTLE_BPS_READ,
                               THROTTLE_BPS_WRITE);
    /* ops average conflicts */
    test_conflicts_for_one_set(true,
                               THROTTLE_OPS_TOTAL,
                               THROTTLE_OPS_READ,
                               THROTTLE_OPS_WRITE);
}
/* functions to test the throttle_is_valid function */
static void test_is_valid_for_value(int value, bool should_be_valid)
{
    int is_max, index;
    for (is_max = 0; is_max < 2; is_max++) {
        for (index = 0; index < BUCKETS_COUNT; index++) {
            memset(&cfg, 0, sizeof(cfg));
            set_cfg_value(is_max, index, value);
            g_assert(throttle_is_valid(&cfg) == should_be_valid);
        }
    }
}

static void test_is_valid(void)
{
    /* negative number are invalid */
    test_is_valid_for_value(-1, false);
    /* zero are valids */
    test_is_valid_for_value(0, true);
    /* positives numers are valids */
    test_is_valid_for_value(1, true);
}

static void test_max_is_missing_limit(void)
{
    int i;

    for (i = 0; i < BUCKETS_COUNT; i++) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.buckets[i].max = 100;
        cfg.buckets[i].avg = 0;
        g_assert(throttle_max_is_missing_limit(&cfg));

        cfg.buckets[i].max = 0;
        cfg.buckets[i].avg = 0;
        g_assert(!throttle_max_is_missing_limit(&cfg));

        cfg.buckets[i].max = 0;
        cfg.buckets[i].avg = 100;
        g_assert(!throttle_max_is_missing_limit(&cfg));
    }
}

static void test_have_timer(void)
{
    /* zero structures */
    memset(&ts, 0, sizeof(ts));
    memset(&tt, 0, sizeof(tt));

    /* no timer set should return false */
    g_assert(!throttle_timers_are_initialized(&tt));

    /* init structures */
    throttle_init(&ts);
    throttle_timers_init(&tt, ctx, QEMU_CLOCK_VIRTUAL,
                         read_timer_cb, write_timer_cb, &ts);

    /* timer set by init should return true */
    g_assert(throttle_timers_are_initialized(&tt));

    throttle_timers_destroy(&tt);
}

static void test_detach_attach(void)
{
    /* zero structures */
    memset(&ts, 0, sizeof(ts));
    memset(&tt, 0, sizeof(tt));

    /* init the structure */
    throttle_init(&ts);
    throttle_timers_init(&tt, ctx, QEMU_CLOCK_VIRTUAL,
                         read_timer_cb, write_timer_cb, &ts);

    /* timer set by init should return true */
    g_assert(throttle_timers_are_initialized(&tt));

    /* timer should no longer exist after detaching */
    throttle_timers_detach_aio_context(&tt);
    g_assert(!throttle_timers_are_initialized(&tt));

    /* timer should exist again after attaching */
    throttle_timers_attach_aio_context(&tt, ctx);
    g_assert(throttle_timers_are_initialized(&tt));

    throttle_timers_destroy(&tt);
}

static bool do_test_accounting(bool is_ops, /* are we testing bps or ops */
                int size,                   /* size of the operation to do */
                double avg,                 /* io limit */
                uint64_t op_size,           /* ideal size of an io */
                double total_result,
                double read_result,
                double write_result)
{
    BucketType to_test[2][3] = { { THROTTLE_BPS_TOTAL,
                                   THROTTLE_BPS_READ,
                                   THROTTLE_BPS_WRITE, },
                                 { THROTTLE_OPS_TOTAL,
                                   THROTTLE_OPS_READ,
                                   THROTTLE_OPS_WRITE, } };
    ThrottleConfig cfg;
    BucketType index;
    int i;

    for (i = 0; i < 3; i++) {
        BucketType index = to_test[is_ops][i];
        cfg.buckets[index].avg = avg;
    }

    cfg.op_size = op_size;

    throttle_init(&ts);
    throttle_timers_init(&tt, ctx, QEMU_CLOCK_VIRTUAL,
                         read_timer_cb, write_timer_cb, &ts);
    throttle_config(&ts, &tt, &cfg);

    /* account a read */
    throttle_account(&ts, false, size);
    /* account a write */
    throttle_account(&ts, true, size);

    /* check total result */
    index = to_test[is_ops][0];
    if (!double_cmp(ts.cfg.buckets[index].level, total_result)) {
        return false;
    }

    /* check read result */
    index = to_test[is_ops][1];
    if (!double_cmp(ts.cfg.buckets[index].level, read_result)) {
        return false;
    }

    /* check write result */
    index = to_test[is_ops][2];
    if (!double_cmp(ts.cfg.buckets[index].level, write_result)) {
        return false;
    }

    throttle_timers_destroy(&tt);

    return true;
}

static void test_accounting(void)
{
    /* tests for bps */

    /* op of size 1 */
    g_assert(do_test_accounting(false,
                                1 * 512,
                                150,
                                0,
                                1024,
                                512,
                                512));

    /* op of size 2 */
    g_assert(do_test_accounting(false,
                                2 * 512,
                                150,
                                0,
                                2048,
                                1024,
                                1024));

    /* op of size 2 and orthogonal parameter change */
    g_assert(do_test_accounting(false,
                                2 * 512,
                                150,
                                17,
                                2048,
                                1024,
                                1024));


    /* tests for ops */

    /* op of size 1 */
    g_assert(do_test_accounting(true,
                                1 * 512,
                                150,
                                0,
                                2,
                                1,
                                1));

    /* op of size 2 */
    g_assert(do_test_accounting(true,
                                2 *  512,
                                150,
                                0,
                                2,
                                1,
                                1));

    /* jumbo op accounting fragmentation : size 64 with op size of 13 units */
    g_assert(do_test_accounting(true,
                                64 * 512,
                                150,
                                13 * 512,
                                (64.0 * 2) / 13,
                                (64.0 / 13),
                                (64.0 / 13)));

    /* same with orthogonal parameters changes */
    g_assert(do_test_accounting(true,
                                64 * 512,
                                300,
                                13 * 512,
                                (64.0 * 2) / 13,
                                (64.0 / 13),
                                (64.0 / 13)));
}

static void test_groups(void)
{
    ThrottleConfig cfg1, cfg2;
    BlockDriverState *bdrv1, *bdrv2, *bdrv3;

    bdrv1 = bdrv_new();
    bdrv2 = bdrv_new();
    bdrv3 = bdrv_new();

    g_assert(bdrv1->throttle_state == NULL);
    g_assert(bdrv2->throttle_state == NULL);
    g_assert(bdrv3->throttle_state == NULL);

    throttle_group_register_bs(bdrv1, "bar");
    throttle_group_register_bs(bdrv2, "foo");
    throttle_group_register_bs(bdrv3, "bar");

    g_assert(bdrv1->throttle_state != NULL);
    g_assert(bdrv2->throttle_state != NULL);
    g_assert(bdrv3->throttle_state != NULL);

    g_assert(!strcmp(throttle_group_get_name(bdrv1), "bar"));
    g_assert(!strcmp(throttle_group_get_name(bdrv2), "foo"));
    g_assert(bdrv1->throttle_state == bdrv3->throttle_state);

    /* Setting the config of a group member affects the whole group */
    memset(&cfg1, 0, sizeof(cfg1));
    cfg1.buckets[THROTTLE_BPS_READ].avg  = 500000;
    cfg1.buckets[THROTTLE_BPS_WRITE].avg = 285000;
    cfg1.buckets[THROTTLE_OPS_READ].avg  = 20000;
    cfg1.buckets[THROTTLE_OPS_WRITE].avg = 12000;
    throttle_group_config(bdrv1, &cfg1);

    throttle_group_get_config(bdrv1, &cfg1);
    throttle_group_get_config(bdrv3, &cfg2);
    g_assert(!memcmp(&cfg1, &cfg2, sizeof(cfg1)));

    cfg2.buckets[THROTTLE_BPS_READ].avg  = 4547;
    cfg2.buckets[THROTTLE_BPS_WRITE].avg = 1349;
    cfg2.buckets[THROTTLE_OPS_READ].avg  = 123;
    cfg2.buckets[THROTTLE_OPS_WRITE].avg = 86;
    throttle_group_config(bdrv3, &cfg1);

    throttle_group_get_config(bdrv1, &cfg1);
    throttle_group_get_config(bdrv3, &cfg2);
    g_assert(!memcmp(&cfg1, &cfg2, sizeof(cfg1)));

    throttle_group_unregister_bs(bdrv1);
    throttle_group_unregister_bs(bdrv2);
    throttle_group_unregister_bs(bdrv3);

    g_assert(bdrv1->throttle_state == NULL);
    g_assert(bdrv2->throttle_state == NULL);
    g_assert(bdrv3->throttle_state == NULL);
}

int main(int argc, char **argv)
{
    qemu_init_main_loop(&error_fatal);
    ctx = qemu_get_aio_context();
    bdrv_init();

    do {} while (g_main_context_iteration(NULL, false));

    /* tests in the same order as the header function declarations */
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/throttle/leak_bucket",        test_leak_bucket);
    g_test_add_func("/throttle/compute_wait",       test_compute_wait);
    g_test_add_func("/throttle/init",               test_init);
    g_test_add_func("/throttle/destroy",            test_destroy);
    g_test_add_func("/throttle/have_timer",         test_have_timer);
    g_test_add_func("/throttle/detach_attach",      test_detach_attach);
    g_test_add_func("/throttle/config/enabled",     test_enabled);
    g_test_add_func("/throttle/config/conflicting", test_conflicting_config);
    g_test_add_func("/throttle/config/is_valid",    test_is_valid);
    g_test_add_func("/throttle/config/max",         test_max_is_missing_limit);
    g_test_add_func("/throttle/config_functions",   test_config_functions);
    g_test_add_func("/throttle/accounting",         test_accounting);
    g_test_add_func("/throttle/groups",             test_groups);
    return g_test_run();
}

