#include "helix.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>

pid_t shell_pgid = 0;
int shell_interactive = 0;

static const CommandMap maps[] = {
    {
        .from = "ls",
        .to = "eza",
        .default_args = {
            "--icons",
            "--group-directories-first",
            NULL
        }
    },
    {
        .from = "cat",
        .to = "bat",
        .default_args = {
            "--paging=never",
            NULL
        }
    },
    {
        .from = "find",
        .to = "fd",
        .default_args = { NULL }
    },
    {
        .from = "grep",
        .to = "rg",
        .default_args = { NULL }
    }
};

/*
 * Put the shell in its own process group and give it the terminal, so that
 * every job we fork can own the terminal in turn. The Ctrl+C and
 * Ctrl+Z keys are then delivered by the kernel straight to whichever group
 * holds it, so the shell never has to forward them by hand.
 */
void shell_init(void)
{
    shell_pgid = getpid();
    shell_interactive = isatty(STDIN_FILENO);

    if (!shell_interactive)
        return;

    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    /* EPERM here just means we already lead a group, e.g. a session leader. */
    if (setpgid(shell_pgid, shell_pgid) == -1 && getpgrp() != shell_pgid) {
        perror("setpgid");
        shell_interactive = 0;
        return;
    }

    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

/* Swap a command for its modern replacement, e.g. ls -> eza --icons. */
static char **apply_map(char **args, int num_args)
{
    size_t num_maps = sizeof(maps) / sizeof(maps[0]);

    for (size_t i = 0; i < num_maps; i++) {

        if (strcmp(args[0], maps[i].from) != 0)
            continue;

        /* Count default arguments */
        int defaults = 0;
        while (maps[i].default_args[defaults] != NULL)
            defaults++;


        char **argv = malloc(
            (num_args + defaults + 1) * sizeof(char *)
        );

        if (argv == NULL)
            return NULL;

        int k = 0;

        argv[k++] = (char *)maps[i].to;

        for (int j = 0; j < defaults; j++)
            argv[k++] = (char *)maps[i].default_args[j];

        for (int j = 1; j < num_args; j++)
            argv[k++] = args[j];

        argv[k] = NULL;

        return argv;
    }

    return NULL;
}

/*
 * Returns a malloc'd argv with the ~/.helixrc alias and the command map
 * applied, or NULL when the command needs neither. Aliases are expanded once,
 * so an alias that names itself cannot loop.
 */
char **expand_command(Command *cmd)
{
    char **args = cmd->args;
    int num_args = cmd->num_args;
    char **aliased = NULL;

    char **words = alias_lookup(args[0]);

    if (words != NULL) {

        int count = 0;
        while (words[count] != NULL)
            count++;

        aliased = malloc((count + num_args) * sizeof(char *));

        if (aliased == NULL)
            return NULL;

        int k = 0;

        for (int j = 0; j < count; j++)
            aliased[k++] = words[j];

        for (int j = 1; j < num_args; j++)
            aliased[k++] = args[j];

        aliased[k] = NULL;

        args = aliased;
        num_args = k;
    }

    char **mapped = apply_map(args, num_args);

    if (mapped != NULL) {
        free(aliased);
        return mapped;
    }

    return aliased;
}

/* Children must not inherit the SIG_IGN we set up for the shell. */
static void child_signals(void)
{
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

static int wait_foreground(pid_t pgid, pid_t *pids, int num_pids,
                           const char *cmdline)
{
    int status = 0;

    if (shell_interactive)
        tcsetpgrp(STDIN_FILENO, pgid);

    for (int i = 0; i < num_pids; i++) {

        if (waitpid(pids[i], &status, WUNTRACED) == -1) {
            perror("waitpid");
            status = -1;
            break;
        }

        /* Ctrl+Z: the job keeps living, so hand it to the jobs table. */
        if (WIFSTOPPED(status)) {

            int id = job_add(pgid, cmdline, JOB_STOPPED);

            printf("\n[%d]+  Stopped\t%s\n", id, cmdline);

            if (shell_interactive)
                tcsetpgrp(STDIN_FILENO, shell_pgid);

            return 128 + SIGTSTP;
        }
    }

    if (shell_interactive)
        tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) == SIGINT)
            printf("\n");
        return 128 + WTERMSIG(status);
    }

    return EXIT_FAILURE;
}

int execute_pipeline(Pipeline *pipeline, const char *cmdline)
{
    if (pipeline == NULL || pipeline->num_cmds == 0)
        return EXIT_SUCCESS;

    /*
     * A builtin only runs inside the shell when it is the whole line. Inside
     * a pipeline it has to be forked like any other command, or its output
     * could not be piped anywhere.
     */
    if (pipeline->num_cmds == 1 &&
        !pipeline->background &&
        pipeline->cmds[0].args != NULL &&
        is_builtin(pipeline->cmds[0].args[0]))
    {
        return run_builtin(&pipeline->cmds[0]);
    }

    int prev_pipe_read = -1;
    pid_t pids[pipeline->num_cmds];
    pid_t pgid = 0;

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

            /* ---------- Job control ---------- */

            /*
             * Both sides set the group: whichever runs first wins the race,
             * and the loser's call is a no-op.
             */
            setpgid(0, pgid ? pgid : getpid());
            child_signals();

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

            char **argv = expand_command(cmd);

            if (argv != NULL) {
                execvp(argv[0], argv);

                perror(argv[0]);
                free(argv);
                exit(127);
            }

            execvp(cmd->args[0], cmd->args);

            fprintf(stderr, "helix: %s: command not found\n", cmd->args[0]);
            exit(127);
        }

        /* ---------- Parent ---------- */

        pids[i] = pid;

        if (pgid == 0)
            pgid = pid;

        setpgid(pid, pgid);

        if (prev_pipe_read != -1)
            close(prev_pipe_read);

        if (has_next) {
            close(pipefd[1]);
            prev_pipe_read = pipefd[0];
        } else {
            prev_pipe_read = -1;
        }
    }

    if (pipeline->background) {

        int id = job_add(pgid, cmdline, JOB_RUNNING);

        printf("[%d] %d\n", id, pgid);

        return EXIT_SUCCESS;
    }

    return wait_foreground(pgid, pids, pipeline->num_cmds, cmdline);
}

void free_pipeline(Pipeline pipeline)
{
    for (int i = 0; i < pipeline.num_cmds; i++)
        free(pipeline.cmds[i].args);

    free(pipeline.cmds);
}
