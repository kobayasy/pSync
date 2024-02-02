/* tpbar.c - Last modified: 03-Feb-2024 (kobayasy)
 *
 * Copyright (C) 2023-2024 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TGETENT
#include <termcap.h>
#endif  /* #ifdef HAVE_TGETENT */
#include "tpbar.h"

void tpbar_init(TPBAR *tpbar) {
#ifdef HAVE_TGETENT
    char ent[1024], *s;
#endif  /* #ifdef HAVE_TGETENT */

    tpbar->up = NULL;
    tpbar->co = -1;
    tpbar->bar = NULL;
    tpbar->row.cur = 0;
    tpbar->row.min = INT_MAX;
    tpbar->row.max = INT_MIN;
#ifdef HAVE_TGETENT
    s = getenv("TERM");
    if (s == NULL)
        goto error;
    if (tgetent(ent, s) != 1)
        goto error;
    tpbar->up = UP;
    tpbar->co = tgetnum("co");
    s = tpbar->buffer;
    if (tgetstr("mr", &s) == NULL)
        goto error;
    --s;
    s += sprintf(s, "%%.*s");
    if (tgetstr("me", &s) == NULL)
        goto error;
    --s;
    s += sprintf(s, "%%s");
    tpbar->bar = tpbar->buffer;
error:
    ;
#endif  /* #ifdef HAVE_TGETENT */
}

int tpbar_getrow(int row, TPBAR *tpbar) {
    switch (row) {
    case INT_MIN:
        row = tpbar->row.min == INT_MAX ? 0 : tpbar->row.min - 1;
        break;
    case INT_MAX:
        row = tpbar->row.max == INT_MIN ? 0 : tpbar->row.max + 1;
        break;
    default:
        row += tpbar->row.cur;
    }
    return row;
}

int tpbar_setrow(char *str, int row, TPBAR *tpbar) {
    int length = 0;
    char *s;
    int n;

    switch (row) {
    case INT_MAX:
    case INT_MIN:
        row = tpbar_getrow(row, tpbar);
        break;
    }
    row -= tpbar->row.cur;
    s = str;
    if (row > 0) {
        memset(s, '\n', row);
        length += row, s += row;
        *s = 0;
        tpbar->row.cur += row;
    }
    else {
        *s++ = '\r', ++length;
        if (tpbar->up != NULL && row < 0) {
            do {
                n = sprintf(s, "%s", tpbar->up);
                length += n, s += n;
                --tpbar->row.cur;
            } while (++row < 0);
        }
    }
    if (tpbar->row.cur < tpbar->row.min)
        tpbar->row.min = tpbar->row.cur;
    if (tpbar->row.cur > tpbar->row.max)
        tpbar->row.max = tpbar->row.cur;
    return length;
}

int tpbar_printf(char *str, intmax_t current, intmax_t goal, TPBAR *tpbar,
                 const char *format, ... ) {
    int length = -1;
    va_list ap;
    unsigned int n;
    char *s;

    va_start(ap, format);
    length = vsprintf(str, format, ap);
    va_end(ap);
    if (length < 0 || tpbar->bar == NULL)
        n = 0;
    else if (current < goal)
        n = current * length / goal;
    else
        n = length;
    if (n > 0 && (s = strdup(str)) != NULL) {
        length = sprintf(str, tpbar->bar, n, s, s + n);
        free(s);
    }
    return length;
}
