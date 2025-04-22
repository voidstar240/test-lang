#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "alloc.h"
#include "scanner.h"
#include "parser.h"
#include "codegen.h"

int interp(const char* src, size_t len) {
    ArenaAlloc str_alloc = alloc_arena_init(alloc_system(), 1048576);
    ArenaAlloc node_alloc = alloc_arena_init(alloc_system(), 1048576);

    uint32_t tkns_len = 0;
    Token* tkns = scan_src(src, &tkns_len);
    printf("\nDebug Token Output:\n");
    debug_print_tokens(src, tkns, tkns_len);

    printf("\nDebug AST Output:\n");
    Parser par = parser_init(
        src,
        len,
        tkns,
        tkns_len,
        (Alloc*)&str_alloc,
        (Alloc*)&node_alloc
    );
    uint32_t expr = parse_expr(&par, 0);
    print_ast2(src, &par.ast, expr, 0);
    printf("\n");

    return 0;
    /*ASTRef fn_decl = parse_function_decl(&par);
    printf("\n");
    if (par.errs.len == 0) {
        printf("No Errors\n\n");
        printf("Syntax Tree:\n");
        print_ast(fn_decl, 0);
        CodeGen cg = codegen_init();
        codegen_init_prim_types(&cg);
        codegen_init_module(&cg, "mymod");
        gen_function(&cg, fn_decl.node);
        return 0;
    } else {
        printf("%zu ERRORS:\n", par.errs.len);
        for (size_t i = 0; i < par.errs.len; i++) {
            print_err(arrlist_get(&par.errs, ParseError, i));
        }
        printf("\n");
        printf("Syntax Tree:\n");
        print_ast(fn_decl, 0);
        return 1;
    }*/
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
