#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "scanner.h"
#include "array_list.h"
#include "alloc.h"
#include "str.h"

typedef struct {
    const char* bytes;
    size_t len;
} StringData;

typedef struct { // TODO use some form of map for better perf
    StringData* strings;
    uint32_t cap;
    uint32_t len;
} StringTable;

typedef struct {
    size_t start; // TODO would token index be more useful?
    uint8_t data[8];
} NodeData;

typedef struct {
    uint8_t* node_tags;
    NodeData* node_datas;
    uint32_t nodes_cap;
    uint32_t nodes_len;
    uint8_t (*extra_data)[8];
    uint32_t extra_cap;
    uint32_t extra_len;
} AST;

typedef struct {
    size_t start;
    uint64_t value;
} NodeDataNumber;

typedef struct {
    size_t start;
    uint32_t str_id;
    uint8_t _pad[4];
} NodeDataString;

typedef struct {
    size_t start;
    uint32_t expr;
    uint8_t _pad[4];
} NodeDataUnary;

typedef struct {
    size_t start;
    uint32_t lhs;
    uint32_t rhs;
} NodeDataBinary;

typedef struct {
    size_t start;
    uint32_t name_id;
    uint32_t extra;
} NodeDataFunctionPrototype;

typedef struct {
    uint32_t name_id;
    uint32_t expr;
} FunctionPrototypeParam;

typedef struct {
    uint8_t _pad[4];
    uint32_t param_count;
} NodeExtraFunctionPrototype;

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
} ASTTag;

typedef struct {
    ASTTag type;
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
            TokenTag expected[8];
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
    const char* src;
    uint32_t src_len;
    Token* tkns;
    uint32_t tkns_len;
    uint32_t tkn_index;
    AST ast;
    StringTable strings;
    ArrayList errs;

    Alloc* str_alloc;
    Alloc* node_alloc;
} Parser;

static inline Parser parser_init(const char* src, uint32_t src_len, Token* tkns, uint32_t tkns_len, Alloc* str_alloc, Alloc* node_alloc) {
    return (Parser){
        .src = src,
        .src_len = src_len,
        .tkns = tkns,
        .tkns_len = tkns_len,
        .tkn_index = 0,
        .strings = (StringTable) {
            .strings = malloc(64),
            .cap = 64,
            .len = 0,
        },
        .ast = (AST) {
            .node_tags = malloc(64),
            .node_datas = malloc(64 * sizeof(NodeData)),
            .nodes_cap = 64,
            .nodes_len = 1,
            .extra_data = malloc(64 * 8),
            .extra_cap = 64,
            .extra_len = 1,
        },
        .errs = arrlist_init(ParseError),

        .str_alloc = str_alloc,
        .node_alloc = node_alloc,
    };
}

uint32_t parse_expr(Parser* par, uint8_t r_bp);

uint32_t parse_function_decl2(Parser* par);

LiteralAST* parse_literal(Parser* par);
ASTRef parse_expression(Parser* par, int8_t rbp);
ASTRef parse_function_decl(Parser* par);

#endif
