/* info.c - Last modified: 22-Mar-2026 (kobayasy)
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

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "tpbar.h"
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
    size_t n;

    if (status < 0) {
        mes = nmes;
        n = sizeof(nmes)/sizeof(*nmes);
        status = -(status + 1);
    }
    else {
        mes = pmes;
        n = sizeof(pmes)/sizeof(*pmes);
    }
    if (status >= n)
        status = 0;
    return mes[status];
}

static int str_catib(STR *str, size_t length, intmax_t num) {
    int status = -1;
    const char *unit = "KMGTPEZYRQ";

    if (unit[0] && (num >= 1024 || num <= -1024)) {
        while (unit[1] && (num >= 1024*1024 || num <= -1024*1024))
            ++unit, num /= 1024;
        if (ISERR(str_catf(str, "%*.3f%ciB", (int)(length - 3), (double)num / 1024, *unit)))
            goto error;
    }
    else
        if (ISERR(str_catf(str, "%*jdB", (int)(length - 1), num)))
            goto error;
    status = 0;
error:
    return status;
}

#define INFONFD 2
#define INFOEOL '\n'

typedef struct s_ilist {
    struct s_ilist *next, *prev;
    int row;
    struct {
        char str[64];
        intmax_t scan;
        intmax_t upload;
        intmax_t download;
        intmax_t remove;
        intmax_t copy;
    } host[INFONFD];
    char name[1];
} ILIST;

static ILIST *new_ILIST(ILIST *ilist) {
    LIST_NEW(ilist);
    ilist->row = 0;
    memset(ilist->host, 0, sizeof(ilist->host));
    return ilist;
}

static ILIST *add_ILIST(ILIST *ilist, const char *name, int row) {
    ILIST *inew = NULL;

    if (!*name)
        goto error;
    inew = malloc(offsetof(ILIST, name) + strlen(name) + 1);
    if (!inew)
        goto error;
    strcpy(inew->name, name);
    inew->row = row;
    memset(inew->host, 0, sizeof(inew->host));
    LIST_INSERT_NEXT(inew, ilist);
error:
    return inew;
}

static each_next(ILIST)

static int delete_func(ILIST *i, void *data) {
    int status = INT_MIN;

    LIST_DELETE(i);
    free(i);
    status = 0;
    return status;
}

static struct {
    ILIST ilist, *i[INFONFD];
    TPBAR tpbar;
    size_t namelen;
    volatile sig_atomic_t *stop;
    pthread_t tid;
} priv;

static int info_print(unsigned int host, const char *line) {
    int status = INT_MIN;
    ILIST *i;
    int update;
    int seek;
    STR buffer;
    char str[1024];
    intmax_t n1, n2;
    char *s;
    unsigned int n;

    i = host < INFONFD ? priv.i[host] : NULL;
    update = 0;
    switch (*line++) {
    case '!':
        switch (*line) {
        case '+':
        case '-':
            line = strmes(strtol(line, NULL, 10));
            break;
        }
        update = -1;
        break;
    case '[':
        if (i) {
            LIST_SEEK_NEXT(i, line, seek);
            if (seek) {
                i = add_ILIST(i, line, tpbar_getrow(INT_MAX, &priv.tpbar));
                if (!i)
                    goto error;
                for (n = 0; n < INFONFD; ++n) {
                    STR_INIT(buffer, i->host[n].str);
                    if (ISERR(str_catf(&buffer, "[ %-25s ]", priv.ilist.host[n])))
                        goto error;
                }
                update = 1;
            }
            priv.i[host] = i;
        }
        break;
    case ']':
        break;
    case 'S':
        if (i) {
            n1 = i->host[host].scan = strtoll(line, NULL, 10);
            if (n1 > 0) {
                STR_INIT(buffer, i->host[host].str);
                if (ISERR(tpbar_printf(&buffer, 0, 1, &priv.tpbar, "[%26jd ]", n1)))
                    break;
                update = 1;
            }
        }
        break;
    case 'U':
        if (i) {
            n1 = i->host[host].upload = strtoll(line, NULL, 10);
            n2 = i->host[host ^ 1].download;
            STR_INIT(buffer, str);
            if (ISERR(str_cats(&buffer, "[", NULL)) ||
                ISERR(str_catib(&buffer, 12, n2)) ||
                ISERR(str_cats(&buffer, " /", NULL)) ||
                ISERR(str_catib(&buffer, 12, n1)) ||
                ISERR(str_cats(&buffer, " ]", NULL)) )
                break;
            STR_INIT(buffer, i->host[host ^ 1].str);
            if (ISERR(tpbar_printf(&buffer, n2, n1, &priv.tpbar, str)))
                break;
            update = 1;
        }
        break;
    case 'D':
        if (i) {
            n1 = i->host[host ^ 1].upload;
            n2 = i->host[host].download = strtoll(line, NULL, 10);
            STR_INIT(buffer, str);
            if (ISERR(str_cats(&buffer, "[", NULL)) ||
                ISERR(str_catib(&buffer, 12, n2)) ||
                ISERR(str_cats(&buffer, " /", NULL)) ||
                ISERR(str_catib(&buffer, 12, n1)) ||
                ISERR(str_cats(&buffer, " ]", NULL)) )
                break;
            STR_INIT(buffer, i->host[host].str);
            if (ISERR(tpbar_printf(&buffer, n2, n1, &priv.tpbar, str)))
                break;
            update = 1;
        }
        break;
    case 'R':
        if (i) {
            n1 = i->host[host].remove = strtoll(line, NULL, 10);
            n2 = i->host[host].copy;
            STR_INIT(buffer, i->host[host].str);
            if (ISERR(tpbar_printf(&buffer, 1, 1, &priv.tpbar, "[%+12jd :%+12jd ]", -n1, n2)))
                break;
            update = 1;
        }
        break;
    case 'C':
        if (i) {
            n1 = i->host[host].remove;
            n2 = i->host[host].copy = strtoll(line, NULL, 10);
            STR_INIT(buffer, i->host[host].str);
            if (ISERR(tpbar_printf(&buffer, 1, 1, &priv.tpbar, "[%+12jd :%+12jd ]", -n1, n2)))
                break;
            update = 1;
        }
        break;
    }
    switch (update) {
    case -1:
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, INT_MAX, &priv.tpbar)))
            break;
        if (host < INFONFD) {
            if (ISERR(str_cats(&buffer, priv.ilist.host[host], ": ", NULL)))
                break;
            s = i->name;
            if (*s)
                if (ISERR(str_cats(&buffer, s, " - ", NULL)))
                    break;
        }
        if (ISERR(str_cats(&buffer, line, NULL)))
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    case  1:
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, i->row, &priv.tpbar)))
            break;
        s = i->name;
        if (priv.tpbar.co > priv.namelen + 60) {
            if (ISERR(str_catf(&buffer, "%-*s", (int)priv.namelen, s)))
                break;
            for (n = 0; n < INFONFD; ++n) {
                s = i->host[n].str;
                if (*s)
                    if (ISERR(str_cats(&buffer, " ", s, NULL)))
                        break;
            }
        }
        else {
            if (ISERR(str_cats(&buffer, s, NULL)))
                break;
        }
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    }
    status = 0;
error:
    return status;
}

static void *info_thread(void *data) {
    int *fd = data;
    unsigned int nfd;
    unsigned int n;
    struct pollfd fds[INFONFD];
    char buffer[1024];
    ssize_t size;
    char *s, *p;

    nfd = 0;
    for (n = 0; n < INFONFD; ++n)
        fds[n].fd = fd[n], fds[n].events = POLLIN, ++nfd;
    while (nfd > 0) {
        if (ISSTOP(priv.stop))
            goto error;
        switch (poll(fds, INFONFD, -1)) {
        case -1:
            switch (errno) {
            case EINTR:
                continue;
            }
        case  0:  /* timeout */
            goto error;
        }
        for (n = 0; n < INFONFD; ++n)
            if (fds[n].fd != -1 && fds[n].revents) {
                if (!(fds[n].revents & POLLIN)) {
                    fds[n].fd = -1, --nfd;
                    continue;
                }
                size = read(fds[n].fd, buffer, sizeof(buffer));
                switch (size) {
                case -1:
                    switch (errno) {
                    case EINTR:
                        continue;
                    }
                case  0:
                    fds[n].fd = -1, --nfd;
                    continue;
                }
                p = buffer;
                while (p = memchr(s = p, INFOEOL, size), p)  {
                    *p++ = 0;
                    if (ISERR(info_print(n, s)))
                        goto error;
                    size -= p - s;
                }
            }
    }
    info_print(UINT_MAX, "!");
error:
    return NULL;
}

int info_start(int *fds, size_t namelen,
               volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    unsigned int n;

    new_ILIST(&priv.ilist);
    strcpy(priv.ilist.host[0].str, "Local");
    strcpy(priv.ilist.host[1].str, "Remote");
    for (n = 0; n < INFONFD; ++n)
        priv.i[n] = &priv.ilist;
    tpbar_init(&priv.tpbar);
    priv.namelen = namelen;
    priv.stop = stop;
    if (pthread_create(&priv.tid, NULL, info_thread, fds) != 0) {
        status = -1;
        goto error;
    }
    status = 0;
error:
    return status;
}

int info_stop(void) {
    int status = INT_MIN;

    if (pthread_join(priv.tid, NULL) != 0) {
        status = -1;
        goto error;
    }
    status = 0;
error:
    each_next_ILIST(&priv.ilist, delete_func, NULL, NULL);
    return status;
}
