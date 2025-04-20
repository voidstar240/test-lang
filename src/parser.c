#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scanner.h"

// TODO return error instead of printf/exit
static uint32_t insert_new_string(
    StringTable* table,
    const char* bytes,
    size_t len
) {
    if (table->len >= table->cap) { // grow
        size_t new_cap = 2UL * (size_t)table->cap;
        if (new_cap > 0xFFFFFFFFUL) { // overflow table->cap
            fprintf(stderr, "Too many strings!");
            exit(1);
        }
        table->strings = realloc(table->strings, new_cap);
        table->cap = new_cap;
    }
    table->strings[table->len] = (StringData) { .bytes = bytes, .len = len };
    return table->len++;
}

static uint32_t insert_string(
    StringTable* table,
    const char* bytes,
    size_t len
) {
    for (uint32_t i = 0; i < table->len; i++) { // check for existing string
        if (table->strings[i].len != len) { continue; }
        if (strncmp(table->strings[i].bytes, bytes, len) == 0) {
            return i;
        }
    }
    return insert_new_string(table, bytes, len);
}


// TODO return error instead of printf/exit
static uint32_t insert_node_ast(AST* ast, ASTTag tag, NodeData data) {
    if (ast->nodes_len == ast->nodes_cap) { // grow
        size_t new_cap = 2UL * (size_t)ast->nodes_cap;
        if (new_cap > 0xFFFFFFFFUL) { // overflow ast->nodes_cap
            fprintf(stderr, "Too many AST nodes!");
            exit(1);
        }
        ast->node_tags = realloc(ast->node_tags, new_cap);
        ast->node_datas = realloc(ast->node_datas, new_cap * sizeof(NodeData));
        ast->nodes_cap = new_cap;
    }
    ast->node_tags[ast->nodes_len] = tag;
    ast->node_datas[ast->nodes_len] = data;
    return ast->nodes_len++;
}

// TODO return error instead of printf/exit
static uint32_t insert_extra_ast(AST* ast, uint8_t (*extra)[8], uint32_t len) {
    if ((ast->extra_len & len) >> 31) { // overflow ast->extra_len
            fprintf(stderr, "Too much AST extra data!");
            exit(1);
    }
    uint32_t new_len = ast->extra_len + len;
    if (new_len >= ast->extra_cap) { // grow
        size_t new_cap = 2UL * (size_t)ast->extra_cap;
        while ((size_t)new_len >= new_cap) { new_cap *= 2; }
        if (new_cap > 0xFFFFFFFFUL) { // overflow ast->nodes_cap
            fprintf(stderr, "Too much AST extra data!");
            exit(1);
        }
        ast->extra_data = realloc(ast->node_datas, new_cap * 8);
        ast->extra_cap = new_cap;
    }
    memcpy(&ast->extra_data[ast->extra_len], extra, len * 8);
    uint32_t pos = ast->extra_len;
    ast->extra_len = new_len;
    return pos;
}

static inline Token peek(Parser* par) {
    return par->tkns[par->tkn_index];
}

