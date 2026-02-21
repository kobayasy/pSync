/* psync.c - Last modified: 21-Feb-2026 (kobayasy)
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include "common.h"
#include "progress.h"
#include "psync.h"

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

#ifdef MISSING_LUTIMES
static int lutimes(const char *path, struct timeval times[2]) {
    unsigned int n;
    struct timespec ts[2];

    for (n = 0; n < 2; ++n)
        ts[n].tv_sec = times[n].tv_sec, ts[n].tv_nsec = times[n].tv_usec * 1000;
    return utimensat(AT_FDCWD, path, ts, AT_SYMLINK_NOFOLLOW);
}
#endif  /* #ifdef MISSING_LUTIMES */

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
    char dirname[];
} PRIV;

static int lock(PRIV *priv) {
    int status = INT_MIN;
    STR loadname;
    char str[PATH_MAX];

    STR_INIT(loadname, str);
    ONERR(str_cats(&loadname, priv->dirname, "/"SYNCDIR, NULL), -1);
    mkdir(loadname.s, S_IRWXU);
    ONERR(str_cats(&loadname, "/"LOCKDIR, NULL), -1);
    if (mkdir(loadname.s, S_IRWXU) != -1)
        status = 0;
    else
        status = 1;
error:
    return status;
}

static int unlock(PRIV *priv) {
    int status = INT_MIN;
    STR pathname, loadname;
    char str1[PATH_MAX], str2[PATH_MAX];
    struct tm tm;
    struct timeval tv[2];

    STR_INIT(pathname, str1);
    STR_INIT(loadname, str2);
    ONERR(str_cats(&pathname, priv->dirname, "/"SYNCDIR"/", NULL), -1);
    ONERR(str_cats(&loadname, pathname.s, LOCKDIR, NULL), -1);
    ONERR(str_catt(&pathname, BACKDIR, localtime_r(&priv->t, &tm)), -1);
    tv[0].tv_sec = priv->t, tv[0].tv_usec = 0;
    tv[1].tv_sec = priv->t, tv[1].tv_usec = 0;
    utimes(loadname.s, tv);
    if (rename(loadname.s, pathname.s) != -1)
        status = 0;
    else {
        ONERR(str_catf(&pathname, "~%d", getpid()), -1);
        if (rename(loadname.s, pathname.s) != -1)
            status = 0;
        else
            status = 1;
    }
error:
    return status;
}

