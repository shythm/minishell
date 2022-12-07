#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define FALSE   0
#define TRUE    1

#define INPUT_SIZE  512

#define HOW_FOREGROUND  0
#define HOW_BACKGROUND  1

#define TOKEN_TYPE_NONE                 0
#define TOKEN_TYPE_AMPERSAND            1
#define TOKEN_TYPE_LEFT_ANGLE_BRACKET   2
#define TOKEN_TYPE_RIGHT_ANGLE_BRACKET  3
#define TOKEN_TYPE_DLEFT_ANGLE_BRACKET  4
#define TOKEN_TYPE_DRIGHT_ANGLE_BRACKET 5
#define TOKEN_TYPE_PIPE                 6

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
        case '<':
            if (type == TOKEN_TYPE_LEFT_ANGLE_BRACKET) {
                type = TOKEN_TYPE_DLEFT_ANGLE_BRACKET;
            } else {
                type = TOKEN_TYPE_NONE;
            }
            break;
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

int execute(char** argv, int how, int type) {
    pid_t pid;
    char** argv2;

    if (type) {
        // get start address of arguments for redirection or pipe
        for (argv2 = argv; *argv2 != (char*)0; argv2++);
        argv2++;
    }

    if ((pid = fork()) < 0) {
        perror("fork error");
        return -1;
    } else if (pid == 0) {
        /* child process */
        if (type == TOKEN_TYPE_RIGHT_ANGLE_BRACKET) {
            int fd = open(*argv2, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            dup2(fd, 1);
            close(fd);
        }
        execvp(*argv, argv);
        fprintf(stderr, "msh: command not found\n");
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
        fprintf(stderr, "TODO: implement change directory. \n");
    } else if (CMD("type")) {
        fprintf(stderr, "TODO: implement text read command. \n");
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
        case TOKEN_TYPE_DLEFT_ANGLE_BRACKET:
            red = tk.type;
            while (get_next_token(&tk) == TOKEN_TYPE_NONE) {
                argv[argc++] = tk.token;
            }
            argv[argc++] = (char*)0;
            break;
    }

    if (command(argv, argc)) { // execute internal command
        execute(argv, how, red); // execute external command
    }

    return 0;
}

int main(void)
{
    char input[INPUT_SIZE];

    printf("lsh # ");
    while (fgets(input, INPUT_SIZE, stdin)) {
        if (interpret(input)) {
            break;
        }
        printf("lsh # ");
    }
    return 0;
}
