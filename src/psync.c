/* psync.c - Last modified: 11-Mar-2023 (kobayasy)
 *
 * Copyright (c) 2018-2023 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "common.h"
#include "progress.h"
#include "psync.h"

#ifndef EXPIRE_DEFAULT
#define EXPIRE_DEFAULT (400*24*60*60)  /* [sec] */
#endif  /* #ifndef EXPIRE_DEFAULT */
#ifndef BACKUP_DEFAULT
#define BACKUP_DEFAULT   (3*24*60*60)  /* [sec] */
#endif  /* #ifndef BACKUP_DEFAULT */
#ifndef LOADBUFFER_SIZE
#define LOADBUFFER_SIZE (16*1024)  /* [byte] */
#endif  /* #ifndef LOADBUFFER_SIZE */
#ifdef _INCLUDE_progress_h
#ifndef PROGRESS_INTERVAL
#define PROGRESS_INTERVAL 1000  /* [msec] */
#endif  /* #ifndef PROGRESS_INTERVAL */
#endif  /* #ifdef _INCLUDE_progress_h */

#ifndef SYMLINK_MAX
#define SYMLINK_MAX (PATH_MAX-1)
#endif  /* #ifndef SYMLINK_MAX */
#if LOADBUFFER_SIZE < SYMLINK_MAX
#undef LOADBUFFER_SIZE
#define LOADBUFFER_SIZE SYMLINK_MAX
#endif  /* #if LOADBUFFER_SIZE < SYMLINK_MAX */

#define FST_UPLD  0x08
#define FST_DNLD  0x80
#define FST_LTYPE 0x07
#define  FST_LREG 0x01
#define  FST_LDIR 0x02
#define  FST_LLNK 0x03
#define FST_RTYPE 0x70
#define  FST_RREG 0x10
#define  FST_RDIR 0x20
#define  FST_RLNK 0x30
typedef struct {
    time_t revision;
    time_t mtime;
    mode_t mode;
    off_t size;
    uint8_t flags;
} FST;
typedef struct s_flist {
    struct s_flist *next, *prev;
    FST st;
    char name[1];
} FLIST;

static FLIST *new_FLIST(FLIST *flist) {
    LIST_NEW(flist);
    memset(&flist->st, 0, sizeof(flist->st));
    return flist;
}

static FLIST *add_FLIST(FLIST *flist, const char *name) {
    FLIST *fnew = NULL;

    if (!*name)
        goto error;
    fnew = malloc(offsetof(FLIST, name) + strlen(name) + 1);
    if (fnew == NULL)
        goto error;
    strcpy(fnew->name, name);
    memset(&fnew->st, 0, sizeof(fnew->st));
    LIST_INSERT_NEXT(fnew, flist);
error:
    return fnew;
}

static each_next(FLIST)

static sets_next(FLIST)

static int write_FLIST(bool synced, FLIST *flist, int fd,
                       volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    size_t length, n;
    FST st;

    ONSTOP(stop, -1);
    if (*flist->name) {
        status = -1;
        goto error;
    }
    for (flist = flist->next; *flist->name; flist = flist->next) {
        ONSTOP(stop, -1);
        n = length = strlen(flist->name);
        WRITE_ONERR(n, fd, write_size, -1);
        if (write_size(fd, flist->name, length) != length) {
            status = -1;
            goto error;
        }
        st = flist->st;
        WRITE_ONERR(st.revision, fd, write_size, -1);
        WRITE_ONERR(st.mtime, fd, write_size, -1);
        WRITE_ONERR(st.mode, fd, write_size, -1);
        if (synced) {
            WRITE_ONERR(st.size, fd, write_size, -1);
            st.flags = st.flags >> 4 | st.flags << 4;
            WRITE_ONERR(st.flags, fd, write_size, -1);
        }
    }
    length = 0;
    WRITE_ONERR(length, fd, write_size, -1);
    status = 0;
error:
    return status;
}

static int read_FLIST(bool synced, FLIST *flist, int fd,
                      volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    size_t length;
    char dirname[PATH_MAX];

    ONSTOP(stop, -1);
    if (*flist->name) {
        status = -1;
        goto error;
    }
    READ_ONERR(length, fd, read_size, -1);
    while (length > 0) {
        ONSTOP(stop, -1);
        if (length > sizeof(dirname)-1) {
            status = -1;
            goto error;
        }
        if (read_size(fd, dirname, length) != length) {
            status = -1;
            goto error;
        }
        dirname[length] = 0;
        flist = add_FLIST(flist, dirname);
        if (flist == NULL) {
            status = -1;
            goto error;
        }
        READ_ONERR(flist->st.revision, fd, read_size, -1);
        READ_ONERR(flist->st.mtime, fd, read_size, -1);
        READ_ONERR(flist->st.mode, fd, read_size, -1);
        if (synced) {
            READ_ONERR(flist->st.size, fd, read_size, -1);
            READ_ONERR(flist->st.flags, fd, read_size, -1);
        }
        READ_ONERR(length, fd, read_size, -1);
    }
    status = 0;
error:
    return status;
}

static int delete_func(FLIST *f, void *data) {
    int status = INT_MIN;

    LIST_DELETE(f);
    free(f);
    status = 0;
    return status;
}

