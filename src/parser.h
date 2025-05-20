#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "scanner.h"

typedef uint32_t TokenIndex;
typedef uint32_t NodeIndex;
typedef uint32_t ExtraIndex;
typedef uint8_t  ExtraData[8];

typedef struct {
    TokenIndex token;
    uint8_t data[12];
} NodeData;

typedef struct {
    const Token* tokens;
    TokenIndex   tokens_len;

    uint8_t*     node_tags;
    NodeData*    node_datas;
    NodeIndex    nodes_cap;
    NodeIndex    nodes_len;

    ExtraData*   extra_data;
    ExtraIndex   extra_cap;
    ExtraIndex   extra_len;
} AST;

typedef struct {
    TokenIndex token;
    uint8_t _pad[4];
    uint64_t value;
} NodeDataNumber;

typedef struct {
    TokenIndex token;
    NodeIndex expr;
    uint8_t _pad[8];
} NodeDataUnary;

typedef struct {
    TokenIndex token;
    NodeIndex lhs;
    NodeIndex rhs;
    uint8_t _pad[4];
} NodeDataBinary;

typedef struct {
    TokenIndex token;
    NodeIndex type_expr;
    NodeIndex assign_expr;
    uint8_t _pad[4];
} NodeDataVarDecl;

typedef struct {
    TokenIndex token;
    ExtraIndex extra;
    uint8_t _pad[8]; // TODO could store param count here
} NodeDataFunctionPrototype;

typedef struct {
    uint32_t name_id;
    NodeIndex expr;
} FunctionPrototypeParam;

typedef struct {
    uint8_t _pad[4];
    uint32_t param_count;
} NodeExtraFunctionPrototype;

typedef struct {
    TokenIndex token;
    ExtraIndex decls_start;
    ExtraIndex decls_count;
    uint8_t _pad[4];
} NodeDataTopLevel;

typedef struct {
    NodeIndex nodes[2];
} NodeExtraTopLevel;

typedef enum {
    AST__NULL,

    AST_NUMBER,
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
    AST_TOP_LEVEL,
} NodeTag;


typedef struct {
    const char* src;
    uint32_t src_len;
    TokenIndex cur_token;
    AST ast;
    const char* err; // TODO error enum maybe?
} Parser;

static inline Parser parser_init(
    const char* src,
    uint32_t src_len,
    const Token* tkns,
    uint32_t tkns_len
) {
    return (Parser){
        .src = src,
        .src_len = src_len,
        .cur_token = 0,
        .ast = (AST) {
            .tokens = tkns,
            .tokens_len = tkns_len,
            .node_tags = malloc(64),
            .node_datas = malloc(64 * sizeof(NodeData)),
            .nodes_cap = 64,
            .nodes_len = 1,
            .extra_data = malloc(64 * 8),
            .extra_cap = 64,
            .extra_len = 1,
        },
        .err = "",
    };
}

NodeIndex parse_expr(Parser* par, uint8_t r_bp);
NodeIndex parse_var_decl(Parser* par);
NodeIndex parse_function_decl(Parser* par);
NodeIndex parse_top_level_decls(Parser* par);

void print_ast(const char* src, AST* ast, NodeIndex index, uint32_t depth);
#endif
