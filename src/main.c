#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "scanner.h"
#include "parser.h"

int interp(const char* data, size_t len) {
    Scanner scnr = scnr_init(data, len);
    Parser par = parser_init(&scnr);
    ASTRef expr = parse_expression(&par, 0);
    printf("\n");
    if (par.err_count == 0) {
        printf("No Errors\n");
        print_ast(expr);
        printf("\n");
        return 0;
    } else {
        printf("%zu ERRORS:\n", par.err_count);
        for (size_t i = 0; i < par.err_count; i++) {
            print_err(par.errs[i]);
        }
        printf("\n");
        return 1;
    }
}

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
        interp(buf, len);
        /*while (!scnr_is_end(&scnr)) {
            const Token tok = scnr_scan_token(&scnr);
            printf("%s(%zu, %zu) ", token_type_str(tok.type), tok.length, tok.col);
        }*/
    }
    return 0;
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

    return interp(buf, (size_t)len);
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
}
