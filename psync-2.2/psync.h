/* psync.h - Last modified: 27-Feb-2020 (kobayasy)
 *
 * Copyright (c) 2018-2020 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#ifndef _INCLUDE_psync_h
#define _INCLUDE_psync_h

#include <signal.h>
#include <time.h>

#define PSYNC_FILEID 0x01665370  /* 'p', 'S', 'f', 1 */

//#define ERROR_UNKNOWN  (-1)
#define ERROR_FTYPE    (-2)
#define ERROR_FPERM    (-3)
#define ERROR_FMAKE    (-4)
#define ERROR_FOPEN    (-5)
#define ERROR_FWRITE   (-6)
#define ERROR_FREAD    (-7)
#define ERROR_FLINK    (-8)
#define ERROR_FREMOVE  (-9)
#define ERROR_FMOVE   (-10)
#define ERROR_SWRITE  (-11)
#define ERROR_SREAD   (-12)
#define ERROR_SUPLD   (-13)
#define ERROR_SDNLD   (-14)
#define ERROR_FUPLD   (-15)
#define ERROR_FDNLD   (-16)
#define ERROR_DMAKE   (-17)
#define ERROR_DOPEN   (-18)
#define ERROR_DWRITE  (-19)
#define ERROR_DREAD   (-20)
#define ERROR_DREMOVE (-21)
#define ERROR_MEMORY  (-22)
#define ERROR_SYSTEM  (-23)
#define ERROR_STOP    (-24)

typedef struct {
    const time_t t;
    time_t expire;
    time_t backup;
    int fdin, fdout;
    int info;
} PSYNC;

typedef enum {
    PSYNC_SLAVE=0,
    PSYNC_MASTER,
    PSYNC_MASTER_PUT,
    PSYNC_MASTER_GET
} PSYNC_MODE;

extern PSYNC *psync_new(const char *dirname,
                        volatile sig_atomic_t *stop );
extern void psync_free(PSYNC *psync);
extern int psync_run(PSYNC_MODE mode, PSYNC *psync);

#endif  /* #ifndef _INCLUDE_psync_h */
