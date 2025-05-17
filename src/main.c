/* main.c - Last modified: 17-May-2025 (kobayasy)
 *
 * Copyright (C) 2018-2025 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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
#include "config.h"
#endif  /* #ifdef HAVE_CONFIG_H */

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "psync_psp1.h"
#include "popen3.h"
#include "info.h"

#ifndef PACKAGE_STRING
#define PACKAGE_STRING "pSync"
#endif  /* #ifndef PACKAGE_STRING */
#ifndef PACKAGE_TARNAME
#define PACKAGE_TARNAME "psync"
#endif  /* #ifndef PACKAGE_TARNAME */
#ifndef CONFFILE
#define CONFFILE "."PACKAGE_TARNAME".conf"
#endif  /* #ifndef CONFFILE */
#ifndef SSH
#define SSH "ssh"
#endif  /* #ifndef SSH */
#ifndef SSHOPTS
#define SSHOPTS "-qCp%u"
#endif  /* #ifndef SSHOPTS */
#ifndef SSHPORT
#define SSHPORT 22
#endif  /* #ifndef SSHPORT */

#define ERROR_ENVS (-26)
#define ERROR_CONF (-27)
#define ERROR_ARGS (-28)

static struct {
    volatile sig_atomic_t stop;
    size_t namelen;
} priv = {
    .stop = 0
};

static void *info_thread(void *data) {
    int *infos = data;
    struct pollfd fds[2];
    unsigned int host;
    char buffer[1024];
    ssize_t size;
    char *line, *s;
    unsigned int n;

    info_init(priv.namelen);
    for (n = 0; n < 2; ++n)
        fds[n].events = POLLIN;
    while (n > 0) {
        if (ISSTOP(&priv.stop))
            goto error;
        n = 0;
        for (host = 0; host < 2; ++host)
            if (infos[host] != -1) {
                fds[n].fd = infos[host];
                ++n;
            }
        if (!n)
            continue;
        if (poll(fds, n, -1) < 1)
            goto error;
        n = 0;
        for (host = 0; host < 2; ++host)
            if (infos[host] != -1) {
                if (fds[n].revents & POLLIN) {
                    size = read(infos[host], buffer, sizeof(buffer));
                    switch (size) {
                    case -1:
                        goto error;
                    case  0:  /* end of file */
                        infos[host] = -1;
                        break;
                    default:
                        buffer[size] = 0;
                        s = buffer;
                        while ((s = strchr(line = s, '\n')) != NULL) {
                            *s++ = 0;
                            info_print(host, line);
                        }
                    }
                }
                else if (fds[n].revents)
                    infos[host] = -1;
                ++n;
            }
    }
    info_print(0, "!");
error:
    return NULL;
}

typedef struct {
    struct {
        bool set;
        struct sigaction act;
    } sighup, sigint, sigterm, sigpipe;
} SIGACT;

static void sigactinit(SIGACT *oldact) {
    oldact->sighup.set = false;
    oldact->sigint.set = false;
    oldact->sigterm.set = false;
    oldact->sigpipe.set = false;
}

static void sigactreset(SIGACT *oldact) {
    if (oldact->sighup.set)
        sigaction(SIGHUP, &oldact->sighup.act, NULL), oldact->sighup.set = false;
    if (oldact->sigint.set)
        sigaction(SIGINT, &oldact->sigint.act, NULL), oldact->sigint.set = false;
    if (oldact->sigterm.set)
        sigaction(SIGTERM, &oldact->sigterm.act, NULL), oldact->sigterm.set = false;
    if (oldact->sigpipe.set)
        sigaction(SIGPIPE, &oldact->sigpipe.act, NULL), oldact->sigpipe.set = false;
}

