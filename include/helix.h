#ifndef HELIX_H
#define HELIX_H

#define MAX_ARGS 64
#define MAX_INPUT 1024

typedef struct {
    char **tokens;
    int count;
} TokenList;

TokenList tokenize(char *input) ;
void free_tokens(TokenList tokens);

void execute(TokenList *tokens);
int is_builtin(char *cmd);
int run_builtin(TokenList tokens);

#endif
