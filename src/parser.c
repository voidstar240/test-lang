#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scanner.h"

static inline ParseError err_str(const char* msg) {
    return (ParseError){
        .type = ERR_GENERIC,
        .tok = {0},
        .data.generic.msg = msg,
    };
}

static inline ParseError err_unexp(Token current, TokenType expected) {
    ParseError err = {0};
    err.type = ERR_UNEXPECTED;
    err.tok = current;
    err.data.unexpected.expected[0] = expected;
    return err;
}

static void log_error(Parser* par, ParseError err) {
    ParseError* old = par->errs;
    par->errs = calloc(par->err_count + 1, sizeof(ParseError));
    if (par->err_count > 0) {
        memcpy(par->errs, old, par->err_count * sizeof(ParseError));
        free(old);
    }
    par->errs[par->err_count] = err;
    par->err_count++;
}

static ASTRef ast_error(Parser* par, ParseError err) {
    log_error(par, err);
    return (ASTRef){0};
}

void print_err(ParseError err) {
    switch (err.type) {
        case ERR_UNEXPECTED:
            printf("Got \"%s\" @ %zu:%zu. Expected one of",
                    token_type_str(err.tok.type),
                    err.tok.line, err.tok.col);
            for (int i = 0; i < 8; i++) {
                TokenType tok = err.data.unexpected.expected[i];
                if (tok == TKN__NULL) { break; }
                printf(" \"%s\"", token_type_str(tok));
            }
            printf(".\n");
            break;
        case ERR_BAD_OP:
            printf("Bad Operator \"%s\" @ %zu:%zu.\n",
                    token_type_str(err.tok.type),
                    err.tok.line, err.tok.col);
            break;
        case ERR_GENERIC:
            printf("GenericErr(%s)\n", err.data.generic.msg);
            break;
        case ERR__NULL: printf("NULL ERROR\n"); break;
    }
}

static inline Token pop_token(Parser* par) {
    const Token out = par->token;
    par->token = scnr_scan_token(par->scnr);
    return out;
}