#define SYNCDIR  ".psync"
#define LASTFILE "last"
#define LOCKDIR  "lock"
#define BACKDIR  "%Y%m%d%H%M.%S%z"
#define UPFILE   "u%lu"
#define DOWNFILE "d%lu"
#define BACKFILE "%lu,%s"
#define LOGFILE  "log"
typedef struct {
    time_t t;
    time_t expire;
    time_t backup;
    int fdin, fdout;
    int info;
    volatile sig_atomic_t *stop;
    time_t tlast;
    FLIST fsynced;
    FLIST flocal, fremote;
    char dirname[1];
} PRIV;

static int lock(PRIV *priv) {
    int status = INT_MIN;
    char loadname[PATH_MAX], *p;

    p = loadname, p += sprintf(p, "%s/"SYNCDIR, priv->dirname);
    mkdir(loadname, S_IRWXU);
    strcpy(p, "/"LOCKDIR);
    status = mkdir(loadname, S_IRWXU) == -1 ? 1 : 0;
    return status;
}

static int unlock(PRIV *priv) {
    int status = INT_MIN;
    char loadname[PATH_MAX], *p;
    char pathname[PATH_MAX], *name;
    struct timeval tv[2];
    struct tm tm;

    p = loadname, p += sprintf(p, "%s/"SYNCDIR"/"LOCKDIR, priv->dirname);
    tv[0].tv_sec = priv->t, tv[0].tv_usec = 0;
    tv[1].tv_sec = priv->t, tv[1].tv_usec = 0;
    utimes(loadname, tv);
    name = pathname, name += sprintf(name, "%s/"SYNCDIR"/", priv->dirname);
    strftime(name, sizeof(pathname) - (name - pathname), BACKDIR, localtime_r(&priv->t, &tm));
    status = rename(loadname, pathname) == -1 ? 1 : 0;
    return status;
}

static PRIV *new_priv(const char *dirname,
                      volatile sig_atomic_t *stop ) {
    PRIV *priv = NULL;
    time_t t;

    if (time(&t) == -1)
        goto error;
    priv = malloc(offsetof(PRIV, dirname) + strlen(dirname) + 1);
    if (priv == NULL)
        goto error;
    strcpy(priv->dirname, dirname);
    priv->t = t;
    priv->expire = t - EXPIRE_DEFAULT;
    priv->backup = t - BACKUP_DEFAULT;
    priv->fdin = -1, priv->fdout = -1;
    priv->info = -1;
    priv->stop = stop;
    priv->tlast = -1;
    new_FLIST(&priv->fsynced);
    new_FLIST(&priv->flocal);
    new_FLIST(&priv->fremote);
    if (lock(priv)) {
        free(priv), priv = NULL;
        goto error;
    }
error:
    return priv;
}

static void free_priv(PRIV *priv) {
    unlock(priv);
    each_next_FLIST(&priv->fsynced, delete_func, NULL, NULL);
    each_next_FLIST(&priv->flocal, delete_func, NULL, NULL);
    each_next_FLIST(&priv->fremote, delete_func, NULL, NULL);
    free(priv);
}

static int save_fsynced(PRIV *priv) {
    int status = INT_MIN;
    char pathname[PATH_MAX];
    int fd = -1;
    uint32_t id;
    struct timeval tv[2];

    ONSTOP(priv->stop, ERROR_STOP);
    sprintf(pathname, "%s/"SYNCDIR"/"LOCKDIR"/"LASTFILE, priv->dirname);
    fd = creat(pathname, S_IRUSR|S_IWUSR);
    if (fd == -1) {
        status = ERROR_DMAKE;
        goto error;
    }
    id = PSYNC_FILEID;
    WRITE_ONERR(id, fd, write_size, ERROR_DWRITE);
    status = write_FLIST(false, &priv->fsynced, fd, priv->stop);
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(status, ERROR_DWRITE);
    close(fd), fd = -1;
    tv[0].tv_sec = priv->t, tv[0].tv_usec = 0;
    tv[1].tv_sec = priv->t, tv[1].tv_usec = 0;
    if (utimes(pathname, tv) == -1) {
        status = ERROR_DWRITE;
        goto error;
    }
    status = 0;
error:
    if (fd != -1)
        close(fd);
    return status;
}

static int load_fsynced(PRIV *priv) {
    int status = INT_MIN;
    struct stat st;
    char pathname[PATH_MAX];
    int fd = -1;
    uint32_t id;
    int n;

    ONSTOP(priv->stop, ERROR_STOP);
    sprintf(pathname, "%s/"SYNCDIR"/"LASTFILE, priv->dirname);
    if (stat(pathname, &st) == -1) {
        status = ERROR_DREAD;
        goto error;
    }
    fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        status = ERROR_DOPEN;
        goto error;
    }
    READ(id, fd, read_size, n);
    if (!ISERR(n) && id == PSYNC_FILEID) {
        status = read_FLIST(false, &priv->fsynced, fd, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_DREAD);
    }
    close(fd), fd = -1;
    priv->tlast = st.st_mtime;
    status = 0;
error:
    if (fd != -1)
        close(fd);
    return status;
}

