#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scanner.h"

// TODO return error instead of printf/exit
static NodeIndex insert_node_ast(AST* ast, NodeTag tag, NodeData data) {
    if (ast->nodes_len >= ast->nodes_cap) { // grow
        if (ast->nodes_cap >> 31) {
            fprintf(stderr, "AST nodes_cap overflow!\n");
            exit(1);
        }
        NodeIndex new_cap = 2 * ast->nodes_cap;
        if ((size_t)new_cap > (SIZE_MAX / sizeof(NodeData))) {
            fprintf(stderr, "AST nodes_cap overflows word size!\n");
            exit(1);
        }
        ast->node_tags = realloc(ast->node_tags, new_cap);
        if (ast->node_tags == NULL) {
            fprintf(stderr, "AST node_tags couldn't allocate memory!\n");
            exit(1);
        }
        ast->node_datas = realloc(ast->node_datas, (size_t)new_cap * sizeof(NodeData));
        if (ast->node_datas == NULL) {
            fprintf(stderr, "AST node_datas couldn't allocate memory!\n");
            exit(1);
        }
        ast->nodes_cap = new_cap;
    }
    ast->node_tags[ast->nodes_len] = tag;
    ast->node_datas[ast->nodes_len] = data;
    return ast->nodes_len++;
}

// TODO return error instead of printf/exit
static ExtraIndex insert_extra_ast(AST* ast, ExtraData* extra, ExtraIndex len) {
    if (ast->extra_len > (UINT32_MAX - len)) {
        fprintf(stderr, "AST extra_len overflow!\n");
        exit(1);
    }
    ExtraIndex new_len = ast->extra_len + len;
    if (new_len >= ast->extra_cap) { // grow
        ExtraIndex new_cap = ast->extra_cap;
        do {
            if (new_cap >> 31) {
                fprintf(stderr, "AST extra_cap overflow!\n");
                exit(1);
            }
            new_cap *= 2;
        } while ((size_t)new_len >= new_cap);
        if ((size_t)new_cap > (SIZE_MAX / sizeof(ExtraData))) {
            fprintf(stderr, "AST extra_cap overflows word size!\n");
            exit(1);
        }
        ast->extra_data = realloc(ast->node_datas, (size_t)new_cap * sizeof(ExtraData));
        if (ast->extra_data == NULL) {
            fprintf(stderr, "AST extra_data couldn't allocate memory!\n");
            exit(1);
        }
        ast->extra_cap = new_cap;
    }
    ExtraIndex pos = ast->extra_len;
    memcpy(&ast->extra_data[pos], extra, len * 8);
    ast->extra_len = new_len;
    return pos;
}

static inline Token peek(Parser* par) {
    return par->ast.tokens[par->cur_token];
}

static inline Token pop_token(Parser* par) {
    Token out;
    do {
        if ((par->cur_token + 1) < par->ast.tokens_len) {
            par->cur_token++;
        }
        out = par->ast.tokens[par->cur_token];
        // ignore comments
    } while (!tkn_is_parsed(out) && !tkn_is_err(out));
    return out;
}

static bool match_token(Parser* par, TokenTag tag) {
    if (tkn_is_err(peek(par))) {
        // TODO report the error
        return false;
    }
    if (peek(par).tag != tag) {
        // TODO report the error
        return false;
    }
    pop_token(par);
    return true;
}

typedef enum {
    PREOP__NULL = 0x00,

    PREOP_BOOL_NOT = 0x01, // Order must match ASTTag
    PREOP_BIT_NOT,
    PREOP_NEGATE,
} PrefixOp;

typedef enum {
    INOP__NULL = 0x00,

    INOP_ASSIGN = 0x40, // Order must match ASTTag
    INOP_EQUAL,
    INOP_NOT_EQUAL,
    INOP_LESS,
    INOP_LESS_EQUAL,
    INOP_GREATER,
    INOP_GREATER_EQUAL,
    INOP_ADD,
    INOP_SUB,
    INOP_MUL,
    INOP_DIV,
    INOP_MOD,

    INOP_CALL,
    INOP_INDEX,
    INOP_ACCESS,
} InfixOp;

