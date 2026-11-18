#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

static int is_op_char(char c) {
    return c == '<' || c == '>' || c == '|' || c == ';' || c == '&';
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace

        if (*cp == '\0') break; // Line was only whitespace

        len = 0;
        if (is_op_char(*cp)) {
            // Operator: a one-character token on its own
            arglist[argnum][len++] = *cp++;
        } else {
            // Word: ends at whitespace or an operator, quoted parts are copied
            // without the quote characters, so NAME="two words" is one token
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' && !is_op_char(*cp)) {
                if (*cp == '"' || *cp == '\'') {
                    char quote = *cp++;
                    while (*cp != '\0' && *cp != quote) {
                        if (len < ARGLEN - 1) {
                            arglist[argnum][len++] = *cp;
                        }
                        cp++;
                    }
                    if (*cp == quote) {
                        cp++;
                    }
                } else {
                    if (len < ARGLEN - 1) {
                        arglist[argnum][len++] = *cp;
                    }
                    cp++;
                }
            }
        }
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    // Free the unused slots and mark the end of the list
    for (int i = argnum; i < MAXARGS + 1; i++) {
        free(arglist[i]);
        arglist[i] = NULL;
    }
    return arglist;
}

int handle_builtin(char** arglist) {
    if (is_assignment(arglist[0])) {
        if (arglist[1] != NULL) {
            fprintf(stderr, "%s: unexpected arguments after assignment\n", arglist[0]);
            return 1;
        }
        char* eq = strchr(arglist[0], '=');
        char name[ARGLEN];
        size_t name_len = eq - arglist[0];
        memcpy(name, arglist[0], name_len);
        name[name_len] = '\0';
        var_set(name, eq + 1);
        return 1;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        var_free_all();
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing directory\n");
        } else if (chdir(arglist[1]) == -1) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        Close the shell\n");
        printf("  cd <dir>    Change the working directory\n");
        printf("  help        Show this message\n");
        printf("  jobs        Show running background jobs\n");
        printf("  history     Show the command history\n");
        printf("  set         Show all shell variables\n");
        printf("  NAME=value  Create or change a shell variable\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        hist_print();
        return 1;
    }

    if (strcmp(arglist[0], "set") == 0) {
        var_print_all();
        return 1;
    }

    return 0;
}

static char* hist_buf[HISTORY_SIZE];
static int hist_count = 0;

void hist_add(const char* cmdline) {
    if (hist_count == HISTORY_SIZE) {
        free(hist_buf[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            hist_buf[i - 1] = hist_buf[i];
        }
        hist_count--;
    }
    hist_buf[hist_count++] = strdup(cmdline);
}

void hist_print(void) {
    for (int i = 0; i < hist_count; i++) {
        printf("%d  %s\n", i + 1, hist_buf[i]);
    }
}

char* hist_expand(const char* cmdline) {
    char* end;
    long n = strtol(cmdline + 1, &end, 10);

    if (end == cmdline + 1 || *end != '\0') {
        fprintf(stderr, "%s: invalid history reference\n", cmdline);
        return NULL;
    }
    if (n < 1 || n > hist_count) {
        fprintf(stderr, "!%ld: event not found\n", n);
        return NULL;
    }
    return strdup(hist_buf[n - 1]);
}

static int is_operator(const char* tok) {
    return strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0 || strcmp(tok, "|") == 0;
}

int parse_pipeline(char** arglist, command_t* cmds) {
    int n = 0;
    int argc = 0;
    memset(&cmds[0], 0, sizeof(command_t));

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (argc == 0 || n + 1 >= MAX_CMDS) {
                fprintf(stderr, "syntax error near |\n");
                return -1;
            }
            cmds[n].argv[argc] = NULL;
            n++;
            memset(&cmds[n], 0, sizeof(command_t));
            argc = 0;
        } else if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
            if (arglist[i + 1] == NULL || is_operator(arglist[i + 1])) {
                fprintf(stderr, "syntax error: missing file name after %s\n", arglist[i]);
                return -1;
            }
            if (arglist[i][0] == '<') {
                cmds[n].infile = arglist[i + 1];
            } else {
                cmds[n].outfile = arglist[i + 1];
            }
            i++;
        } else {
            cmds[n].argv[argc++] = arglist[i];
        }
    }

    if (argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        return -1;
    }
    cmds[n].argv[argc] = NULL;
    return n + 1;
}

static char* builtin_names[] = {"exit", "cd", "help", "jobs", "history", "set", NULL};

static char* builtin_generator(const char* text, int state) {
    static int index;
    static size_t len;

    if (state == 0) {
        index = 0;
        len = strlen(text);
    }
    while (builtin_names[index] != NULL) {
        char* name = builtin_names[index++];
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0) {
        return rl_completion_matches(text, builtin_generator);
    }
    return NULL;
}

void init_readline(void) {
    rl_attempted_completion_function = shell_completion;
}
