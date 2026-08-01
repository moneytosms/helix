#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALIASES 64
#define MAX_ALIAS_WORDS 16
#define MAX_SETTINGS 32

/*
 * ~/.helixrc, one directive per line:
 *
 *   # comment
 *   alias ll="ls -la"
 *   set HISTORY_SIZE=500
 *   set DASHBOARD=off
 *
 * Everything read here lives until the shell exits, so nothing is freed.
 */

typedef struct {
    char *name;
    char *words[MAX_ALIAS_WORDS + 1];   /* NULL terminated, ready for execvp */
} Alias;

typedef struct {
    char *key;
    char *value;
} Setting;

static Alias aliases[MAX_ALIASES];
static int num_aliases = 0;

static Setting settings[MAX_SETTINGS];
static int num_settings = 0;

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;

    char *end = s + strlen(s);

    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\n' || end[-1] == '\r'))
        end--;

    *end = '\0';

    return s;
}

static char *unquote(char *s)
{
    size_t len = strlen(s);

    if (len >= 2 && (s[0] == '"' || s[0] == '\'') && s[len - 1] == s[0]) {
        s[len - 1] = '\0';
        s++;
    }

    return s;
}

static void add_alias(char *name, char *value)
{
    if (num_aliases == MAX_ALIASES)
        return;

    Alias *alias = &aliases[num_aliases];

    alias->name = strdup(name);

    int count = 0;
    char *copy = strdup(value);

    for (char *word = strtok(copy, " \t");
         word != NULL && count < MAX_ALIAS_WORDS;
         word = strtok(NULL, " \t"))
        alias->words[count++] = strdup(word);

    free(copy);

    if (count == 0) {
        free(alias->name);
        return;
    }

    alias->words[count] = NULL;

    num_aliases++;
}

static void add_setting(char *key, char *value)
{
    if (num_settings == MAX_SETTINGS)
        return;

    settings[num_settings].key   = strdup(key);
    settings[num_settings].value = strdup(value);

    num_settings++;
}

void config_load(void)
{
    const char *home = getenv("HOME");

    if (home == NULL)
        return;

    char path[512];

    snprintf(path, sizeof(path), "%s/.helixrc", home);

    FILE *f = fopen(path, "r");

    if (f == NULL)
        return;

    char line[MAX_INPUT];
    int lineno = 0;

    while (fgets(line, sizeof(line), f) != NULL) {

        lineno++;

        char *text = trim(line);

        if (text[0] == '\0' || text[0] == '#')
            continue;

        if (strncmp(text, "alias ", 6) == 0) {

            char *rest = trim(text + 6);
            char *eq = strchr(rest, '=');

            if (eq == NULL) {
                fprintf(stderr, "helix: %s:%d: alias needs name=value\n",
                        path, lineno);
                continue;
            }

            *eq = '\0';

            add_alias(trim(rest), unquote(trim(eq + 1)));
        }
        else if (strncmp(text, "set ", 4) == 0) {

            char *rest = trim(text + 4);
            char *eq = strchr(rest, '=');

            if (eq == NULL) {
                fprintf(stderr, "helix: %s:%d: set needs KEY=value\n",
                        path, lineno);
                continue;
            }

            *eq = '\0';

            add_setting(trim(rest), unquote(trim(eq + 1)));
        }
        else {
            fprintf(stderr, "helix: %s:%d: unknown directive\n", path, lineno);
        }
    }

    fclose(f);
}

const char *setting_get(const char *key)
{
    for (int i = 0; i < num_settings; i++) {
        if (strcmp(settings[i].key, key) == 0)
            return settings[i].value;
    }

    return NULL;
}

int setting_int(const char *key, int fallback)
{
    const char *value = setting_get(key);

    return value != NULL ? atoi(value) : fallback;
}

char **alias_lookup(const char *name)
{
    for (int i = 0; i < num_aliases; i++) {
        if (strcmp(aliases[i].name, name) == 0)
            return aliases[i].words;
    }

    return NULL;
}

void alias_list(void)
{
    for (int i = 0; i < num_aliases; i++) {

        printf("alias %s=\"", aliases[i].name);

        for (int j = 0; aliases[i].words[j] != NULL; j++)
            printf("%s%s", j ? " " : "", aliases[i].words[j]);

        printf("\"\n");
    }
}
