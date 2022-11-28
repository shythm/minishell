#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1

#define EOL	1
#define ARG	2
#define AMPERSAND 3
#define TOKEN_TYPE_REDIRECTION_R 4
#define TOKEN_TYPE_REDIRECTION_L 5

#define FOREGROUND 0
#define BACKGROUND 1

#define INPUT_SIZE  512

static  char    input[INPUT_SIZE];
static  char    tokens[1024];
char            *ptr, *tok;

int get_token(char** outptr) {
    int	type;

    *outptr = tok;
    while ((*ptr == ' ') || (*ptr == '\t')) ptr++;

    *tok++ = *ptr;

    switch (*ptr++) {
        case '\0' : type = EOL; break;
        case '>': type = TOKEN_TYPE_REDIRECTION_R; break;
        case '<': type = TOKEN_TYPE_REDIRECTION_L; break;
        case '&': type = AMPERSAND; break;
        default : type = ARG;
            while ((*ptr != ' ') && (*ptr != '&') &&
                (*ptr != '\t') && (*ptr != '\0'))
                *tok++ = *ptr++;
    }
    *tok++ = '\0';

    return type;
}

int execute(char **comm, int how, int redirection) {
    int	pid;

    if ((pid = fork()) < 0) {
        fprintf(stderr, "minish : fork error\n");
        return(-1);
    }
    else if (pid == 0) {
        execvp(*comm, comm);
        fprintf(stderr, "minish : command not found\n");
        exit(127);
    }
    if (how == BACKGROUND) {	/* Background execution */
        printf("[%d]\n", pid);
        return 0;
    }
    /* Foreground execution */
    while (waitpid(pid, NULL, 0) < 0)
        if (errno != EINTR) return -1;
    return 0;
}

int parse_and_execute(char* input)
{
    char* arg[1024];
    int	type, how;
    int	quit = FALSE;
    int	narg = 0;
    int	finished = FALSE;
    int redirection = FALSE;
    char redirection_arg[100];

    ptr = input;
    tok = tokens;
    while (!finished) {
        switch (type = get_token(&arg[narg])) {
        case ARG:
            narg++;
            break;
        case TOKEN_TYPE_REDIRECTION_R:
            strcpy(redirection_arg, arg[narg]);
            redirection = TOKEN_TYPE_REDIRECTION_R;
            break;
        case TOKEN_TYPE_REDIRECTION_L:
            strcpy(redirection_arg, arg[narg]);
            redirection = TOKEN_TYPE_REDIRECTION_L;
            break;
        case EOL:
        case AMPERSAND:
            if (!strcmp(arg[0], "quit")) quit = TRUE;
            else if (!strcmp(arg[0], "exit")) quit = TRUE;
            else if (!strcmp(arg[0], "cd")) {
                if (chdir(arg[1]) == -1) {
                    perror("chdir");
                }
            }
            else if (!strcmp(arg[0], "type")) {
                if (narg > 1) {
                    int  fid;
                    int	 readcount;
                    char buf[512];
                    if ((fid = open(arg[1], O_RDONLY)) >= 0) {
                        while ((readcount = read(fid, &buf, 1)) > 0) {
                            write(1, &buf, readcount);
                        }
                    } else {
                        perror("open");
                    }
                    close(fid);
                }
            } else {
                if (redirection) {
                    printf("%s \n", redirection_arg);
                }
                how = (type == AMPERSAND) ? BACKGROUND : FOREGROUND;
                arg[narg] = NULL;
                if (narg != 0)
                    execute(arg, how, 0);
            }
            narg = 0;
            if (type == EOL)
                finished = TRUE;
            break; 
        }

    }
    return quit;
}

int main(void)
{
    int	quit_sig;

    printf("msh # ");
    while (fgets(input, INPUT_SIZE, stdin)) {
        for (int i = 0; i < INPUT_SIZE; i++) {
            if (input[i] == '\n') {
                input[i] = '\0';
                break;
            }
        }

        quit_sig = parse_and_execute(input);
        if (quit_sig) {
            break;
        }
        printf("msh # ");
    }

    return 0;
}
