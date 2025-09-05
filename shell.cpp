/*a very small shell
features:
1. shows prompt as "<cwd> > "
2. reads a command line like: ls -l, rm file, mkdir -p dir
3. parses into program + argv[] by whitespace
4. forks; child execs; parent waits
5. built-ins: exit (quit), cd [dir]
6. ignores Ctrl+C in the shell, but children still get it
*/
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 128
#define PROMPT_BUF 4096

static void on_sigint(int signo) {
    (void)signo; // ignore in shell; just reprint a newline
    write(STDOUT_FILENO, "\n", 1);
}

//
static void print_prompt(void) {
    char cwd[PROMPT_BUF];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "?");
    }
    printf("%s> ", cwd);
    fflush(stdout);
}

static int parse_line(char *line, char **argv) {
    int n = 0;
    // strip trailing newline
    size_t len = strlen(line);
    if (len && line[len-1] == '\n') line[len-1] = '\0';

    // skip leading spaces
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return 0;

    // simple whitespace split (no quotes/escapes for simplicity)
    while (*p && n < MAX_ARGS - 1) {
        // start of token
        argv[n++] = p;
        // move to end of token
        while (*p && *p != ' ' && *p != '\t') p++;
        if (!*p) break;
        *p++ = '\0';
        while (*p == ' ' || *p == '\t') p++;
    }
    argv[n] = NULL;
    return n;
}

int main(void) {
    // install a handler so Ctrl+C doesn't kill the shell itself
    struct sigaction sa = {0};
    sa.sa_handler = on_sigint; // Ctrl + C
    sigemptyset(&sa.sa_mask);// the signal mask (signals blocked while handler runs)
    sa.sa_flags = SA_RESTART; // so getline() restarts
    sigaction(SIGINT, &sa, NULL);

    char *line = NULL;
    size_t cap = 0;

    for (;;) {
        print_prompt();
        ssize_t got = getline(&line, &cap, stdin);
        if (got < 0) {
            // EOF (Ctrl+D) or error -> exit shell
            printf("\n");
            break;
        }

        char *argv[MAX_ARGS];
        int argc = parse_line(line, argv);
        if (argc == 0) continue; // empty line

        // built-in: exit
        if (strcmp(argv[0], "exit") == 0) {
            break;
        }
        // built-in: cd
        if (strcmp(argv[0], "cd") == 0) {
            const char *dir = (argc > 1) ? argv[1] : getenv("HOME");
            if (!dir) dir = ".";
            if (chdir(dir) != 0) perror("cd");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            // child: restore default SIGINT so Ctrl+C stops programs
            signal(SIGINT, SIG_DFL);
            execvp(argv[0], argv);
            // if we reach here, exec failed
            perror("exec");
            _exit(127);
        } else {
            int status = 0;
            while (waitpid(pid, &status, 0) < 0) {
                if (errno == EINTR) continue;
                perror("waitpid");
                break;
            }
        }
    }

    free(line);
    return 0;
}