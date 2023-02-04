/* info.c - Last modified: 05-Feb-2023 (kobayasy)
 *
 * Copyright (c) 2023 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_TGETENT
#include <termcap.h>
#endif  /* #ifdef HAVE_TGETENT */
#include "info.h"

static const char *strmes(int status) {
    static const char *pmes[] = {
        /*   0 */                        "No error",
        /*   1: ERROR_NOTREADYLOCAL */   "Local not ready",
        /*   2: ERROR_NOTREADYREMOTE */  "Remote not ready"
    };
    static const char *nmes[] = {
        /*  -1 */                        "Unknown",
        /*  -2: ERROR_FTYPE */           "File type",
        /*  -3: ERROR_FPERM */           "File permission",
        /*  -4: ERROR_FMAKE */           "Make file",
        /*  -5: ERROR_FOPEN */           "Open file",
        /*  -6: ERROR_FWRITE */          "Write file",
        /*  -7: ERROR_FREAD */           "Read file",
        /*  -8: ERROR_FLINK */           "Link file",
        /*  -9: ERROR_FREMOVE */         "Remove file",
        /* -10: ERROR_FMOVE */           "Move file",
        /* -11: ERROR_SWRITE */          "Write file-stat",
        /* -12: ERROR_SREAD */           "Read file-stat",
        /* -13: ERROR_SUPLD */           "Upload file-stat",
        /* -14: ERROR_SDNLD */           "Download file-stat",
        /* -15: ERROR_FUPLD */           "Upload file",
        /* -16: ERROR_FDNLD */           "Download file",
        /* -17: ERROR_DMAKE */           "Make data-file",
        /* -18: ERROR_DOPEN */           "Open data-file",
        /* -19: ERROR_DWRITE */          "Write data-file",
        /* -20: ERROR_DREAD */           "Read data-file",
        /* -21: ERROR_DREMOVE */         "Remove data-file",
        /* -22: ERROR_MEMORY */          "Memory",
        /* -23: ERROR_SYSTEM */          "System",
        /* -24: ERROR_STOP */            "Interrupted",
        /* -25: ERROR_PROTOCOL */        "Protocol",
        /* -26: ERROR_ENVS */            "Environment",
        /* -27: ERROR_CONF */            "Configuration",
        /* -28: ERROR_ARGS */            "Argument"
    };
    const char **mes;
    size_t size;

    if (status < 0) {
        mes = nmes;
        size = sizeof(nmes)/sizeof(*nmes);
        status = -(status + 1);
    }
    else {
        mes = pmes;
        size = sizeof(pmes)/sizeof(*pmes);
    }
    if (status >= size)
        status = 0;
    return mes[status];
}

static char *strfnum(char *str, size_t length, intmax_t num) {
    const char *unit = "kMGTPEZYRQ";

    if (unit[0] && (num >= 1024 || num <= -1024)) {
        while (unit[1] && (num >= 1024*1024 || num <= -1024*1024))
            ++unit, num /= 1024;
        sprintf(str, "%*.3f%cB", (int)(length - 2), (double)(int32_t)num / 1024, *unit);
    }
    else
        sprintf(str, "%*jdB", (int)(length - 1), num);
    return str;
}

typedef struct {
    char *up;
    int co;
    char *bar;
    struct {
        int cur;
        int min, max;
    } row;
    char buffer[1024];
} TENT;

static void tinit(TENT *tent) {
#ifdef HAVE_TGETENT
    char ent[1024], *s;
#endif  /* #ifdef HAVE_TGETENT */

    tent->up = NULL;
    tent->co = -1;
    tent->bar = NULL;
    tent->row.cur = 0;
    tent->row.min = INT_MAX;
    tent->row.max = INT_MIN;
#ifdef HAVE_TGETENT
    s = getenv("TERM");
    if (s == NULL)
        goto error;
    if (tgetent(ent, s) != 1)
        goto error;
    tent->up = UP;
    tent->co = tgetnum("co");
    s = tent->buffer;
    if (tgetstr("mr", &s) == NULL)
        goto error;
    --s;
    s += sprintf(s, "%%.*s");
    if (tgetstr("me", &s) == NULL)
        goto error;
    --s;
    s += sprintf(s, "%%s");
    tent->bar = tent->buffer;
error:
    ;
#endif  /* #ifdef HAVE_TGETENT */
}

static int tgetrow(int row, TENT *tent) {
    switch (row) {
    case INT_MIN:
        row = tent->row.min == INT_MAX ? 0 : tent->row.min - 1;
        break;
    case INT_MAX:
        row = tent->row.max == INT_MIN ? 0 : tent->row.max + 1;
        break;
    default:
        row += tent->row.cur;
    }
    return row;
}

static int tsetrow(char *str, int row, TENT *tent) {
    int length = 0;
    char *s;
    int n;

    switch (row) {
    case INT_MAX:
    case INT_MIN:
        row = tgetrow(row, tent);
        break;
    }
    row -= tent->row.cur;
    s = str;
    if (row > 0) {
        memset(s, '\n', row);
        length += row, s += row;
        *s = 0;
        tent->row.cur += row;
    }
    else {
        *s++ = '\r', ++length;
        if (tent->up != NULL && row < 0) {
            do {
                n = sprintf(s, "%s", tent->up);
                length += n, s += n;
                --tent->row.cur;
            } while (++row < 0);
        }
    }
    if (tent->row.cur < tent->row.min)
        tent->row.min = tent->row.cur;
    if (tent->row.cur > tent->row.max)
        tent->row.max = tent->row.cur;
    return length;
}

