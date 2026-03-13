#ifndef NQ_COMPILER_H
#define NQ_COMPILER_H

#include "common.h"
#include "ast.h"
#include "chunk.h"
#include "object.h"

// ─────────────────────────────────────────────
//  Local variable (tracked during compilation)
// ─────────────────────────────────────────────
typedef struct {
    char  name[64];
    int   depth;
    bool  initialized;
    bool  used;       // set when OP_GET_LOCAL references this slot
    int   decl_line;  // for "unused variable" warning
} Local;

// ─────────────────────────────────────────────
//  Compiler context — one per function/script
// ─────────────────────────────────────────────
typedef struct CompilerCtx {
    struct CompilerCtx* enclosing;   // parent compiler (for nested functions)
    ObjFunction*        function;    // function being compiled
    int                 scope_depth; // 0 = global scope

    Local  locals[NQ_LOCALS_MAX];
    int    local_count;
} CompilerCtx;

// ─────────────────────────────────────────────
//  Compilation result
// ─────────────────────────────────────────────
typedef struct {
    ObjFunction* function;  // compiled function (NULL on error)
    bool         had_error;
} CompileResult;

// ─────────────────────────────────────────────
//  Compiler API
// ─────────────────────────────────────────────
CompileResult compile(ASTNode* ast, const char* source_path);

#endif // NQ_COMPILER_H
