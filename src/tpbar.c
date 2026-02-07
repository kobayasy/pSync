/* tpbar.c - Last modified: 07-Feb-2026 (kobayasy)
 *
 * Copyright (C) 2023-2026 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TGETENT
#include <termcap.h>
#endif  /* #ifdef HAVE_TGETENT */
#include "common.h"
#include "tpbar.h"

void tpbar_init(TPBAR *tpbar) {
#ifdef HAVE_TGETENT
    char *s;
    char ent[1024];
    char *mr, *me;
    STR str;
#endif  /* #ifdef HAVE_TGETENT */

    tpbar->up = NULL;
    tpbar->co = -1;
    tpbar->bar = NULL;
#ifdef HAVE_TGETENT
    s = getenv("TERM");
    if (s == NULL || tgetent(ent, s) != 1)
        goto error;
    s = tpbar->buffer;
    tpbar->up = tgetstr("up", &s);
    tpbar->co = tgetnum("co");
    mr = tgetstr("mr", &s);
    me = tgetstr("me", &s);
    if (mr == NULL || me == NULL)
        goto error;
    str_init(&str, s, TPBAR_BUFFER_SIZE - (s - tpbar->buffer));
    if (ISERR(str_cats(&str, mr, "%.*s", me, "%s", NULL)))
        goto error;
    tpbar->bar = str.s;
error:
#endif  /* #ifdef HAVE_TGETENT */
    tpbar->row.cur = 0;
    tpbar->row.min = INT_MAX;
    tpbar->row.max = INT_MIN;
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

int tpbar_setrow(STR *str, int row, TPBAR *tpbar) {
    int status = -1;
    int n;

    switch (row) {
    case INT_MAX:
    case INT_MIN:
        row = tpbar_getrow(row, tpbar);
        break;
    }
    n = row - tpbar->row.cur;
    if (n > 0)
        do {
            if (ISERR(str_cats(str, "\n", NULL)))
                goto error;
        } while (--n > 0);
    else {
        if (ISERR(str_cats(str, "\r", NULL)))
            goto error;
        if (tpbar->up != NULL && n < 0)
            do {
                if (ISERR(str_cats(str, tpbar->up, NULL)))
                    goto error;
            } while (++n < 0);
    }
    tpbar->row.cur = row;
    if (tpbar->row.cur < tpbar->row.min)
        tpbar->row.min = tpbar->row.cur;
    if (tpbar->row.cur > tpbar->row.max)
        tpbar->row.max = tpbar->row.cur;
    status = 0;
error:
    return status;
}

int tpbar_printf(STR *str, intmax_t current, intmax_t goal, TPBAR *tpbar,
                 const char *format, ... ) {
    int status = -1;
    va_list ap;
    STR head;
    int n;
    char *s = NULL;

    va_start(ap, format);
    head = *str;
    if (ISERR(str_catfv(str, format, ap)))
        goto error;
    if (current >= 0 && tpbar->bar != NULL) {
        n = str->e - head.e;
        if (current < goal)
            n = current * n / goal;
        if (n > 0) {
            s = strdup(head.e);
            if (s != NULL) {
                *str = head;
                if (ISERR(str_catf(str, tpbar->bar, n, s, s + n)))
                    goto error;
                free(s), s = NULL;
            }
        }
    }
    status = 0;
error:
    free(s);
    va_end(ap);
    return status;
}