static inline Token pop_token(Parser* par) {
    // TODO prevent overrunning tkns
    Token out;
    do {
        out = par->tkns[++par->tkn_index];
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

static uint32_t parse_number(Parser* par) {
    const Token num = peek(par);
    if (!match_token(par, TKN_NUMBER)) {
        exit(1);
        // TODO handle error
        return 0;
    }
    const NodeDataNumber data = {
        .start = num.start,
        .value = str_to_uint(&par->src[num.start], num.len),
    };
    // TODO change AST_LITERAL to number
    return insert_node_ast(&par->ast, AST_LITERAL, *(NodeData*)&data);
}

static uint32_t parse_symbol2(Parser* par) {
    const Token sym = peek(par);
    if (!match_token(par, TKN_IDENTIFIER)) {
        // TODO handle error
        exit(1);
        return 0;
    }
    const NodeDataString data = {
        .start = sym.start,
        .str_id = insert_string(&par->strings, &par->src[sym.start], sym.len),
    };
    return insert_node_ast(&par->ast, AST_SYMBOL, *(NodeData*)&data);
}

static uint32_t parse_expr_lhs(Parser* par, uint8_t r_bp) {
    Token l_tkn = peek(par);
    if (tkn_is_err(l_tkn)) {
        // TODO handle error
        exit(1);
    }
    
    const PrefixOp op = prefix_op(l_tkn.tag);
    if (op) {
        pop_token(par);
        const uint32_t expr = parse_expr(par, bind_pow(op));
        const NodeDataUnary data = {
            .start = l_tkn.start,
            .expr = expr,
        };
        const ASTTag tag = (op - PREOP_BOOL_NOT) + AST_UNARY_BOOL_NOT;
        return insert_node_ast(&par->ast, tag, *(NodeData*)&data);
    } else switch (l_tkn.tag) {
        case TKN_NUMBER:
            return parse_number(par);
        case TKN_IDENTIFIER:
            return parse_symbol2(par);
        case TKN_L_PAREN:
            pop_token(par);
            const uint32_t lhs = parse_expr(par, 0);
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

uint32_t parse_expr(Parser* par, uint8_t r_bp) {
    uint32_t lhs = parse_expr_lhs(par, r_bp);
    if (lhs == 0) {
        // TODO handle error
        exit(1);
    }
    
    while (true) {
        const Token op_tkn = peek(par);
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
                const uint32_t rhs = parse_expr(par, bind_pow(op));
                const NodeDataBinary data = {
                    .start = op_tkn.start,
                    .lhs = lhs,
                    .rhs = rhs,
                };
                const ASTTag tag = (op - INOP_ASSIGN) + AST_BINARY_ASSIGN;
                lhs = insert_node_ast(&par->ast, tag, *(NodeData*)&data);
                break;
        }
    }
    return lhs;
}

uint32_t parse_function_decl2(Parser *par) {
    const size_t start = peek(par).start;
    if (!match_token(par, TKN_FN)) {
        return 0; // TODO communicate error and sync
    }

    const Token name = peek(par);
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

    const uint32_t name_id = insert_string(
        &par->strings,
        &par->src[name.start],
        name.len);

    const NodeExtraFunctionPrototype extra = {
        .param_count = 0,
    };
    uint32_t extra_id = insert_extra_ast(&par->ast, (uint8_t(*)[8])&extra, 1);

    const NodeDataFunctionPrototype data =  {
        .start = start,
        .name_id = name_id,
        .extra = extra_id,
    };
    // TODO CHANGE THE AST TAG TYPE
    return insert_node_ast(&par->ast, AST_FUNCTION_DECL, *(NodeData*)&data);
}







static inline ParseError err_str(const char* msg) {
    return (ParseError){
        .type = ERR_GENERIC,
        .tok = {0},
        .data.generic.msg = msg,
    };
}

static inline ParseError err_unexp(Token current, TokenTag expected) {
    ParseError err = {0};
    err.type = ERR_UNEXPECTED;
    err.tok = current;
    err.data.unexpected.expected[0] = expected;
    return err;
}

static void log_error(Parser* par, ParseError err) {
    arrlist_push(&par->errs, ParseError, err);
}

static ASTRef ast_error(Parser* par, ParseError err) {
    log_error(par, err);
    return (ASTRef){0};
}

void print_err(ParseError err) {
    switch (err.type) {
        case ERR_UNEXPECTED:
            printf("Got \"%s\" @ %d. Expected one of",
                    tkn_tag_name(err.tok.tag),
                    err.tok.start);
            for (int i = 0; i < 8; i++) {
                TokenTag tok = err.data.unexpected.expected[i];
                if (tok == 0) { break; }
                printf(" \"%s\"", tkn_tag_name(tok));
            }
            printf(".\n");
            break;
        case ERR_BAD_OP:
            printf("Bad Operator \"%s\" @ %d.\n",
                    tkn_tag_name(err.tok.tag),
                    err.tok.start);
            break;
        case ERR_GENERIC:
            printf("GenericErr(%s)\n", err.data.generic.msg);
            break;
        case ERR__NULL: printf("NULL ERROR\n"); break;
    }
}

static inline bool expect_token(Parser* par, TokenTag type) {
    if (peek(par).tag != type) {
        ParseError err = {0};
        err.type = ERR_UNEXPECTED;
        err.tok = peek(par);
        err.data.unexpected.expected[0] = type;
        log_error(par, err);
        return false;
    }
    pop_token(par);
    return true;
}

// TODO return errors too
static int64_t int_from_str(const char* bytes, size_t len) {
    int64_t out = 0;
    for (size_t i = 0; i < len; i++) {
        switch (bytes[i]) {
            case '0':
                out *= 10;
                continue;
            case '1':
                out *= 10;
                out += 1;
                continue;
            case '2':
                out *= 10;
                out += 2;
                continue;
            case '3':
                out *= 10;
                out += 3;
                continue;
            case '4':
                out *= 10;
                out += 4;
                continue;
            case '5':
                out *= 10;
                out += 5;
                continue;
            case '6':
                out *= 10;
                out += 6;
                continue;
            case '7':
                out *= 10;
                out += 7;
                continue;
            case '8':
                out *= 10;
                out += 8;
                continue;
            case '9':
                out *= 10;
                out += 9;
                continue;
            case '.':
                break;
        }
        break;
    }
    return out;
}

LiteralAST* parse_literal(Parser* par) {
    LiteralAST* lit = alloc_alloc(par->node_alloc, sizeof(LiteralAST));
    lit->number = int_from_str(
        &par->src[peek(par).start],
        peek(par).len);
    pop_token(par);
    return lit;
}

SymbolAST* parse_symbol(Parser* par) {
    SymbolAST* sym = alloc_alloc(par->node_alloc, sizeof(SymbolAST));
    Token ident = pop_token(par);
    sym->name = str_alloc(
        par->str_alloc,
        &par->src[ident.start],
        ident.len);
    return sym;
}

static ASTTag type_of_prefix(PrefixOp op) {
    switch (op) {
        case PREOP_BOOL_NOT: return AST_UNARY_BOOL_NOT;
        case PREOP_BIT_NOT:  return AST_UNARY_BIT_NOT;
        case PREOP_NEGATE:   return AST_UNARY_NEGATE;

        case PREOP__NULL:    return AST__NULL;
    }
}

static ASTRef parse_nud(Parser* par) {
    PrefixOp op = prefix_op(peek(par).tag);
    if (op) {
        pop_token(par);
        ASTRef expr = parse_expression(par, bind_pow(op));
        if (!valid_ast(expr)) {
            return ast_error(par, err_str("Expr. parse failed."));
        }
        UnaryAST* unary = alloc_alloc(par->node_alloc, sizeof(UnaryAST));
        unary->inner = expr;
        return (ASTRef) {
            .type = type_of_prefix(op),
            .node = unary,
        };
    }
    switch (peek(par).tag) {
        case TKN_NUMBER:
            return (ASTRef) {
                .type = AST_LITERAL,
                .node = parse_literal(par),
            };
        case TKN_IDENTIFIER:
            return (ASTRef) {
                .type = AST_SYMBOL,
                .node = parse_symbol(par),
            };
        case TKN_L_PAREN:
            pop_token(par);
            ASTRef expr = parse_expression(par, 0);
            expect_token(par, TKN_R_PAREN);
            return expr;
        default:;
            ParseError err = err_unexp(peek(par), TKN_NUMBER);
            err.data.unexpected.expected[1] = TKN_IDENTIFIER;
            return ast_error(par, err);
    }
}

static ASTTag type_of_infix(InfixOp op) {
    switch (op) {
        case INOP_ASSIGN:        return AST_BINARY_ASSIGN;
        case INOP_EQUAL:         return AST_BINARY_EQUAL;
        case INOP_NOT_EQUAL:     return AST_BINARY_EQUAL;
        case INOP_LESS:          return AST_BINARY_LESS;
        case INOP_LESS_EQUAL:    return AST_BINARY_EQUAL;
        case INOP_GREATER:       return AST_BINARY_GREATER;
        case INOP_GREATER_EQUAL: return AST_BINARY_EQUAL;
        case INOP_ADD:           return AST_BINARY_ADD;
        case INOP_SUB:           return AST_BINARY_SUB;
        case INOP_MUL:           return AST_BINARY_MUL;
        case INOP_DIV:           return AST_BINARY_DIV;
        case INOP_MOD:           return AST_BINARY_MOD;

        case INOP_CALL:          return AST_CALL;
        case INOP_INDEX:         return AST_INDEX;
        case INOP_ACCESS:        return AST_ACCESS;

        case INOP__NULL:         return AST__NULL;
    }
}

static ASTRef parse_call(Parser* par, ASTRef callee) {
    ArrayList args = arrlist_init(ASTRef);
    while (true) {
        if (peek(par).tag == TKN_R_PAREN) {
            pop_token(par);
            break;
        }

        ASTRef arg_expr = parse_expression(par, 0);
        if (!valid_ast(arg_expr)) {
            arrlist_deinit(&args);
            // TODO make error more specific (include arg num)
            return ast_error(par, err_str("Expr. parse failed."));
        }
        arrlist_push(&args, ASTRef, arg_expr);

        if (peek(par).tag == TKN_COMMA) {
            pop_token(par);
        }
    }

    CallAST* call = alloc_alloc(par->node_alloc, sizeof(CallAST));
    call->callee = callee;
    call->args = (ASTRef*)arrlist_to_array(&args);
    call->args_len = args.len;

    return (ASTRef) {
        .type = AST_CALL,
        .node = call,
    };
}

static ASTRef parse_index(Parser* par, ASTRef item) {
    IndexAST* index = alloc_alloc(par->node_alloc, sizeof(IndexAST));
    index->item = item;
    index->index = parse_expression(par, 0);
    if (!expect_token(par, TKN_R_BRACKET)) { return (ASTRef){0}; }
    return (ASTRef){
        .type = AST_INDEX,
        .node = index,
    };
}

static ASTRef parse_access(Parser* par, ASTRef parent) {
    if (peek(par).tag != TKN_IDENTIFIER) {
        ParseError err = {0};
        err.type = ERR_UNEXPECTED;
        err.tok = peek(par);
        err.data.unexpected.expected[0] = TKN_IDENTIFIER;
        return ast_error(par, err);
    }
    AccessAST* acc = alloc_alloc(par->node_alloc, sizeof(AccessAST));
    acc->parent = parent;
    acc->child = parse_symbol(par);
    return (ASTRef){
        .type = AST_ACCESS,
        .node = acc,
    };
}

static ASTRef parse_led(Parser* par, ASTRef left, int8_t lbp) {
    InfixOp op = infix_op(peek(par).tag);
    pop_token(par);
    switch (op) {
        case INOP_CALL:   return parse_call(par, left);
        case INOP_INDEX:  return parse_index(par, left);
        case INOP_ACCESS: return parse_access(par, left);
        default: break;
    }
    ASTRef rhs = parse_expression(par, lbp);
    if (!valid_ast(rhs)) {
        return ast_error(par, err_str("Failed to parse expression."));
    }

    BinaryAST* bin = malloc(sizeof(BinaryAST));
    bin->lhs = left;
    bin->rhs = rhs;
    return (ASTRef) {
        .type = type_of_infix(op),
        .node = bin,
    };
}

ASTRef parse_expression(Parser* par, int8_t rbp) {
    // Pratt Parser

    ASTRef left = parse_nud(par);
    if (!valid_ast(left)) {
        return ast_error(par, err_str("Failed to parse NUD."));
    }
    
    while (true) {
        InfixOp op = infix_op(peek(par).tag);
        if (op == INOP__NULL) return left;

        uint8_t lbp = bind_pow(op);
        bool assoc = left_assoc(op);
        if (assoc) {
            if (lbp <= rbp) break;
        } else {
            if (lbp < rbp) break;
        }

        left = parse_led(par, left, lbp);
        if (!valid_ast(left)) {
            return ast_error(par, err_str("Failed to parse LED."));
        }
    }
    return left;
}

static ASTRef parse_var_decl(Parser* par) {
    if (!expect_token(par, TKN_VAR)) {
        return ast_error(par, err_str("Failed to parse var decl"));
    }

    Token name = peek(par);
    if (!expect_token(par, TKN_IDENTIFIER)) {
        return ast_error(par, err_str("Failed to parse var decl"));
    }

    Token type = {0};
    if (peek(par).tag == TKN_COLON) {
        pop_token(par);
        type = peek(par);
        if (!expect_token(par, TKN_IDENTIFIER)) {
            return ast_error(par, err_str("Failed to parse var decl"));
        }
    }

    if (!expect_token(par, TKN_EQUAL)) {
        return ast_error(par, err_str("Failed to parse var decl"));
    }
    ASTRef expr = parse_expression(par, 0);
    if (!valid_ast(expr)) {
        return ast_error(par, err_str("Failed to parse var decl expr"));
    }

    VarDeclAST* var_decl = alloc_alloc(par->node_alloc, sizeof(VarDeclAST));
    var_decl->name = str_alloc(
        par->str_alloc,
        &par->src[name.start],
        name.len);
    var_decl->type = str_alloc(
        par->str_alloc,
        &par->src[type.start],
        type.len);
    var_decl->expr = expr;

    return (ASTRef) { .node = var_decl, .type = AST_VAR_DECL };
}

static ASTRef parse_return(Parser* par) {
    if (!expect_token(par, TKN_RETURN)) {
        return (ASTRef){0};
    }
    ASTRef expr = parse_expression(par, 0);
    if (!valid_ast(expr)) {
        return (ASTRef){0};
    }
    ReturnAST* ast = alloc_alloc(par->node_alloc, sizeof(ReturnAST));
    ast->expr = expr;
    return (ASTRef){
        .type = AST_RETURN,
        .node = ast,
    };
}

ASTRef parse_statement(Parser* par) {
    ASTRef inner;
    if (peek(par).tag == TKN_VAR) {
        inner = parse_var_decl(par);
    } else if (peek(par).tag == TKN_RETURN) {
        inner = parse_return(par);
    } else {
        inner = parse_expression(par, 0);
    }

    // TODO possible sync point?
    if (!expect_token(par, TKN_SEMICOLON)) {
        return (ASTRef){0};
    }
    return inner;
}

ASTRef parse_function_decl(Parser* par) {
    if (!expect_token(par, TKN_FN)) {
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    Token name_token = peek(par);
    if (!expect_token(par, TKN_IDENTIFIER)) {
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    if (!expect_token(par, TKN_L_PAREN)) {
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    ArrayList params = arrlist_init(Param);
    while (true) {
        if (peek(par).tag != TKN_IDENTIFIER) {
            break;
        }
        Param param = {0};
        param.name = str_alloc(
            par->str_alloc,
            &par->src[peek(par).start],
            peek(par).len);
        pop_token(par);

        if (!expect_token(par, TKN_COLON)) {
            arrlist_deinit(&params);
            return ast_error(par, err_str("Failed to parse fn param"));
        }

        if (peek(par).tag != TKN_IDENTIFIER) {
            arrlist_deinit(&params);
            return ast_error(par, err_str("Failed to parse fn param type"));
        }
        param.type = str_alloc(
            par->str_alloc,
            &par->src[peek(par).start],
            peek(par).len);
        pop_token(par);

        arrlist_push(&params, Param, param);

        if (peek(par).tag == TKN_COMMA) {
            pop_token(par);
        }
    }

    if (!expect_token(par, TKN_R_PAREN)) {
        arrlist_deinit(&params);
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    Str ret_type = str_lit("");
    if (peek(par).tag == TKN_IDENTIFIER) {
        ret_type = str_alloc(
            par->str_alloc,
            &par->src[peek(par).start],
            peek(par).len);
        pop_token(par);
    }

    if (!expect_token(par, TKN_L_BRACE)) {
        arrlist_deinit(&params);
        return ast_error(par, err_str("Failed to parse fn decl block"));
    }

    ArrayList statements = arrlist_init(ASTRef);
    while (true) {
        if (peek(par).tag == TKN_R_BRACE) {
            break;
        }
        
        ASTRef stmt = parse_statement(par);
        if (!valid_ast(stmt)) {
            arrlist_deinit(&params);
            arrlist_deinit(&statements);
            return ast_error(par, err_str("Failed to parse fn decl block"));
        }

        arrlist_push(&statements, ASTRef, stmt);
    }

    if (!expect_token(par, TKN_R_BRACE)) {
        arrlist_deinit(&params);
        arrlist_deinit(&statements);
        return ast_error(par, err_str("Failed to parse fn decl block"));
    }

    FunctionDeclAST* fn = alloc_alloc(par->node_alloc, sizeof(FunctionDeclAST));
    fn->name = str_alloc(
        par->str_alloc,
        &par->src[name_token.start],
        name_token.len);
    fn->params = arrlist_to_array(&params);
    fn->params_len = params.len;
    fn->ret_type = ret_type;
    fn->statements = arrlist_to_array(&statements);
    fn->statements_len = statements.len;

    return (ASTRef){ .node = fn, .type = AST_FUNCTION_DECL, };
}

static void print_indent(size_t depth) {
    /*for (size_t i = 1; i < depth; i++) {
        printf("│   ");
    }
    if (depth > 0) {
        if (last) {
            printf("└── ");
        } else {
            printf("├── ");
        }
    }*/
    for (size_t i = 0; i < depth; i++) {
        printf("  ");
    }
}

static void print_literal(const LiteralAST* lit) {
    printf("Literal { %ld }\n", lit->number);
}

static void print_symbol(const SymbolAST* sym) {
    printf("Symbol { \"%.*s\" }\n", (int)sym->name.len, sym->name.cstr);
}

static void print_unary(const UnaryAST* unary, const char* op_name, size_t depth) {
    printf("Unary%s {\n", op_name);
    print_indent(depth + 1);
    printf("inner = ");
    print_ast(unary->inner, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_binary(const BinaryAST* binary, const char* op_name, size_t depth) {
    printf("Binary%s {\n", op_name);
    print_indent(depth + 1);
    printf("lhs = ");
    print_ast(binary->lhs, depth + 1);
    print_indent(depth + 1);
    printf("rhs = ");
    print_ast(binary->rhs, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_access(const AccessAST* access, size_t depth) {
    printf("Access {\n");
    print_indent(depth + 1);
    printf("parent = ");
    print_ast(access->parent, depth + 1);
    print_indent(depth + 1);
    printf("child = ");
    print_symbol(access->child);
    print_indent(depth);
    printf("}\n");
}

static void print_call(const CallAST* call, size_t depth) {
    printf("Call {\n");
    print_indent(depth + 1);
    printf("callee = ");
    print_ast(call->callee, depth + 1);
    for (uint8_t i = 0; i < call->args_len; i++) {
        print_indent(depth + 1);
        printf("args[%d] = ", i);
        print_ast(call->args[i], depth + 1);
    }
    print_indent(depth);
    printf("}\n");
}

static void print_index(const IndexAST* index, size_t depth) {
    printf("Index {\n");
    print_indent(depth + 1);
    printf("item = ");
    print_ast(index->item, depth + 1);
    print_indent(depth + 1);
    printf("index = ");
    print_ast(index->index, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_param(const Param* param, size_t depth) {
    printf("Param {\n");
    print_indent(depth + 1);
    printf("name = %.*s\n", (int)param->name.len, param->name.cstr);
    print_indent(depth + 1);
    printf("type = %.*s\n", (int)param->type.len, param->type.cstr);
    print_indent(depth);
    printf("}\n");
}

static void print_function_decl(const FunctionDeclAST* fn_decl, size_t depth) {
    printf("FunctionDecl {\n");
    print_indent(depth + 1);
    printf("name = %.*s\n", (int)fn_decl->name.len, fn_decl->name.cstr);
    for (size_t i = 0; i < fn_decl->params_len; i++) {
        print_indent(depth + 1);
        printf("params[%zu] = ", i);
        print_param(&fn_decl->params[i], depth + 1);
    }
    print_indent(depth + 1);
    if (fn_decl->ret_type.len > 0) {
        printf("ret_type = %.*s\n", (int)fn_decl->ret_type.len, fn_decl->ret_type.cstr);
    } else {
        printf("ret_type = VOID\n");
    }

    for (size_t i = 0; i < fn_decl->statements_len; i++) {
        print_indent(depth + 1);
        printf("statements[%zu] = ", i);
        print_ast(fn_decl->statements[i], depth + 1);
    }
    print_indent(depth);
    printf("}\n");
}

static void print_var_decl(const VarDeclAST* var_decl, size_t depth) {
    printf("VarDecl {\n");
    print_indent(depth + 1);
    printf("name = %.*s\n", (int)var_decl->name.len, var_decl->name.cstr);
    print_indent(depth + 1);
    printf("type = %.*s\n", (int)var_decl->type.len, var_decl->type.cstr);
    print_indent(depth + 1);
    printf("expr = ");
    print_ast(var_decl->expr, depth + 1);
    print_indent(depth);
    printf("}\n");
}

static void print_return(const ReturnAST* ret, size_t depth) {
    printf("Return {\n");
    print_indent(depth + 1);
    printf("expr = ");
    print_ast(ret->expr, depth + 1);
    print_indent(depth);
    printf("}\n");
}

void print_ast(ASTRef ast, size_t depth) {
    switch (ast.type) {
        case AST_LITERAL: print_literal(ast.node); return;
        case AST_SYMBOL: print_symbol(ast.node); return;

        case AST_UNARY_BOOL_NOT: print_unary(ast.node, "BoolNot", depth); return;
        case AST_UNARY_BIT_NOT: print_unary(ast.node, "BitNot", depth); return;
        case AST_UNARY_NEGATE: print_unary(ast.node, "Negate", depth); return;

        case AST_BINARY_ASSIGN: print_binary(ast.node, "Assign", depth); return;
        case AST_BINARY_EQUAL: print_binary(ast.node, "Equal", depth); return;
        case AST_BINARY_NOT_EQUAL: print_binary(ast.node, "NotEqual", depth); return;
        case AST_BINARY_LESS: print_binary(ast.node, "Less", depth); return;
        case AST_BINARY_LESS_EQUAL: print_binary(ast.node, "LessEqual", depth); return;
        case AST_BINARY_GREATER: print_binary(ast.node, "Greater", depth); return;
        case AST_BINARY_GREATER_EQUAL: print_binary(ast.node, "GreaterEqual", depth); return;
        case AST_BINARY_ADD: print_binary(ast.node, "Add", depth); return;
        case AST_BINARY_SUB: print_binary(ast.node, "Sub", depth); return;
        case AST_BINARY_MUL: print_binary(ast.node, "Mul", depth); return;
        case AST_BINARY_DIV: print_binary(ast.node, "Div", depth); return;
        case AST_BINARY_MOD: print_binary(ast.node, "Mod", depth); return;

        case AST_ACCESS: print_access(ast.node, depth); return;
        case AST_CALL: print_call(ast.node, depth); return;
        case AST_INDEX: print_index(ast.node, depth); return;

        case AST_FUNCTION_DECL: print_function_decl(ast.node, depth); return;
        case AST_VAR_DECL: print_var_decl(ast.node, depth); return;
        case AST_RETURN: print_return(ast.node, depth); return;

        case AST__NULL: printf("NULL\n"); return;
    }
}
