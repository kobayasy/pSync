/* common.c - Last modified: 07-Feb-2026 (kobayasy)
 *
 * Copyright (C) 2018-2026 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif  /* #ifdef HAVE_CONFIG_H */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef POLL_TIMEOUT
#include <poll.h>
#endif  /* #ifdef POLL_TIMEOUT */
#include "common.h"

void str_init(STR *str, char *buffer, size_t size) {
    str->e = str->s = buffer, str->size = size;
    *str->s = 0;
    str->hold = false;
}

int str_cats(STR *str, ...) {
    int status = -1;
    va_list ap;
    struct {
        size_t size;
        char *e;
    } tmp;
    const char *arg;
    size_t size;

    va_start(ap, str);
    tmp.size = str->size, tmp.e = str->e;
    for (arg = va_arg(ap, const char *); arg != NULL; arg = va_arg(ap, const char *)) {
        size = strlen(arg);
        if (size >= tmp.size)
            goto error;
        strcpy(tmp.e, arg);
        tmp.size -= size, tmp.e += size;
    }
    if (!str->hold)
        str->size = tmp.size, str->e = tmp.e;
    status = 0;
error:
    va_end(ap);
    return status;
}

int str_catfv(STR *str, const char *format, va_list ap) {
    int status = -1;
    int size;

    size = vsnprintf(str->e, str->size, format, ap);
    if (size < 0 || size >= str->size)
        goto error;
    if (!str->hold)
        str->size -= size, str->e += size;
    status = 0;
error:
    return status;
}

int str_catf(STR *str, const char *format, ...) {
    int status = -1;
    va_list ap;

    va_start(ap, format);
    status = str_catfv(str, format, ap);
    va_end(ap);
    return status;
}

int str_catt(STR *str, const char *format, const struct tm *tm) {
    int status = -1;
    int size;

    size = strftime(str->e, str->size, format, tm);
    if (size == 0)
        goto error;
    if (!str->hold)
        str->size -= size, str->e += size;
    status = 0;
error:
    return status;
}

const char *basename_c(const char *path) {
    const char *s = strrchr(path, '/');

    if (s != NULL)
        ++s;
    else
        s = path;
    return s;
}

ssize_t write_size(int fd, const void *buf, size_t count) {
    ssize_t status = -1;
#ifdef POLL_TIMEOUT
    struct pollfd fds = {
        .fd = fd,
        .events = POLLOUT
    };
#endif  /* #ifdef POLL_TIMEOUT */
    size_t size;
    ssize_t n;

    size = count;
    while (size > 0) {
#ifdef POLL_TIMEOUT
        switch (poll(&fds, 1, POLL_TIMEOUT)) {
        case -1:
            switch (errno) {
            case EINTR:
                continue;
            }
        case  0:  /* timeout */
            goto error;
        }
        if (!(fds.revents & POLLOUT))
            goto error;
#endif  /* #ifdef POLL_TIMEOUT */
        n = write(fd, buf, size);
        switch (n) {
        case -1:
            switch (errno) {
            case EINTR:
                continue;
            }
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
#ifdef POLL_TIMEOUT
    struct pollfd fds = {
        .fd = fd,
        .events = POLLIN
    };
#endif  /* #ifdef POLL_TIMEOUT */
    size_t size;
    ssize_t n;

    size = count;
    while (size > 0) {
#ifdef POLL_TIMEOUT
        switch (poll(&fds, 1, POLL_TIMEOUT)) {
        case -1:
            switch (errno) {
            case EINTR:
                continue;
            }
        case  0:  /* timeout */
            goto error;
        }
        if (!(fds.revents & POLLIN))
            goto error;
#endif  /* #ifdef POLL_TIMEOUT */
        n = read(fd, buf, size);
        switch (n) {
        case -1:
            switch (errno) {
            case EINTR:
                continue;
            }
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
