#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 30
#define ARGLEN 256
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_CMDS 10

typedef struct {
    char* argv[MAXARGS + 1];
    char* infile;
    char* outfile;
} command_t;

// Function prototypes
char** tokenize(char* cmdline);
int execute(char** arglist);
int handle_builtin(char** arglist);
void hist_add(const char* cmdline);
void hist_print(void);
char* hist_expand(const char* cmdline);
int parse_pipeline(char** arglist, command_t* cmds);
int execute_pipeline(command_t* cmds, int n, int background, const char* label);
void init_readline(void);
int run_line(char** arglist);
void add_job(pid_t pid, const char* cmd);
void print_jobs(void);
void reap_jobs(void);
int is_if_line(const char* line);
void handle_if(const char* ifline);

#endif // SHELL_H
