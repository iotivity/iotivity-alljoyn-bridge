//******************************************************************
//
// Copyright 2017 Intel Corporation All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t sQuitFlag = false;

static void SigIntCB(int sig)
{
    (void) sig;
    sQuitFlag = true;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return EXIT_FAILURE;
    }
    char *path = argv[1];
    char *name = basename(strdup(argv[1]));

    signal(SIGINT, SigIntCB);

    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0)
    {
        if (dup2(pipefd[1], fileno(stdout)) < 0)
        {
            perror("dup2");
            return EXIT_FAILURE;
        }
        close(pipefd[1]);
        close(pipefd[0]);
        char *args[argc + 1];
        args[0] = path;
        args[1] = name;
        for (int i = 2; i < argc; ++i)
        {
            args[i] = argv[i];
        }
        args[argc] = NULL;
        execv(path, args);
        perror("execl");
        return EXIT_FAILURE;
    }
    close(pipefd[1]);

    char line[512];
    char *lp = line;
    char c;
    while (!sQuitFlag)
    {
        ssize_t n = read(pipefd[0], &c, 1);
        if (n < 0)
        {
            perror("read");
            return EXIT_FAILURE;
        }
        if (n == 0)
        {
            break;
        }
        *lp++ = c;
        if (c == '\n')
        {
            *lp = '\0';
            printf("%s", line);
            lp = line;

            if (!strncmp(line, "exec", strlen("exec")))
            {
                char *args[11];
                args[0] = path;
                args[1] = name;
                sscanf(line, "exec %ms %ms %ms %ms %ms %ms %ms %ms",
                       &args[2], &args[3], &args[4], &args[5], &args[6], &args[7], &args[8], &args[9]);
                args[10] = NULL;
                pid_t pid = fork();
                if (pid < 0)
                {
                    perror("fork");
                    return EXIT_FAILURE;
                }
                if (pid == 0)
                {
                    execv(path, args);
                    perror("execv");
                    return EXIT_FAILURE;
                }
                for (int i = 2; i < 10; ++i)
                {
                    free(args[i]);
                }
            }
        }
    }
    close(pipefd[0]);

    while (waitpid(-1, NULL, 0))
    {
        if (errno == ECHILD)
        {
            break;
        }
    }
    return EXIT_SUCCESS;
}
