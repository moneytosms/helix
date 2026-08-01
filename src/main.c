#include "helix.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

int last_exit_status=0;

int main(void)
{
    char prompt[256];
    struct utsname info;

    shell_init();
    history_load();
    atexit(history_save);

    while (1) {

        /* Report background jobs that finished while we were away. */
        job_notify();

        if (uname(&info) == -1) {
            perror("uname");
            return EXIT_FAILURE;
        }

        char *hostname = info.nodename;
        char *username = getenv("USER");

        const char *chevron =
            (last_exit_status == 0)
                ? "\033[32m❯"
                : "\033[31m❯";

        snprintf(prompt, sizeof(prompt),
                 "\033[36m%s \033[0mon \033[33m%s %s \033[0m",
                 username,
                 hostname,
                 chevron);

        char *line = helix_readline(prompt);

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
                last_exit_status = execute_pipeline(&pipeline, original);
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