static PRIV *new_priv(const char *dirname,
                      volatile sig_atomic_t *stop ) {
    PRIV *priv = NULL;
    time_t t;

    if (time(&t) == -1)
        goto error;
    priv = malloc(sizeof(*priv) + strlen(dirname) + 1);
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
    STR pathname;
    char str[PATH_MAX];
    int fd = -1;
    uint32_t id;
    struct timeval tv[2];

    ONSTOP(priv->stop, ERROR_STOP);
    STR_INIT(pathname, str);
    ONERR(str_cats(&pathname, priv->dirname, "/"SYNCDIR"/"LOCKDIR"/"LASTFILE, NULL), ERROR_MEMORY);
    fd = creat(pathname.s, S_IRUSR|S_IWUSR);
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
    if (utimes(pathname.s, tv) == -1) {
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
    STR pathname;
    char str[PATH_MAX];
    struct stat st;
    int fd = -1;
    uint32_t id;
    int n;

    ONSTOP(priv->stop, ERROR_STOP);
    STR_INIT(pathname, str);
    ONERR(str_cats(&pathname, priv->dirname, "/"SYNCDIR"/"LASTFILE, NULL), ERROR_MEMORY);
    if (stat(pathname.s, &st) == -1) {
        status = ERROR_DREAD;
        goto error;
    }
    fd = open(pathname.s, O_RDONLY);
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

static int get_flocal_r(FLIST **flocal, FLIST **flast, STR pathname, char *name, const char *entname,
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
    FLIST *fdir;
    struct dirent *ent;

    ONERR(str_cats(&pathname, entname, NULL), ERROR_MEMORY);
    if (lstat(pathname.s, &st) == -1) {
        status = ERROR_SREAD;
        goto error;
    }
    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        if (access(pathname.s, R_OK) != 0) {
            status = ERROR_FPERM;
            goto error;
        }
        ltype = FST_LREG;
        break;
    case S_IFDIR:
        if (access(pathname.s, R_OK|W_OK|X_OK) != 0) {
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
        dir = opendir(pathname.s);
        if (dir == NULL) {
            status = ERROR_FOPEN;
            goto error;
        }
        ONERR(str_cats(&pathname, "/", NULL), ERROR_MEMORY);
        fdir = *flocal;
        while ((ent = readdir(dir)) != NULL) {
            ONSTOP(stop, ERROR_STOP);
            if (!strcmp(ent->d_name, ".") ||
                !strcmp(ent->d_name, "..") )
                continue;
            if (ISERR(status = get_flocal_r(flocal, flast, pathname, name, ent->d_name,
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
    STR pathname;
    char str[PATH_MAX];
    struct stat st;
    DIR *dir = NULL;
    FLIST *flocal, *flast;
    struct dirent *ent;

    ONSTOP(priv->stop, ERROR_STOP);
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'S');
#endif  /* #ifdef _INCLUDE_progress_h */
    STR_INIT(pathname, str);
    ONERR(str_cats(&pathname, priv->dirname, NULL), ERROR_MEMORY);
    if (stat(pathname.s, &st) == -1) {
        status = ERROR_SREAD;
        goto error;
    }
    switch (st.st_mode & S_IFMT) {
    case S_IFDIR:
        if (access(pathname.s, R_OK|W_OK|X_OK) != 0) {
            status = ERROR_FPERM;
            goto error;
        }
        break;
    default:
        status = ERROR_FTYPE;
        goto error;
    }
    dir = opendir(pathname.s);
    if (dir == NULL) {
        status = ERROR_FOPEN;
        goto error;
    }
    ONERR(str_cats(&pathname, "/", NULL), ERROR_MEMORY);
    flocal = &priv->flocal, flast = &priv->fsynced;
    while ((ent = readdir(dir)) != NULL) {
        ONSTOP(priv->stop, ERROR_STOP);
        if (!strcmp(ent->d_name, ".") ||
            !strcmp(ent->d_name, "..") ||
            !strcmp(ent->d_name, SYNCDIR) )
            continue;
        if (ISERR(status = get_flocal_r(&flocal, &flast, pathname, pathname.e, ent->d_name,
#ifdef _INCLUDE_progress_h
                                        &progress,
#endif  /* #ifdef _INCLUDE_progress_h */
                                        priv->stop ))) {
            if (priv->info != -1)
                switch (status) {
                case ERROR_FTYPE:
                case ERROR_FPERM:
                    dprintf(priv->info, "!Unsupported file: %s\n", ent->d_name);
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

static int preload(PRIV *priv) {
    int status = INT_MIN;
#ifdef _INCLUDE_progress_h
    PROGRESS progress;
#endif  /* #ifdef _INCLUDE_progress_h */
    STR pathname, loadname;
    char str1[PATH_MAX], str2[PATH_MAX];
    unsigned long count;
    FLIST *fsynced;
    char buffer[SYMLINK_MAX+1];

    ONSTOP(priv->stop, ERROR_STOP);
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'U');
#endif  /* #ifdef _INCLUDE_progress_h */
    STR_INIT(pathname, str1);
    STR_INIT(loadname, str2);
    ONERR(str_cats(&pathname, priv->dirname, "/", NULL), ERROR_MEMORY);
    ONERR(str_cats(&loadname, pathname.s, SYNCDIR"/"LOCKDIR"/", NULL), ERROR_MEMORY);
    pathname.hold = true;
    loadname.hold = true;
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_UPLD|FST_LTYPE)) {
        case FST_UPLD|FST_LREG:
        case FST_UPLD|FST_LLNK:
            ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
            ONERR(str_catf(&loadname, UPFILE, ++count), ERROR_MEMORY);
            switch (fsynced->st.flags & FST_LTYPE) {
            case FST_LREG:
                if (link(pathname.s, loadname.s) == -1) {
                    status = ERROR_FLINK;
                    goto error;
                }
                break;
            case FST_LLNK:
                if (readlink(pathname.s, buffer, sizeof(buffer)) != fsynced->st.size) {
                    status = ERROR_FREAD;
                    goto error;
                }
                buffer[fsynced->st.size] = 0;
                if (symlink(buffer, loadname.s) == -1) {
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
    STR loadname;
    char str[PATH_MAX];
    unsigned long count;
    FLIST *fsynced;
    off_t size;
    int fd = -1;
    ssize_t n;
    char buffer[LOADBUFFER_SIZE];

    ONSTOP(priv->stop, ERROR_STOP);
    STR_INIT(loadname, str);
    ONERR(str_cats(&loadname, priv->dirname, "/"SYNCDIR"/"LOCKDIR"/", NULL), ERROR_MEMORY);
    loadname.hold = true;
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_UPLD|FST_LTYPE)) {
        case FST_UPLD|FST_LREG:
        case FST_UPLD|FST_LLNK:
            ONERR(str_catf(&loadname, UPFILE, ++count), ERROR_MEMORY);
            size = fsynced->st.size;
            switch (fsynced->st.flags & FST_LTYPE) {
            case FST_LREG:
                fd = open(loadname.s, O_RDONLY);
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
                if (readlink(loadname.s, buffer, size) != size) {
                    status = ERROR_FREAD;
                    goto error;
                }
                if (write_size(priv->fdout, buffer, size) != size) {
                    status = ERROR_FUPLD;
                    goto error;
                }
                break;
            }
            if (unlink(loadname.s) == -1) {
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
    STR loadname;
    char str[PATH_MAX];
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
    STR_INIT(loadname, str);
    ONERR(str_cats(&loadname, priv->dirname, "/"SYNCDIR"/"LOCKDIR"/", NULL), ERROR_MEMORY);
    loadname.hold = true;
    count = 0;
    for (fsynced = priv->fsynced.next; *fsynced->name; fsynced = fsynced->next) {
        ONSTOP(priv->stop, ERROR_STOP);
        switch (fsynced->st.flags & (FST_DNLD|FST_RTYPE)) {
        case FST_DNLD|FST_RREG:
        case FST_DNLD|FST_RLNK:
            ONERR(str_catf(&loadname, DOWNFILE, ++count), ERROR_MEMORY);
            size = fsynced->st.size;
            switch (fsynced->st.flags & FST_RTYPE) {
            case FST_RREG:
                fd = creat(loadname.s, S_IRUSR|S_IWUSR);
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
                if (chmod(loadname.s, fsynced->st.mode & (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
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
                if (symlink(buffer, loadname.s) == -1) {
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
            if (lutimes(loadname.s, tv) == -1) {
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
    STR pathname, loadname;
    char str1[PATH_MAX], str2[PATH_MAX];
#ifdef _INCLUDE_progress_h
    PROGRESS progress;
#endif  /* #ifdef _INCLUDE_progress_h */
    unsigned long count;
    FLIST *fsynced;
    struct timeval tv[2];

    ONSTOP(priv->stop, ERROR_STOP);
    STR_INIT(pathname, str1);
    STR_INIT(loadname, str2);
    ONERR(str_cats(&pathname, priv->dirname, "/", NULL), ERROR_MEMORY);
    ONERR(str_cats(&loadname, pathname.s, SYNCDIR"/"LOCKDIR"/", NULL), ERROR_MEMORY);
    pathname.hold = true;
    loadname.hold = true;
#ifdef _INCLUDE_progress_h
    progress_init(&progress, 0, priv->info, PROGRESS_INTERVAL, 'R');
#endif  /* #ifdef _INCLUDE_progress_h */
    count = 0;
    for (fsynced = priv->fsynced.prev; *fsynced->name; fsynced = fsynced->prev)
        switch (fsynced->st.flags & (FST_DNLD|FST_LTYPE)) {
        case FST_DNLD|FST_LREG:
            ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
            ONERR(str_catf(&loadname, BACKFILE, ++count, basename_c(fsynced->name)), ERROR_MEMORY);
            if (rename(pathname.s, loadname.s) == -1) {
                status = ERROR_FMOVE;
                goto error;
            }
#ifdef _INCLUDE_progress_h
            progress_update(&progress, 1);
#endif  /* #ifdef _INCLUDE_progress_h */
            break;
        case FST_DNLD|FST_LLNK:
            ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
            if (unlink(pathname.s) == -1) {
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
                ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
                if (rmdir(pathname.s) == -1) {
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
            ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
            ONERR(str_catf(&loadname, DOWNFILE, ++count), ERROR_MEMORY);
            if (rename(loadname.s, pathname.s) == -1) {
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
                ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
                if (mkdir(pathname.s, fsynced->st.mode & (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
                    status = ERROR_FMAKE;
                    goto error;
                }
                break;
            case FST_LDIR:
                ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
                if (chmod(pathname.s, fsynced->st.mode & (S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
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
            ONERR(str_cats(&pathname, fsynced->name, NULL), ERROR_MEMORY);
            tv[0].tv_sec = fsynced->st.mtime, tv[0].tv_usec = 0;
            tv[1].tv_sec = fsynced->st.mtime, tv[1].tv_usec = 0;
            if (lutimes(pathname.s, tv) == -1) {
                status = ERROR_SWRITE;
                goto error;
            }
            break;
        }
    ONERR(str_cats(&pathname, SYNCDIR"/"LASTFILE, NULL), ERROR_MEMORY);
    ONERR(str_cats(&loadname, LASTFILE, NULL), ERROR_MEMORY);
    if (rename(loadname.s, pathname.s) == -1) {
        status = ERROR_DWRITE;
        goto error;
    }
    status = 0;
error:
    return status;
}

static int logging(PRIV *priv) {
    int status = INT_MIN;
    STR pathname;
    char str[PATH_MAX];
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
    STR_INIT(pathname, str);
    ONERR(str_cats(&pathname, priv->dirname, "/"SYNCDIR"/"LOCKDIR"/"LOGFILE, NULL), ERROR_MEMORY);
    fp = fopen(pathname.s, "w");
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
            fprintf(fp, "D %s -> %lu,%s\n", fsynced->name, count.backup--, basename_c(fsynced->name));
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
            fprintf(fp, "M %s -> %lu,%s\n", fsynced->name, count.backup--, basename_c(fsynced->name));
            break;
        case FST_DNLD|FST_RREG|FST_LDIR:
        case FST_DNLD|FST_RREG|FST_LLNK:
            fprintf(fp, "M %s\n", fsynced->name);
            break;
        case FST_DNLD|FST_RDIR|FST_LREG:
            fprintf(fp, "M %s/ -> %lu,%s\n", fsynced->name, count.backup--, basename_c(fsynced->name));
            break;
        case FST_DNLD|FST_RDIR|FST_LDIR:
        case FST_DNLD|FST_RDIR|FST_LLNK:
            fprintf(fp, "M %s/\n", fsynced->name);
            break;
        case FST_DNLD|FST_RLNK|FST_LREG:
            fprintf(fp, "M %s@ -> %lu,%s\n", fsynced->name, count.backup--, basename_c(fsynced->name));
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

static int clean_r(STR pathname, const char *entname, time_t backup,
                   volatile sig_atomic_t *stop ) {
    int status = INT_MIN;
    struct stat st;
    DIR *dir = NULL;
    struct dirent *ent;

    ONERR(str_cats(&pathname, "/", entname, NULL), ERROR_MEMORY);
    if (stat(pathname.s, &st) == -1) {
        status = ERROR_DREAD;
        goto error;
    }
    if (backup == -1 || st.st_mtime < backup)
        switch (st.st_mode & S_IFMT) {
        case S_IFDIR:
            dir = opendir(pathname.s);
            if (dir == NULL) {
                status = ERROR_DOPEN;
                goto error;
            }
            while ((ent = readdir(dir)) != NULL) {
                ONSTOP(stop, ERROR_STOP);
                if (!strcmp(ent->d_name, ".") ||
                    !strcmp(ent->d_name, "..") )
                    continue;
                if (ISERR(status = clean_r(pathname, ent->d_name, -1, stop)))
                    goto error;
            }
            closedir(dir), dir = NULL;
            ONERR(str_cats(&pathname, "", NULL), ERROR_MEMORY);
            if (rmdir(pathname.s) == -1) {
                status = ERROR_DREMOVE;
                goto error;
            }
            break;
        default:
            if (unlink(pathname.s) == -1) {
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
    STR pathname;
    char str[PATH_MAX];
    DIR *dir = NULL;
    struct dirent *ent;
 
    ONSTOP(priv->stop, ERROR_STOP);
    STR_INIT(pathname, str);
    ONERR(str_cats(&pathname, priv->dirname, "/"SYNCDIR, NULL), ERROR_MEMORY);
    dir = opendir(pathname.s);
    if (dir == NULL) {
        status = ERROR_DOPEN;
        goto error;
    }
    while ((ent = readdir(dir)) != NULL) {
        ONSTOP(priv->stop, ERROR_STOP);
        if (!strcmp(ent->d_name, ".") ||
            !strcmp(ent->d_name, "..") ||
            !strcmp(ent->d_name, LASTFILE) ||
            !strcmp(ent->d_name, LOCKDIR) )
            continue;
        if (ISERR(status = clean_r(pathname, ent->d_name, priv->backup, priv->stop)))
            goto error;
    }
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
} PARAM;

static void *write_FLIST_thread(void *data) {
    PARAM *param = data;

    param->status = write_FLIST(true, &param->priv->flocal, param->priv->fdout, param->priv->stop);
    return NULL;
}

static void *upload_thread(void *data) {
    PARAM *param = data;

    param->status = upload(param->priv);
    return NULL;
}

static int run(PRIV *priv) {
    int status = INT_MIN;
    PARAM param = {
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
    if (pthread_create(&param.tid, NULL, write_FLIST_thread, &param) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    status = read_FLIST(true, &priv->fremote, priv->fdin, priv->stop);
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(status, ERROR_SDNLD);
    if (pthread_join(param.tid, NULL) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(param.status, ERROR_SUPLD);
    status = sets_next_FLIST(&priv->flocal, &priv->fremote, make_fsynced_func, priv, priv->stop);
    ONSTOP(priv->stop, ERROR_STOP);
    ONERR(status, ERROR_SYSTEM);
    if (ISERR(status = save_fsynced(priv)))
        goto error;
    if (ISERR(status = preload(priv)))
        goto error;
    if (pthread_create(&param.tid, NULL, upload_thread, &param) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    if (ISERR(status = download(priv)))
        goto error;
    if (pthread_join(param.tid, NULL) != 0) {
        status = ERROR_SYSTEM;
        goto error;
    }
    ONERR(param.status, param.status);
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

int psync_run(PSYNC *psync) {
    return run((PRIV *)psync);
}
