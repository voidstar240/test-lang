#include <string.h>
#include <stdio.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Types.h>

#include "codegen.h"
#include "parser.h"

LLVMTypeRef type_map_get(TypeMap* tm, const char* name) {
    struct Pair {
        const char* name;
        LLVMTypeRef type;
    };
    for (size_t i = tm->inner.len - 1; i != -1; i--) {
        const struct Pair pair = arrlist_get(&tm->inner, struct Pair, i);
        if (strcmp(pair.name, name) == 0) {
            return pair.type;
        }
    }
    return (LLVMTypeRef){0};
}

LLVMValueRef symmap_get(SymbolMap* sm, const char* name) {
    for (size_t i = sm->inner.len - 1; i != -1; i--) {
        const SymbolDef def = arrlist_get(&sm->inner, SymbolDef, i);
        if (strcmp(def.name, name) == 0) {
            return def.value;
        }
    }
    return (LLVMValueRef){0};
}

void codegen_init_prim_types(CodeGen* cg) {
    type_map_push(&cg->types, "bool", LLVMInt1TypeInContext(cg->ctx));
    type_map_push(&cg->types, "u8", LLVMInt8TypeInContext(cg->ctx));
    type_map_push(&cg->types, "i8", LLVMInt8TypeInContext(cg->ctx));
    type_map_push(&cg->types, "u16", LLVMInt16TypeInContext(cg->ctx));
    type_map_push(&cg->types, "i16", LLVMInt16TypeInContext(cg->ctx));
    type_map_push(&cg->types, "u32", LLVMInt32TypeInContext(cg->ctx));
    type_map_push(&cg->types, "i32", LLVMInt32TypeInContext(cg->ctx));
    type_map_push(&cg->types, "u64", LLVMInt64TypeInContext(cg->ctx));
    type_map_push(&cg->types, "i64", LLVMInt64TypeInContext(cg->ctx));

    type_map_push(&cg->types, "f16", LLVMHalfTypeInContext(cg->ctx));
    type_map_push(&cg->types, "f32", LLVMFloatTypeInContext(cg->ctx));
    type_map_push(&cg->types, "f64", LLVMDoubleTypeInContext(cg->ctx));
}

void codegen_init_module(CodeGen* cg, const char* name) {
    cg->mod = LLVMModuleCreateWithNameInContext(name, cg->ctx);
    cg->build = LLVMCreateBuilderInContext(cg->ctx);
}

static LLVMValueRef gen_ast(CodeGen* cg, ASTRef ast);

static LLVMValueRef gen_literal(CodeGen* cg, LiteralAST* lit) {
    return LLVMConstInt(type_map_get(&cg->types, "i32"), lit->number, 0);
}

static LLVMValueRef gen_symbol(CodeGen* cg, SymbolAST* sym) {
    const LLVMValueRef val = symmap_get(&cg->symbols, sym->name.cstr);
    if (val == (LLVMValueRef){0}) {
        printf("\nERROR: NO VALUE FOR %.*s\n\n", (int)sym->name.len, sym->name.cstr);
    }
    return val;
}

static LLVMValueRef gen_add(CodeGen* cg, BinaryAST* bin) {
    LLVMValueRef lhs = gen_ast(cg, bin->lhs);
    LLVMValueRef rhs = gen_ast(cg, bin->rhs);
    return LLVMBuildAdd(cg->build, lhs, rhs, "addtmp");
}

static LLVMValueRef gen_sub(CodeGen* cg, BinaryAST* bin) {
    LLVMValueRef lhs = gen_ast(cg, bin->lhs);
    LLVMValueRef rhs = gen_ast(cg, bin->rhs);
    return LLVMBuildSub(cg->build, lhs, rhs, "subtmp");
}

static LLVMValueRef gen_mul(CodeGen* cg, BinaryAST* bin) {
    LLVMValueRef lhs = gen_ast(cg, bin->lhs);
    LLVMValueRef rhs = gen_ast(cg, bin->rhs);
    return LLVMBuildMul(cg->build, lhs, rhs, "multmp");
}

static LLVMValueRef gen_div(CodeGen* cg, BinaryAST* bin) {
    LLVMValueRef lhs = gen_ast(cg, bin->lhs);
    LLVMValueRef rhs = gen_ast(cg, bin->rhs);
    return LLVMBuildSDiv(cg->build, lhs, rhs, "divtmp"); // TODO this needs more type info
}

static LLVMValueRef gen_mod(CodeGen* cg, BinaryAST* bin) {
    LLVMValueRef lhs = gen_ast(cg, bin->lhs);
    LLVMValueRef rhs = gen_ast(cg, bin->rhs);
    return LLVMBuildSRem(cg->build, lhs, rhs, "modtmp"); // TODO this also needs more type info
}

