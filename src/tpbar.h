/* tpbar.h - Last modified: 03-Feb-2024 (kobayasy)
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

#ifndef _INCLUDE_tpbar_h
#define _INCLUDE_tpbar_h

#include <stdint.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* #ifdef HAVE_CONFIG_H */

typedef struct {
    char *up;
    int co;
    char *bar;
    struct {
        int cur;
        int min, max;
    } row;
#ifdef HAVE_TGETENT
    char buffer[1024];
#endif  /* #ifdef HAVE_TGETENT */
} TPBAR;

extern void tpbar_init(TPBAR *tpbar);
extern int tpbar_getrow(int row, TPBAR *tpbar);
extern int tpbar_setrow(char *str, int row, TPBAR *tpbar);
extern int tpbar_printf(char *str, intmax_t current, intmax_t goal, TPBAR *tpbar,
                        const char *format, ... );

#endif  /* #ifndef _INCLUDE_tpbar_h */
