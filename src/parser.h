#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "scanner.h"
#include "array_list.h"
#include "alloc.h"
#include "str.h"

typedef enum {
    AST__NULL,

    AST_LITERAL,
    AST_SYMBOL,

    AST_UNARY_BOOL_NOT,
    AST_UNARY_BIT_NOT,
    AST_UNARY_NEGATE,

    AST_BINARY_ASSIGN,
    AST_BINARY_EQUAL,
    AST_BINARY_NOT_EQUAL,
    AST_BINARY_LESS,
    AST_BINARY_LESS_EQUAL,
    AST_BINARY_GREATER,
    AST_BINARY_GREATER_EQUAL,
    AST_BINARY_ADD,
    AST_BINARY_SUB,
    AST_BINARY_MUL,
    AST_BINARY_DIV,
    AST_BINARY_MOD,

    AST_CALL,
    AST_INDEX,
    AST_ACCESS,

    AST_FUNCTION_DECL,
    AST_VAR_DECL,
    AST_RETURN,

    AST__COUNT,
    AST__MAX = 0xff,
} ASTType;

typedef struct {
    ASTType type;
    void* node;
} ASTRef;

typedef struct {
    int64_t number;
} LiteralAST;

typedef struct {
    Str name;
} SymbolAST;

typedef struct {
    ASTRef inner;
} UnaryAST;

typedef struct {
    ASTRef lhs;
    ASTRef rhs;
} BinaryAST;

typedef struct {
    ASTRef callee;
    ASTRef* args;
    uint8_t args_len;
} CallAST;

typedef struct {
    ASTRef item;
    ASTRef index;
} IndexAST;

typedef struct {
    ASTRef parent;
    SymbolAST* child;
} AccessAST;

typedef struct {
    Str name;
    Str type;
} Param;

typedef struct {
    Str name;
    Param* params;
    size_t params_len;
    Str ret_type;
    ASTRef* statements;
    size_t statements_len;
} FunctionDeclAST;

typedef struct {
    Str name;
    Str type;
    ASTRef expr;
} VarDeclAST;

typedef struct {
    ASTRef expr;
} ReturnAST;

static inline bool valid_ast(ASTRef ast) {
    return (ast.type != AST__NULL) && (ast.node != NULL);
}

void print_ast(ASTRef ast, size_t depth);


typedef enum {
    ERR__NULL =      0x00,

    ERR_UNEXPECTED = 0x01,
    ERR_BAD_OP =     0x02,

    ERR_GENERIC =    0x10,
} ParseErrorType;

typedef struct {
    ParseErrorType type;
    Token tok;
    union {
        struct {
            const char* msg;
        } generic;
        struct {
            TokenType expected[8];
        } unexpected;
    } data;
} ParseError;

static inline bool valid_err(ParseError err) {
    return err.type == ERR__NULL;
}

static inline bool is_src_err(ParseError err) {
    return (err.type >= ERR_UNEXPECTED) && (err.type < ERR_GENERIC);
}

void print_err(ParseError err);


typedef struct {
    Scanner* scnr;
    Token token;
    ArrayList errs;
    Alloc* str_alloc;
    Alloc* node_alloc;
} Parser;

static inline Parser parser_init(Scanner* scnr, Alloc* str_alloc, Alloc* node_alloc) {
    return (Parser){
        .scnr = scnr,
        .token = scnr_scan_token(scnr),
        .errs = arrlist_init(ParseError),
        .str_alloc = str_alloc,
        .node_alloc = node_alloc,
    };
}

LiteralAST* parse_literal(Parser* par);
ASTRef parse_expression(Parser* par, int8_t rbp);
ASTRef parse_function_decl(Parser* par);

#endif
