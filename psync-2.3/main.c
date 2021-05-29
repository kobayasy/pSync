/* main.c - Last modified: 28-May-2021 (kobayasy)
 *
 * Copyright (c) 2018-2021 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <libgen.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include "psync_utils.h"
#include "psync_psp1.h"
#include "popen3.h"

#define VERSION "2.3"
#define CONFFILE ".psync.conf"

#define ERROR_HELP    3
#define ERROR_ENVS (-26)
#define ERROR_CONF (-27)
#define ERROR_ARGS (-28)

static const char *error_message(int status) {
    static const char *message[] = {
        [0]               = "No error",
        [1]               = "Unknown",
        [-ERROR_FTYPE]    = "File type",
        [-ERROR_FPERM]    = "File permission",
        [-ERROR_FMAKE]    = "Make file",
        [-ERROR_FOPEN]    = "Open file",
        [-ERROR_FWRITE]   = "Write file",
        [-ERROR_FREAD]    = "Read file",
        [-ERROR_FLINK]    = "Link file",
        [-ERROR_FREMOVE]  = "Remove file",
        [-ERROR_FMOVE]    = "Move file",
        [-ERROR_SWRITE]   = "Write file-stat",
        [-ERROR_SREAD]    = "Read file-stat",
        [-ERROR_SUPLD]    = "Upload file-stat",
        [-ERROR_SDNLD]    = "Download file-stat",
        [-ERROR_FUPLD]    = "Upload file",
        [-ERROR_FDNLD]    = "Download file",
        [-ERROR_DMAKE]    = "Make data-file",
        [-ERROR_DOPEN]    = "Open data-file",
        [-ERROR_DWRITE]   = "Write data-file",
        [-ERROR_DREAD]    = "Read data-file",
        [-ERROR_DREMOVE]  = "Remove data-file",
        [-ERROR_MEMORY]   = "Memory",
        [-ERROR_SYSTEM]   = "System",
        [-ERROR_STOP]     = "Interrupted",
        [-ERROR_PROTOCOL] = "Protocol",
        [-ERROR_ENVS]     = "Environment",
        [-ERROR_CONF]     = "Configuration",
        [-ERROR_ARGS]     = "Argument"
    };
    int n;

    if (!ISERR(status))
        n = 0;
    else if (status < -(sizeof(message)/sizeof(*message)))
        n = 1;
    else
        n = -status;
    return message[n];
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

static struct {
    volatile sig_atomic_t stop;
} priv = {
    0
};

#define INFOBUF 128
typedef struct {
    void (*func)(unsigned int id, const char *info);
    int fd;
} INFO_PARAM;

static void *info_thread(void *data) {
    INFO_PARAM *param = data;
    unsigned int id;
    struct pollfd fds[2];
    char buffer[INFOBUF];
    ssize_t size;
    char *info, *s;

    for (id = 0; id < 2; ++id) {
        param[id].func(id, "[");
        fds[id].fd = param[id].fd, fds[id].events = POLLIN;
    }
    while (fds[0].fd != -1 || fds[1].fd != -1) {
        if (ISSTOP(&priv.stop))
            goto error;
        if (poll(fds, 2, -1) < 1)
            goto error;
        for (id = 0; id < 2; ++id)
            if (fds[id].revents & POLLIN) {
                size = read(fds[id].fd, buffer, sizeof(buffer));
                switch (size) {
                case -1:
                    goto error;
                case  0:  /* end of file */
                    fds[id].fd = -1;
                    break;
                default:
                    buffer[size] = 0;
                    s = buffer;
                    while (info = s, s = strchr(info, '\n'), s != NULL) {
                        *s++ = 0;
                        param[id].func(id, info);
                    }
                }
            }
            else if (fds[id].revents)
                fds[id].fd = -1;
    }
error:
    for (id = 0; id < 2; ++id)
        param[id].func(id, "]");
    return NULL;
}

static int strfnum(char *str, size_t size, intmax_t num) {
    int status = INT_MIN;
    const char *unit = "kMGTPEZY";

    if (num <  1024 && num > -1024)
        status = snprintf(str, size, "%10jd", num);
    else {
        while (num >=  1024*1024 || num <= -1024*1024) {
            if (!*++unit)
                goto error;
            num /= 1024;
        }
        status = snprintf(str, size, "%9.3f%c", (double)(int32_t)num / 1024, *unit);
    }
error:
    return status;
}

