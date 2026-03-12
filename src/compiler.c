#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────
//  Compiler state (module-level)
// ─────────────────────────────────────────────
static CompilerCtx* current = NULL;
static bool had_compile_error = false;
static const char* src_path  = NULL;

// ─────────────────────────────────────────────
//  Error reporting
// ─────────────────────────────────────────────
static void compileError(int line, const char* fmt, ...) {
    had_compile_error = true;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, NQ_COLOR_RED "[ Error Kompilasi ]" NQ_COLOR_RESET);
    if (src_path) fprintf(stderr, " %s", src_path);
    fprintf(stderr, ":%d\n  ", line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n\n");
    va_end(args);
}

// ─────────────────────────────────────────────
//  Current chunk helper
// ─────────────────────────────────────────────
static Chunk* currentChunk(void) {
    return current->function->chunk;
}

// ─────────────────────────────────────────────
//  Emit helpers
// ─────────────────────────────────────────────
static void emit(uint8_t byte, int line) {
    writeChunk(currentChunk(), byte, line);
}

static void emit2(uint8_t a, uint8_t b, int line) {
    emit(a, line);
    emit(b, line);
}

static int emitJump(uint8_t jump_op, int line) {
    emit(jump_op, line);
    emit(0xFF, line);  // placeholder hi
    emit(0xFF, line);  // placeholder lo
    return currentChunk()->count - 2; // position of hi byte
}

static void patchJumpAt(int offset) {
    patchJump(currentChunk(), offset);
}

static void emitLoop(int loop_start, int line) {
    emit(OP_LOOP, line);
    int offset = currentChunk()->count - loop_start + 2;
    if (offset > 0xFFFF) {
        compileError(line, "Loop terlalu besar.");
    }
    emit((offset >> 8) & 0xFF, line);
    emit(offset & 0xFF,        line);
}

static int emitConst(Value val, int line) {
    int idx = addConstant(currentChunk(), val);
    if (idx > 255) {
        compileError(line, "Terlalu banyak konstanta dalam satu fungsi.");
        return 0;
    }
    emit2(OP_CONST, (uint8_t)idx, line);
    return idx;
}

// ─────────────────────────────────────────────
//  String constant helper
// ─────────────────────────────────────────────
static uint8_t identConst(const char* name, int line) {
    ObjString* s = copyString(name, (int)strlen(name));
    int idx = addConstant(currentChunk(), OBJ_VAL(s));
    if (idx > 255) {
        compileError(line, "Terlalu banyak nama variabel global.");
        return 0;
    }
    return (uint8_t)idx;
}

