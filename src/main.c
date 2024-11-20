#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "lexer.h"

int repl(void) {
    while (1) {
        char buf[1024];
        printf("> ");
        fgets(buf, 1024, stdin);
        uint16_t len = 0;
        for (;buf[len] != '\n'; len++) {}
        len++;
        if (len == 5) {
            if (buf[0] == 'e' &&
                buf[1] == 'x' &&
                buf[2] == 'i' &&
                buf[3] == 't'
            ) {
                break;
            }
        }
        Scanner scnr = scnr_init(buf, len);
        while (!scnr_is_end(&scnr)) {
            const Token tok = scnr_scan_token(&scnr);
            printf("%s(%zu, %zu) ", token_type_str(tok.type), tok.length, tok.col);
        }
        printf("\n");
    }
    return 1;
}

int run_file(char* path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "ERROR: Failed to open file at %s.\n", path);
        return 1;
    }

    const size_t n = 1048576;
    char* buf = malloc(n);
    int len = read(fd, buf, n);
    if (len == -1) {
        fprintf(stderr, "ERROR: Failed to read file.\n");
        return 1;
    }

    if (len == 0) {
        fprintf(stderr, "ERROR: File is empty.\n");
        return 1;
    }

    Scanner scnr = scnr_init(buf, len);
    while (!scnr_is_end(&scnr)) {
        Token tok = scnr_scan_token(&scnr);
        printf("%s(%zu:%zu) ", token_type_str(tok.type), tok.line, tok.col);
    }
    printf("\n");
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        return repl();
    }

    if (argc == 2) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "Too many arguments.\n");
    return 1;
    
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "ERROR: Failed to open file at %s.\n", argv[1]);
        return 1;
    }

    char buf[4096];
    int len = read(fd, buf, 4096);
    if (len == -1) {
        fprintf(stderr, "ERROR: Failed to read file.\n");
        return 1;
    }

    if (len == 0) {
        fprintf(stderr, "ERROR: File is empty.\n");
        return 1;
    }

    Scanner scnr = scnr_init(buf, len);
    scnr_skip_whitespace(&scnr);
    printf("Input %d bytes.\n", len);
    printf("%.*s", len, buf);
    printf("%*c\n", (int)scnr.col, '^');
    printf("First token at column %zu.\n", scnr.col);
}