static inline bool expect_token(Parser* par, TokenType type) {
    if (par->token.type != type) {
        ParseError err = {0};
        err.type = ERR_UNEXPECTED;
        err.tok = par->token;
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
    LiteralAST* lit = malloc(sizeof(LiteralAST));
    lit->number = int_from_str(par->token.start, par->token.length);
    pop_token(par);
    return lit;
}

SymbolAST* parse_symbol(Parser* par) {
    SymbolAST* sym = malloc(sizeof(SymbolAST));
    Token ident = pop_token(par);
    sym->bytes = ident.start;
    sym->len = ident.length;
    return sym;
}

typedef enum {
    PREOP__NULL    = 0x00,

    PREOP_BOOL_NOT = 0x01,
    PREOP_BIT_NOT  = 0x02,
    PREOP_NEGATE   = 0x03,

    PREOP__MAX     = 0xff,
} PrefixOp;

typedef enum {
    INOP__NULL         = 0x00,

    INOP_ASSIGN        = 0x11,
    INOP_EQUAL         = 0x12,
    INOP_NOT_EQUAL     = 0x13,
    INOP_LESS          = 0x14,
    INOP_LESS_EQUAL    = 0x15,
    INOP_GREATER       = 0x16,
    INOP_GREATER_EQUAL = 0x17,
    INOP_ADD           = 0x18,
    INOP_SUB           = 0x19,
    INOP_MUL           = 0x1a,
    INOP_DIV           = 0x1b,
    INOP_MOD           = 0x1c,

    INOP_CALL          = 0x1d,
    INOP_INDEX         = 0x1e,
    INOP_ACCESS           = 0x1f,

    INOP__MAX          = 0xff,
} InfixOp;

static PrefixOp prefix_op_of(TokenType tok) {
    switch (tok) {
        case TKN_BANG:  return PREOP_BOOL_NOT;
        case TKN_TILDE: return PREOP_BIT_NOT;
        case TKN_MINUS: return PREOP_NEGATE;
        default:        return PREOP__NULL;
    }
}

static InfixOp infix_op_of(TokenType tok) {
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

static uint8_t bind_pow_of(uint8_t op) {
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
        case PREOP_BOOL_NOT:
        case PREOP_BIT_NOT:
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

typedef enum {
    ASSOC_LEFT,
    ASSOC_RIGHT,
} Assoc;

static Assoc assoc_of(InfixOp op) {
    switch (op) {
        case INOP_ASSIGN:
            return ASSOC_RIGHT;
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
            return ASSOC_LEFT;
        case INOP__MAX:
        case INOP__NULL:
            return ASSOC_LEFT;
    }
}

static ASTType type_of_prefix(PrefixOp op) {
    switch (op) {
        case PREOP_BOOL_NOT: return AST_UNARY_BOOL_NOT;
        case PREOP_BIT_NOT:  return AST_UNARY_BIT_NOT;
        case PREOP_NEGATE:   return AST_UNARY_NEGATE;

        case PREOP__MAX:     return AST__NULL;
        case PREOP__NULL:    return AST__NULL;
    }
}

static ASTRef make_unary(PrefixOp op, ASTRef inner) {
    ASTRef expr;
    expr.type = type_of_prefix(op);
    UnaryAST* un = malloc(sizeof(UnaryAST));
    expr.node = un;
    un->inner = inner;
    return expr;
}

static ASTRef parse_nud(Parser* par) {
    PrefixOp op = prefix_op_of(par->token.type);
    if (op) {
        pop_token(par);
        ASTRef expr = parse_expression(par, bind_pow_of(op));
        if (!valid_ast(expr)) {
            return ast_error(par, err_str("Expr. parse failed."));
        }
        return make_unary(op, expr);
    }
    switch (par->token.type) {
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
        default:;
            ParseError err = err_unexp(par->token, TKN_NUMBER);
            err.data.unexpected.expected[1] = TKN_IDENTIFIER;
            return ast_error(par, err);
    }
}

static ASTType type_of_infix(InfixOp op) {
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

        case INOP__MAX:          return AST__NULL;
        case INOP__NULL:         return AST__NULL;
    }
}

static ASTRef make_binary(InfixOp op, ASTRef lhs, ASTRef rhs) {
    ASTRef expr;
    expr.type = type_of_infix(op);
    BinaryAST* bin = malloc(sizeof(BinaryAST));
    expr.node = bin;
    bin->lhs = lhs;
    bin->rhs = rhs;
    return expr;
}

static ASTRef parse_call(Parser* par, ASTRef callee) {
    CallAST* call = malloc(sizeof(CallAST));
    call->callee = callee;
    call->args_len = 0;
    while (true) {
        if (par->token.type == TKN_R_PAREN) {
            pop_token(par);
            break;
        }
        if (call->args_len < 128) {
            ASTRef* old = call->args;
            call->args = calloc(call->args_len + 1, sizeof(ASTRef));
            if (call->args_len > 0) {
                memcpy(call->args, old, call->args_len * sizeof(ASTRef));
                free(old);
            }
            call->args[call->args_len] = parse_expression(par, 0);
            if (!valid_ast(call->args[call->args_len])) {
                // TODO Free args
                return ast_error(par, err_str("Expr. parse failed."));
            }
            call->args_len++;
        }
        if (par->token.type == TKN_COMMA) {
            pop_token(par);
        }
    }
    return (ASTRef) {
        .type = AST_CALL,
        .node = call,
    };
}

static ASTRef parse_index(Parser* par, ASTRef item) {
    IndexAST* index = malloc(sizeof(IndexAST));
    index->item = item;
    index->index = parse_expression(par, 0);
    if (!expect_token(par, TKN_R_BRACKET)) { return (ASTRef){0}; }
    return (ASTRef){
        .type = AST_INDEX,
        .node = index,
    };
}

static ASTRef parse_access(Parser* par, ASTRef parent) {
    if (par->token.type != TKN_IDENTIFIER) {
        ParseError err = {0};
        err.type = ERR_UNEXPECTED;
        err.tok = par->token;
        err.data.unexpected.expected[0] = TKN_IDENTIFIER;
        return ast_error(par, err);
    }
    AccessAST* acc = malloc(sizeof(AccessAST));
    acc->parent = parent;
    acc->child = parse_symbol(par);
    return (ASTRef){
        .type = AST_ACCESS,
        .node = acc,
    };
}

static ASTRef parse_led(Parser* par, ASTRef left, int8_t lbp) {
    InfixOp op = infix_op_of(par->token.type);
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
    return make_binary(op, left, rhs);
}

ASTRef parse_expression(Parser* par, int8_t rbp) {
    // Pratt Parser

    ASTRef left = parse_nud(par);
    if (!valid_ast(left)) {
        return ast_error(par, err_str("Failed to parse NUD."));
    }
    
    while (true) {
        InfixOp op = infix_op_of(par->token.type);
        if (op == INOP__NULL) return left;

        uint8_t lbp = bind_pow_of(op);
        Assoc assoc = assoc_of(op);
        if (assoc == ASSOC_LEFT) {
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


static void print_literal(const LiteralAST* lit) {
    printf("Literal(%ld)", lit->number);
}

static void print_symbol(const SymbolAST* sym) {
    printf("Symbol(%.*s)", (int)sym->len, sym->bytes);
}

static void print_unary(const UnaryAST* unary, const char* op_name) {
    printf("Unary%s(inner: ", op_name);
    print_ast(unary->inner);
    printf(")");
}

static void print_binary(const BinaryAST* binary, const char* op_name) {
    printf("Binary%s(lhs: ", op_name);
    print_ast(binary->lhs);
    printf(" rhs: ");
    print_ast(binary->rhs);
    printf(")");
}

static void print_access(const AccessAST* access) {
    printf("Access(parent: ");
    print_ast(access->parent);
    printf(" child: ");
    print_symbol(access->child);
    printf(")");
}

static void print_call(const CallAST* call) {
    printf("Call(callee: ");
    print_ast(call->callee);
    for (uint8_t i = 0; i < call->args_len; i++) {
        printf(" arg%d: ", i);
        print_ast(call->args[i]);
    }
    printf(")");
}

static void print_index(const IndexAST* index) {
    printf("Index(item: ");
    print_ast(index->item);
    printf(" index: ");
    print_ast(index->index);
    printf(")");
}

void print_ast(ASTRef ast) {
    switch (ast.type) {
        case AST_LITERAL: print_literal(ast.node); return;
        case AST_SYMBOL: print_symbol(ast.node); return;

        case AST_UNARY_BOOL_NOT: print_unary(ast.node, "BoolNot"); return;
        case AST_UNARY_BIT_NOT: print_unary(ast.node, "BitNot"); return;
        case AST_UNARY_NEGATE: print_unary(ast.node, "Negate"); return;

        case AST_BINARY_ASSIGN: print_binary(ast.node, "Assign"); return;
        case AST_BINARY_EQUAL: print_binary(ast.node, "Equal"); return;
        case AST_BINARY_NOT_EQUAL: print_binary(ast.node, "NotEqual"); return;
        case AST_BINARY_LESS: print_binary(ast.node, "Less"); return;
        case AST_BINARY_LESS_EQUAL: print_binary(ast.node, "LessEqual"); return;
        case AST_BINARY_GREATER: print_binary(ast.node, "Greater"); return;
        case AST_BINARY_GREATER_EQUAL: print_binary(ast.node, "GreaterEqual"); return;
        case AST_BINARY_ADD: print_binary(ast.node, "Add"); return;
        case AST_BINARY_SUB: print_binary(ast.node, "Sub"); return;
        case AST_BINARY_MUL: print_binary(ast.node, "Mul"); return;
        case AST_BINARY_DIV: print_binary(ast.node, "Div"); return;
        case AST_BINARY_MOD: print_binary(ast.node, "Mod"); return;

        case AST_ACCESS: print_access(ast.node); return;
        case AST_CALL: print_call(ast.node); return;
        case AST_INDEX: print_index(ast.node); return;

        case AST__NULL: printf("NULL"); return;
        case AST__COUNT: printf("COUNT"); return;
        case AST__MAX: printf("MAX"); return;
    }
}
