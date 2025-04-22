#ifndef CODEGEN_H
#define CODEGEN_H

#include <llvm-c/Core.h>

#include "array_list.h"
//#include "parser.h"

// TODO this whole file needs to be updated to work with new AST representation

typedef struct {
    ArrayList inner;
} TypeMap;

static inline TypeMap type_map_init(void) {
    struct Pair {
        const char* name;
        LLVMTypeRef type;
    };
    return (TypeMap){ .inner = arrlist_init(struct Pair) };
}

static inline void type_map_push(TypeMap* tm, const char* name, LLVMTypeRef type) {
    struct Pair {
        const char* name;
        LLVMTypeRef type;
    };
    const struct Pair val = { .name = name, .type = type };
    arrlist_push(&tm->inner, struct Pair, val);
}

LLVMTypeRef type_map_get(TypeMap* tm, const char* name);

typedef struct {
    ArrayList inner;
} SymbolMap;

typedef struct {
    const char* name;
    LLVMValueRef value;
} SymbolDef;

static inline SymbolMap symmap_init(void) {
    return (SymbolMap){ .inner = arrlist_init(SymbolDef) };
}

static inline void symmap_push(SymbolMap* sm, const char* name, LLVMValueRef val) {
    const SymbolDef def = { .name = name, .value = val };
    arrlist_push(&sm->inner, SymbolDef, def);
}

static inline void symmap_pop(SymbolMap* sm, size_t count) {
    arrlist_pop(&sm->inner, count);
}

LLVMValueRef symmap_get(SymbolMap* sm, const char* name);

typedef struct {
    LLVMContextRef ctx;
    LLVMBuilderRef build;
    LLVMModuleRef mod;
    TypeMap types;
    SymbolMap symbols;
} CodeGen;

static inline CodeGen codegen_init(void) {
    return (CodeGen){
        .ctx = LLVMContextCreate(),
        .build = (LLVMBuilderRef){0},
        .mod = (LLVMModuleRef){0},
        .types = type_map_init(),
        .symbols = symmap_init(),
    };
}

void codegen_init_prim_types(CodeGen* cg);
void codegen_init_module(CodeGen* cg, const char* name);

// REMOVED: uses old AST representation
//void gen_function(CodeGen* cg, FunctionDeclAST* fn_ast);

#endif
