// IMPORTS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#define MAXLINE 4096 // max length for input


// DEFINE FUNCTIONS
// Error Handling
static void err_doit(int , int , const char *, va_list);
void err_ret(const char *, ...);
void err_sys(const char *, ...);

char *gnu_getcwd();         // return current directory
char **split(char *);       // split shell args
int find_pipe(char **);     // find pipe character in array
void print_prompt(); // print prompt and shows the current directory
char *find_path(char *);   // find the path of the specified executable

// commands
void cd_command(char **);             // cd command
void pipe_command(char **, int, int); // pipe command


// MAIN FUNCTION
int main(void) {
    // Variables
    char   buf[MAXLINE];
    pid_t  pid;
    int    status;

    print_prompt();
    while (fgets(buf, MAXLINE, stdin) != NULL) {
        if (buf[strlen(buf) - 1] == '\n') { // if the buffer has a newline at the end...
            buf[strlen(buf) - 1] = 0;       // ...replace newline with null
        }

        char **args = split(buf);  // split buffer by white-spaces and save to the args array

        // empty command
        if (args[0] == NULL) {
            print_prompt();
            continue;
        }

        // cd command
        if (!strcmp(args[0], "cd")) { // if "cd" is entered
            cd_command(args);         // ...call cd command
            continue;
        }

        if (!strcmp(args[0], "path")) {
            // if "cd" is entered
            const char* s = getenv(args[1]);
            printf("getenv returns: %s", s);
            continue;
        }

        // pipe command
        int const pipe_pos = find_pipe(args);
        if (pipe_pos != -1) {
            pipe_command(args, pipe_pos, status);
            continue;
        }

        // quit command
        if (strcmp(args[0], "quit") == 0) { // if "quit" is entered...
            printf("exiting...\n");
            break; // break out of loop
        }

        // fork error
        if ((pid = fork()) < 0) {  // if fork() fails
            err_sys("fork error"); // return error
        }

        /* ----- CHILD PROCESS ----- */
        else if (pid == 0) {
            // TODO: change execvp to execv
            // int execv(const char *path, char *const argv[]);
            // int execvp(const char *file, char *const argv[]);
            execv(find_path(args[0]), args);
            //execvp(args[0], args); // replace process with specifed process in args[]
            err_ret("couldn't execute: %s", buf); // if execvp fails, return error
            _exit(127);
        } else {
            if ((pid = waitpid(pid, &status, 0)) < 0)
                err_sys("waitpid error");
            print_prompt();
        }
    }
    exit(0);
}


// ------------------------------ FUNCTIONS ------------------------------ //

// ------ ERROR HANDLING ------ //
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

// ------ RETURN CURRENT DIRECTORY ------ //
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

// ------ SPLIT BUFFER BY WHITESPACE ------ //
char **split(char *str) {
    char **newArray = (char **)malloc(MAXLINE * sizeof(char *));
    char *token = strtok(str, " ");
    int i = 0;

    while (token != NULL && i < MAXLINE - 1) {
        newArray[i++] = token;
        token = strtok(NULL, " ");
    }
    newArray[i] = NULL;

    return newArray;
}

// ------ SPLIT COMMAND PATHS BY COLON ------ //
char **split_cmd_dirs(char *str) {
    char **newArray = (char **)malloc(MAXLINE * sizeof(char *));
    char *token = strtok(str, ":");
    int i = 0;

    while (token != NULL && i < MAXLINE - 1) {
        newArray[i++] = token;
        token = strtok(NULL, ":");
    }
    newArray[i] = NULL;

    return newArray;
}

// ------ FIND LOCATION OF PIPE ------ //
int find_pipe(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            return i;
        }
    }
    return -1;
}

// ------ FIND LOCATION OF COMMAND EXECUTABLE ------ //
char *find_path(char *executable) {
    // store directories of commands in cmd_dir
    char *cmd_dirs = getenv("PATH");

    // split all directories to be stored in the path array
    char **path = split_cmd_dirs(cmd_dirs);
    char full_path[MAXLINE];

    for (int i = 0; path[i] != NULL; i++) {
        // tack on the file name to the end of the path
        snprintf(full_path, sizeof(full_path), "%s/%s", path[i], executable);

        // check if path exists
        if (access(full_path, F_OK) == 0) {
            free(path);
            return strdup(full_path);
        }
    }

    free(path);
    return NULL;
}

void print_prompt() {
    printf("%s %% ", strrchr(gnu_getcwd(), '/') + 1);
}

// ------ CD COMMAND ------ //
void cd_command(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: missing argument\n"); // Handle missing argument
    } else if (chdir(args[1]) != 0) {
        perror("cd"); // Print error if chdir() fails
    }
    print_prompt(); // Print prompt after handling cd command
}

// ------ PIPE COMMAND ------ //
void pipe_command(char **args, int pipe_pos, int status) {
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
        close(STDOUT_FILENO);    // close fd 1 so it's the lowest fd open
        dup(p[1]);                  // write pipe output (this goes to the lowest fd open)
        close(p[1]);                // clear pipe write data
        execv(find_path(args_l[0]), args_l);
        //execvp(args_l[0], args_l);  // execute left side
        err_ret("couldn't execute: %s", args_l[0]); // if execvp fails, return error
        _exit(127);
    }

    // right side
    if ((pid2 = fork()) < 0) {
        err_sys("fork error");
    } else if (pid2 == 0) {
        close(p[1]);               // clear pipe write data
        close(STDIN_FILENO);    // close fd 0 so it's the lowest fd open
        dup(p[0]);                 // reads pipe input (this goes to the lowest fd open)
        close(p[0]);               // clear pipe read data

        execv(find_path(args_r[0]), args_r);
        //execvp(args_r[0], args_r);  // execute right side
        err_ret("couldn't execute: %s", args_r[0]); // if execvp fails, return error
        _exit(127);
    }

    close(p[0]);  // close both ends in parent
    close(p[1]);

    if (waitpid(pid1, &status, 0) < 0)
        err_sys("waitpid error");

    if (waitpid(pid2, &status, 0) < 0)
        err_sys("waitpid error");

    print_prompt();
}
