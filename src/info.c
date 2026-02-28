/* info.c - Last modified: 28-Feb-2026 (kobayasy)
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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
    int sid;
    int row;
    char *name;
    struct {
        intmax_t filescan;
        intmax_t upload;
        intmax_t downloaded;
        intmax_t fileremove;
        intmax_t filecopy;
    } host[2];
} PROGRESS;
typedef struct {
    int sid[2];
    PROGRESS progress[2];
    TPBAR tpbar;
    size_t namelen;
    char buffer[];
} PRIV;

void *info_new(size_t namelen) {
    PRIV *priv = NULL;
    unsigned int host;

    priv = malloc(sizeof(*priv) + (namelen + 1) * 2);
    if (!priv)
        goto error;
    for (host = 0; host < 2; ++host) {
        priv->sid[host] = 0;
        priv->progress[host].sid = INT_MIN;
        priv->progress[host].name = priv->buffer + (namelen + 1) * host;
    }
    tpbar_init(&priv->tpbar);
    priv->namelen = namelen;
    if (priv->tpbar.co < priv->namelen + 1+29+1)
        priv->tpbar.up = NULL;
error:
    return priv;
}

void info_free(void *p) {
    PRIV *priv = p;

    free(priv);
}

void info_print(void *p, unsigned int host, const char *line) {
    PRIV *priv = p;
    int update = INT_MIN;
    static const char *hostname[] = {"Local: ", "Remote: "};
    PROGRESS *progress = &priv->progress[priv->sid[host] & 1];
    STR buffer;
    char str[1024];
    intmax_t n1, n2;
    char str1[13], str2[13];

    switch (*line++) {
    case '.':
        update = 0;
        break;
    case '!':
        if (strchr("+-", *line))
            line = strmes(strtol(line, NULL, 10));
        update = -1;
        break;
    case '[':
        if (progress->sid != priv->sid[host]) {
            progress->sid = priv->sid[host];
            progress->row = tpbar_getrow(INT_MAX, &priv->tpbar);
            strcpy(progress->name, line);
            memset(progress->host, 0, sizeof(progress->host));
            update = 1;
        }
        break;
    case ']':
        ++priv->sid[host];
        break;
    case 'S':
        if (priv->tpbar.up) {
            progress->host[host].filescan = strtoll(line, NULL, 10);
            update = 2;
        }
        break;
    case 'U':
        if (priv->tpbar.up) {
            progress->host[host].upload = strtoll(line, NULL, 10);
            update = 3;
        }
        break;
    case 'D':
        if (priv->tpbar.up) {
            progress->host[host].downloaded = strtoll(line, NULL, 10);
            update = 3;
        }
        break;
    case 'R':
        if (priv->tpbar.up) {
            progress->host[host].fileremove = strtoll(line, NULL, 10);
            update = 4;
        }
        break;
    case 'C':
        if (priv->tpbar.up) {
            progress->host[host].filecopy = strtoll(line, NULL, 10);
            update = 4;
        }
        break;
    }
    switch (update) {
    case  0:
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, INT_MAX, &priv->tpbar)) ||
            ISERR(str_cats(&buffer, line, NULL)) )
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    case -1:
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, INT_MAX, &priv->tpbar)) ||
            ISERR(str_cats(&buffer, hostname[host], NULL)) )
            break;
        if (progress->sid == priv->sid[host])
            if (ISERR(str_cats(&buffer, progress->name, " - ", NULL)))
                break;
        if (ISERR(str_cats(&buffer, line, NULL)))
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    case  1:
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, progress->row, &priv->tpbar)) ||
            ISERR(str_cats(&buffer, progress->name, NULL)) )
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    case  2:
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, progress->row, &priv->tpbar)) ||
            ISERR(str_catf(&buffer, "%-*s ", (int)priv->namelen, progress->name)) ||
            ISERR(tpbar_printf(&buffer, 0, 1, &priv->tpbar, "[%26jd ]",
                               progress->host[0].filescan + progress->host[1].filescan )) )
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    case  3:
        n1 = progress->host[0].downloaded + progress->host[1].downloaded;
        n2 = progress->host[0].upload + progress->host[1].upload;
        STR_INIT(buffer, str1);
        if (n1 > 0 && ISERR(str_catib(&buffer, buffer.size - 1, n1)))
            break;
        STR_INIT(buffer, str2);
        if (n2 > 0 && ISERR(str_catib(&buffer, buffer.size - 1, n2)))
            break;
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, progress->row, &priv->tpbar)) ||
            ISERR(str_catf(&buffer, "%-*s ", (int)priv->namelen, progress->name)) ||
            ISERR(tpbar_printf(&buffer, n1, n2, &priv->tpbar, "[%12s /%12s ]",
                               str1, str2 )) )
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    case  4:
        n1 = progress->host[0].fileremove + progress->host[1].fileremove;
        n2 = progress->host[0].filecopy + progress->host[1].filecopy;
        STR_INIT(buffer, str1);
        if (n1 > 0 && ISERR(str_catf(&buffer, "%+12jd", -n1)))
            break;
        STR_INIT(buffer, str2);
        if (n2 > 0 && ISERR(str_catf(&buffer, "%+12jd",  n2)))
            break;
        STR_INIT(buffer, str);
        if (ISERR(tpbar_setrow(&buffer, progress->row, &priv->tpbar)) ||
            ISERR(str_catf(&buffer, "%-*s ", (int)priv->namelen, progress->name)) ||
            ISERR(tpbar_printf(&buffer, 1, 1, &priv->tpbar, "[%12s :%12s ]",
                               str1, str2 )) )
            break;
        write(STDOUT_FILENO, buffer.s, str_len(&buffer));
        break;
    }
}
