/* psync_psp1.c - Last modified: 20-May-2022 (kobayasy)
 *
 * Copyright (c) 2018-2022 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "psync.h"
#include "psync_psp1.h"

#ifndef EXPIRE_DEFAULT
#define EXPIRE_DEFAULT 400  /* [day] */
#endif  /* #ifndef EXPIRE_DEFAULT */
#ifndef BACKUP_DEFAULT
#define BACKUP_DEFAULT   3  /* [day] */
#endif  /* #ifndef BACKUP_DEFAULT */

typedef struct s_clist {
    struct s_clist *next, *prev;
    char *dirname;
    time_t expire;
    time_t backup;
    void *data;
    char name[1];
} CLIST;

static CLIST *new_CLIST(CLIST *clist) {
    LIST_NEW(clist);
    clist->dirname = clist->name;
    clist->expire = 0;
    clist->backup = 0;
    clist->data = NULL;
    return clist;
}

static CLIST *add_CLIST(CLIST *clist, const char *name, const char *dirname) {
    CLIST *cnew = NULL;
    size_t length;

    if (!*name)
        goto error;
    length = strlen(name) + 1;
    cnew = malloc(offsetof(CLIST, name) + length + strlen(dirname) + 1);
    if (cnew == NULL)
        goto error;
    strcpy(cnew->name, name);
    cnew->dirname = strcpy(cnew->name + length, dirname);
    cnew->expire = 0;
    cnew->backup = 0;
    cnew->data = NULL;
    LIST_INSERT_NEXT(cnew, clist);
error:
    return cnew;
}

static each_next(CLIST)

static int delete_func(CLIST *c, void *data) {
    int status = INT_MIN;

    LIST_DELETE(c);
    if (c->data != NULL)
        free(c->data);
    free(c);
    status = 0;
    return status;
}

typedef struct {
    int fdin, fdout;
    int info;
    volatile sig_atomic_t *stop;
    CLIST head, *config;
} PRIV;

static PRIV *new_priv(volatile sig_atomic_t *stop) {
    PRIV *priv = NULL;

    priv = malloc(sizeof(PRIV));
    if (priv == NULL)
        goto error;
    priv->fdin = -1, priv->fdout = -1;
    priv->info = -1;
    priv->stop = stop;
    priv->config = new_CLIST(&priv->head);
    priv->config->expire = EXPIRE_DEFAULT;
    priv->config->backup = BACKUP_DEFAULT;
error:
    return priv;
}

static void free_priv(PRIV *priv) {
    each_next_CLIST(&priv->head, delete_func, NULL, NULL);
    if (priv->head.data != NULL)
        free(priv->head.data);
    free(priv);
}

static CLIST *add_config(const char *name, const char *dirname, PRIV *priv) {
    CLIST *config = NULL;
    int seek;

    LIST_SEEK_NEXT(priv->config, name, seek);
    if (!seek)
        goto error;
    config = add_CLIST(priv->config, name, dirname);
    if (config == NULL)
        goto error;
    config->expire = priv->head.expire;
    config->backup = priv->head.backup;
error:
    return config;
}

static CLIST *seek_config(const char *name, PRIV *priv) {
    CLIST *config = NULL;
    int seek;

    LIST_SEEK_NEXT(priv->config, name, seek);
    if (seek)
        goto error;
    config = priv->config;
error:
    return config;
}

static int greeting(PRIV *priv) {
    int status = INT_MIN;
    uint32_t id = PSYNC_PROTID;

    WRITE_ONERR(id, priv->fdout, write_size, -1);
    READ_ONERR(id, priv->fdin, read_size, -1);
    if (id != PSYNC_PROTID) {
        status = -1;
        goto error;
    }
    status = 0;
error:
    return status;
}

static int run(PSYNC_MODE mode, PRIV *priv) {
    int status = INT_MIN;
    PSYNC *psync = NULL;
    int ack_local, ack_remote;

    psync = psync_new(priv->config->dirname, priv->stop);
    ack_local = psync == NULL ? -1 : 0;
    WRITE_ONERR(ack_local, priv->fdout, write_size, ERROR_PROTOCOL);
    READ_ONERR(ack_remote, priv->fdin, read_size, ERROR_PROTOCOL);
    if (!ack_local) {
        if (!ack_remote) {
            psync->expire = psync->t - priv->config->expire * (24*60*60);
            psync->backup = psync->t - priv->config->backup * (24*60*60);
            psync->fdin = priv->fdin, psync->fdout = priv->fdout;
            psync->info = priv->info;
            status = psync_run(mode, psync);
        }
        else
            status = ERROR_NOTREADYREMOTE;
        psync_free(psync), psync = NULL;
    }
    else
        status = ERROR_NOTREADYLOCAL;
error:
    if (psync != NULL)
        psync_free(psync);
    return status;
}

