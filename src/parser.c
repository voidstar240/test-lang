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
    arrlist_push(&par->errs, ParseError, err);
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
    lit->number = int_from_str(par->token.start, par->token.len);
    pop_token(par);
    return lit;
}

SymbolAST* parse_symbol(Parser* par) {
    SymbolAST* sym = malloc(sizeof(SymbolAST));
    Token ident = pop_token(par);
    sym->bytes = ident.start;
    sym->len = ident.len;
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
    ArrayList args = arrlist_init(ASTRef);
    while (true) {
        if (par->token.type == TKN_R_PAREN) {
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

        if (par->token.type == TKN_COMMA) {
            pop_token(par);
        }
    }

    CallAST* call = malloc(sizeof(CallAST));
    call->callee = callee;
    call->args = (ASTRef*)arrlist_to_array(&args);
    call->args_len = args.len;

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

static ASTRef parse_var_decl(Parser* par) {
    if (!expect_token(par, TKN_VAR)) {
        return ast_error(par, err_str("Failed to parse var decl"));
    }

    Token name = par->token;
    if (!expect_token(par, TKN_IDENTIFIER)) {
        return ast_error(par, err_str("Failed to parse var decl"));
    }

    Token type = {0};
    if (par->token.type == TKN_COLON) {
        pop_token(par);
        type = par->token;
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

    VarDeclAST* var_decl = malloc(sizeof(VarDeclAST));
    var_decl->name = name.start;
    var_decl->name_len = name.len;
    var_decl->type = type.start;
    var_decl->type_len = type.len;
    var_decl->expr = expr;

    return (ASTRef) { .node = var_decl, .type = AST_VAR_DECL };
}

ASTRef parse_statement(Parser* par) {
    ASTRef inner;
    if (par->token.type == TKN_VAR) {
        inner = parse_var_decl(par);
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

    Token name_token = par->token;
    if (!expect_token(par, TKN_IDENTIFIER)) {
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    if (!expect_token(par, TKN_L_PAREN)) {
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    ArrayList params = arrlist_init(Param);
    while (true) {
        if (par->token.type != TKN_IDENTIFIER) {
            break;
        }
        Param param = {0};
        param.name = par->token.start;
        param.name_len = par->token.len;
        pop_token(par);

        if (!expect_token(par, TKN_COLON)) {
            arrlist_deinit(&params);
            return ast_error(par, err_str("Failed to parse fn param"));
        }

        if (par->token.type != TKN_IDENTIFIER) {
            arrlist_deinit(&params);
            return ast_error(par, err_str("Failed to parse fn param type"));
        }
        param.type = par->token.start;
        param.type_len = par->token.len;
        pop_token(par);

        arrlist_push(&params, Param, param);

        if (par->token.type == TKN_COMMA) {
            pop_token(par);
        }
    }

    if (!expect_token(par, TKN_R_PAREN)) {
        arrlist_deinit(&params);
        return ast_error(par, err_str("Failed to parse fn decl"));
    }

    const char* ret_type = NULL;
    size_t ret_type_len = 0;
    if (par->token.type == TKN_IDENTIFIER) {
        ret_type = par->token.start;
        ret_type_len = par->token.len;
        pop_token(par);
    }

    if (!expect_token(par, TKN_L_BRACE)) {
        arrlist_deinit(&params);
        return ast_error(par, err_str("Failed to parse fn decl block"));
    }

    ArrayList statements = arrlist_init(ASTRef);
    while (true) {
        if (par->token.type == TKN_R_BRACE) {
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

    FunctionDeclAST* fn = malloc(sizeof(FunctionDeclAST));
    fn->name = name_token.start;
    fn->name_len = name_token.len;
    fn->params = arrlist_to_array(&params);
    fn->params_len = params.len;
    fn->ret_type = ret_type;
    fn->ret_type_len = ret_type_len;
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
    printf("Symbol { \"%.*s\" }\n", (int)sym->len, sym->bytes);
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
    printf("name = %.*s\n", (int)param->name_len, param->name);
    print_indent(depth + 1);
    printf("type = %.*s\n", (int)param->type_len, param->type);
    print_indent(depth);
    printf("}\n");
}

static void print_function_decl(const FunctionDeclAST* fn_decl, size_t depth) {
    printf("FunctionDecl {\n");
    print_indent(depth + 1);
    printf("name = %.*s\n", (int)fn_decl->name_len, fn_decl->name);
    for (size_t i = 0; i < fn_decl->params_len; i++) {
        print_indent(depth + 1);
        printf("params[%zu] = ", i);
        print_param(&fn_decl->params[i], depth + 1);
    }
    print_indent(depth + 1);
    if (fn_decl->ret_type_len != 0) {
        printf("ret_type = %.*s\n", (int)fn_decl->ret_type_len, fn_decl->ret_type);
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
    printf("name = %.*s\n", (int)var_decl->name_len, var_decl->name);
    print_indent(depth + 1);
    printf("type = %.*s\n", (int)var_decl->type_len, var_decl->type);
    print_indent(depth + 1);
    printf("expr = ");
    print_ast(var_decl->expr, depth + 1);
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

        case AST__NULL: printf("NULL\n"); return;
        case AST__COUNT: printf("COUNT\n"); return;
        case AST__MAX: printf("MAX\n"); return;
    }
}
