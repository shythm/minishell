#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>

#define FALSE   0
#define TRUE    1

#define INPUT_SIZE  512

#define HOW_FOREGROUND  0
#define HOW_BACKGROUND  1

#define TOKEN_TYPE_NONE                 0
#define TOKEN_TYPE_AMPERSAND            1
#define TOKEN_TYPE_LEFT_ANGLE_BRACKET   2
#define TOKEN_TYPE_RIGHT_ANGLE_BRACKET  3
#define TOKEN_TYPE_DRIGHT_ANGLE_BRACKET 4
#define TOKEN_TYPE_PIPE                 5

typedef struct Tokenizer {
    char* token;
    int index;
    int type;
    int _last;
    char* _ptr;
} Tokenizer;

int _is_token_seperator(char ch) {
    switch (ch) {
        case ' ':
        case '\t':
        case '\n':
            return TRUE;
    }
    return FALSE;
}

int init_tokenizer(Tokenizer* tk, char* target) {
    // initialize fields
    tk->token = NULL;
    tk->index = -1;
    tk->type = TOKEN_TYPE_NONE;

    // get effective start position of token
    while (_is_token_seperator(*target)) {
        target++;
    };
    tk->_ptr = target;

    // if first token is null charactor, set _last field TRUE
    tk->_last = FALSE;
    if (*tk->_ptr == '\0') {
        tk->_last = TRUE;
    }
}

int _get_token_type(char* ch) {
    int type = TOKEN_TYPE_NONE;

    switch (*ch) {
        case '<': type = TOKEN_TYPE_LEFT_ANGLE_BRACKET; break;
        case '>': type = TOKEN_TYPE_RIGHT_ANGLE_BRACKET; break;
        case '&': type = TOKEN_TYPE_AMPERSAND; break;
        case '|': type = TOKEN_TYPE_PIPE; break;
    }

    if (*(++ch) == '\0') {
        return type;
    }

    switch (*ch) {
        case '>':
            if (type == TOKEN_TYPE_RIGHT_ANGLE_BRACKET) {
                type = TOKEN_TYPE_DRIGHT_ANGLE_BRACKET;
            } else {
                type = TOKEN_TYPE_NONE;
            }
            break;
    }

    return type;
}

int get_next_token(Tokenizer* tk) {
    char* ptr = tk->_ptr;
    
    // if there is no next token return 0
    if (tk->_last == TRUE) {
        return -1;
    }

    // set start position of token
    tk->token = ptr;
    
    // get end position of token
    while (!_is_token_seperator(*ptr)) {
        if (*ptr == '\0') {
            tk->_last = TRUE;
            break;
        }
        ptr++;
    }

    *(ptr++) = '\0'; // separate from other tokens
    tk->type = _get_token_type(tk->token); // get token type
    tk->index++; // increase index

    // get effective start position of next token
    if (tk->_last == FALSE) {
        while (_is_token_seperator(*ptr)) {
            ptr++;
        }
        if (*ptr == '\0') {
            tk->_last = TRUE;
        }
    }
    tk->_ptr = ptr;

    return tk->type;
}

int is_token_last(Tokenizer* tk) {
    return tk->_last;
}