static int get_flocal_r(FLIST **flocal, FLIST **flast, char *pathname, char *name,
#ifdef _INCLUDE_progress_h
                        PROGRESS *progress,
#endif  /* #ifdef _INCLUDE_progress_h */
                        volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    struct stat st;
    uint8_t ltype;
    int seek;
    FLIST *fprev;
    DIR *dir = NULL;
    char *p;
    FLIST *fdir;
    struct dirent *ent;

    if (lstat(pathname, &st) == -1) {
        status = ERROR_SREAD;
        goto error;
    }
    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        if (access(pathname, R_OK) != 0) {
            status = ERROR_FPERM;
            goto error;
        }
        ltype = FST_LREG;
        break;
    case S_IFDIR:
        if (access(pathname, R_OK|W_OK|X_OK) != 0) {
            status = ERROR_FPERM;
            goto error;
        }
        ltype = FST_LDIR;
        break;
    case S_IFLNK:
        ltype = FST_LLNK;
        break;
    default:
        status = ERROR_FTYPE;
        goto error;
    }
    LIST_SEEK_NEXT(*flocal, name, seek);
    LIST_SEEK_NEXT(*flast, name, seek);
    if (seek) {
        *flocal = add_FLIST(*flocal, name);
        if (*flocal == NULL) {
            status = ERROR_MEMORY;
            goto error;
        }
        (*flocal)->st.revision = st.st_ctime > st.st_mtime ? st.st_ctime : st.st_mtime;
    }
    else {
        fprev = (*flast)->prev;
        LIST_DELETE(*flast);
        LIST_INSERT_NEXT(*flast, *flocal);
        *flocal = *flast, *flast = fprev;
        if ((*flocal)->st.mtime != st.st_mtime)
            (*flocal)->st.revision = st.st_ctime > st.st_mtime ? st.st_ctime : st.st_mtime;
    }
    (*flocal)->st.mtime = st.st_mtime;
    (*flocal)->st.mode = st.st_mode & (S_IFMT|S_IRWXU|S_IRWXG|S_IRWXO);
    (*flocal)->st.size = st.st_size;
    (*flocal)->st.flags |= ltype;
    switch (ltype & FST_LTYPE) {
    case FST_LDIR:
        dir = opendir(pathname);
        if (dir == NULL) {
            status = ERROR_FOPEN;
            goto error;
        }
        p = name, p += strlen(p), *p++ = '/';
        fdir = *flocal;
        while ((ent = readdir(dir)) != NULL) {
            ONSTOP(stop, ERROR_STOP);
            if (!strcmp(ent->d_name, ".") ||
                !strcmp(ent->d_name, "..") )
                continue;
            strcpy(p, ent->d_name);
            if (ISERR(status = get_flocal_r(flocal, flast, pathname, name,
#ifdef _INCLUDE_progress_h
                                            progress,
#endif  /* #ifdef _INCLUDE_progress_h */
                                            stop )))
                goto error;
            if ((*flocal)->st.revision > fdir->st.revision)
                fdir->st.revision = (*flocal)->st.revision;
        }
        closedir(dir), dir = NULL;
        break;
#ifdef _INCLUDE_progress_h
    case FST_LREG:
    case FST_LLNK:
        progress_update(progress, 1);
        break;
#endif  /* #ifdef _INCLUDE_progress_h */
    }
    status = 0;
error:
    if (dir != NULL)
        closedir(dir);
    return status;
}
static int get_flocal(PRIV *priv) {
    int status = INT_MIN;
#ifdef _INCLUDE_progress_h
    PROGRESS progress;
#endif  /* #ifdef _INCLUDE_progress_h */
    struct stat st;
    DIR *dir = NULL;
    char pathname[PATH_MAX], *name;
    FLIST *flocal, *flast;
    struct dirent *ent;

    ONSTOP(priv->stop, ERROR_STOP);
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'S');
#endif  /* #ifdef _INCLUDE_progress_h */
    if (stat(priv->dirname, &st) == -1) {
        status = ERROR_SREAD;
        goto error;
    }
    switch (st.st_mode & S_IFMT) {
    case S_IFDIR:
        if (access(priv->dirname, R_OK|W_OK|X_OK) != 0) {
            status = ERROR_FPERM;
            goto error;
        }
        break;
    default:
        status = ERROR_FTYPE;
        goto error;
    }
    dir = opendir(priv->dirname);
    if (dir == NULL) {
        status = ERROR_FOPEN;
        goto error;
    }
    name = pathname, name += sprintf(name, "%s/", priv->dirname);
    flocal = &priv->flocal, flast = &priv->fsynced;
    while ((ent = readdir(dir)) != NULL) {
        ONSTOP(priv->stop, ERROR_STOP);
        if (!strcmp(ent->d_name, ".") ||
            !strcmp(ent->d_name, "..") ||
            !strcmp(ent->d_name, SYNCDIR) )
            continue;
        strcpy(name, ent->d_name);
        if (ISERR(status = get_flocal_r(&flocal, &flast, pathname, name,
#ifdef _INCLUDE_progress_h
                                        &progress,
#endif  /* #ifdef _INCLUDE_progress_h */
                                        priv->stop ))) {
            if (priv->info != -1)
                switch (status) {
                case ERROR_FTYPE:
                case ERROR_FPERM:
                    dprintf(priv->info, "!Unsupported file: %s\n", name);
                    break;
                }
            goto error;
        }
    }
    closedir(dir), dir = NULL;
#ifdef _INCLUDE_progress_h
    progress_term(&progress);
