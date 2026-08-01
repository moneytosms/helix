#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RESET   "\033[0m"

#define KEEP_DIRS 2     /* trailing path components shown in full */

/*
 * How many columns a string takes on screen: colour escapes are invisible and
 * a UTF-8 character is one column, however many bytes it is spread over.
 */
size_t display_width(const char *s)
{
    size_t width = 0;

    while (*s != '\0') {

        if (*s == '\033') {
            while (*s != '\0' && *s != 'm')
                s++;

            if (*s != '\0')
                s++;

            continue;
        }

        /* Skip UTF-8 continuation bytes, count everything else once. */
        if (((unsigned char)*s & 0xC0) != 0x80)
            width++;

        s++;
    }

    return width;
}

/*
 * The working directory with $HOME as ~, cut down to the last few components:
 * /home/sms/projects/helix/src becomes …/projects/helix/src.
 */
static const char *short_dir(void)
{
    static char out[PATH_MAX];
    char cwd[PATH_MAX];
    char path[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return "?";

    const char *home = getenv("HOME");
    size_t home_len = home ? strlen(home) : 0;

    if (home != NULL && strncmp(cwd, home, home_len) == 0 &&
        (cwd[home_len] == '/' || cwd[home_len] == '\0'))
        snprintf(path, sizeof(path), "~%s", cwd + home_len);
    else
        snprintf(path, sizeof(path), "%s", cwd);

    /* Walk back over the last few separators to find where to cut. */
    char *cut = NULL;
    int seen = 0;

    for (char *p = path + strlen(path) - 1; p > path; p--) {

        if (*p != '/')
            continue;

        if (++seen == KEEP_DIRS) {
            cut = p;
            break;
        }
    }

    if (cut == NULL)
        snprintf(out, sizeof(out), "%s", path);
    else
        snprintf(out, sizeof(out), "…%s", cut);

    return out;
}

/* First line of a git command's output, or NULL when git had nothing to say. */
static char *git_line(const char *cmd, char *out, size_t size)
{
    FILE *f = popen(cmd, "r");

    if (f == NULL)
        return NULL;

    char *ok = fgets(out, size, f);

    pclose(f);

    if (ok == NULL)
        return NULL;

    out[strcspn(out, "\n")] = '\0';

    return out[0] != '\0' ? out : NULL;
}

/* `*` for uncommitted work, and how far ahead or behind the upstream we are. */
static void git_marks(char *out, size_t size)
{
    char buf[128];
    int behind = 0, ahead = 0;
    int len = 0;

    out[0] = '\0';

    if (git_line("git status --porcelain 2>/dev/null", buf, sizeof(buf)))
        len += snprintf(out + len, size - len, RED " *" RESET);

    if (git_line("git rev-list --count --left-right @{u}...HEAD 2>/dev/null",
                 buf, sizeof(buf)) &&
        sscanf(buf, "%d %d", &behind, &ahead) == 2) {

        if (ahead > 0)
            len += snprintf(out + len, size - len, CYAN " ⇡%d" RESET, ahead);

        if (behind > 0)
            snprintf(out + len, size - len, CYAN " ⇣%d" RESET, behind);
    }
}

/* Worth mentioning only once a command has taken long enough to notice. */
static void command_time(char *out, size_t size)
{
    out[0] = '\0';

    if (last_duration < 2.0)
        return;

    if (last_duration < 60.0)
        snprintf(out, size, DIM " took " RESET YELLOW "%.1fs" RESET,
                 last_duration);
    else
        snprintf(out, size, DIM " took " RESET YELLOW "%dm%ds" RESET,
                 (int)(last_duration / 60),
                 (int)last_duration % 60);
}

const char *render_prompt(size_t *width)
{
    static char prompt[1024];
    char host[256];
    char branch[128];
    char marks[128];
    char took[64];

    if (gethostname(host, sizeof(host)) == -1)
        snprintf(host, sizeof(host), "unknown");

    const char *user = getenv("USER");

    int n = snprintf(prompt, sizeof(prompt),
                     BOLD YELLOW " %s" RESET DIM " on " RESET
                     BOLD GREEN "%s" RESET DIM " at " RESET
                     BOLD CYAN "%s" RESET,
                     user ? user : "?",
                     host,
                     short_dir());

    if (git_line("git branch --show-current 2>/dev/null",
                 branch, sizeof(branch)) != NULL) {

        git_marks(marks, sizeof(marks));

        n += snprintf(prompt + n, sizeof(prompt) - n,
                      DIM " via " RESET BOLD MAGENTA " %s" RESET "%s",
                      branch, marks);
    }

    command_time(took, sizeof(took));

    n += snprintf(prompt + n, sizeof(prompt) - n, "%s", took);

    if (last_exit_status != 0)
        n += snprintf(prompt + n, sizeof(prompt) - n,
                      " " RED "[%d]" RESET, last_exit_status);

    /* The chevron carries the exit status: green when the last command worked. */
    snprintf(prompt + n, sizeof(prompt) - n, " %s❯ " RESET,
             last_exit_status == 0 ? GREEN : RED);

    if (width != NULL)
        *width = display_width(prompt);

    return prompt;
}
