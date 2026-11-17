#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

typedef struct {
    char** lines;
    int count;
    int cap;
} block_t;

static void block_add(block_t* b, const char* line) {
    if (b->count == b->cap) {
        b->cap = (b->cap == 0) ? 8 : b->cap * 2;
        b->lines = realloc(b->lines, b->cap * sizeof(char*));
    }
    b->lines[b->count++] = strdup(line);
}

static void block_free(block_t* b) {
    for (int i = 0; i < b->count; i++) {
        free(b->lines[i]);
    }
    free(b->lines);
    b->lines = NULL;
    b->count = 0;
    b->cap = 0;
}

// Removes leading and trailing spaces and tabs, edits the string in place
static char* trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
    return s;
}

// Tokenizes and runs one command line and returns its exit status
static int run_text(char* text) {
    char** arglist = tokenize(text);
    if (arglist == NULL) {
        return 0;
    }
    int status = run_line(arglist);
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
    return status;
}

int is_if_line(const char* line) {
    while (*line == ' ' || *line == '\t') line++;
    return strncmp(line, "if", 2) == 0 &&
           (line[2] == '\0' || line[2] == ' ' || line[2] == '\t');
}

void handle_if(const char* ifline) {
    const char* p = ifline;
    while (*p == ' ' || *p == '\t') p++;
    p += 2; // skip the word "if"

    char* cond = strdup(p);
    char* c = trim(cond);
    if (*c == '\0') {
        fprintf(stderr, "syntax error: missing condition after if\n");
        free(cond);
        return;
    }

    block_t then_b = {0};
    block_t else_b = {0};
    int state = 0; // 0 = waiting for then, 1 = in then block, 2 = in else block
    int done = 0;
    int ok = 1;

    while (!done) {
        char* line = readline("> ");
        if (line == NULL) {
            fprintf(stderr, "syntax error: unexpected end of input, missing fi\n");
            ok = 0;
            break;
        }
        char* t = trim(line);
        if (*t == '\0') {
            free(line);
            continue;
        }

        if (state == 0) {
            if (strcmp(t, "then") != 0) {
                fprintf(stderr, "syntax error: expected then\n");
                ok = 0;
                done = 1;
            } else {
                state = 1;
            }
        } else if (strcmp(t, "else") == 0) {
            if (state == 2) {
                fprintf(stderr, "syntax error: unexpected else\n");
                ok = 0;
                done = 1;
            } else {
                state = 2;
            }
        } else if (strcmp(t, "fi") == 0) {
            done = 1;
        } else {
            block_add(state == 1 ? &then_b : &else_b, t);
        }
        free(line);
    }

    if (ok) {
        int status = run_text(c);
        block_t* chosen = (status == 0) ? &then_b : &else_b;
        for (int i = 0; i < chosen->count; i++) {
            run_text(chosen->lines[i]);
        }
    }

    free(cond);
    block_free(&then_b);
    block_free(&else_b);
}