#endif  /* #ifdef _INCLUDE_progress_h */
    status = 0;
error:
    if (dir != NULL)
        closedir(dir);
    return status;
}

static int add_deleted_func(SETS sets, FLIST *flocal, FLIST *flast, void *data) {
    int status = INT_MIN;
    PRIV *priv = data;

    switch (sets) {
    case SETS_1AND2:
        LIST_DELETE(flast);
        free(flast);
        break;
    case SETS_1NOT2:
        break;
    case SETS_2NOT1:
        LIST_DELETE(flast);
        switch (flast->st.mode & S_IFMT) {
        case 0:  /* deleted */
            if (flast->st.revision > priv->expire)
                LIST_INSERT_PREV(flast, flocal);
            else
                free(flast);
            break;
        default:
            flast->st.revision = priv->tlast + 1;
            flast->st.mtime = 0;
            flast->st.mode = 0;
            flast->st.size = 0;
            LIST_INSERT_PREV(flast, flocal);
        }
        break;
    }
    status = 0;
    return status;
}

static int make_fsynced_func(SETS sets, FLIST *flocal, FLIST *fremote, void *data) {
    int status = INT_MIN;
    PRIV *priv = data;

    switch (sets) {
    case SETS_1AND2:
        if (flocal->st.revision < fremote->st.revision) {
            fremote->st.flags |= flocal->st.flags;
            if (flocal->st.mtime != fremote->st.mtime)
                fremote->st.flags |= FST_DNLD;
            LIST_DELETE(flocal);
            free(flocal);
            LIST_DELETE(fremote);
            LIST_INSERT_PREV(fremote, &priv->fsynced);
        }
        else {
            flocal->st.flags |= fremote->st.flags;
            if (flocal->st.revision > fremote->st.revision &&
                flocal->st.mtime != fremote->st.mtime )
                flocal->st.flags |= FST_UPLD;
            LIST_DELETE(fremote);
            free(fremote);
            LIST_DELETE(flocal);
            LIST_INSERT_PREV(flocal, &priv->fsynced);
        }
        break;
    case SETS_1NOT2:
        flocal->st.flags |= FST_UPLD;
        LIST_DELETE(flocal);
        LIST_INSERT_PREV(flocal, &priv->fsynced);
        break;
    case SETS_2NOT1:
        fremote->st.flags |= FST_DNLD;
        LIST_DELETE(fremote);
        LIST_INSERT_PREV(fremote, &priv->fsynced);
        break;
    }
    status = 0;
    return status;
}

static int make_fsynced_put_func(SETS sets, FLIST *flocal, FLIST *fremote, void *data) {
    int status = INT_MIN;
    PRIV *priv = data;

    switch (sets) {
    case SETS_1AND2:
        flocal->st.flags |= fremote->st.flags;
        if (flocal->st.revision != fremote->st.revision &&
            flocal->st.mtime != fremote->st.mtime )
            flocal->st.flags |= FST_UPLD;
        LIST_DELETE(fremote);
        free(fremote);
        LIST_DELETE(flocal);
        LIST_INSERT_PREV(flocal, &priv->fsynced);
        break;
    case SETS_1NOT2:
        flocal->st.flags |= FST_UPLD;
        LIST_DELETE(flocal);
        LIST_INSERT_PREV(flocal, &priv->fsynced);
        break;
    case SETS_2NOT1:
        fremote->st.flags |= FST_UPLD;
        fremote->st.revision = priv->t;
        fremote->st.mtime = 0;
        fremote->st.mode = 0;
        fremote->st.size = 0;
        LIST_DELETE(fremote);
        LIST_INSERT_PREV(fremote, &priv->fsynced);
        break;
    }
    status = 0;
    return status;
}

static int make_fsynced_get_func(SETS sets, FLIST *flocal, FLIST *fremote, void *data) {
    int status = INT_MIN;
    PRIV *priv = data;

    switch (sets) {
    case SETS_1AND2:
        fremote->st.flags |= flocal->st.flags;
        if (flocal->st.revision != fremote->st.revision &&
            flocal->st.mtime != fremote->st.mtime )
            fremote->st.flags |= FST_DNLD;
        LIST_DELETE(flocal);
        free(flocal);
        LIST_DELETE(fremote);
        LIST_INSERT_PREV(fremote, &priv->fsynced);
        break;
    case SETS_1NOT2:
        flocal->st.flags |= FST_DNLD;
        flocal->st.revision = priv->t;
        flocal->st.mtime = 0;
        flocal->st.mode = 0;
        flocal->st.size = 0;
        LIST_DELETE(flocal);
        LIST_INSERT_PREV(flocal, &priv->fsynced);
        break;
    case SETS_2NOT1:
        fremote->st.flags |= FST_DNLD;
        LIST_DELETE(fremote);
        LIST_INSERT_PREV(fremote, &priv->fsynced);
        break;
    }
    status = 0;
    return status;
}