static int sigactset(void (*handler)(int signo), SIGACT *oldact) {
    int status = INT_MIN;
    struct sigaction act;

    act.sa_handler = handler;
    act.sa_flags = 0;
    if (sigemptyset(&act.sa_mask) == -1) {
        status = -1;
        goto error;
    }
    if (sigaction(SIGHUP, &act, &oldact->sighup.act) == -1) {
        sigactreset(oldact);
        status = -1;
        goto error;
    }
    oldact->sighup.set = true;
    if (sigaction(SIGINT, &act, &oldact->sigint.act) == -1) {
        sigactreset(oldact);
        status = -1;
        goto error;
    }
    oldact->sigint.set = true;
    if (sigaction(SIGTERM, &act, &oldact->sigterm.act) == -1) {
        sigactreset(oldact);
        status = -1;
        goto error;
    }
    oldact->sigterm.set = true;
    if (sigaction(SIGPIPE, &act, &oldact->sigpipe.act) == -1) {
        sigactreset(oldact);
        status = -1;
        goto error;
    }
    oldact->sigpipe.set = true;
    status = 0;
error:
    return status;
}

static void sighandler(int signo) {
    priv.stop = 1;
}

typedef struct {
    PSYNC_MODE mode;
    PSP *psp;
    bool verbose;
} RUN_PARAM;

static int run_local(int fdin, int fdout, int info, pid_t pid, void *data) {
    int status = INT_MIN;
    RUN_PARAM *param = data;
    int info_pipe[2] = {-1, -1};
    int infos[2];
    pthread_t info_tid;
    SIGACT oldact;

    param->psp->fdin = fdout, param->psp->fdout = fdin;
    if (param->verbose) {
        if (pipe(info_pipe) == -1) {
            status = ERROR_SYSTEM;
            goto error;
        }
        infos[0] = info_pipe[0];
        infos[1] = info;
        param->psp->info = info_pipe[1];
        if (pthread_create(&info_tid, NULL, info_thread, infos) != 0) {
            status = ERROR_SYSTEM;
            goto error;
        }
    }
    sigactinit(&oldact);
    ONERR(sigactset(sighandler, &oldact), ERROR_SYSTEM);
    status = psp_run(param->mode, param->psp);
    sigactreset(&oldact);
    if (param->verbose) {
        close(info_pipe[1]), info_pipe[1] = -1;
        if (pthread_join(info_tid, NULL) != 0) {
            status = ERROR_SYSTEM;
            goto error;
        }
        close(info_pipe[0]), info_pipe[0] = -1;
    }
error:
    if (info_pipe[0] != -1)
        close(info_pipe[0]);
    if (info_pipe[1] != -1)
        close(info_pipe[1]);
    if (pid != 0)
        waitpid(pid, NULL, 0);
    return status;
}

static int run_remote(int fdin, int fdout, int info, pid_t pid, void *data) {
    int status = INT_MIN;
    RUN_PARAM *param = data;
    SIGACT oldact;

    param->psp->fdin = fdout, param->psp->fdout = fdin;
    if (param->verbose)
        param->psp->info = info;
    sigactinit(&oldact);
    ONERR(sigactset(sighandler, &oldact), ERROR_SYSTEM);
    status = psp_run(param->mode, param->psp);
    sigactreset(&oldact);
error:
    if (pid != 0)
        waitpid(pid, NULL, 0);
    return status;
}

#define ARGVTOK " \t\r\n"
static int run(PSYNC_MODE mode, PSP *psp, bool verbose, char *hostname) {
    int status = INT_MIN;
    RUN_PARAM param = {
        .mode = mode,
        .psp = psp,
        .verbose = verbose
    };
    signed long port;
    char opts[128];
    unsigned int argc;
    char *argv[64];
    char *s, *p;

    if (hostname != NULL) {
        s = strchr(hostname, '@');
        if (s != NULL)
            ++s;
        else
            s = hostname;
        s = strrchr(s, '#');
        if (s != NULL) {
            *s++ = 0;
            if (!*s) {
                status = ERROR_ARGS;
                goto error;
            }
            port = strtol(s, &p, 10);
            if (*p) {
                status = ERROR_ARGS;
                goto error;
            }
        }
        else
            port = SSHPORT;
        if (port < 0 || port > 65535) {
            status = ERROR_ARGS;
            goto error;
        }
        snprintf(opts, sizeof(opts), SSHOPTS, (unsigned int)port);
        argc = 0;
        argv[argc++] = SSH;
        for (s = strtok(opts, ARGVTOK); s != NULL; s = strtok(NULL, ARGVTOK))
            argv[argc++] = s;
        argv[argc++] = hostname;
        argv[argc++] = PACKAGE_TARNAME" --remote";
        argv[argc] = NULL;
        status = popen3(argv, run_local, &param);
    }
    else
        status = run_remote(STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO, 0, &param);
error:
    return status;
}

