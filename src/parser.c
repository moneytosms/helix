#include "helix.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int is_operator(char *tok)
{
    return tok &&
           (!strcmp(tok, "|") ||
            !strcmp(tok, "<") ||
            !strcmp(tok, ">") ||
            !strcmp(tok, ">>"));
}

Pipeline parse_pipeline(char **tokens)
{
    Pipeline pipeline = {0};

    pipeline.num_cmds = 1;

    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0)
            pipeline.num_cmds++;
    }

    pipeline.cmds = calloc(pipeline.num_cmds, sizeof(Command));
    if (pipeline.cmds == NULL) {
        perror("calloc");
        return pipeline;
    }


    int i = 0;

    for (int cmd = 0; cmd < pipeline.num_cmds; cmd++) {

        Command *curr = &pipeline.cmds[cmd];
        int argc = 0;

        while (tokens[i] != NULL &&
               strcmp(tokens[i], "|") != 0) {

            if (strcmp(tokens[i], "<") == 0) {

                i++;

                if (tokens[i] == NULL || is_operator(tokens[i])) {
                    fprintf(stderr,
                            "syntax error: expected filename after '<'\n");
                
                    free_pipeline(pipeline);
                    pipeline.cmds = NULL;
                    pipeline.num_cmds = 0;
                    return pipeline;
                }

                curr->infile = tokens[i++];
            }

            else if (strcmp(tokens[i], ">") == 0) {

                i++;

                if (tokens[i] == NULL || is_operator(tokens[i])) {
                    fprintf(stderr,
                            "syntax error: expected filename after '<'\n");
                
                    free_pipeline(pipeline);
                    pipeline.cmds = NULL;
                    pipeline.num_cmds = 0;
                    return pipeline;
                }

                curr->outfile = tokens[i++];
                curr->append = 0;
            }

            else if (strcmp(tokens[i], ">>") == 0) {

                i++;

                if (tokens[i] == NULL || is_operator(tokens[i])) {
                    fprintf(stderr,
                            "syntax error: expected filename after '>'\n");
                    free_pipeline(pipeline);
                    
                    pipeline.cmds = NULL;
                    pipeline.num_cmds = 0;
                    
                    return pipeline;
                }

                curr->outfile = tokens[i++];
                curr->append = 1;
            }

            else {

                char **tmp = realloc(
                    curr->args,
                    (argc + 2) * sizeof(char *)
                );

                if (tmp == NULL) {
                  perror("realloc");
                  
                  free_pipeline(pipeline);
                  pipeline.cmds = NULL;
                  pipeline.num_cmds = 0;
                  return pipeline;
                }

                curr->args = tmp;
                curr->args[argc++] = tokens[i++];
            }
        }

        if (argc == 0) {
          fprintf(stderr,
                  "syntax error: empty command\n");
          
          free_pipeline(pipeline);
          pipeline.cmds = NULL;
          pipeline.num_cmds = 0;
          return pipeline;
            
        }

        curr->args[argc] = NULL;
        curr->num_args = argc;

        if (tokens[i] != NULL)
            i++; 
    }

    return pipeline;
}