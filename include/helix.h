#ifndef HELIX_H
#define HELIX_H

#include <stddef.h>
#include <sys/types.h>

#define HELIX_VERSION "1.0.0"

#define MAX_ARGS 64
#define MAX_INPUT 1024

/* ---------- lexer ---------- */

/*
 * tokenize() splits the line in place, so the tokens point back into the
 * buffer you passed in. Keep that buffer alive until you are done with them.
 */
typedef struct {
    char **tokens;
    int count;
} TokenList;

TokenList tokenize(char *input);
void free_tokens(TokenList tokens);

/* ---------- parser ---------- */

typedef struct {
  char **args;
  char *infile;
  char *outfile;
  int append;
  int num_args;
}Command;

typedef struct {
  Command *cmds;
  int num_cmds;
  int background;   /* 1 if the line ended with '&' */
} Pipeline;

Pipeline parse_pipeline(char **tokens);
void free_pipeline(Pipeline pipeline);

/* ---------- executor ---------- */

typedef struct {
    const char *from;
    const char *to;
    const char *default_args[8];
} CommandMap;

/* cmdline is the untouched input line, kept for the jobs table display */
int execute_pipeline(Pipeline *pipeline, const char *cmdline);
char **expand_command(Command *cmd);
void shell_init(void);

extern int last_exit_status;
extern pid_t shell_pgid;
extern int shell_interactive;

/* ---------- builtins ---------- */

int run_builtin(Command *cmd);
int is_builtin(char *cmd);

/* ---------- jobs ---------- */

#define MAX_JOBS 64

enum { JOB_RUNNING, JOB_STOPPED };

typedef struct {
    int id;           /* job number shown to the user: [1], [2] ... */
    pid_t pgid;       /* process group, so signals reach the whole pipeline */
    char *cmd;        /* original command line, for display */
    int status;
} Job;

extern Job jobs[MAX_JOBS];
extern int num_jobs;

int job_add(pid_t pgid, const char *cmd, int status);
Job *job_find(int id);
Job *job_recent(void);
void job_remove(pid_t pgid);
void job_notify(void);          /* reap finished children, print notices */
void jobs_list(void);
int job_foreground(Job *job);   /* hand over the terminal and wait */
int job_background(Job *job);   /* SIGCONT and leave it running */

#endif
