/*
 * QEMU I/O channel file test
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

#include "io/channel-file.h"
#include "io-channel-helpers.h"


static void test_io_channel_file(void)
{
    QIOChannel *src, *dst;
    QIOChannelTest *test;

#define TEST_FILE "tests/test-io-channel-file.txt"
    unlink(TEST_FILE);
    src = QIO_CHANNEL(qio_channel_file_new_path(
                          TEST_FILE,
                          O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600,
                          &error_abort));
    dst = QIO_CHANNEL(qio_channel_file_new_path(
                          TEST_FILE,
                          O_RDONLY | O_BINARY, 0,
                          &error_abort));

    test = qio_channel_test_new();
    qio_channel_test_run_writer(test, src);
    qio_channel_test_run_reader(test, dst);
    qio_channel_test_validate(test);

    unlink(TEST_FILE);
    object_unref(OBJECT(src));
    object_unref(OBJECT(dst));
}


#ifndef _WIN32
static void test_io_channel_pipe(bool async)
{
    QIOChannel *src, *dst;
    QIOChannelTest *test;
    int fd[2];

    if (pipe(fd) < 0) {
        perror("pipe");
        abort();
    }

    src = QIO_CHANNEL(qio_channel_file_new_fd(fd[1]));
    dst = QIO_CHANNEL(qio_channel_file_new_fd(fd[0]));

    test = qio_channel_test_new();
    qio_channel_test_run_threads(test, async, src, dst);
    qio_channel_test_validate(test);

    object_unref(OBJECT(src));
    object_unref(OBJECT(dst));
}


static void test_io_channel_pipe_async(void)
{
    test_io_channel_pipe(true);
}

static void test_io_channel_pipe_sync(void)
{
    test_io_channel_pipe(false);
}
#endif /* ! _WIN32 */


int main(int argc, char **argv)
{
    module_call_init(MODULE_INIT_QOM);

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/io/channel/file", test_io_channel_file);
#ifndef _WIN32
    g_test_add_func("/io/channel/pipe/sync", test_io_channel_pipe_sync);
    g_test_add_func("/io/channel/pipe/async", test_io_channel_pipe_async);
#endif
    return g_test_run();
}
