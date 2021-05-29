/* popen3.c - Last modified: 15-Aug-2018 (kobayasy)
 *
 * Copyright (c) 2018 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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
#include <unistd.h>
#include "popen3.h"

int popen3(char *const argv[],
           int (*func)(int fdin, int fdout, int fderr, pid_t pid, void *data),
                                                                  void *data ) {
    int status = INT_MIN;
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    pid_t pid;

    if (pipe(stdin_pipe) == -1)
        goto error;
    if (pipe(stdout_pipe) == -1)
        goto error;
    if (pipe(stderr_pipe) == -1)
        goto error;
    pid = fork();
    if (pid == -1)
        goto error;
    if (pid == 0) {
        close(stdin_pipe[1]), stdin_pipe[1] = -1;
        close(stdout_pipe[0]), stdout_pipe[0] = -1;
        close(stderr_pipe[0]), stderr_pipe[0] = -1;
        if (stdin_pipe[0] != STDIN_FILENO) {
            dup2(stdin_pipe[0], STDIN_FILENO);
            close(stdin_pipe[0]), stdin_pipe[0] = -1;
        }
        if (stdout_pipe[1] != STDOUT_FILENO) {
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[1]), stdout_pipe[1] = -1;
        }
        if (stderr_pipe[1] != STDERR_FILENO) {
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]), stderr_pipe[1] = -1;
        }
        execvp(argv[0], argv);
        goto error;
    }
    close(stdin_pipe[0]), stdin_pipe[0] = -1;
    close(stdout_pipe[1]), stdout_pipe[1] = -1;
    close(stderr_pipe[1]), stderr_pipe[1] = -1;
    status = func(stdin_pipe[1], stdout_pipe[0], stderr_pipe[0], pid, data);
    close(stdin_pipe[1]), stdin_pipe[1] = -1;
    close(stdout_pipe[0]), stdout_pipe[0] = -1;
    close(stderr_pipe[0]), stderr_pipe[0] = -1;
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
