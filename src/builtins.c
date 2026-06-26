#include "helix.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>

int is_builtin(char *cmd) {
  return (!strcmp(cmd,"cd")|| !strcmp(cmd,"exit")|| !strcmp(cmd,"pwd"));
}

int run_builtin(Command *cmd)
{
    char *name = cmd->args[0];

    if (!strcmp(name, "cd")) {

        char *path = cmd->args[1];

        if (path == NULL)
            path = getenv("HOME");

        if (chdir(path) != 0) {
            perror("cd");
            return EXIT_FAILURE;
        }

        pid_t pid = fork();

        if (pid == 0) {

            execvp(
                "eza",
                (char *[]) {
                    "eza",
                    "--icons",
                    "--group-directories-first",
                    NULL
                }
            );

            perror("eza");
            exit(EXIT_FAILURE);
        }

        waitpid(pid, NULL, 0);

        return EXIT_SUCCESS;
    }

    if (!strcmp(name, "pwd")) {

        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("pwd");
            return EXIT_FAILURE;
        }

        printf("%s\n", cwd);
        return EXIT_SUCCESS;
    }

    if (!strcmp(name, "exit")) {

        int status =
            cmd->args[1]
                ? atoi(cmd->args[1])
                : 0;

        exit(status);
    }

    return EXIT_FAILURE;
}