#include "helix.h"
#include <string.h>
#include <stdlib.h>

Pipeline parse_pipeline(char **tokens) {
    Pipeline pipeline = {0};

    pipeline.num_cmds = 1;
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0)
            pipeline.num_cmds++;
    }

    pipeline.cmds = calloc(pipeline.num_cmds, sizeof(Command));

    int i = 0;

    for (int cmd = 0; cmd < pipeline.num_cmds; cmd++) {

        Command *curr = &pipeline.cmds[cmd];

        int argc = 0;

        while (tokens[i] != NULL &&
               strcmp(tokens[i], "|") != 0) {

            if (strcmp(tokens[i], "<") == 0) {

                i++;
                curr->infile = tokens[i];
                i++;
            }

            else if (strcmp(tokens[i], ">") == 0) {

                i++;
                curr->outfile = tokens[i];
                curr->append = 0;
                i++;
            }

            else if (strcmp(tokens[i], ">>") == 0) {

                i++;
                curr->outfile = tokens[i];
                curr->append = 1;
                i++;
            }

            else {

                curr->args = realloc(
                    curr->args,
                    (argc + 2) * sizeof(char *)
                );

                curr->args[argc++] = tokens[i++];
            }
        }

        curr->args[argc] = NULL;
        curr->num_args = argc;

        if (tokens[i] != NULL)   
            i++;
    }

    return pipeline;
}