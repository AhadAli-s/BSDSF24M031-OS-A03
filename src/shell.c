#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>


char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
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
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
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

int handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
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
    if (strcmp(arglist[0], "history") == 0) {
        hist_print();
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        Close the shell\n");
        printf("  cd <dir>    Change the working directory\n");
        printf("  help        Show this message\n");
        printf("  history     Show the command history\n");
        printf("  jobs        Show background jobs (not implemented yet)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;
}
static char* builtin_names[] = {"exit", "cd", "help", "jobs", "history", NULL};

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
