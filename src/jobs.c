#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

Job jobs[MAX_JOBS];
int num_jobs = 0;

static int next_id = 1;

static const char *status_name(int status)
{
    return status == JOB_STOPPED ? "Stopped" : "Running";
}

int job_add(pid_t pgid, const char *cmd, int status)
{
    if (num_jobs >= MAX_JOBS) {
        fprintf(stderr, "helix: too many jobs\n");
        return -1;
    }

    Job *job = &jobs[num_jobs++];

    job->id     = next_id++;
    job->pgid   = pgid;
    job->cmd    = strdup(cmd ? cmd : "");
    job->status = status;

    return job->id;
}

Job *job_find(int id)
{
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].id == id)
            return &jobs[i];
    }

    return NULL;
}

static Job *job_find_pgid(pid_t pgid)
{
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].pgid == pgid)
            return &jobs[i];
    }

    return NULL;
}

/* Most recently added job, which is what plain `fg` and `bg` act on. */
Job *job_recent(void)
{
    if (num_jobs == 0)
        return NULL;

    return &jobs[num_jobs - 1];
}

void job_remove(pid_t pgid)
{
    for (int i = 0; i < num_jobs; i++) {

        if (jobs[i].pgid != pgid)
            continue;

        free(jobs[i].cmd);

        for (int j = i; j < num_jobs - 1; j++)
            jobs[j] = jobs[j + 1];

        num_jobs--;

        /* Empty table, so start numbering from [1] again. */
        if (num_jobs == 0)
            next_id = 1;

        return;
    }
}

/*
 * Called once per prompt instead of from a SIGCHLD handler: almost nothing
 * (printf, free) is safe to call inside a signal handler, and the user only
 * ever sees the notice when the prompt is redrawn anyway.
 */
void job_notify(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status,
                          WNOHANG | WUNTRACED | WCONTINUED)) > 0) {

        Job *job = job_find_pgid(pid);

        if (job == NULL)
            continue;   /* another child of the same pipeline */

        if (WIFSTOPPED(status)) {
            job->status = JOB_STOPPED;
            printf("[%d]+  Stopped\t%s\n", job->id, job->cmd);
        }
        else if (WIFCONTINUED(status)) {
            job->status = JOB_RUNNING;
        }
        else {
            printf("[%d]+  Done\t%s\n", job->id, job->cmd);
            job_remove(job->pgid);
        }
    }
}

void jobs_list(void)
{
    for (int i = 0; i < num_jobs; i++) {
        printf("[%d]+  %-8s%s\n",
               jobs[i].id,
               status_name(jobs[i].status),
               jobs[i].cmd);
    }
}

int job_foreground(Job *job)
{
    int status = 0;

    if (shell_interactive)
        tcsetpgrp(STDIN_FILENO, job->pgid);

    if (job->status == JOB_STOPPED)
        kill(-job->pgid, SIGCONT);

    job->status = JOB_RUNNING;

    /* Reap the whole group; stop early if the job suspends itself again. */
    while (waitpid(-job->pgid, &status, WUNTRACED) > 0) {
        if (WIFSTOPPED(status))
            break;
    }

    if (shell_interactive)
        tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (WIFSTOPPED(status)) {
        job->status = JOB_STOPPED;
        printf("\n[%d]+  Stopped\t%s\n", job->id, job->cmd);
        return 128 + SIGTSTP;
    }

    job_remove(job->pgid);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        printf("\n");
        return 128 + WTERMSIG(status);
    }

    return EXIT_FAILURE;
}

int job_background(Job *job)
{
    if (kill(-job->pgid, SIGCONT) == -1) {
        perror("bg");
        return EXIT_FAILURE;
    }

    job->status = JOB_RUNNING;

    printf("[%d]+ %s &\n", job->id, job->cmd);

    return EXIT_SUCCESS;
}
