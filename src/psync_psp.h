/* psync_psp.h - Last modified: 07-Feb-2026 (kobayasy)
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

#ifndef _INCLUDE_psync_psp_h
#define _INCLUDE_psync_psp_h

#include <signal.h>
#include <time.h>
#include "psync.h"

#define PSYNC_PROTID 0x03705370  /* 'p', 'S', 'p', 3 */

#define ERROR_NOTREADYLOCAL  1
#define ERROR_NOTREADYREMOTE 2
#define ERROR_PROTOCOL    (-25)

typedef struct {
    int fdin, fdout;
    int info;
} PSP;

typedef struct {
    const char *dirname;
    time_t expire;
    time_t backup;
    const char name[1];
} PSP_CONFIG;

extern PSP *psp_new(volatile sig_atomic_t *stop);
extern void psp_free(PSP *psp);
extern PSP_CONFIG *psp_config(const char *name, const char *dirname, PSP *psp);
extern int psp_run(PSP *psp);

#endif  /* #ifndef _INCLUDE_psync_psp_h */
