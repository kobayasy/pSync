/* popen3.c - Last modified: 07-Feb-2026 (kobayasy)
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
#include "config.h"
#endif  /* #ifdef HAVE_CONFIG_H */

#include <unistd.h>
#include <sys/types.h>
#include "popen3.h"

int popen3(char *const argv[],
           int (*func)(int fdin, int fdout, int fderr, pid_t pid, void *data),
                                                                  void *data ) {
    int status = -1;
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    pid_t pid;

    if (pipe(stdin_pipe) == -1 ||
        pipe(stdout_pipe) == -1 ||
        pipe(stderr_pipe) == -1 )
        goto error;
    pid = fork();
    if (pid == -1)
        goto error;
    if (pid == 0) {
        close(stdin_pipe[1]), stdin_pipe[1] = -1;
        close(stdout_pipe[0]), stdout_pipe[0] = -1;
        close(stderr_pipe[0]), stderr_pipe[0] = -1;
        if (dup2(stdin_pipe[0], STDIN_FILENO) == -1 ||
            dup2(stdout_pipe[1], STDOUT_FILENO) == -1 ||
            dup2(stderr_pipe[1], STDERR_FILENO) == -1 )
            goto error;
        close(stdin_pipe[0]), stdin_pipe[0] = -1;
        close(stdout_pipe[1]), stdout_pipe[1] = -1;
        close(stderr_pipe[1]), stderr_pipe[1] = -1;
        status = execvp(argv[0], argv);
    }
    else {
        close(stdin_pipe[0]), stdin_pipe[0] = -1;
        close(stdout_pipe[1]), stdout_pipe[1] = -1;
        close(stderr_pipe[1]), stderr_pipe[1] = -1;
        status = func(stdin_pipe[1], stdout_pipe[0], stderr_pipe[0], pid, data);
        close(stdin_pipe[1]), stdin_pipe[1] = -1;
        close(stdout_pipe[0]), stdout_pipe[0] = -1;
        close(stderr_pipe[0]), stderr_pipe[0] = -1;
    }
error:
    if (stdin_pipe[0] != -1)
        close(stdin_pipe[0]);
    if (stdin_pipe[1] != -1)
        close(stdin_pipe[1]);
    if (stdout_pipe[0] != -1)
        close(stdout_pipe[0]);
    if (stdout_pipe[1] != -1)
        close(stdout_pipe[1]);
    if (stderr_pipe[0] != -1)
        close(stderr_pipe[0]);
    if (stderr_pipe[1] != -1)
        close(stderr_pipe[1]);
    return status;
}