static int run_master(PSYNC_MODE mode, PRIV *priv) {
    int status = INT_MIN;
    size_t length, n;
    int ack;

    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(greeting(priv), ERROR_PROTOCOL);
    for (priv->config = priv->head.next; *priv->config->name; priv->config = priv->config->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        n = length = strlen(priv->config->name);
        WRITE_ONERR(n, priv->fdout, write_size, ERROR_PROTOCOL);
        if (write_size(priv->fdout, priv->config->name, length) != length) {
            status = ERROR_PROTOCOL;
            goto error;
        }
        READ_ONERR(ack, priv->fdin, read_size, ERROR_PROTOCOL);
        if (!ack) {
            if (priv->info != -1)
                dprintf(priv->info, "R%s\n", priv->config->name);
            if (ISERR(status = run(mode, priv)))
                goto error;
            if (priv->info != -1) {
                if (status)
                    dprintf(priv->info, "!%+d\n", status);
                dprintf(priv->info, "R\n");
            }
        }
    }
    length = 0;
    WRITE_ONERR(length, priv->fdout, write_size, ERROR_PROTOCOL);
    status = 0;
error:
    if (priv->info != -1 && status)
        dprintf(priv->info, "!%+d\n", status);
    return status;
}

static int run_slave(PSYNC_MODE mode, PRIV *priv) {
    int status = INT_MIN;
    size_t length;
    char *name = NULL;
    int seek;
    int ack;

    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(greeting(priv), ERROR_PROTOCOL);
    READ_ONERR(length, priv->fdin, read_size, ERROR_PROTOCOL);
    while (length > 0) {
        ONSTOP(priv->stop, ERROR_STOP);
        name = malloc(length + 1);
        if (name == NULL) {
            status = ERROR_MEMORY;
            goto error;
        }
        if (read_size(priv->fdin, name, length) != length) {
            status = ERROR_PROTOCOL;
            goto error;
        }
        name[length] = 0;
        LIST_SEEK_NEXT(priv->config, name, seek);
        free(name), name = NULL;
        ack = seek ? -1 : 0;
        WRITE_ONERR(ack, priv->fdout, write_size, ERROR_PROTOCOL);
        if (!ack) {
            if (priv->info != -1)
                dprintf(priv->info, "R%s\n", priv->config->name);
            if (ISERR(status = run(mode, priv)))
                goto error;
            if (priv->info != -1) {
                if (status)
                    dprintf(priv->info, "!%+d\n", status);
                dprintf(priv->info, "R\n");
            }
        }
        READ_ONERR(length, priv->fdin, read_size, ERROR_PROTOCOL);
    }
    status = 0;
error:
    if (name != NULL)
        free(name);
    if (priv->info != -1 && status)
        dprintf(priv->info, "!%+d\n", status);
    return status;
}

PSP *psp_new(volatile sig_atomic_t *stop) {
    return (PSP *)new_priv(stop);
}

void psp_free(PSP *psp) {
    free_priv((PRIV *)psp);
}

PSP_CONFIG *psp_config(const char *name, const char *dirname, PSP *psp) {
    CLIST *config;

    config = dirname == NULL ? seek_config(name, (PRIV *)psp) : add_config(name, dirname, (PRIV *)psp);
    return config != NULL ? (PSP_CONFIG *)&config->dirname : NULL;
}

int psp_run(PSYNC_MODE mode, PSP *psp) {
    int (*func)(PSYNC_MODE mode, PRIV *priv);

    switch (mode) {
    case PSYNC_MASTER:
    case PSYNC_MASTER_PUT:
    case PSYNC_MASTER_GET:
        func = run_master;
        break;
    case PSYNC_SLAVE:
    default:
        func = run_slave;
    }
    return func(mode, (PRIV *)psp);
}