static PrefixOp prefix_op(TokenTag tok) {
    switch (tok) {
        case TKN_BANG:  return PREOP_BOOL_NOT;
        case TKN_TILDE: return PREOP_BIT_NOT;
        case TKN_MINUS: return PREOP_NEGATE;
        default:        return PREOP__NULL;
    }
}

static InfixOp infix_op(TokenTag tok) {
    switch (tok) {
        case TKN_EQUAL:         return INOP_ASSIGN;
        case TKN_EQUAL_EQUAL:   return INOP_EQUAL;
        case TKN_BANG_EQUAL:    return INOP_NOT_EQUAL;
        case TKN_LESS:          return INOP_LESS;
        case TKN_LESS_EQUAL:    return INOP_LESS_EQUAL;
        case TKN_GREATER:       return INOP_GREATER;
        case TKN_GREATER_EQUAL: return INOP_GREATER_EQUAL;
        case TKN_PLUS:          return INOP_ADD;
        case TKN_MINUS:         return INOP_SUB;
        case TKN_STAR:          return INOP_MUL;
        case TKN_SLASH:         return INOP_DIV;
        case TKN_PERCENT:       return INOP_MOD;
        case TKN_L_PAREN:       return INOP_CALL;
        case TKN_L_BRACKET:     return INOP_INDEX;
        case TKN_DOT:           return INOP_ACCESS;
        default:                return INOP__NULL;
    }
}

static uint8_t bind_pow(uint8_t op) {
    switch (op) {
        case INOP_ASSIGN:
            return 1;
        case INOP_EQUAL:
        case INOP_NOT_EQUAL:
            return 2;
        case INOP_LESS:
        case INOP_LESS_EQUAL:
        case INOP_GREATER:
        case INOP_GREATER_EQUAL:
            return 3;
        case INOP_ADD:
        case INOP_SUB:
            return 4;
        case INOP_MUL:
        case INOP_DIV:
        case INOP_MOD:
            return 5;
        case PREOP_BIT_NOT:
        case PREOP_BOOL_NOT:
        case PREOP_NEGATE:
            return 6;
        case INOP_CALL:
        case INOP_INDEX:
            return 7;
        case INOP_ACCESS:
            return 8;
        default:
            return 0;
    }
}

static bool left_assoc(InfixOp op) {
    switch (op) {
        case INOP_ASSIGN:
            return false;
        case INOP_EQUAL:
        case INOP_NOT_EQUAL:
        case INOP_LESS:
        case INOP_LESS_EQUAL:
        case INOP_GREATER:
        case INOP_GREATER_EQUAL:
        case INOP_ADD:
        case INOP_SUB:
        case INOP_MUL:
        case INOP_DIV:
        case INOP_MOD:
        case INOP_CALL:
        case INOP_INDEX:
        case INOP_ACCESS:
            return true;
        case INOP__NULL:
            return false;
    }
}

static uint64_t str_to_uint(const char* str, uint32_t len) {
    uint64_t val = 0;
    for (uint32_t i = 0; i < len; i++) {
        const char c = str[i];
        if (c < '0' || c > '9') {
            return val;
        }
        val *= 10;
        val += c - '0';
    }
    return val;
}

static NodeIndex parse_number(Parser* par) {
    const Token num = peek(par);
    const TokenIndex tkn_index = par->cur_token;
    if (!match_token(par, TKN_NUMBER)) {
        exit(1);
        // TODO handle error
        return 0;
    }
    const NodeDataNumber data = {
        .token = tkn_index,
        .value = str_to_uint(&par->src[num.start], num.len),
    };
    return insert_node_ast(&par->ast, AST_NUMBER, *(NodeData*)&data);
}

