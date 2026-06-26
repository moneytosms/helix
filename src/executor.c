#include "helix.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int execute_pipeline(Pipeline *pipeline)
{
    if (pipeline == NULL || pipeline->num_cmds == 0)
        return EXIT_SUCCESS;

    if (pipeline->num_cmds == 1 &&
        pipeline->cmds[0].args != NULL &&
        is_builtin(pipeline->cmds[0].args[0]))
    {
        return run_builtin(&pipeline->cmds[0]);
    }

    int prev_pipe_read = -1;
    pid_t pids[pipeline->num_cmds];

    for (int i = 0; i < pipeline->num_cmds; i++) {

        Command *cmd = &pipeline->cmds[i];

        int pipefd[2];
        int has_next = (i < pipeline->num_cmds - 1);

        if (has_next) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                return EXIT_FAILURE;
            }
        }

        pid_t pid = fork();

        if (pid == -1) {
          if (has_next) {
              close(pipefd[0]);
              close(pipefd[1]);
          }
      
          if (prev_pipe_read != -1)
              close(prev_pipe_read);
      
          perror("fork");
          return EXIT_FAILURE;
        }

        if (pid == 0) {

            /* ---------- Pipeline input ---------- */

            if (prev_pipe_read != -1) {
                if (dup2(prev_pipe_read, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            /* ---------- Pipeline output ---------- */

            if (has_next) {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            /* ---------- Input redirection ---------- */

            if (cmd->infile != NULL) {

                int fd = open(cmd->infile, O_RDONLY);

                if (fd == -1) {
                    perror(cmd->infile);
                    exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                close(fd);
            }

            /* ---------- Output redirection ---------- */

            if (cmd->outfile != NULL) {

              int flags =
                  O_WRONLY |
                  O_CREAT |
                  (cmd->append ? O_APPEND : O_TRUNC);

                int fd = open(cmd->outfile, flags, 0644);

                if (fd == -1) {
                    perror(cmd->outfile);
                    exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                close(fd);
            }

            /* ---------- Cleanup ---------- */

            if (prev_pipe_read != -1)
                close(prev_pipe_read);

            if (has_next) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            if (is_builtin(cmd->args[0])) {
                exit(run_builtin(cmd));
            }
            execvp(cmd->args[0], cmd->args);

            perror(cmd->args[0]);
            exit(127);
        }

        /* ---------- Parent ---------- */

        pids[i] = pid;

        if (prev_pipe_read != -1)
            close(prev_pipe_read);

        if (has_next) {
            close(pipefd[1]);
            prev_pipe_read = pipefd[0];
        } else {
            prev_pipe_read = -1;
        }
    }

    int status = 0;

    for (int i = 0; i < pipeline->num_cmds; i++) {
        if (waitpid(pids[i], &status, 0) == -1) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return EXIT_FAILURE;
}

void free_pipeline(Pipeline pipeline)
{
    for (int i = 0; i < pipeline.num_cmds; i++)
        free(pipeline.cmds[i].args);

    free(pipeline.cmds);
}