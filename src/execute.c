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
            // printf("Child pid:%d exited with status %d\n", cpid, status >> 8);
            return 0;
    }
}
int execute_pipeline(command_t* cmds, int n) {
    int prev_read = -1;
    int fd[2];
    pid_t pids[MAX_CMDS];
    int status;

    for (int i = 0; i < n; i++) {
        if (i < n - 1) {
            if (pipe(fd) == -1) {
                perror("pipe");
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            return -1;
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
            exit(1);
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

    for (int i = 0; i < n; i++) {
        waitpid(pids[i], &status, 0);
    }
    return 0;
}