static NodeIndex parse_symbol(Parser* par) {
    const TokenIndex tkn_index = par->cur_token;
    if (!match_token(par, TKN_IDENTIFIER)) {
        // TODO handle error
        exit(1);
        return 0;
    }
    const NodeData data = {
        .token = tkn_index,
    };
    return insert_node_ast(&par->ast, AST_SYMBOL, data);
}

static NodeIndex parse_expr_lhs(Parser* par, uint8_t r_bp) {
    const Token tkn = peek(par);
    const TokenIndex tkn_index = par->cur_token;
    if (tkn_is_err(tkn)) {
        // TODO handle error
        exit(1);
    }
    
    const PrefixOp op = prefix_op(tkn.tag);
    if (op) {
        pop_token(par);
        const NodeIndex expr = parse_expr(par, bind_pow(op));
        const NodeDataUnary data = {
            .token = tkn_index,
            .expr = expr,
        };
        const NodeTag tag = (op - PREOP_BOOL_NOT) + AST_UNARY_BOOL_NOT;
        return insert_node_ast(&par->ast, tag, *(NodeData*)&data);
    } else switch (tkn.tag) {
        case TKN_NUMBER:
            return parse_number(par);
        case TKN_IDENTIFIER:
            return parse_symbol(par);
        case TKN_L_PAREN:
            pop_token(par);
            const NodeIndex lhs = parse_expr(par, 0);
            if (!match_token(par, TKN_R_PAREN)) {
                // TODO handle error
                exit(1);
            }
            return lhs;
        default:
            // TODO handle error
            exit(1);
    }
}

NodeIndex parse_expr(Parser* par, uint8_t r_bp) {
    NodeIndex lhs = parse_expr_lhs(par, r_bp);
    if (lhs == 0) {
        // TODO handle error
        exit(1);
    }
    
    while (true) {
        const Token op_tkn = peek(par);
        const TokenIndex tkn_index = par->cur_token;
        if (tkn_is_err(op_tkn)) {
            // TODO handle error
            exit(1);
        }
        const InfixOp op = infix_op(op_tkn.tag);
        if (!op) { break; }
        
        const uint8_t l_bp = bind_pow(op);
        const bool l_assoc = left_assoc(op);
        if (l_assoc) {
            if (l_bp <= r_bp) { break; }
        } else {
            if (l_bp < r_bp) { break; }
        }
        
        pop_token(par);
        // handle special inops
        switch (op) {
            case INOP_CALL:
                // TODO parse call
                break;
            case INOP_INDEX:
                // TODO parse index
                break;
            case INOP_ACCESS:
                // TODO parse access
                break;
            default:;
                const NodeIndex rhs = parse_expr(par, bind_pow(op));
                const NodeDataBinary data = {
                    .token = tkn_index,
                    .lhs = lhs,
                    .rhs = rhs,
                };
                const NodeTag tag = (op - INOP_ASSIGN) + AST_BINARY_ASSIGN;
                lhs = insert_node_ast(&par->ast, tag, *(NodeData*)&data);
                break;
        }
    }
    return lhs;
}

NodeIndex parse_var_decl(Parser* par) {
    if (!match_token(par, TKN_VAR)) {
        return 0; // TODO handle error
    }

    const TokenIndex name_index = par->cur_token;
    if (!match_token(par, TKN_IDENTIFIER)) {
        return 0; // TODO handle error
    }

    if (!match_token(par, TKN_COLON)) {
        return 0; // TODO handle error
    }
    
    // TODO type expr options:
    //   keyword    null  _  any
    //   symbol     MyType
    //   access     Namespace.Type
    //   call       matrix<2, 3>()
    NodeIndex type_expr = parse_symbol(par);
    if (!type_expr) {
        return 0; // TODO handle error
    }

    if (!match_token(par, TKN_EQUAL)) {
        return 0; // TODO handle error
    }

    NodeIndex assign_expr = parse_expr(par, 0);
    if (!assign_expr) {
        return 0; // TODO handle error
    }

    if (!match_token(par, TKN_SEMICOLON)) {
        return 0; // TODO handle error
    }

    NodeDataVarDecl data = {
        .token = name_index,
        .type_expr = type_expr,
        .assign_expr = assign_expr,
    };
    return insert_node_ast(&par->ast, AST_VAR_DECL, *(NodeData*)&data);
}

