#include "helix.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>

int is_builtin(char *cmd) {
  return (!strcmp(cmd,"cd")|| !strcmp(cmd,"exit")|| !strcmp(cmd,"pwd")||
          !strcmp(cmd,"jobs")|| !strcmp(cmd,"fg")|| !strcmp(cmd,"bg")||
          !strcmp(cmd,"history"));
}

/* `fg 2` picks job 2, plain `fg` picks the most recent one. */
static Job *pick_job(Command *cmd)
{
    Job *job = cmd->args[1]
                 ? job_find(atoi(cmd->args[1]))
                 : job_recent();

    if (job == NULL)
        fprintf(stderr, "helix: %s: no such job\n", cmd->args[0]);

    return job;
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

    if (!strcmp(name, "jobs")) {
        jobs_list();
        return EXIT_SUCCESS;
    }

    if (!strcmp(name, "fg")) {

        Job *job = pick_job(cmd);

        if (job == NULL)
            return EXIT_FAILURE;

        return job_foreground(job);
    }

    if (!strcmp(name, "bg")) {

        Job *job = pick_job(cmd);

        if (job == NULL)
            return EXIT_FAILURE;

        return job_background(job);
    }

    if (!strcmp(name, "history")) {
        history_print();
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