static int tbar(char *str, intmax_t current, intmax_t goal, TENT *tent,
                const char *format, ... ) {
    int length = -1;
    va_list ap;
    unsigned int n;
    char *s;

    va_start(ap, format);
    length = vsprintf(str, format, ap);
    va_end(ap);
    if (length < 0 || tent->bar == NULL)
        n = 0;
    else if (current < goal)
        n = current * length / goal;
    else
        n = length;
    if (n > 0 && (s = strdup(str)) != NULL) {
        length = sprintf(str, tent->bar, n, s, s + n);
        free(s);
    }
    return length;
}

typedef struct {
    int sid;
    int row;
    char name[1024];
    struct {
        intmax_t filescan;
        intmax_t upload;
        intmax_t downloaded;
        intmax_t fileremove;
        intmax_t filecopy;
    } host[2];
} PROGRESS;
struct {
    int sid[2];
    PROGRESS progress[2];
    TENT tent;
    size_t namelen;
} g;

void info_init(size_t namelen) {
    g.sid[0] = 0, g.sid[1] = 0;
    g.progress[0].sid = INT_MIN, g.progress[1].sid = INT_MIN;
    tinit(&g.tent);
    g.namelen = namelen;
    if (g.tent.co < g.namelen + 1+27+1)
        g.tent.up = NULL;
}

void info_print(unsigned int host, const char *line) {
    int update = 0;
    static const char *hostname[] = {"Local: ", "Remote: "};
    PROGRESS *progress = &g.progress[g.sid[host] & 1];
    int n;
    char buffer[1024], *s;
    intmax_t n1, n2;
    char buffer1[12], buffer2[12];

    switch (*line++) {
    case '!':
        if (!*line)
            update = -1;
        else if (strchr("+-", *line) != NULL) {
            n = strtol(line, NULL, 10);
            line = strmes(n);
            update = n < 0 ? -3 : -2;
        }
        else
            update = -2;
        break;
    case '[':
        if (progress->sid != g.sid[host]) {
            progress->sid = g.sid[host];
            progress->row = tgetrow(INT_MAX, &g.tent);
            strcpy(progress->name, line);
            memset(progress->host, 0, sizeof(progress->host));
            update = 1;
        }
        break;
    case ']':
        ++g.sid[host];
        break;
    case 'S':
        if (g.tent.up != NULL) {
            progress->host[host].filescan = strtoll(line, NULL, 10);
            update = 2;
        }
        break;
    case 'U':
        if (g.tent.up != NULL) {
            progress->host[host].upload = strtoll(line, NULL, 10);
            update = 3;
        }
        break;
    case 'D':
        if (g.tent.up != NULL) {
            progress->host[host].downloaded = strtoll(line, NULL, 10);
            update = 3;
        }
        break;
    case 'R':
        if (g.tent.up != NULL) {
            progress->host[host].fileremove = strtoll(line, NULL, 10);
            update = 4;
        }
        break;
    case 'C':
        if (g.tent.up != NULL) {
            progress->host[host].filecopy = strtoll(line, NULL, 10);
            update = 4;
        }
        break;
    }
    switch (update) {
    case -1:
        s = buffer;
        s += tsetrow(s, INT_MAX, &g.tent);
        s += sprintf(s, "%s", line);
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    case -2:
        s = buffer;
        s += tsetrow(s, INT_MAX, &g.tent);
        s += sprintf(s, "%s%s", hostname[host], line);
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    case -3:
        s = buffer;
        s += tsetrow(s, INT_MAX, &g.tent);
        s += sprintf(s, "%sError - %s", hostname[host], line);
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    case  1:
        s = buffer;
        s += tsetrow(s, progress->row, &g.tent);
        s += sprintf(s, "%s", progress->name);
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    case  2:
        s = buffer;
        s += tsetrow(s, progress->row, &g.tent);
        s += sprintf(s, "%-*s ", (int)g.namelen, progress->name);
        s += tbar(s, 0, 1, &g.tent, "[%24jd ]",
                  progress->host[0].filescan + progress->host[1].filescan );
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    case  3:
        n1 = progress->host[0].downloaded + progress->host[1].downloaded;
        n2 = progress->host[0].upload + progress->host[1].upload;
        if (n1 > 0)
            strfnum(buffer1, sizeof(buffer1)-1, n1);
        else
            *buffer1 = 0;
        if (n2 > 0)
            strfnum(buffer2, sizeof(buffer2)-1, n2);
        else
            *buffer2 = 0;
        s = buffer;
        s += tsetrow(s, progress->row, &g.tent);
        s += sprintf(s, "%-*s ", (int)g.namelen, progress->name);
        s += tbar(s, n1, n2, &g.tent, "[%11s /%11s ]", buffer1, buffer2);
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    case  4:
        n1 = progress->host[0].fileremove + progress->host[1].fileremove;
        n2 = progress->host[0].filecopy + progress->host[1].filecopy;
        if (n1 > 0)
            snprintf(buffer1, sizeof(buffer1), "%+11jd", -n1);
        else
            *buffer1 = 0;
        if (n2 > 0)
            snprintf(buffer2, sizeof(buffer2), "%+11jd",  n2);
        else
            *buffer2 = 0;
        s = buffer;
        s += tsetrow(s, progress->row, &g.tent);
        s += sprintf(s, "%-*s ", (int)g.namelen, progress->name);
        s += tbar(s, 1, 1, &g.tent, "[%11s :%11s ]", buffer1, buffer2);
        write(STDOUT_FILENO, buffer, s - buffer);
        break;
    }
}