static char *strtrim(char *str) {
    char *s;

    while (isspace(*str))
        ++str;
    if (*str) {
        s = str + strlen(str);
        while (isspace(*--s));
        s[1] = 0;
    }
    return str;
}

#define CONFREM '#'
#define CONFVAR '='
static int get_config(const char *confname, PSP *psp) {
    int status = INT_MIN;
    size_t length = 0;
    FILE *fp = NULL;
    PSP_CONFIG *head;
    unsigned int line;
    char buffer[1024];
    char *name, *dirname, *s, *p;
    size_t l;

    fp = fopen(confname, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Can not open \"~/%s\".\n", confname);
        status = ERROR_CONF;
        goto error;
    }
    head = psp_config("", NULL, psp);
    line = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        ++line;
        if ((s = strchr(buffer, CONFREM)) != NULL)
            *s = 0;
        name = strtrim(buffer);
        if (!*name)
            continue;
        s = name;
        while (*++s && !isspace(*s));
        if (*s)
            *s++ = 0;
        dirname = strtrim(s);
        s = strchr(name, CONFVAR);
        if (s != NULL) {
            if (*dirname) {
                fprintf(stderr, "Error: Line %u in \"~/%s\": Too many parameters \"%s\".\n", line, confname, dirname);
                status = ERROR_CONF;
                goto error;
            }
            *s++ = 0, p = NULL;
            if (!strcmp(name, "expire"))
                head->expire = strtoul(s, &p, 10) * 60*60*24;
            else if (!strcmp(name, "backup"))
                head->backup = strtoul(s, &p, 10) * 60*60*24;
            if (p == NULL || *p) {
                fprintf(stderr, "Error: Line %u in \"~/%s\": Invarid parameter \"%s%c%s\".\n", line, confname, name, CONFVAR, s);
                status = ERROR_CONF;
                goto error;
            }
        }
        else {
            if (!*dirname) {
                fprintf(stderr, "Error: Line %u in \"~/%s\": Too few parameters \"%s\".\n", line, confname, name);
                status = ERROR_CONF;
                goto error;
            }
            if (psp_config(name, dirname, psp) == NULL) {
                fprintf(stderr, "Error: Line %u in \"~/%s\": Redefined \"%s\".\n", line, confname, name);
                status = ERROR_CONF;
                goto error;
            }
            l = strlen(name);
            if (l > length)
                length = l;
        }
    }
    status = length;
error:
    if (fp != NULL)
        fclose(fp);
    return status;
}

typedef struct {
    enum {
        RUN=0,
        USAGE
    } command;
    PSYNC_MODE mode;
    char *hostname;
    bool verbose;
} OPTS;

static int get_opts(char *argv[], OPTS *opts) {
    int status = INT_MIN;
    char *s;

    while (*++argv != NULL)
        switch (**argv) {
        case '-':
            s = *argv;
            switch (*++s) {
            case 0:
                fprintf(stderr, "Error: Invarid option: %s\n", *argv);
                status = ERROR_ARGS;
                goto error;
            case '-':
                ++s;
                if (!strcmp(s, "help"))
                    opts->command = USAGE;
                else if (!strcmp(s, "remote"))
                    opts->mode = PSYNC_SLAVE;
                else if (!strcmp(s, "sync"))
                    opts->mode = PSYNC_MASTER;
                else if (!strcmp(s, "put"))
                    opts->mode = PSYNC_MASTER_PUT;
                else if (!strcmp(s, "get"))
                    opts->mode = PSYNC_MASTER_GET;
                else if (!strcmp(s, "verbose"))
                    opts->verbose = true;
                else if (!strcmp(s, "quiet"))
                    opts->verbose = false;
                else {
                    fprintf(stderr, "Error: Invarid option: %s\n", *argv);
                    status = ERROR_ARGS;
                    goto error;
                }
                break;
            default:
                do {
                    switch (*s) {
                    case 's':
                        opts->mode = PSYNC_MASTER;
                        break;
                    case 'p':
                        opts->mode = PSYNC_MASTER_PUT;
                        break;
                    case 'g':
                        opts->mode = PSYNC_MASTER_GET;
                        break;
                    case 'v':
                        opts->verbose = true;
                        break;
                    case 'q':
                        opts->verbose = false;
                        break;
                    default:
                        fprintf(stderr, "Error: Invarid option: -%c\n", *s);
                        status = ERROR_ARGS;
                        goto error;
                    }
                } while (*++s);
            }
            break;
        default:
            if (opts->hostname != NULL) {
                fprintf(stderr, "Error: Invarid argument: %s\n", *argv);
                status = ERROR_ARGS;
                goto error;
            }
            opts->hostname = *argv;
        }
    switch (opts->command) {
    case RUN:
        switch (opts->mode) {
        case PSYNC_MASTER:
        case PSYNC_MASTER_PUT:
        case PSYNC_MASTER_GET:
            if (opts->hostname == NULL) {
                fprintf(stderr, "Error: HOST is required\n");
                status = ERROR_ARGS;
                goto error;
            }
            break;
        case PSYNC_SLAVE:
        default:
            if (opts->hostname != NULL) {
                fprintf(stderr, "Error: HOST is not required\n");
                status = ERROR_ARGS;
                goto error;
            }
        }
        break;
    default:
        ;
    }
    status = 0;
error:
    return status;
}

