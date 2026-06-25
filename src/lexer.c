#include "helix.h"
#include <stdlib.h>
#include <string.h>

TokenList tokenize(char *input)
{

    TokenList list = {
        .count = 0,
        .tokens = malloc(MAX_INPUT * sizeof(char *))
    };

    if (list.tokens == NULL) {
        exit(EXIT_FAILURE);
    }

    char *saveptr;

    for (char *token = strtok_r(input, " \t\r\n", &saveptr);
         token != NULL;
         token = strtok_r(NULL, " \t\r\n", &saveptr))
    {
        list.tokens[list.count++] = token;
    }

    list.tokens[list.count] = NULL;

    return list;
}

void free_tokens(TokenList tokens)
{
    free(tokens.tokens);
}
