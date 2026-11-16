#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

    init_readline();

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (cmdline[0] == '!') {
            char* expanded = hist_expand(cmdline);
            free(cmdline);
            if (expanded == NULL) {
                continue;
            }
            cmdline = expanded;
            printf("%s\n", cmdline);
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            hist_add(cmdline);
            add_history(cmdline);

            command_t cmds[MAX_CMDS];
            int ncmds = parse_pipeline(arglist, cmds);

            if (ncmds > 0) {
                if (ncmds > 1 || handle_builtin(cmds[0].argv) == 0) {
                    execute_pipeline(cmds, ncmds);
                }
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
