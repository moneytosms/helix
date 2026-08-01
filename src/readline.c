#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define HISTORY_MAX 1000

static char *history[HISTORY_MAX];
static int history_len = 0;
static int history_limit = HISTORY_MAX;

static struct termios orig_termios;

/* ---------- history ---------- */

static const char *history_path(void)
{
    static char path[512];
    const char *home = getenv("HOME");

    if (home == NULL)
        return NULL;

    snprintf(path, sizeof(path), "%s/.helix_history", home);

    return path;
}

void history_add(const char *line)
{
    if (line[0] == '\0')
        return;

    /* Don't store the same command twice in a row. */
    if (history_len > 0 && strcmp(history[history_len - 1], line) == 0)
        return;

    if (history_len == history_limit) {
        free(history[0]);
        memmove(history, history + 1, (history_len - 1) * sizeof(char *));
        history_len--;
    }

    history[history_len++] = strdup(line);
}

void history_load(void)
{
    history_limit = setting_int("HISTORY_SIZE", HISTORY_MAX);

    if (history_limit < 1 || history_limit > HISTORY_MAX)
        history_limit = HISTORY_MAX;

    const char *path = history_path();

    if (path == NULL)
        return;

    FILE *f = fopen(path, "r");

    if (f == NULL)
        return;

    char line[MAX_INPUT];

    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        history_add(line);
    }

    fclose(f);
}

void history_save(void)
{
    const char *path = history_path();

    if (path == NULL)
        return;

    FILE *f = fopen(path, "w");

    if (f == NULL) {
        perror("history");
        return;
    }

    for (int i = 0; i < history_len; i++) {
        fputs(history[i], f);
        fputc('\n', f);
    }

    fclose(f);
}

void history_print(void)
{
    for (int i = 0; i < history_len; i++)
        printf("%5d  %s\n", i + 1, history[i]);
}

/* ---------- raw mode ---------- */

static int raw_mode_on(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        return -1;

    raw = orig_termios;

    /*
     * ISIG goes too, so Ctrl+C and Ctrl+Z arrive as plain bytes while we are
     * editing a line. The keys only need to become signals once a child owns
     * the terminal, and by then we have already left raw mode.
     */
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void raw_mode_off(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

/* ---------- line editing ---------- */

static void redraw(const char *prompt, size_t prompt_width,
                   const char *buf, int cursor)
{
    printf("\r\033[K%s", prompt);
    highlight_print(buf);

    printf("\r");

    if (prompt_width + cursor > 0)
        printf("\033[%zuC", prompt_width + cursor);

    fflush(stdout);
}

static void load_history_entry(char *buf, int *len, int *cursor, int pos)
{
    if (pos == history_len) {
        buf[0] = '\0';
        *len = *cursor = 0;
        return;
    }

    snprintf(buf, MAX_INPUT, "%s", history[pos]);

    *len = (int)strlen(buf);
    *cursor = *len;
}

/*
 * Reads one line with editing, history and live highlighting. Returns a
 * malloc'd string the caller owns, or NULL on end of input.
 * prompt_width is the printable width of prompt, needed to place the cursor.
 */
char *helix_readline(const char *prompt, size_t prompt_width)
{
    /* No terminal (a script piping into us): plain line reads will do. */
    if (!shell_interactive || raw_mode_on() == -1) {

        char line[MAX_INPUT];

        fputs(prompt, stdout);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
            return NULL;

        line[strcspn(line, "\n")] = '\0';

        return strdup(line);
    }

    char buf[MAX_INPUT] = "";
    int len = 0;
    int cursor = 0;
    int pos = history_len;   /* where up/down currently sit */

    redraw(prompt, prompt_width, buf, cursor);

    for (;;) {

        char c;

        if (read(STDIN_FILENO, &c, 1) != 1) {
            raw_mode_off();
            return NULL;
        }

        if (c == '\r' || c == '\n') {
            printf("\r\n");
            fflush(stdout);
            raw_mode_off();
            return strdup(buf);
        }

        if (c == 4) {                       /* Ctrl+D */
            if (len == 0) {
                raw_mode_off();
                return NULL;
            }
            continue;
        }

        if (c == 3) {                       /* Ctrl+C: abandon the line */
            printf("^C\r\n");
            fflush(stdout);
            raw_mode_off();
            return strdup("");
        }

        if (c == 1) {                       /* Ctrl+A */
            cursor = 0;
        }
        else if (c == 5) {                  /* Ctrl+E */
            cursor = len;
        }
        else if (c == 11) {                 /* Ctrl+K */
            len = cursor;
            buf[len] = '\0';
        }
        else if (c == 12) {                 /* Ctrl+L */
            printf("\033[2J\033[H");
        }
        else if (c == 127 || c == 8) {      /* Backspace */
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor, len - cursor + 1);
                cursor--;
                len--;
            }
        }
        else if (c == 27) {                 /* escape sequence */

            char seq[2];

            /* A bare Escape with nothing after it will simply wait here. */
            if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
                read(STDIN_FILENO, &seq[1], 1) != 1)
                continue;

            if (seq[0] != '[')
                continue;

            switch (seq[1]) {

            case 'A':                       /* up */
                if (pos > 0)
                    load_history_entry(buf, &len, &cursor, --pos);
                break;

            case 'B':                       /* down */
                if (pos < history_len)
                    load_history_entry(buf, &len, &cursor, ++pos);
                break;

            case 'C':                       /* right */
                if (cursor < len)
                    cursor++;
                break;

            case 'D':                       /* left */
                if (cursor > 0)
                    cursor--;
                break;

            case 'H':
                cursor = 0;
                break;

            case 'F':
                cursor = len;
                break;
            }
        }
        else if ((unsigned char)c >= 32 && len < MAX_INPUT - 1) {

            memmove(buf + cursor + 1, buf + cursor, len - cursor + 1);

            buf[cursor++] = c;
            len++;
        }

        redraw(prompt, prompt_width, buf, cursor);
    }
}