NodeIndex parse_function_decl(Parser *par) {
    if (!match_token(par, TKN_FN)) {
        return 0; // TODO communicate error and sync
    }

    const TokenIndex name_index = par->cur_token;
    if (!match_token(par, TKN_IDENTIFIER)) {
        return 0;
    }

    if (!match_token(par, TKN_L_PAREN)) {
        return 0;
    }

    // TODO parse parameter list

    if (!match_token(par, TKN_R_PAREN)) {
        return 0;
    }
    if (!match_token(par, TKN_L_BRACE)) {
        return 0;
    }

    // TODO parse expressions

    if (!match_token(par, TKN_R_BRACE)) {
        return 0;
    }

    const NodeExtraFunctionPrototype extra = {
        .param_count = 0,
    };
    ExtraIndex extra_id = insert_extra_ast(&par->ast, (uint8_t(*)[8])&extra, 1);

    const NodeDataFunctionPrototype data =  {
        .token = name_index,
        .extra = extra_id,
    };
    return insert_node_ast(&par->ast, AST_FUNCTION_DECL, *(NodeData*)&data);
}

NodeIndex parse_top_level_decls(Parser* par) {
    // TODO implement better allocation stratagey
    NodeIndex* decls = malloc(16 * sizeof(NodeIndex));
    size_t decls_cap = 16;
    size_t decls_len = 0;
    while (true) {
        if (peek(par).tag == TKN_EOF) {
            break;
        } else if (peek(par).tag == TKN_VAR) {
            if (decls_len >= decls_cap) {
                decls_cap *= 2;
                decls = realloc(decls, decls_cap * sizeof(NodeIndex));
            }
            NodeIndex var_decl = parse_var_decl(par);
            if (var_decl == 0) {
                // TODO handle properly
                fprintf(stderr, "Invalid varaible declaration.\n");
                exit(1);
            }
            decls[decls_len] = var_decl;
            decls_len++;
        } else {
            // TODO handle properly
            fprintf(stderr, "Unrecognized token.\n");
            exit(1);
        }
    }

    if (decls_len % 2 == 1) {
        decls[decls_len] = 0;
    }
    ExtraIndex extra_len = (decls_len / 2) + (decls_len & 0x1);
    ExtraIndex extra = insert_extra_ast(&par->ast, (ExtraData*)decls,
                                        extra_len);
    const NodeDataTopLevel top_data = {
        .token = par->cur_token,
        .decls_start = extra,
        .decls_count = decls_len,
    };
    const NodeIndex top_node = insert_node_ast(&par->ast, AST_TOP_LEVEL,
                                               *(NodeData*)&top_data);
    free(decls);
    return top_node;
}

static inline void print_indent(size_t depth) {
    for (size_t i = 0; i < depth; i++) {
        printf("  ");
    }
}

static void print_number(NodeData data) {
    printf("Number(%lu)\n", ((NodeDataNumber*)&data)->value);
}

static void print_symbol(const char* src, AST* ast, NodeIndex index) {
    const char* str = &src[ast->tokens[ast->node_datas[index].token].start];
    const uint32_t len = ast->tokens[ast->node_datas[index].token].len;
    printf("Symbol(%.*s)\n", (int)len, str);
}

