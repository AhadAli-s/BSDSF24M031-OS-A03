#include "shell.h"

int execute(char* arglist[]) {
    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            execvp(arglist[0], arglist);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            return 0;
    }
}

int execute_pipeline(command_t* cmds, int n, int background, const char* label) {
    int prev_read = -1;
    int fd[2];
    pid_t pids[MAX_CMDS];
    int status;

    for (int i = 0; i < n; i++) {
        if (i < n - 1) {
            if (pipe(fd) == -1) {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            return 1;
        }

        if (pid == 0) { // Child process
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (i < n - 1) {
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }
            if (cmds[i].infile != NULL) {
                int f = open(cmds[i].infile, O_RDONLY);
                if (f == -1) {
                    perror(cmds[i].infile);
                    exit(1);
                }
                dup2(f, STDIN_FILENO);
                close(f);
            }
            if (cmds[i].outfile != NULL) {
                int f = open(cmds[i].outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (f == -1) {
                    perror(cmds[i].outfile);
                    exit(1);
                }
                dup2(f, STDOUT_FILENO);
                close(f);
            }
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("Command not found");
            exit(127);
        }

        // Parent process
        pids[i] = pid;
        if (prev_read != -1) {
            close(prev_read);
        }
        if (i < n - 1) {
            close(fd[1]);
            prev_read = fd[0];
        }
    }

    if (background) {
        add_job(pids[n - 1], label);
        return 0;
    }

    int last_status = 0;
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], &status, 0);
        if (i == n - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else {
                last_status = 128 + WTERMSIG(status);
            }
        }
    }
    return last_status;
}

#define MAX_JOBS 64

typedef struct {
    int id;
    pid_t pid;
    char cmd[256];
} job_t;

static job_t jobs[MAX_JOBS];
static int njobs = 0;
static int next_job_id = 1;

void add_job(pid_t pid, const char* cmd) {
    if (njobs == MAX_JOBS) {
        fprintf(stderr, "too many background jobs\n");
        return;
    }
    jobs[njobs].id = next_job_id++;
    jobs[njobs].pid = pid;
    strncpy(jobs[njobs].cmd, cmd, sizeof(jobs[njobs].cmd) - 1);
    jobs[njobs].cmd[sizeof(jobs[njobs].cmd) - 1] = '\0';
    printf("[%d] %d\n", jobs[njobs].id, (int)pid);
    njobs++;
}

void reap_jobs(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < njobs; i++) {
            if (jobs[i].pid == pid) {
                printf("[%d] Done  %s\n", jobs[i].id, jobs[i].cmd);
                for (int j = i + 1; j < njobs; j++) {
                    jobs[j - 1] = jobs[j];
                }
                njobs--;
                break;
            }
        }
    }
}

void print_jobs(void) {
    reap_jobs();
    if (njobs == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < njobs; i++) {
        printf("[%d] %d Running  %s\n", jobs[i].id, (int)jobs[i].pid, jobs[i].cmd);
    }
}

// Runs one command line piece (a pipeline) and returns its exit status,
// or -1 after a syntax error
static int run_segment(char** seg, int n, int background) {
    char* owned[MAXARGS + 1];
    int nowned = 0;
    int status;

    // Variable expansion: an argument that starts with $ is replaced
    // by a new copy of the value of the variable with that name
    for (int k = 0; k < n; k++) {
        if (seg[k][0] == '$' && seg[k][1] != '\0') {
            const char* value = var_get(seg[k] + 1);
            if (value == NULL) {
                value = getenv(seg[k] + 1);
            }
            seg[k] = strdup(value != NULL ? value : "");
            owned[nowned++] = seg[k];
        }
    }

    command_t cmds[MAX_CMDS];
    int ncmds = parse_pipeline(seg, cmds);

    if (ncmds <= 0) {
        status = -1;
    } else if (ncmds == 1 && handle_builtin(cmds[0].argv) == 1) {
        status = 0;
    } else {
        char label[256] = "";
        for (int k = 0; k < n; k++) {
            if (k > 0) {
                strncat(label, " ", sizeof(label) - strlen(label) - 1);
            }
            strncat(label, seg[k], sizeof(label) - strlen(label) - 1);
        }
        status = execute_pipeline(cmds, ncmds, background, label);
    }

    for (int k = 0; k < nowned; k++) {
        free(owned[k]);
    }
    return status;
}

int run_line(char** arglist) {
    int i = 0;
    int last_status = 0;

    while (arglist[i] != NULL) {
        char* seg[MAXARGS + 1];
        int n = 0;
        int background = 0;

        while (arglist[i] != NULL && strcmp(arglist[i], ";") != 0 &&
               strcmp(arglist[i], "&") != 0) {
            seg[n++] = arglist[i++];
        }
        if (arglist[i] != NULL) {
            if (strcmp(arglist[i], "&") == 0) {
                background = 1;
            }
            i++;
        }
        seg[n] = NULL;

        if (n == 0) {
            if (background) {
                fprintf(stderr, "syntax error near &\n");
                return 1;
            }
            continue;
        }

        int status = run_segment(seg, n, background);
        if (status < 0) {
            return 1;
        }
        last_status = status;
    }
    return last_status;
}
