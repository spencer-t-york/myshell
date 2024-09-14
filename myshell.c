#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#define MAXLINE 4096 // max length for input

// Error Handling
static void err_doit(int , int , const char *, va_list);
void err_ret(const char *, ...);
void err_sys(const char *, ...);

char *gnu_getcwd();     // return current directory
char **split(char *);   // split shell args
int find_pipe(char **); // find pipe character in array

// MAIN FUNCTION
int main(void) {
    char   buf[MAXLINE];
    pid_t  pid;
    int    status;

    printf("%s %% ", strrchr(gnu_getcwd(), '/') + 1);  // print prompt and shows the current directory
    while (fgets(buf, MAXLINE, stdin) != NULL) {
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = 0; // replace newline with null

        char **args = split(buf);  // split buf by spaces and save to array

        // empty command
        if (args[0] == NULL) {
            printf("%s %% ", strrchr(gnu_getcwd(), '/') + 1);
            continue;
        }

        // cd command
        if (!strcmp(args[0], "cd")) { // if "cd" is entered
            if (args[1] == NULL) {
                fprintf(stderr, "cd: missing argument\n"); // Handle missing argument
            } else if (chdir(args[1]) != 0) {
                perror("cd"); // Print error if chdir fails
            }
            printf("%s %% ", strrchr(gnu_getcwd(), '/') + 1);
            continue;
        }

        // pipe command
        int const pipe_pos = find_pipe(args);
        if (pipe_pos != -1) {
            args[pipe_pos] = NULL; // set the pipe = NULL
            char **args_l = args, **args_r = &args[pipe_pos+1]; // split args left and right of pipe
            pid_t pid1, pid2;
            int p[2];

            if (pipe(p) < 0) {
                perror("pipe");
                exit(1);
            }

            // left side
            if ((pid1 = fork()) < 0) {
                err_sys("fork error");
            } else if (pid1 == 0) {
                close(p[0]);                // clear pipe read data
                // TODO: change dup2() to dup() -----------------------------------------------------
                close(STDOUT_FILENO);    // close fd 1 so it's the lowest fd open
                dup(p[1]);                  // write pipe output (this goes to the lowest fd open)
                close(p[1]);                // clear pipe write data

                execvp(args_l[0], args_l);  // execute left side
                err_ret("couldn't execute: %s", args_l[0]); // if execvp fails, return error
                _exit(127);
            }

            // right side
            if ((pid2 = fork()) < 0) {
                err_sys("fork error");
            } else if (pid2 == 0) {
                close(p[1]);               // clear pipe write data
                // TODO: change dup2() to dup() -----------------------------------------------------
                close(STDIN_FILENO);    // close fd 0 so it's the lowest fd open
                dup(p[0]);                 // reads pipe input (this goes to the lowest fd open)
                close(p[0]);               // clear pipe read data

                execvp(args_r[0], args_r);  // execute right side
                err_ret("couldn't execute: %s", args_r[0]); // if execvp fails, return error
                _exit(127);
            }

            close(p[0]);  // close both ends in parent
            close(p[1]);

            if (waitpid(pid1, &status, 0) < 0)
                err_sys("waitpid error");

            if (waitpid(pid2, &status, 0) < 0)
                err_sys("waitpid error");

            printf("%s %% ", strrchr(gnu_getcwd(), '/') + 1);
            continue;
        }

        if ((pid = fork()) < 0) {  // if fork() fails
            err_sys("fork error"); // return error
        }

        /* ----- CHILD PROCESS ----- */
        else if (pid == 0) {
            if (!strcmp(args[0], "quit")) {      // if "quit" is entered
                _exit(0);                             // exit child process
            } else {
                execvp(args[0], args); // replace process with specifed process in args[]
                err_ret("couldn't execute: %s", buf); // if execvp fails, return error
                _exit(127);
            }
        }

        /* ----- PARENT PROCESS ----- */
        if (strcmp(args[0], "quit") == 0) { // if "quit" was entered, exit parent process
            printf("exiting...\n");
            break; // break out of loop
        }

        if ((pid = waitpid(pid, &status, 0)) < 0)
            err_sys("waitpid error");
        // display shell prompt again with current directory
        printf("%s %% ", strrchr(gnu_getcwd(), '/') + 1);
    }
    exit(0);
}


// ---------- FUNCTIONS ---------- //

// ERROR HANDLING
static void err_doit(int errnoflag, int error, const char *fmt, va_list ap) {
    char buf[MAXLINE];
    vsnprintf(buf, MAXLINE-1, fmt, ap);
    if (errnoflag)
        snprintf(buf+strlen(buf), MAXLINE-strlen(buf)-1, ": %s", strerror(error));
    strcat(buf, "\n");
    fflush(stdout);    /* in case stdout and stderr are the same */
    fputs(buf, stderr);
    fflush(NULL);      /* flushes all stdio output streams */
}

void err_ret(const char *fmt, ...) {
    va_list   ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
}

void err_sys(const char *fmt, ...) {
    va_list   ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    exit(1);
}

// RETURN CURRENT DIRECTORY FUNCTION
// Function adapted from GNU C Library manual:
// https://www.gnu.org/software/libc/manual/html_node/Working-Directory.html
char *gnu_getcwd ()
{
  size_t size = 100;

  while (1)
    {
      char *buffer = (char *) malloc (size);
      if (getcwd (buffer, size) == buffer)
        return buffer;
      free (buffer);
      if (errno != ERANGE)
        return 0;
      size *= 2;
    }
}

// SPLIT BUFFER BY WHITESPACE FUNCTION
char **split(char *str) {
    char **newArray = (char **)malloc(MAXLINE * sizeof(char *));
    char *token = strtok(str, " ");
    int i =  0;

    while (token != NULL && i < MAXLINE - 1) {
        newArray[i++] = token;
        token = strtok(NULL, " ");
    }
    newArray[i] = NULL;

    return newArray;
}

// FUNCTION TO FIND LOCATION OF PIPE
int find_pipe(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            return i;
        }
    }
    return -1;
}