static void print_unary(
    const char* src,
    AST* ast,
    NodeIndex index,
    uint32_t depth,
    const char* name
) {
    printf("Unary %s {\n", name);
    NodeDataUnary data = *(NodeDataUnary*)&ast->node_datas[index];
    print_indent(depth + 1);
    printf("expr = ");
    print_ast(src, ast, data.expr, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_binary(
    const char* src,
    AST* ast,
    NodeIndex index,
    uint32_t depth,
    const char* name
) {
    printf("Binary %s {\n", name);
    NodeDataBinary data = *(NodeDataBinary*)&ast->node_datas[index];
    print_indent(depth + 1);
    printf("lhs = ");
    print_ast(src, ast, data.lhs, depth + 1);
    print_indent(depth + 1);
    printf("rhs = ");
    print_ast(src, ast, data.rhs, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_var_decl(
        const char* src,
        AST* ast,
        NodeIndex index,
        uint32_t depth
) {
    printf("Var Decl {\n");
    NodeDataVarDecl data = *(NodeDataVarDecl*)&ast->node_datas[index];
    print_indent(depth + 1);
    const char* name = &src[ast->tokens[data.token].start];
    const uint32_t len = ast->tokens[data.token].len;
    printf("name = %.*s\n", (int)len, name);
    print_indent(depth + 1);
    printf("type_expr = ");
    print_ast(src, ast, data.type_expr, depth + 1);
    print_indent(depth + 1);
    printf("assign_expr = ");
    print_ast(src, ast, data.assign_expr, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_top_level(
        const char* src,
        AST* ast,
        NodeIndex index,
        uint32_t depth
) {
    NodeDataTopLevel* const data = (NodeDataTopLevel*)&ast->node_datas[index];
    printf("Top Level {\n");
    for (uint32_t i = 0; i < data->decls_count; i++) {
        NodeExtraTopLevel* const extra =
            (NodeExtraTopLevel*)&ast->extra_data[data->decls_start + (i / 2)];
        print_indent(depth + 1);
        printf("%d: ", i);
        print_var_decl(src, ast, extra->nodes[i % 2], depth + 1);
    }
    print_indent(depth);
    printf("}\n");
}

void print_ast(const char* src, AST* ast, NodeIndex index, uint32_t depth) {
    if (ast->nodes_len <= index) { return; }

    switch (ast->node_tags[index]) {
        case AST_NUMBER:
            print_number(ast->node_datas[index]); return;
        case AST_SYMBOL:
            print_symbol(src, ast, index); return;
        case AST_UNARY_BOOL_NOT:
            print_unary(src, ast, index, depth, "!"); return;
        case AST_UNARY_BIT_NOT:
            print_unary(src, ast, index, depth, "~"); return;
        case AST_UNARY_NEGATE:
            print_unary(src, ast, index, depth, "-"); return;
        case AST_BINARY_ASSIGN:
            print_binary(src, ast, index, depth, "="); return;
        case AST_BINARY_EQUAL:
            print_binary(src, ast, index, depth, "=="); return;
        case AST_BINARY_NOT_EQUAL:
            print_binary(src, ast, index, depth, "!="); return;
        case AST_BINARY_LESS:
            print_binary(src, ast, index, depth, "<"); return;
        case AST_BINARY_LESS_EQUAL:
            print_binary(src, ast, index, depth, "<="); return;
        case AST_BINARY_GREATER:
            print_binary(src, ast, index, depth, ">"); return;
        case AST_BINARY_GREATER_EQUAL:
            print_binary(src, ast, index, depth, ">="); return;
        case AST_BINARY_ADD:
            print_binary(src, ast, index, depth, "+"); return;
        case AST_BINARY_SUB:
            print_binary(src, ast, index, depth, "-"); return;
        case AST_BINARY_MUL:
            print_binary(src, ast, index, depth, "*"); return;
        case AST_BINARY_DIV:
            print_binary(src, ast, index, depth, "/"); return;
        case AST_BINARY_MOD:
            print_binary(src, ast, index, depth, "%"); return;
        case AST_VAR_DECL:
            print_var_decl(src, ast, index, depth); return;
        case AST_TOP_LEVEL:
            print_top_level(src, ast, index, depth); return;
        case AST__NULL: printf("NULL\n"); return;
        default: printf("UNIMPLEMENTED"); return;
    }
}
