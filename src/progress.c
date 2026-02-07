/* progress.c - Last modified: 07-Feb-2026 (kobayasy)
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
#include "config.h"
#endif  /* #ifdef HAVE_CONFIG_H */

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "progress.h"

#define DIFFTS(_now, _last) \
    ((long)((_now).tv_sec  - (_last).tv_sec)  *    1000 + \
           ((_now).tv_nsec - (_last).tv_nsec) / 1000000 )

int progress_init(PROGRESS *progress, intmax_t update,
                  int fd, unsigned long interval, char id ) {
    int status = -1;
    struct timespec now;

    progress->fd = fd;
    if (progress->fd != -1) {
        progress->interval = interval;
        sprintf(progress->format, "%c%%+jd\n", id);
        progress->update = update;
        if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
            progress->fd = -1;
            goto error;
        }
        progress->data = progress->update, progress->last = now;
    }
    status = 0;
error:
    return status;
}

int progress_update(PROGRESS *progress, intmax_t update) {
    int status = -1;
    struct timespec now;

    if (progress->fd != -1) {
        progress->update += update;
        if (progress->update != progress->data) {
            if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                goto error;
            if (DIFFTS(now, progress->last) >= progress->interval) {
                dprintf(progress->fd, progress->format, progress->update);
                progress->data = progress->update, progress->last = now;
            }
        }
    }
    status = 0;
error:
    return status;
}

int progress_term(PROGRESS *progress) {
    int status = -1;

    if (progress->fd != -1) {
        if (progress->update != progress->data)
            dprintf(progress->fd, progress->format, progress->update);
        progress->fd = -1;
    }
    status = 0;
    return status;
}