static int preload(PRIV *priv) {
    int status = INT_MIN;
#ifdef _INCLUDE_progress_h
    PROGRESS progress;
#endif  /* #ifdef _INCLUDE_progress_h */
    char pathname[PATH_MAX], *name;
    char loadname[PATH_MAX], *p;
    unsigned long count;
    FLIST *fsynced;
    char buffer[SYMLINK_MAX+1];

    ONSTOP(priv->stop, ERROR_STOP);
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'U');
#endif  /* #ifdef _INCLUDE_progress_h */
    name = pathname, name += sprintf(name, "%s/", priv->dirname);
    p = loadname, p += sprintf(p, "%s/"SYNCDIR"/"LOCKDIR"/", priv->dirname);
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_UPLD|FST_LTYPE)) {
        case FST_UPLD|FST_LREG:
        case FST_UPLD|FST_LLNK:
            strcpy(name, fsynced->name);
            sprintf(p, UPFILE, ++count);
            switch (fsynced->st.flags & FST_LTYPE) {
            case FST_LREG:
                if (link(pathname, loadname) == -1) {
                    status = ERROR_FLINK;
                    goto error;
                }
                break;
            case FST_LLNK:
                if (readlink(pathname, buffer, sizeof(buffer)) != fsynced->st.size) {
                    status = ERROR_FREAD;
                    goto error;
                }
                buffer[fsynced->st.size] = 0;
                if (symlink(buffer, loadname) == -1) {
                    status = ERROR_FWRITE;
                    goto error;
                }
                break;
            }
#ifdef _INCLUDE_progress_h
            progress_update(&progress, fsynced->st.size);
#endif  /* #ifdef _INCLUDE_progress_h */
            break;
        }
    }
#ifdef _INCLUDE_progress_h
    progress_term(&progress);
#endif  /* #ifdef _INCLUDE_progress_h */
    status = 0;
error:
    return status;
}

static int upload(PRIV *priv) {
    int status = INT_MIN;
    char loadname[PATH_MAX], *p;
    unsigned long count;
    FLIST *fsynced;
    off_t size;
    int fd = -1;
    ssize_t n;
    char buffer[LOADBUFFER_SIZE];

    ONSTOP(priv->stop, ERROR_STOP);
    p = loadname, p += sprintf(p, "%s/"SYNCDIR"/"LOCKDIR"/", priv->dirname);
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_UPLD|FST_LTYPE)) {
        case FST_UPLD|FST_LREG:
        case FST_UPLD|FST_LLNK:
            sprintf(p, UPFILE, ++count);
            size = fsynced->st.size;
            switch (fsynced->st.flags & FST_LTYPE) {
            case FST_LREG:
                fd = open(loadname, O_RDONLY);
                if (fd == -1) {
                    status = ERROR_FOPEN;
                    goto error;
                }
                while (size > 0) {
                    ONSTOP(priv->stop, ERROR_STOP);
                    n = read_size(fd, buffer, size > sizeof(buffer) ? sizeof(buffer) : size);
                    if (n == -1) {
                        status = ERROR_FREAD;
                        goto error;
                    }
                    if (write_size(priv->fdout, buffer, n) != n) {
                        status = ERROR_FUPLD;
                        goto error;
                    }
                    size -= n;
                }
                close(fd), fd = -1;
                break;
            case FST_LLNK:
                if (readlink(loadname, buffer, size) != size) {
                    status = ERROR_FREAD;
                    goto error;
                }
                if (write_size(priv->fdout, buffer, size) != size) {
                    status = ERROR_FUPLD;
                    goto error;
                }
                break;
            }
            if (unlink(loadname) == -1) {
                status = ERROR_FREMOVE;
                goto error;
            }
            break;
        }
    }
    status = 0;
error:
    if (fd != -1)
        close(fd);
    return status;
}

static int download(PRIV *priv) {
    int status = INT_MIN;
#ifdef _INCLUDE_progress_h
    PROGRESS progress;
#endif  /* #ifdef _INCLUDE_progress_h */
    char loadname[PATH_MAX], *p;
    unsigned long count;
    FLIST *fsynced;
    off_t size;
    int fd = -1;
    ssize_t n;
    char buffer[LOADBUFFER_SIZE];
    struct timeval tv[2];

    ONSTOP(priv->stop, ERROR_STOP);
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'D');
#endif  /* #ifdef _INCLUDE_progress_h */
    p = loadname, p += sprintf(p, "%s/"SYNCDIR"/"LOCKDIR"/", priv->dirname);
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE)) {
        case FST_DNLD|FST_RREG:
        case FST_DNLD|FST_RLNK:
            sprintf(p, DOWNFILE, ++count);
            size = fsynced->st.size;
            switch (fsynced->st.flags & FST_RTYPE) {
            case FST_RREG:
                fd = creat(loadname, S_IRUSR|S_IWUSR);
                if (fd == -1) {
                    status = ERROR_FMAKE;
                    goto error;
                }
                while (size > 0) {
                    ONSTOP(priv->stop, ERROR_STOP);
                    n = read_size(priv->fdin, buffer, size > sizeof(buffer) ? sizeof(buffer) : size);
                    if (n == -1) {
                        status = ERROR_FDNLD;
                        goto error;
                    }
                    if (write_size(fd, buffer, n) != n) {
                        status = ERROR_FWRITE;
                        goto error;
                    }
                    size -= n;
#ifdef _INCLUDE_progress_h
                    progress_update(&progress, n);
#endif  /* #ifdef _INCLUDE_progress_h */
                }
                close(fd), fd = -1;
                if (chmod(loadname, fsynced->st.mode & (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
                    status = ERROR_SWRITE;
                    goto error;
                }
                break;
            case FST_RLNK:
                if (size > sizeof(buffer)-1) {
                    status = ERROR_SYSTEM;
                    goto error;
                }
                if (read_size(priv->fdin, buffer, size) != size) {
                    status = ERROR_FDNLD;
                    goto error;
                }
                buffer[size] = 0;
                if (symlink(buffer, loadname) == -1) {
                    status = ERROR_FWRITE;
                    goto error;
                }
#ifdef _INCLUDE_progress_h
                progress_update(&progress, size);
#endif  /* #ifdef _INCLUDE_progress_h */
                break;
            }
            tv[0].tv_sec = fsynced->st.mtime, tv[0].tv_usec = 0;
            tv[1].tv_sec = fsynced->st.mtime, tv[1].tv_usec = 0;
            if (lutimes(loadname, tv) == -1) {
                status = ERROR_SWRITE;
                goto error;
            }
            break;
        }
    }
#ifdef _INCLUDE_progress_h
    progress_term(&progress);
#endif  /* #ifdef _INCLUDE_progress_h */
    status = 0;
error:
    if (fd != -1)
        close(fd);
    return status;
}

