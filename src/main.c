#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int last_exit_status=0;
double last_duration=0;

static double seconds_now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void usage(void)
{
    printf("usage: helix [--version] [--help]\n\n"
           "  a small unix shell: pipes, redirection, job control,\n"
           "  line editing with history, syntax highlighting and a\n"
           "  ~/.helixrc for aliases and settings.\n\n"
           "  builtins: cd exit pwd jobs fg bg history alias\n");
}

int main(int argc, char **argv)
{
    if (argc > 1) {

        if (strcmp(argv[1], "--version") == 0) {
            printf("helix %s\n", HELIX_VERSION);
            return EXIT_SUCCESS;
        }

        if (strcmp(argv[1], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }

        fprintf(stderr, "helix: unknown option: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    config_load();
    shell_init();
    history_load();
    atexit(history_save);

    const char *dashboard = setting_get("DASHBOARD");

    if (shell_interactive && (dashboard == NULL || strcmp(dashboard, "off") != 0))
        render_dashboard();

    for (;;) {

        /* Report background jobs that finished while we were away. */
        job_notify();

        /* A blank line between the last command's output and the prompt. */
        if (shell_interactive)
            printf("\n");

        size_t width;
        const char *prompt = render_prompt(&width);

        char *line = helix_readline(prompt, width);

        if (line == NULL) {
            printf("\n");
            break;
        }

        /* tokenize() chops the line up, so keep a copy for history and jobs. */
        char *original = strdup(line);

        TokenList tokens = tokenize(line);

        if (tokens.tokens[0] != NULL) {

            history_add(original);

            Pipeline pipeline = parse_pipeline(tokens.tokens);

            if (pipeline.cmds != NULL) {

                double started = seconds_now();

                last_exit_status = execute_pipeline(&pipeline, original);
                last_duration = seconds_now() - started;

                free_pipeline(pipeline);
            } else {
                last_exit_status = 2;   /* syntax error */
            }
        }

        free_tokens(tokens);
        free(original);
        free(line);
    }

    return last_exit_status;
}
