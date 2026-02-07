/* psync_psp.c - Last modified: 07-Feb-2026 (kobayasy)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif  /* #ifdef HAVE_CONFIG_H */

#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "common.h"
#include "psync.h"
#include "psync_psp.h"

typedef struct s_clist {
    struct s_clist *next, *prev;
    char *dirname;
    time_t expire;
    time_t backup;
    char name[1];
} CLIST;

static CLIST *new_CLIST(CLIST *clist) {
    LIST_NEW(clist);
    clist->dirname = clist->name;
    clist->expire = 0;
    clist->backup = 0;
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
    LIST_INSERT_NEXT(cnew, clist);
error:
    return cnew;
}

static each_next(CLIST)

static sets_next(CLIST)

static int write_CLIST(CLIST *clist, int fd,
                       volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    size_t length, n;

    ONSTOP(stop, -1);
    if (*clist->name) {
        status = -1;
        goto error;
    }
    for (clist = clist->next; *clist->name; clist = clist->next) {
        ONSTOP(stop, -1);
        n = length = strlen(clist->name);
        WRITE_ONERR(n, fd, write_size, -1);
        if (write_size(fd, clist->name, length) != length) {
            status = -1;
            goto error;
        }
    }
    length = 0;
    WRITE_ONERR(length, fd, write_size, -1);
    status = 0;
error:
    return status;
}

static int read_CLIST(CLIST *clist, int fd,
                      volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    size_t length;
    char *name = NULL;

    ONSTOP(stop, -1);
    if (*clist->name) {
        status = -1;
        goto error;
    }
    READ_ONERR(length, fd, read_size, -1);
    while (length > 0) {
        ONSTOP(stop, -1);
        name = malloc(length + 1);
        if (name == NULL) {
            status = -1;
            goto error;
        }
        if (read_size(fd, name, length) != length) {
            status = -1;
            goto error;
        }
        name[length] = 0;
        clist = add_CLIST(clist, name, "");
        if (clist == NULL) {
            status = -1;
            goto error;
        }
        free(name), name = NULL;
        READ_ONERR(length, fd, read_size, -1);
    }
    status = 0;
error:
    free(name);
    return status;
}

static int delete_func(CLIST *c, void *data) {
    int status = INT_MIN;

    LIST_DELETE(c);
    free(c);
    status = 0;
    return status;
}

typedef struct {
    int fdin, fdout;
    int info;
    volatile sig_atomic_t *stop;
    CLIST *config;
    CLIST clocal, cremote;
} PRIV;

static PRIV *new_priv(volatile sig_atomic_t *stop) {
    PRIV *priv = NULL;

    priv = malloc(sizeof(PRIV));
    if (priv == NULL)
        goto error;
    priv->fdin = -1, priv->fdout = -1;
    priv->info = -1;
    priv->stop = stop;
    priv->config = new_CLIST(&priv->clocal);
    priv->config->expire = EXPIRE_DEFAULT;
    priv->config->backup = BACKUP_DEFAULT;
    new_CLIST(&priv->cremote);
error:
    return priv;
}

static void free_priv(PRIV *priv) {
    each_next_CLIST(&priv->clocal, delete_func, NULL, NULL);
    each_next_CLIST(&priv->cremote, delete_func, NULL, NULL);
    free(priv);
}

static CLIST *add_config(const char *name, const char *dirname, PRIV *priv) {
    CLIST *config = NULL;
    int seek;

    if (!*name)
        goto error;
    LIST_SEEK_NEXT(priv->config, name, seek);
    if (!seek)
        goto error;
    config = add_CLIST(priv->config, name, dirname);
    if (config == NULL)
        goto error;
    config->expire = priv->clocal.expire;
    config->backup = priv->clocal.backup;
error:
    return config;
}

static CLIST *seek_config(const char *name, PRIV *priv) {
    CLIST *config = NULL;
    int seek;

    if (!*name)
        config = &priv->clocal;
    else {
        LIST_SEEK_NEXT(priv->config, name, seek);
        if (seek)
            goto error;
        config = priv->config;
    }
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

static int psync(PRIV *priv, CLIST *config) {
    int status = INT_MIN;
    PSYNC *psync = NULL;
    int ack_local, ack_remote, n;

    psync = psync_new(config->dirname, priv->stop);
    n = ack_local = psync == NULL ? -1 : 0;
    WRITE_ONERR(n, priv->fdout, write_size, ERROR_PROTOCOL);
    READ_ONERR(ack_remote, priv->fdin, read_size, ERROR_PROTOCOL);
    if (!ack_local) {
        if (!ack_remote) {
            psync->expire = psync->t - config->expire;
            psync->backup = psync->t - config->backup;
            psync->fdin = priv->fdin, psync->fdout = priv->fdout;
            psync->info = priv->info;
            status = psync_run(psync);
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

static int psync_func(SETS sets, CLIST *clocal, CLIST *cremote, void *data) {
    int status = INT_MIN;
    PRIV *priv = data;

    switch (sets) {
    case SETS_1AND2:
        if (priv->info != -1)
            dprintf(priv->info, "[%s\n", clocal->name);
        if (ISERR(status = psync(priv, clocal)))
            goto error;
        if (priv->info != -1) {
            if (status)
                dprintf(priv->info, "!%+d\n", status);
            dprintf(priv->info, "]\n");
        }
        break;
    case SETS_1NOT2:
        break;
    case SETS_2NOT1:
        break;
    }
    status = 0;
error:
    return status;
}

typedef struct {
    PRIV *priv;
    int status;
    pthread_t tid;
} PARAM;

static void *write_CLIST_thread(void *data) {
    PARAM *param = data;

    param->status = write_CLIST(&param->priv->clocal, param->priv->fdout, param->priv->stop);
    return NULL;
}

static int run(PRIV *priv) {
    int status = INT_MIN;
    PARAM param = {
        .priv   = priv,
        .status = INT_MIN
    };

    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(greeting(priv), ERROR_PROTOCOL);
    if (pthread_create(&param.tid, NULL, write_CLIST_thread, &param) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    status = read_CLIST(&priv->cremote, priv->fdin, priv->stop);
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(status, ERROR_SDNLD);
    if (pthread_join(param.tid, NULL) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(param.status, ERROR_SUPLD);
    status = sets_next_CLIST(&priv->clocal, &priv->cremote, psync_func, priv, priv->stop);
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(status, ERROR_SYSTEM);
    each_next_CLIST(&priv->cremote, delete_func, NULL, NULL);
    status = 0;
error:
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

int psp_run(PSP *psp) {
    return run((PRIV *)psp);
}