static int commit(PRIV *priv) {
    int status = INT_MIN;
    char pathname[PATH_MAX], *name;
    char loadname[PATH_MAX], *p;
#ifdef _INCLUDE_progress_h
    PROGRESS progress;
#endif  /* #ifdef _INCLUDE_progress_h */
    unsigned long count;
    FLIST *fsynced;
    struct timeval tv[2];

    ONSTOP(priv->stop, ERROR_STOP);
    name = pathname, name += sprintf(name, "%s/", priv->dirname);
    p = loadname, p += sprintf(p, "%s/"SYNCDIR"/"LOCKDIR"/", priv->dirname);
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'R');
#endif  /* #ifdef _INCLUDE_progress_h */
    count = 0;
    for (fsynced = priv->fsynced.prev; *fsynced->name; fsynced = fsynced->prev)
        switch (fsynced->st.flags & (FST_DNLD|FST_LTYPE)) {
        case FST_DNLD|FST_LREG:
            strcpy(name, fsynced->name);
            sprintf(p, BACKFILE, ++count, basename(name));
            if (rename(pathname, loadname) == -1) {
                status = ERROR_FMOVE;
                goto error;
            }
#ifdef _INCLUDE_progress_h
            progress_update(&progress, 1);
#endif  /* #ifdef _INCLUDE_progress_h */
            break;
        case FST_DNLD|FST_LLNK:
            strcpy(name, fsynced->name);
            if (unlink(pathname) == -1) {
                status = ERROR_FREMOVE;
                goto error;
            }
#ifdef _INCLUDE_progress_h
            progress_update(&progress, 1);
#endif  /* #ifdef _INCLUDE_progress_h */
            break;
        case FST_DNLD|FST_LDIR:
            switch (fsynced->st.flags & FST_RTYPE) {
            case FST_RREG:
            case FST_RLNK:
            case 0:  /* deleted */
                strcpy(name, fsynced->name);
                if (rmdir(pathname) == -1) {
                    status = ERROR_FREMOVE;
                    goto error;
                }
                break;
            }
            break;
        }