#define PROGBUF 128
static void info_func(unsigned int id, const char *info) {
    static struct {
        const char *id, *dir;
        char name[PROGBUF];
        intmax_t preload, upload, download;
        char buffer[PROGBUF];
    } progress[] = {
        {.id = "L:", .dir = "D:"},
        {.id = "R:", .dir = "U:"}
    };
    static unsigned int newline = 1;
    enum {INFO_NONE=0, INFO_NEWLINE, INFO_STATUS, INFO_MESSAGE, INFO_PROGRESS} state = INFO_NONE;
    int status = INT_MIN;
    char *s;

    switch (*info++) {
    case '[':
        *progress[id].name = 0;
        progress[id].preload = 0, progress[id].upload = 0, progress[id].download = 0;
        *progress[id].buffer = 0;
        break;
    case ']':
        state = INFO_NEWLINE;
        break;
    case '!':
        switch (*info) {
        case '+':
        case '-':
            status = strtol(info, NULL, 10);
            state = INFO_STATUS;
            break;
        default:
            state = INFO_MESSAGE;
            break;
        }
        break;
    case 'R':
        strcpy(progress[id].name, info);
        progress[id].preload = 0, progress[id].upload = 0, progress[id].download = 0;
        *progress[id].buffer = 0;
        state = INFO_PROGRESS;
        break;
    case 'U':
        progress[id].preload = strtoll(info, NULL, 10);
        id = !id;
        if (progress[!id].preload > progress[id].upload) {
            progress[id].upload = progress[!id].preload;
            state = INFO_PROGRESS;
        }
        break;
    case 'D':
        if (progress[!id].preload > progress[id].upload)
            progress[id].upload = progress[!id].preload;
        progress[id].download = strtoll(info, NULL, 10);
        state = INFO_PROGRESS;
        break;
    }
    switch (state) {
    case INFO_NONE:
        break;
    case INFO_NEWLINE:
        if (newline == 0)
            dprintf(STDOUT_FILENO, "\n"), newline = 1;
        break;
    case INFO_STATUS:
        switch (status) {
        case ERROR_NOTREADYLOCAL:
            dprintf(STDOUT_FILENO, "\n%s%s  Skip, not ready\n" + newline, progress[id].id, progress[id].name), newline = 1;
            break;
        default:
            if (ISERR(status))
                dprintf(STDOUT_FILENO, "\n%s%s  %s\n" + newline, progress[id].id, progress[id].name, error_message(status)), newline = 1;
        }
        break;
    case INFO_MESSAGE:
        dprintf(STDOUT_FILENO, "\n%s%s  %s\n" + newline, progress[id].id, progress[id].name, info), newline = 1;
        break;
    case INFO_PROGRESS:
        if (!strcmp(progress[id].name, progress[!id].name)) {
            if (progress[id].upload > 0) {
                s = progress[id].buffer;
                s += sprintf(s, "  %s", progress[id].dir);
                s += strfnum(s, 11, progress[id].upload);
                if (progress[id].download > 0) {
                    s += sprintf(s, " ->");
                    s += strfnum(s, 11, progress[id].download);
                }
            }
            dprintf(STDOUT_FILENO, "\r%s%s%s" + newline, progress[id].name, progress[0].buffer, progress[1].buffer), newline = 0;
        }
        else if (newline == 0)
            dprintf(STDOUT_FILENO, "\n"), newline = 1;
        break;
    }
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
    INFO_PARAM info_param[2] = {{.func = info_func}, {.func = info_func}};
    pthread_t info_tid;
    SIGACT oldact;

    param->psp->fdin = fdout, param->psp->fdout = fdin;
    if (param->verbose) {
        if (pipe(info_pipe) == -1) {
            status = ERROR_SYSTEM;
            goto error;
        }
        info_param[0].fd = info_pipe[0];
        info_param[1].fd = info;
        param->psp->info = info_pipe[1];
        if (pthread_create(&info_tid, NULL, info_thread, info_param) != 0) {
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

static int run(PSYNC_MODE mode, PSP *psp, bool verbose, char *hostname) {
    int status = INT_MIN;
    RUN_PARAM param = {
        .mode = mode,
        .psp = psp,
        .verbose = verbose
    };
    char *remote_args[] = {"ssh", "-qCp", "22", hostname, "psync --remote", 0};
    char **port = &remote_args[2];
    char *s;

    if (hostname != NULL) {
        s = strchr(hostname, '@');
        if (s != NULL)
            ++s;
        else
            s = hostname;
        s = strrchr(s, '#');
        if (s != NULL)
            *s++ = 0, *port = s;
        status = popen3(remote_args, run_local, &param);
    }
    else
        status = run_remote(STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO, 0, &param);
    return status;
}

#define CONFBUF 128
#define CONFREM '#'
#define CONFTOK " \t\r\n"
static int get_config(const char *confname, PSP *psp) {
    int status = INT_MIN;
    FILE *fp = NULL;
    unsigned int line;
    char buffer[CONFBUF];
    PSP_CONFIG *config;
    char *name;
    unsigned int argc;
    char *argv, *s;

    fp = fopen(confname, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Can not open \"~/%s\".\n", confname);
        status = ERROR_CONF;
        goto error;
    }
    line = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        ++line;
        config = NULL, name = NULL;
        argc = 0;
        if ((argv = strchr(buffer, CONFREM)) != NULL)
            *argv = 0;
        for (argv = strtok(buffer, CONFTOK); argv != NULL; argv = strtok(NULL, CONFTOK))
            switch (++argc) {
            case 1:
                if (!isalnum(*argv)) {
                    fprintf(stderr, "Error: Line %u in \"~/%s\": Invarid parameter \"%s\".\n", line, confname, argv);
                    status = ERROR_CONF;
                    goto error;
                }
                name = argv;
                break;
            case 2:
                if (name == NULL) {
                    status = ERROR_SYSTEM;
                    goto error;
                }
                config = psp_config(name, argv, psp);
                if (config == NULL) {
                    fprintf(stderr, "Error: Line %u in \"~/%s\": Redefined \"%s\".\n", line, confname, name);
                    status = ERROR_CONF;
                    goto error;
                }
                break;
            case 3:
                if (config == NULL) {
                    status = ERROR_SYSTEM;
                    goto error;
                }
                config->backup = strtoul(argv, &s, 10);
                if (*s || !isdigit(*argv)) {
                    fprintf(stderr, "Error: Line %u in \"~/%s\": Invarid parameter \"%s\".\n", line, confname, argv);
                    status = ERROR_CONF;
                    goto error;
                }
                break;
            case 4:
                if (config == NULL) {
                    status = ERROR_SYSTEM;
                    goto error;
                }
                config->expire = strtoul(argv, &s, 10);
                if (*s || !isdigit(*argv)) {
                    fprintf(stderr, "Error: Line %u in \"~/%s\": Invarid parameter \"%s\".\n", line, confname, argv);
                    status = ERROR_CONF;
                    goto error;
                }
                break;
            default:
                fprintf(stderr, "Error: Line %u in \"~/%s\": Too many parameters \"%s\".\n", line, confname, name);
                status = ERROR_CONF;
                goto error;
            }
        if (argc == 1) {
            fprintf(stderr, "Error: Line %u in \"~/%s\": Too few parameters \"%s\".\n", line, confname, name);
            status = ERROR_CONF;
            goto error;
        }
    }
    status = 0;
error:
    if (fp != NULL)
        fclose(fp);
    return status;
}

typedef struct {
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
                if (!strcmp(s, "help")) {
                    status = ERROR_HELP;
                    goto error;
                }
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
    status = 0;
error:
    return status;
}

static void usage(const char *argv0, FILE *fp) {
    fprintf(fp, "pSync version %s (protocol %c%c%c%u)\n"
                "\n", VERSION, PSYNC_PROTID, PSYNC_PROTID >> 8, PSYNC_PROTID >> 16, PSYNC_PROTID >> 24 );
    fprintf(fp, "Usage: %s [--sync] [-v|-q] [USER@]HOST[#PORT]\n"
                "       %s --put [-v|-q] [USER@]HOST[#PORT]\n"
                "       %s --get [-v|-q] [USER@]HOST[#PORT]\n"
                "       %s --help\n"
                "\n", argv0, argv0, argv0, argv0 );
    fputs("USER@HOST#PORT\n"
          "  HOST           hostname\n"
          "  USER           username (default: current login user)\n"
          "  PORT           SSH port (default: 22)\n"
          "\n", fp );
    fputs("runmode\n"
          "  -s, --sync     synchronous mode (default)\n"
          "  -p, --put      oneway put mode\n"
          "  -g, --get      oneway get mode\n"
          "\n", fp );
    fputs("options\n"
          "  -v, --verbose  show progress during transfer (default)\n"
          "  -q, --quiet    suppress non-error messages\n"
          "\n", fp );
    fputs("subcommand\n"
          "  --help         show this help\n"
          "\n", fp );
}

int main(int argc, char *argv[]) {
    int status = INT_MIN;
    PSP *psp = NULL;
    OPTS opts = {
        .mode = PSYNC_MASTER,
        .verbose = true,
        .hostname = NULL
    };
    char *s;

    s = getenv("HOME");
    if (s == NULL) {
        fprintf(stderr, "Error: $HOME is not set.\n");
        status = ERROR_ENVS;
        goto run;
    }
    if (chdir(s) == -1) {
        fprintf(stderr, "Error: Can not change diractory to %s\n", s);
        status = ERROR_ENVS;
        goto run;
    }
    psp = psp_new(&priv.stop);
    if (psp == NULL) {
        fprintf(stderr, "Error: Out of memory.\n");
        status = ERROR_MEMORY;
        goto run;
    }
    status = get_config(CONFFILE, psp);
    if (ISERR(status))
        goto run;
    status = get_opts(argv, &opts);
run:
    switch (status) {
    case 0:
        status = run(opts.mode, psp, opts.verbose, opts.hostname);
        break;
    default:
        if (ISERR(status))
            fputc('\n', stdout);
        usage(basename(*argv), stdout);
    }
    if (psp != NULL)
        psp_free(psp);
    if (ISERR(status))
        status = status < -255 ? 255 : -status;
    else
        status = 0;
    return status;
}
