#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define CYAN   "\033[36m"
#define YELLOW "\033[33m"
#define RESET  "\033[0m"

#define CACHE_SIZE 128

static struct {
    char *name;
    int found;
} cache[CACHE_SIZE];

static int cache_len = 0;

static int search_path(const char *cmd)
{
    const char *path = getenv("PATH");

    if (path == NULL)
        return 0;

    char copy[4096];
    char full[4096];

    snprintf(copy, sizeof(copy), "%s", path);

    for (char *dir = strtok(copy, ":");
         dir != NULL;
         dir = strtok(NULL, ":")) {

        snprintf(full, sizeof(full), "%s/%s", dir, cmd);

        if (access(full, X_OK) == 0)
            return 1;
    }

    return 0;
}

/*
 * Called for every keystroke, so results are remembered for the session. A
 * new binary dropped into  will not be noticed until the next start.
 */
int command_exists(const char *cmd)
{
    if (cmd[0] == '\0')
        return 0;

    if (is_builtin((char *)cmd) || alias_lookup(cmd) != NULL)
        return 1;

    /* A path, not a bare name: ask the filesystem directly. */
    if (strchr(cmd, '/') != NULL)
        return access(cmd, X_OK) == 0;

    for (int i = 0; i < cache_len; i++) {
        if (strcmp(cache[i].name, cmd) == 0)
            return cache[i].found;
    }

    int found = search_path(cmd);

    if (cache_len < CACHE_SIZE) {
        cache[cache_len].name  = strdup(cmd);
        cache[cache_len].found = found;
        cache_len++;
    }

    return found;
}

static int is_operator(const char *word)
{
    return !strcmp(word, "|") || !strcmp(word, "<") ||
           !strcmp(word, ">") || !strcmp(word, ">>") ||
           !strcmp(word, "&");
}

/*
 * Prints the line the user is typing with colour: command names green when
 * they exist and red when they do not, operators cyan, quoted text yellow.
 * Words are split on whitespace only, exactly like the lexer does.
 */
void highlight_print(const char *buf)
{
    int i = 0;
    int expect_command = 1;

    while (buf[i] != '\0') {

        if (isspace((unsigned char)buf[i])) {
            putchar(buf[i++]);
            continue;
        }

        int start = i;

        while (buf[i] != '\0' && !isspace((unsigned char)buf[i]))
            i++;

        int len = i - start;

        char word[MAX_INPUT];
        snprintf(word, sizeof(word), "%.*s", len, buf + start);

        const char *color;

        if (is_operator(word)) {
            color = CYAN;
            expect_command = (strcmp(word, "|") == 0);
        }
        else if (word[0] == '"' || word[0] == '\'') {
            color = YELLOW;
        }
        else if (expect_command) {
            color = command_exists(word) ? GREEN : RED;
            expect_command = 0;
        }
        else {
            color = RESET;
        }

        printf("%s%s" RESET, color, word);
    }
}
