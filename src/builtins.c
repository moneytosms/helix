#include "helix.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>

int is_builtin(char *cmd) {
  return (!strcmp(cmd,"cd")|| !strcmp(cmd,"exit")|| !strcmp(cmd,"pwd"));
}

int run_builtin(TokenList tokens)
{
    char *cmd = tokens.tokens[0];
    int status;
    if (!strcmp(cmd, "cd")) {
        char *path = tokens.tokens[1];

        if (path == NULL)
            path = getenv("HOME");

        if (chdir(path) != 0) {
            perror("cd");
            last_exit_status = 1;
            return 1;
        }

        pid_t pid = fork();

        if (pid == 0) {
          execvp("eza", (char *[]){"eza", "--icons", "--group-directories-first", NULL});
          perror("eza");
            last_exit_status = 1;
            exit(EXIT_FAILURE);
        }

        waitpid(pid, NULL, 0);
        last_exit_status = 0;
        return 1;
    }

    if (!strcmp(cmd, "pwd")) {
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) == NULL){
            perror("pwd");
            last_exit_status = 1;
        }
        else{
            printf("%s\n", cwd);
            last_exit_status = 0;
        }
        return 1;
    }

    if (!strcmp(cmd, "exit")) {
        status = tokens.tokens[1] != NULL ? atoi(tokens.tokens[1]) : 0;
        last_exit_status = status;
        exit(status);
    }

    return 0;
}


int get_last_exit_status() {
    return last_exit_status;
}
