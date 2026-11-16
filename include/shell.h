#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_CMDS 10

typedef struct {
    char* argv[MAXARGS + 1];
    char* infile;
    char* outfile;
} command_t;

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int handle_builtin(char** arglist);
int execute(char** arglist);
void hist_add(const char* cmdline);
void hist_print(void);
char* hist_expand(const char* cmdline);
void init_readline(void);
int parse_pipeline(char** arglist, command_t* cmds);
int execute_pipeline(command_t* cmds, int n);

#endif // SHELL_H