int execute(char** argv, int how, int red, int pip) {
    pid_t pid;
    char** argv2;
    int red_fd;

    // redirection or pipe -> required rest of arguments
    if (red || pip) {
        // get start address of arguments for redirection or pipe
        for (argv2 = argv; *argv2 != (char*)0; argv2++);
        argv2++;
    }

    if ((pid = fork()) < 0) {
        perror("fork error");
        return -1;
    } else if (pid == 0) {
        /* child process */
        if (red) { // redirection
            int oflag = 0;
            switch (red) {
            case TOKEN_TYPE_RIGHT_ANGLE_BRACKET:
                oflag |= O_WRONLY | O_CREAT | O_TRUNC; break;
            case TOKEN_TYPE_DRIGHT_ANGLE_BRACKET:
                oflag |= O_WRONLY | O_CREAT | O_APPEND; break;
            case TOKEN_TYPE_LEFT_ANGLE_BRACKET:
                oflag |= O_RDONLY; break;
            }

            int red_fd;
            if ((red_fd = open(argv2[0], oflag, 0664)) < 0) { // open file
                perror("error");
                exit(1);
            }

            if (red == TOKEN_TYPE_LEFT_ANGLE_BRACKET) {
                dup2(red_fd, 0); // file -> stdin (< redirection)
            } else {
                dup2(red_fd, 1); // file -> stdout (>, >> redirection)
            }

            close(red_fd);
        }

        if (pip) { // pipe
            int pip_fd[2];
            if (pipe(pip_fd) == -1) { // create pipe
                perror("pipe error");
                exit(1);
            }

            if ((pid = fork()) < 0) { // create new process
                perror("fork error");
                exit(1);
            } else if (pid == 0) {
                /* child process of pipe */
                close(pip_fd[1]); // close pipe for write
                dup2(pip_fd[0], 0); // read pipe -> stdin
                execvp(*argv2, argv2);
                fprintf(stderr, "pipe error: command not found\n");
                exit(127);
            }
            
            close(pip_fd[0]); // close pip for read
            dup2(pip_fd[1], 1); // write pipe -> stdout
        }
        execvp(*argv, argv);
        fprintf(stderr, "error: command not found\n");
        exit(127);
    }

    /* parent process */
    if (how == HOW_BACKGROUND) { // background execution
        printf("background execution (PID: %d)\n", pid);
        return 0;
    }
    while (waitpid(pid, NULL, 0) < 0) // foreground execution
        if (errno != EINTR) return -1;

    return 0;
}

int command(char** argv, int argc) {
#define CMD(X) (!strcmp(*argv, (X)))
    if (CMD("exit")) {
        exit(0);
    } else if (CMD("quit")) {
        exit(0);
    } else if (CMD("cd")) {
        // https://stackoverflow.com/questions/4998530/can-chdir-accept-relative-paths
        if (chdir(argv[1]) < 0) {
            perror("error");
        }
    } else if (CMD("type")) {
        FILE* fp;
        char buf[100];
        if ((fp = fopen(argv[1], "r")) != NULL) {
            while (fgets(buf, 100, fp)) {
                fputs(buf, stdout);
            }
            fclose(fp);
        } else {
            perror("error");
        }
    } else {
        return 1; // external command
    }

    return 0;
#undef CMD
}

int interpret(char* input) {
    Tokenizer tk;
    init_tokenizer(&tk, input);
    if (is_token_last(&tk)) return 0;

    char* argv[1024];
    int argc = 0;
    int how = HOW_FOREGROUND;
    int red = 0;
    int pip = 0;

    while (get_next_token(&tk) == TOKEN_TYPE_NONE) {
        argv[argc++] = tk.token;
    }
    argv[argc++] = (char*)0;

    switch (tk.type) {
        case TOKEN_TYPE_AMPERSAND:
            how = HOW_BACKGROUND; break;
        case TOKEN_TYPE_RIGHT_ANGLE_BRACKET:
        case TOKEN_TYPE_DRIGHT_ANGLE_BRACKET:
        case TOKEN_TYPE_LEFT_ANGLE_BRACKET:
            red = tk.type;
            while (get_next_token(&tk) == TOKEN_TYPE_NONE) {
                argv[argc++] = tk.token;
            }
            argv[argc++] = (char*)0;
        case TOKEN_TYPE_PIPE:
            pip = tk.type;
        default:
            while (get_next_token(&tk) == TOKEN_TYPE_NONE) {
                argv[argc++] = tk.token;
            }
            argv[argc++] = (char*)0;

    }

    if (command(argv, argc)) { // execute internal command
        execute(argv, how, red, pip); // execute external command
    }

    return 0;
}

int main(void)
{
    char* buffer = "";

    while (buffer) {
        buffer = readline("lsh # ");
        if (interpret(buffer)) {
            break;
        }
        free(buffer);
    }

    return 0;
}
