/* common.c - Last modified: 05-Feb-2022 (kobayasy)
 *
 * Copyright (c) 2018-2022 by Yuichi Kobayashi <kobayasy@kobayasy.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <limits.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include "common.h"

#ifndef POLL_TIMEOUT
#define POLL_TIMEOUT 100000  /* [msec] */
#endif  /* #ifndef POLL_TIMEOUT */

ssize_t write_size(int fd, const void *buf, size_t count) {
    ssize_t status = -1;
    size_t size;
    struct pollfd fds;
    ssize_t n;

    size = count;
    fds.fd = fd, fds.events = POLLOUT;
    while (size > 0) {
        if (poll(&fds, 1, POLL_TIMEOUT) != 1)
            goto error;
        if (!(fds.revents & POLLOUT))
            goto error;
        n = write(fd, buf, size);
        switch (n) {
        case -1:
        case  0:  /* end of file */
            goto error;
        }
        size -= n;
        buf += n;
    }
    status = count;
error:
    return status;
}

ssize_t read_size(int fd, void *buf, size_t count) {
    ssize_t status = -1;
    size_t size;
    struct pollfd fds;
    ssize_t n;

    size = count;
    fds.fd = fd, fds.events = POLLIN;
    while (size > 0) {
        if (poll(&fds, 1, POLL_TIMEOUT) != 1)
            goto error;
        if (!(fds.revents & POLLIN))
            goto error;
        n = read(fd, buf, size);
        switch (n) {
        case -1:
        case  0:  /* end of file */
            goto error;
        }
        size -= n;
        buf += n;
    }
    status = count;
error:
    return status;
}

int strcmp_next(const char *s1, const char *s2) {
    int n = strcmp(s1, s2);

    if (n < 0) {
        if (!*s1)
            n = INT_MAX;
    }
    else if (n > 0) {
        if (!*s2)
            n = INT_MIN;
    }
    return n;
}
