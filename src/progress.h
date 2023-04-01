/* progress.h - Last modified: 29-Mar-2023 (kobayasy)
 *
 * Copyright (C) 2018-2023 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#ifndef _INCLUDE_progress_h
#define _INCLUDE_progress_h

#include <stdint.h>

typedef struct {
    int fd;
    unsigned long interval;
    char format[7];
    intmax_t update, data;
    struct timespec last;
} PROGRESS;

extern int progress_init(PROGRESS *progress, intmax_t update,
                         int fd, unsigned long interval, char id );
extern int progress_update(PROGRESS *progress, intmax_t update);
extern int progress_term(PROGRESS *progress);

#endif  /* #ifndef _INCLUDE_progress_h */