static LLVMValueRef gen_cmp_eq(CodeGen* cg, LLVMIntPredicate op, BinaryAST* bin) {
    LLVMValueRef lhs = gen_ast(cg, bin->lhs);
    LLVMValueRef rhs = gen_ast(cg, bin->rhs);
    return LLVMBuildICmp(cg->build, op, lhs, rhs, "cmptmp");
}

static LLVMValueRef gen_return(CodeGen* cg, ReturnAST* ret) {
    return LLVMBuildRet(cg->build, gen_ast(cg, ret->expr));
}

static LLVMValueRef gen_ast(CodeGen* cg, ASTRef ast) {
    switch (ast.type) {
        case AST_LITERAL: return gen_literal(cg, ast.node);
        case AST_SYMBOL: return gen_symbol(cg, ast.node);


        //case AST_UNARY_BOOL_NOT: break;
        //case AST_UNARY_BIT_NOT: break;
        //case AST_UNARY_NEGATE: break;

        //case AST_BINARY_ASSIGN: break;
        case AST_BINARY_EQUAL:         return gen_cmp_eq(cg, LLVMIntEQ, ast.node);
        case AST_BINARY_NOT_EQUAL:     return gen_cmp_eq(cg, LLVMIntNE, ast.node);
        case AST_BINARY_LESS:          return gen_cmp_eq(cg, LLVMIntSLT, ast.node);
        case AST_BINARY_LESS_EQUAL:    return gen_cmp_eq(cg, LLVMIntSLE, ast.node);
        case AST_BINARY_GREATER:       return gen_cmp_eq(cg, LLVMIntSGT, ast.node);
        case AST_BINARY_GREATER_EQUAL: return gen_cmp_eq(cg, LLVMIntSGE, ast.node);
        case AST_BINARY_ADD: return gen_add(cg, ast.node);
        case AST_BINARY_SUB: return gen_sub(cg, ast.node);
        case AST_BINARY_MUL: return gen_mul(cg, ast.node);
        case AST_BINARY_DIV: return gen_div(cg, ast.node);
        case AST_BINARY_MOD: return gen_mod(cg, ast.node);

        //case AST_CALL: break;
        //case AST_INDEX: break;
        //case AST_ACCESS: break;

        //case AST_FUNCTION_DECL: break;
        //case AST_VAR_DECL: break;
        case AST_RETURN: return gen_return(cg, ast.node);

        //case AST__NULL: break;
        //case AST__COUNT: break;
        //case AST__MAX: break;
        default: return (LLVMValueRef){0};
    }
}

void gen_function(CodeGen* cg, FunctionDeclAST* fn_ast) {
    // get param types
    LLVMTypeRef* param_types = calloc(fn_ast->params_len, sizeof(LLVMTypeRef));
    for (size_t i = 0; i < fn_ast->params_len; i++) {
        const LLVMTypeRef type = type_map_get(&cg->types, fn_ast->params[i].type.cstr);
        if (type == (LLVMTypeRef){0}) {
            printf("\nERROR: INVALID TYPE %.*s\n\n", (int)fn_ast->params[i].type.len, fn_ast->params[i].type.cstr);
            return;
        }
        param_types[i] = type;
    }

    // get return type
    LLVMTypeRef ret_type = type_map_get(&cg->types, fn_ast->ret_type.cstr);
    if (ret_type == (LLVMTypeRef){0}) {
        printf("\nERROR: INVALID RETURN TYPE %.*s\n\n", (int)fn_ast->ret_type.len, fn_ast->ret_type.cstr);
        return;
    }

    // construct function type
    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, fn_ast->params_len, 0);

    // register function
    LLVMValueRef fn = LLVMAddFunction(cg->mod, fn_ast->name.cstr, fn_type);

    // add params to symbol map
    for (size_t i = 0; i < fn_ast->params_len; i++) {
        symmap_push(&cg->symbols, fn_ast->params[i].name.cstr, LLVMGetParam(fn, i));
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(cg->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(cg->build, entry);

    for (size_t i = 0; i < fn_ast->statements_len; i++) {
        gen_ast(cg, fn_ast->statements[i]);
    }

    LLVMBool out = LLVMVerifyModule(cg->mod, LLVMPrintMessageAction, NULL);
    printf("\nVERIFY ERROR: %d\n\n", out);
    char* mod_str = LLVMPrintModuleToString(cg->mod);
    printf("MODULE:\n%s\n\n", mod_str);
}