// ─────────────────────────────────────────────
//  Local variable management
// ─────────────────────────────────────────────
static int resolveLocal(CompilerCtx* ctx, const char* name) {
    for (int i = ctx->local_count - 1; i >= 0; i--) {
        if (strcmp(ctx->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1; // not found → global
}

static void addLocal(const char* name, int line) {
    if (current->local_count >= NQ_LOCALS_MAX) {
        compileError(line, "Terlalu banyak variabel lokal dalam fungsi.");
        return;
    }
    Local* local = &current->locals[current->local_count++];
    strncpy(local->name, name, 63);
    local->name[63]     = '\0';
    local->depth        = current->scope_depth;
    local->initialized  = false;
}

// ─────────────────────────────────────────────
//  Forward declaration
// ─────────────────────────────────────────────
static void compileNode(ASTNode* node);
static void compileExpr(ASTNode* node);
static void compileBlock(ASTNode* block);

// ─────────────────────────────────────────────
//  Begin/end compiler context
// ─────────────────────────────────────────────
static void initCompilerCtx(CompilerCtx* ctx, ObjFunction* fn) {
    ctx->enclosing    = current;
    ctx->function     = fn;
    ctx->scope_depth  = (current ? current->scope_depth : 0);
    ctx->local_count  = 0;
    current           = ctx;
}

static ObjFunction* endCompilerCtx(void) {
    emit(OP_NIL,    0);
    emit(OP_RETURN, 0);
    ObjFunction* fn = current->function;
    current = current->enclosing;
    return fn;
}

// ─────────────────────────────────────────────
//  Compile expression nodes
// ─────────────────────────────────────────────
static void compileExpr(ASTNode* node) {
    if (!node) return;
    int line = node->line;

    switch (node->type) {
        case NODE_NUMBER:
            emitConst(NUMBER_VAL(node->data.number.value), line);
            break;

        case NODE_STRING: {
            ObjString* s = copyString(node->data.string.value,
                                      node->data.string.length);
            emitConst(OBJ_VAL(s), line);
            break;
        }

        case NODE_BOOL:
            emit(node->data.boolean.value ? OP_TRUE : OP_FALSE, line);
            break;

        case NODE_NIL:
            emit(OP_NIL, line);
            break;

        case NODE_IDENT: {
            const char* name = node->data.ident.name;
            int slot = resolveLocal(current, name);
            if (slot >= 0) {
                emit2(OP_GET_LOCAL, (uint8_t)slot, line);
            } else {
                uint8_t idx = identConst(name, line);
                emit2(OP_GET_GLOBAL, idx, line);
            }
            break;
        }

        case NODE_UNARY: {
            compileExpr(node->data.unary.operand);
            switch (node->data.unary.op) {
                case TK_MINUS: emit(OP_NEGATE, line); break;
                case TK_NOT:   emit(OP_NOT,    line); break;
                default: break;
            }
            break;
        }

        case NODE_BINARY: {
            TokenType op = node->data.binary.op;

            // Short-circuit 'and' / 'or'
            if (op == TK_AND) {
                compileExpr(node->data.binary.left);
                int jump = emitJump(OP_JUMP_IF_FALSE, line);
                emit(OP_POP, line);
                compileExpr(node->data.binary.right);
                patchJumpAt(jump);
                break;
            }
            if (op == TK_OR) {
                compileExpr(node->data.binary.left);
                // If truthy, skip right side
                int else_jump = emitJump(OP_JUMP_IF_FALSE, line);
                int end_jump  = emitJump(OP_JUMP, line);
                patchJumpAt(else_jump);
                emit(OP_POP, line);
                compileExpr(node->data.binary.right);
                patchJumpAt(end_jump);
                break;
            }

            compileExpr(node->data.binary.left);
            compileExpr(node->data.binary.right);

            switch (op) {
                case TK_PLUS:    emit(OP_ADD,    line); break;
                case TK_MINUS:   emit(OP_SUB,    line); break;
                case TK_STAR:    emit(OP_MUL,    line); break;
                case TK_SLASH:   emit(OP_DIV,    line); break;
                case TK_PERCENT: emit(OP_MOD,    line); break;
                case TK_DOTDOT:  emit(OP_CONCAT, line); break;
                case TK_EQEQ:   emit(OP_EQ,     line); break;
                case TK_BANGEQ:  emit(OP_NEQ,    line); break;
                case TK_LT:      emit(OP_LT,     line); break;
                case TK_GT:      emit(OP_GT,     line); break;
                case TK_LTEQ:    emit(OP_LTE,    line); break;
                case TK_GTEQ:    emit(OP_GTE,    line); break;
                default:
                    compileError(line, "Operator tidak dikenal dalam ekspresi binary.");
                    break;
            }
            break;
        }

        case NODE_CALL: {
            // Push callee
            compileExpr(node->data.call.callee);
            // Push args
            int argc = node->data.call.args.count;
            for (int i = 0; i < argc; i++) {
                compileExpr(node->data.call.args.items[i]);
            }
            if (argc > 255) {
                compileError(line, "Terlalu banyak argumen (maks 255).");
                argc = 255;
            }
            emit2(OP_CALL, (uint8_t)argc, line);
            break;
        }

        default:
            compileError(line, "Ekspresi tidak dapat dikompilasi (tipe: %d).", node->type);
            break;
    }
}

// ─────────────────────────────────────────────
//  Compile statement nodes
// ─────────────────────────────────────────────
static void compileNode(ASTNode* node) {
    if (!node || had_compile_error) return;
    int line = node->line;

    switch (node->type) {

        case NODE_LET: {
            const char* name = node->data.let.name;
            compileExpr(node->data.let.value);

            if (current->scope_depth > 0) {
                // Local variable
                addLocal(name, line);
                current->locals[current->local_count - 1].initialized = true;
            } else {
                // Global variable
                uint8_t idx = identConst(name, line);
                emit2(OP_DEFINE_GLOBAL, idx, line);
            }
            break;
        }

        case NODE_ASSIGN: {
            const char* name = node->data.assign.name;
            compileExpr(node->data.assign.value);

            int slot = resolveLocal(current, name);
            if (slot >= 0) {
                emit2(OP_SET_LOCAL, (uint8_t)slot, line);
            } else {
                uint8_t idx = identConst(name, line);
                emit2(OP_SET_GLOBAL, idx, line);
            }
            emit(OP_POP, line); // assignment is a statement, discard result
            break;
        }

        case NODE_PRINT:
            compileExpr(node->data.print.expr);
            emit(OP_PRINT, line);
            break;

        case NODE_IF: {
            // Compile condition
            compileExpr(node->data.if_stmt.cond);

            // Jump over then-block if false
            int then_jump = emitJump(OP_JUMP_IF_FALSE, line);
            emit(OP_POP, line); // pop condition (was truthy)

            compileBlock(node->data.if_stmt.then_block);

            int else_jump = -1;
            if (node->data.if_stmt.else_block) {
                else_jump = emitJump(OP_JUMP, line);
            }

            patchJumpAt(then_jump);
            emit(OP_POP, line); // pop condition (was falsy)

            if (node->data.if_stmt.else_block) {
                compileBlock(node->data.if_stmt.else_block);
                patchJumpAt(else_jump);
            }
            break;
        }

        case NODE_LOOP: {
            int loop_start = currentChunk()->count;

            // Compile condition
            compileExpr(node->data.loop.cond);

            // Exit loop if false
            int exit_jump = emitJump(OP_JUMP_IF_FALSE, line);
            emit(OP_POP, line); // pop true condition

            compileBlock(node->data.loop.body);

            // Jump back to loop start
            emitLoop(loop_start, line);

            patchJumpAt(exit_jump);
            emit(OP_POP, line); // pop false condition
            break;
        }

        case NODE_FUNCTION: {
            // Compile function body into its own CompilerCtx
            ObjFunction* fn = newFunction();
            fn->arity = node->data.function.param_count;
            fn->name  = copyString(node->data.function.name,
                                   (int)strlen(node->data.function.name));

            CompilerCtx fn_ctx;
            initCompilerCtx(&fn_ctx, fn);
            fn_ctx.scope_depth = 1;

            // IMPORTANT: slot 0 of every call frame is the function object itself.
            // Reserve it with a dummy local so parameters start at slot 1.
            addLocal("", line);
            current->locals[current->local_count - 1].initialized = true;

            // Declare parameters as locals in function scope (slots 1, 2, ...)
            for (int i = 0; i < node->data.function.param_count; i++) {
                const char* pname = node->data.function.params[i];
                addLocal(pname, line);
                current->locals[current->local_count - 1].initialized = true;
            }

            // Compile body
            compileBlock(node->data.function.body);

            // End function
            ObjFunction* compiled_fn = endCompilerCtx();

            // Store function as constant and define global
            int fn_idx = addConstant(currentChunk(), OBJ_VAL(compiled_fn));
            emit2(OP_CONST, (uint8_t)fn_idx, line);
            uint8_t name_idx = identConst(node->data.function.name, line);
            emit2(OP_DEFINE_GLOBAL, name_idx, line);
            break;
        }

        case NODE_RETURN: {
            if (current->enclosing == NULL) {
                compileError(line, "'return' tidak dapat digunakan di luar fungsi.");
                break;
            }
            if (node->data.ret.expr) {
                compileExpr(node->data.ret.expr);
            } else {
                emit(OP_NIL, line);
            }
            emit(OP_RETURN, line);
            break;
        }

        case NODE_EXPR_STMT:
            compileExpr(node->data.expr_stmt.expr);
            emit(OP_POP, line); // discard expression result
            break;

        case NODE_IMPORT:
            // TODO: implement module loading
            compileError(line, "'import' belum didukung pada versi ini.");
            break;

        case NODE_BLOCK:
        case NODE_PROGRAM:
            compileBlock(node);
            break;

        default:
            compileError(line, "Node tidak dapat dikompilasi (tipe: %d).", node->type);
            break;
    }
}

// ─────────────────────────────────────────────
//  Compile a block (list of statements)
// ─────────────────────────────────────────────
static void compileBlock(ASTNode* block) {
    if (!block) return;
    NodeList* stmts = &block->data.block.stmts;
    for (int i = 0; i < stmts->count; i++) {
        compileNode(stmts->items[i]);
        if (had_compile_error) break;
    }
}

// ─────────────────────────────────────────────
//  Public compile entry point
// ─────────────────────────────────────────────
CompileResult compile(ASTNode* ast, const char* source_path) {
    had_compile_error = false;
    src_path          = source_path;

    ObjFunction* script = newFunction();
    script->name  = NULL; // top-level script has no name

    CompilerCtx ctx;
    initCompilerCtx(&ctx, script);
    ctx.scope_depth = 0;

    // Compile all top-level statements
    compileBlock(ast);

    emit(OP_HALT, 0);
    ObjFunction* fn = endCompilerCtx();

    CompileResult result;
    result.function   = had_compile_error ? NULL : fn;
    result.had_error  = had_compile_error;
    return result;
}