static int usage(FILE *fp) {
    int status = INT_MIN;

    fprintf(fp, PACKAGE_STRING" (protocol %c%c%c%u)\n"
                "\n", PSYNC_PROTID, PSYNC_PROTID >> 8, PSYNC_PROTID >> 16, PSYNC_PROTID >> 24 );
    fprintf(fp, "Usage: "PACKAGE_TARNAME" [--sync] [-v|-q] [USER@]HOST[#PORT]\n"
                "       "PACKAGE_TARNAME" --put [-v|-q] [USER@]HOST[#PORT]\n"
                "       "PACKAGE_TARNAME" --get [-v|-q] [USER@]HOST[#PORT]\n"
                "       "PACKAGE_TARNAME" --help\n"
                "\n" );
    fprintf(fp, "USER@HOST#PORT\n"
                "  HOST           hostname\n"
                "  USER           username (default: current login user)\n"
                "  PORT           SSH port (default: %u)\n"
                "\n", SSHPORT );
    fprintf(fp, "runmode\n"
                "  -s, --sync     synchronous mode (default)\n"
                "  -p, --put      oneway put mode\n"
                "  -g, --get      oneway get mode\n"
                "\n" );
    fprintf(fp, "options\n"
                "  -v, --verbose  verbose mode (default)\n"
                "  -q, --quiet    quiet mode\n"
                "\n" );
    fprintf(fp, "subcommand\n"
                "  --help         show this help\n"
                "\n" );
    status = 0;
    return status;
}

int main(int argc, char *argv[]) {
    int status = INT_MIN;
    PSP *psp = NULL;
    OPTS opts = {
        .command = RUN,
        .mode = PSYNC_MASTER,
        .verbose = true,
        .hostname = NULL
    };
    char *s;

    s = getenv("HOME");
    if (s == NULL) {
        fprintf(stderr, "Error: $HOME is not set.\n");
        status = ERROR_ENVS;
        goto error;
    }
    if (chdir(s) == -1) {
        fprintf(stderr, "Error: Can not change diractory to %s\n", s);
        status = ERROR_ENVS;
        goto error;
    }
    psp = psp_new(&priv.stop);
    if (psp == NULL) {
        fprintf(stderr, "Error: Out of memory.\n");
        status = ERROR_MEMORY;
        goto error;
    }
    status = get_config(CONFFILE, psp);
    if (ISERR(status))
        goto error;
    priv.namelen = status;
    status = get_opts(argv, &opts);
    if (ISERR(status))
        goto error;
    switch (opts.command) {
    case RUN:
        status = run(opts.mode, psp, opts.verbose, opts.hostname);
        switch (status) {
        case ERROR_ARGS:
            fprintf(stderr, "Error: PORT is invalid.\n");
            break;
        }
        break;
    case USAGE:
    default:
        status = usage(stdout);
    }
error:
    if (psp != NULL)
        psp_free(psp);
    if (ISERR(status))
        status = status < -255 ? 255 : -status;
    else
        status = 0;
    return status;
}