#ifdef _INCLUDE_progress_h
    progress_term(&progress);
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'C');
#endif  /* #ifdef _INCLUDE_progress_h */
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next)
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE)) {
        case FST_DNLD|FST_RREG:
        case FST_DNLD|FST_RLNK:
            strcpy(name, fsynced->name);
            sprintf(p, DOWNFILE, ++count);
            if (rename(loadname, pathname) == -1) {
                status = ERROR_FMOVE;
                goto error;
            }
#ifdef _INCLUDE_progress_h
            progress_update(&progress, 1);
#endif  /* #ifdef _INCLUDE_progress_h */
            break;
        case FST_DNLD|FST_RDIR:
            switch (fsynced->st.flags & FST_LTYPE) {
            case FST_LREG:
            case FST_LLNK:
            case 0:  /* deleted */
                strcpy(name, fsynced->name);
                if (mkdir(pathname, fsynced->st.mode & (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
                    status = ERROR_FMAKE;
                    goto error;
                }
                break;
            case FST_LDIR:
                strcpy(name, fsynced->name);
                if (chmod(pathname, fsynced->st.mode & (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
                    status = ERROR_SWRITE;
                    goto error;
                }
                break;
            }
            break;
        }
#ifdef _INCLUDE_progress_h
    progress_term(&progress);
#endif  /* #ifdef _INCLUDE_progress_h */
    for (fsynced = priv->fsynced.prev; *fsynced->name; fsynced = fsynced->prev)
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE)) {
        case FST_DNLD|FST_RDIR:
            strcpy(name, fsynced->name);
            tv[0].tv_sec = fsynced->st.mtime, tv[0].tv_usec = 0;
            tv[1].tv_sec = fsynced->st.mtime, tv[1].tv_usec = 0;
            if (lutimes(pathname, tv) == -1) {
                status = ERROR_SWRITE;
                goto error;
            }
            break;
        }
    strcpy(name, SYNCDIR"/"LASTFILE);
    strcpy(p, LASTFILE);
    if (rename(loadname, pathname) == -1) {
        status = ERROR_DWRITE;
        goto error;
    }
    status = 0;
error:
    return status;
}

static int logging(PRIV *priv) {
    int status = INT_MIN;
    char pathname[PATH_MAX];
    FILE *fp = NULL;
    struct {
        unsigned long backup;
        unsigned long deleted;
        unsigned long added;
        unsigned long modified;
        unsigned long uploaded;
    } count;
    FLIST *fsynced;

    ONSTOP(priv->stop, ERROR_STOP);
    sprintf(pathname, "%s/"SYNCDIR"/"LOCKDIR"/"LOGFILE, priv->dirname);
    fp = fopen(pathname, "w");
    if (fp == NULL) {
        status = ERROR_DMAKE;
        goto error;
    }
    memset(&count, 0, sizeof(count));
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE|FST_LTYPE)) {
        case FST_DNLD|FST_LREG:
            ++count.backup;
        case FST_DNLD|FST_LDIR:
        case FST_DNLD|FST_LLNK:
            ++count.deleted;
            break;
        case FST_DNLD|FST_RREG:
        case FST_DNLD|FST_RDIR:
        case FST_DNLD|FST_RLNK:
            ++count.added;
            break;
        case FST_DNLD|FST_LREG|FST_RREG:
        case FST_DNLD|FST_LREG|FST_RDIR:
        case FST_DNLD|FST_LREG|FST_RLNK:
            ++count.backup;
        case FST_DNLD|FST_LDIR|FST_RREG:
        case FST_DNLD|FST_LDIR|FST_RDIR:
        case FST_DNLD|FST_LDIR|FST_RLNK:
        case FST_DNLD|FST_LLNK|FST_RREG:
        case FST_DNLD|FST_LLNK|FST_RDIR:
        case FST_DNLD|FST_LLNK|FST_RLNK:
            ++count.modified;
            break;
        }
        switch (fsynced->st.flags & FST_UPLD) {
        case FST_UPLD:
            ++count.uploaded;
            break;
        }
    }
    fprintf(fp, "%lu deleted, %lu added, %lu modified / %lu uploaded\n",
            count.deleted, count.added, count.modified, count.uploaded );
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE|FST_LTYPE)) {
        case FST_DNLD|FST_LREG:
            strcpy(pathname, fsynced->name);
            fprintf(fp, "D %s -> %lu,%s\n", fsynced->name, count.backup--, basename(pathname));
            break;
        case FST_DNLD|FST_LDIR:
            fprintf(fp, "D %s/\n", fsynced->name);
            break;
        case FST_DNLD|FST_LLNK:
            fprintf(fp, "D %s@\n", fsynced->name);
            break;
        }
    }
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE|FST_LTYPE)) {
        case FST_DNLD|FST_RREG:
            fprintf(fp, "A %s\n", fsynced->name);
            break;
        case FST_DNLD|FST_RDIR:
            fprintf(fp, "A %s/\n", fsynced->name);
            break;
        case FST_DNLD|FST_RLNK:
            fprintf(fp, "A %s@\n", fsynced->name);
            break;
        }
    }
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE|FST_LTYPE)) {
        case FST_DNLD|FST_RREG|FST_LREG:
            strcpy(pathname, fsynced->name);
            fprintf(fp, "M %s -> %lu,%s\n", fsynced->name, count.backup--, basename(pathname));
            break;
        case FST_DNLD|FST_RREG|FST_LDIR:
        case FST_DNLD|FST_RREG|FST_LLNK:
            fprintf(fp, "M %s\n", fsynced->name);
            break;
        case FST_DNLD|FST_RDIR|FST_LREG:
            strcpy(pathname, fsynced->name);
            fprintf(fp, "M %s/ -> %lu,%s\n", fsynced->name, count.backup--, basename(pathname));
            break;
        case FST_DNLD|FST_RDIR|FST_LDIR:
        case FST_DNLD|FST_RDIR|FST_LLNK:
            fprintf(fp, "M %s/\n", fsynced->name);
            break;
        case FST_DNLD|FST_RLNK|FST_LREG:
            strcpy(pathname, fsynced->name);
            fprintf(fp, "M %s@ -> %lu,%s\n", fsynced->name, count.backup--, basename(pathname));
            break;
        case FST_DNLD|FST_RLNK|FST_LDIR:
        case FST_DNLD|FST_RLNK|FST_LLNK:
            fprintf(fp, "M %s@\n", fsynced->name);
            break;
        }
    }
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_UPLD|FST_LTYPE)) {
        case FST_UPLD|FST_LREG:
            fprintf(fp, "U %s\n", fsynced->name);
            break;
        case FST_UPLD|FST_LDIR:
            fprintf(fp, "U %s/\n", fsynced->name);
            break;
        case FST_UPLD|FST_LLNK:
            fprintf(fp, "U %s@\n", fsynced->name);
            break;
        case FST_UPLD:
            fprintf(fp, "U %s%%\n", fsynced->name);
            break;
        }
    }
    status = 0;
error:
    if (fp != NULL)
        fclose(fp);
    return status;
}

