#ifndef NQ_VM_H
#define NQ_VM_H

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "object.h"
#include "table.h"

// ─────────────────────────────────────────────
//  Call frame — one per active function invocation
// ─────────────────────────────────────────────
typedef struct {
    ObjFunction* function;
    uint8_t*     ip;          // instruction pointer for this frame
    Value*       slots;       // pointer into VM stack (base of this frame)
} CallFrame;

// ─────────────────────────────────────────────
//  Interpretation result
// ─────────────────────────────────────────────
typedef enum {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_COMPILE_ERROR,
} InterpretResult;

// ─────────────────────────────────────────────
//  The Nolqu Virtual Machine
// ─────────────────────────────────────────────
struct VM {
    // Call stack
    CallFrame frames[NQ_FRAMES_MAX];
    int       frame_count;

    // Value stack
    Value  stack[NQ_STACK_MAX];
    Value* stack_top;

    // Global variables
    Table globals;

    // Source path (for error messages)
    const char* source_path;
};

// ─────────────────────────────────────────────
//  VM API
// ─────────────────────────────────────────────
void            initVM(VM* vm);
void            freeVM(VM* vm);
InterpretResult runVM(VM* vm, ObjFunction* script, const char* source_path);

// Utility: print a runtime error with source location
void vmRuntimeError(VM* vm, const char* fmt, ...);

#endif // NQ_VM_H
