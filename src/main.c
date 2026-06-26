#include "helix.h"
#include "stdio.h"
#include <stdlib.h>
#include <sys/utsname.h>

int last_exit_status=0;

int main(void)
{
    char input[1024];
    struct utsname info;
    
    while (1) {

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

        printf("\033[36m%s \033[0mon \033[33m%s %s \033[0m",
               username,
               hostname,
               chevron);

        fflush(stdout);

        
        if (!fgets(input, sizeof(input), stdin))
            break;

        TokenList tokens = tokenize(input);

        if (tokens.tokens[0] == NULL) {
            free_tokens(tokens);
            continue;
        }

        Pipeline pipeline = parse_pipeline(tokens.tokens);
        
        if (pipeline.cmds == NULL) {
            free_tokens(tokens);
            continue;
        }
        
        last_exit_status = execute_pipeline(&pipeline);
        
        free_pipeline(pipeline);
        free_tokens(tokens);

        printf("\n");
        fflush(stdout);
    }

    return EXIT_SUCCESS;
}