static int clean_r(char *pathname, char *name,
                   volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    struct stat st;
    DIR *dir = NULL;
    struct dirent *ent;

    if (lstat(pathname, &st) == -1) {
        status = ERROR_DREAD;
        goto error;
    }
    switch (st.st_mode & S_IFMT) {
    case S_IFDIR:
        dir = opendir(pathname);
        if (dir == NULL) {
            status = ERROR_DOPEN;
            goto error;
        }
        *name++ = '/';
        while ((ent = readdir(dir)) != NULL) {
            ONSTOP(stop, ERROR_STOP);
            if (!strcmp(ent->d_name, ".") ||
                !strcmp(ent->d_name, "..") )
                continue;
            strcpy(name, ent->d_name);
            if (ISERR(status = clean_r(pathname, name + strlen(name), stop)))
                goto error;
        }
        *--name = 0;
        closedir(dir), dir = NULL;
        if (rmdir(pathname) == -1) {
            status = ERROR_DREMOVE;
            goto error;
        }
        break;
    default:
        if (unlink(pathname) == -1) {
            status = ERROR_DREMOVE;
            goto error;
        }
    }
    status = 0;
error:
    if (dir != NULL)
        closedir(dir);
    return status;
}
static int clean(PRIV *priv) {
    int status = INT_MIN;
    char pathname[PATH_MAX], *p;
    DIR *dir = NULL;
    struct dirent *ent;
    struct stat st;
 
    ONSTOP(priv->stop, ERROR_STOP);
    p = pathname, p += sprintf(p, "%s/"SYNCDIR, priv->dirname);
    dir = opendir(pathname);
    if (dir == NULL) {
        status = ERROR_DOPEN;
        goto error;
    }
    *p++ = '/';
    while ((ent = readdir(dir)) != NULL) {
        ONSTOP(priv->stop, ERROR_STOP);
        if (!strcmp(ent->d_name, ".") ||
            !strcmp(ent->d_name, "..") ||
            !strcmp(ent->d_name, LASTFILE) ||
            !strcmp(ent->d_name, LOCKDIR) )
            continue;
        strcpy(p, ent->d_name);
        if (stat(pathname, &st) == -1) {
            status = ERROR_DREAD;
            goto error;
        }
        if (st.st_mtime > priv->backup)
            continue;
        if (ISERR(status = clean_r(pathname, p + strlen(p), priv->stop)))
            goto error;
    }
    *--p = 0;
    closedir(dir), dir = NULL;
    status = 0;
error:
    if (dir != NULL)
        closedir(dir);
    return status;
}

typedef struct {
    PRIV *priv;
    int status;
    pthread_t tid;
} LOAD_PARAM;

static void *upload_thread(void *data) {
    LOAD_PARAM *param = data;

    param->status = upload(param->priv);
    return NULL;
}

static int run(int (*func)(SETS sets, FLIST *f1, FLIST *f2, void *data), PRIV *priv) {
    int status = INT_MIN;
    LOAD_PARAM upload_param = {
        .priv   = priv,
        .status = INT_MIN
    };

    load_fsynced(priv);
    if (ISERR(status = get_flocal(priv)))
        goto error;
    ONSTOP(priv->stop, ERROR_STOP);
    status = sets_next_FLIST(&priv->flocal, &priv->fsynced, add_deleted_func, priv, priv->stop);
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(status, ERROR_SYSTEM);
    if (func != NULL) {
        status = read_FLIST(true, &priv->fremote, priv->fdin, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_SDNLD);
        status = sets_next_FLIST(&priv->flocal, &priv->fremote, func, priv, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_SYSTEM);
        status = write_FLIST(true, &priv->fsynced, priv->fdout, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_SUPLD);
    }
    else {
        status = write_FLIST(true, &priv->flocal, priv->fdout, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_SUPLD);
        status = each_next_FLIST(&priv->flocal, delete_func, NULL, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_SYSTEM);
        status = read_FLIST(true, &priv->fsynced, priv->fdin, priv->stop);
        ONSTOP(priv->stop, ERROR_STOP);
        ONERR(status, ERROR_SDNLD);
    }
    if (ISERR(status = save_fsynced(priv)))
        goto error;
    if (ISERR(status = preload(priv)))
        goto error;
    if (pthread_create(&upload_param.tid, NULL, upload_thread, &upload_param) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    if (ISERR(status = download(priv)))
        goto error;
    if (pthread_join(upload_param.tid, NULL) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    ONERR(upload_param.status, upload_param.status);
    if (ISERR(status = commit(priv)))
        goto error;
    if (ISERR(status = logging(priv)))
        goto error;
    if (ISERR(status = clean(priv)))
        goto error;
    status = 0;
error:
    return status;
}

PSYNC *psync_new(const char *dirname,
                 volatile sig_atomic_t *stop ) {
    return (PSYNC *)new_priv(dirname, stop);
}

void psync_free(PSYNC *psync) {
    free_priv((PRIV *)psync);
}

int psync_run(PSYNC_MODE mode, PSYNC *psync) {
    int (*func)(SETS sets, FLIST *f1, FLIST *f2, void *data);

    switch (mode) {
    case PSYNC_MASTER:
        func = make_fsynced_func;
        break;
    case PSYNC_MASTER_PUT:
        func = make_fsynced_put_func;
        break;
    case PSYNC_MASTER_GET:
        func = make_fsynced_get_func;
        break;
    case PSYNC_SLAVE:
    default:
        func = NULL;
    }
    return run(func, (PRIV *)psync);
}
