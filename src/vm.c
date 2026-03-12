#include "vm.h"
#include "memory.h"
#include "object.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ─────────────────────────────────────────────
//  VM init / free
// ─────────────────────────────────────────────
void initVM(VM* vm) {
    vm->stack_top   = vm->stack;
    vm->frame_count = 0;
    vm->source_path = NULL;
    initTable(&vm->globals);

    // Initialize the global string interning table
    initStringTable(&nq_string_table);
}

void freeVM(VM* vm) {
    freeTable(&vm->globals);
    freeStringTable(&nq_string_table);
    freeAllObjects();
}

// ─────────────────────────────────────────────
//  Stack operations
// ─────────────────────────────────────────────
static void push(VM* vm, Value v) {
    if (vm->stack_top >= vm->stack + NQ_STACK_MAX) {
        fprintf(stderr,
            NQ_COLOR_RED "[ Error Runtime ]" NQ_COLOR_RESET
            " Stack overflow! Kemungkinan rekursi tak terbatas.\n");
        exit(1);
    }
    *vm->stack_top++ = v;
}

static Value pop(VM* vm) {
    return *--vm->stack_top;
}

static Value peek(VM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

// ─────────────────────────────────────────────
//  Runtime error reporting
// ─────────────────────────────────────────────
void vmRuntimeError(VM* vm, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, NQ_COLOR_RED "\n[ Error Runtime ]" NQ_COLOR_RESET);
    if (vm->source_path) fprintf(stderr, " %s", vm->source_path);

    // Print call stack with line numbers
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame*   frame = &vm->frames[i];
        ObjFunction* fn    = frame->function;
        int offset = (int)(frame->ip - fn->chunk->code - 1);
        int line   = fn->chunk->lines[offset];
        fprintf(stderr, ":%d\n", line);

        if (fn->name) {
            fprintf(stderr, "  dari fungsi '%s'\n", fn->name->chars);
        }
    }

    fprintf(stderr, "  ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n\n");
    va_end(args);
}

// ─────────────────────────────────────────────
//  Type error helpers (Nolqu-friendly messages)
// ─────────────────────────────────────────────
static bool checkNumber(VM* vm, Value v, const char* hint) {
    if (IS_NUMBER(v)) return true;
    vmRuntimeError(vm,
        "Operasi ini memerlukan angka, bukan %s.\n  Petunjuk: %s",
        IS_STRING(v) ? "teks" :
        IS_BOOL(v)   ? "boolean" :
        IS_NIL(v)    ? "nil" : "objek",
        hint);
    return false;
}

// ─────────────────────────────────────────────
//  String concatenation helper
// ─────────────────────────────────────────────
static ObjString* valueToString(Value v) {
    if (IS_STRING(v)) return AS_STRING(v);

    char buf[64];
    if (IS_NIL(v))  snprintf(buf, sizeof(buf), "nil");
    else if (IS_BOOL(v)) snprintf(buf, sizeof(buf), "%s", AS_BOOL(v) ? "true" : "false");
    else if (IS_NUMBER(v)) {
        double n = AS_NUMBER(v);
        if (n == (long long)n) snprintf(buf, sizeof(buf), "%lld", (long long)n);
        else snprintf(buf, sizeof(buf), "%g", n);
    } else {
        snprintf(buf, sizeof(buf), "<objek>");
    }
    return copyString(buf, (int)strlen(buf));
}

static Value concatStrings(ObjString* a, ObjString* b) {
    int   len = a->length + b->length;
    char* buf = (char*)malloc((size_t)(len + 1));
    memcpy(buf,              a->chars, (size_t)a->length);
    memcpy(buf + a->length,  b->chars, (size_t)b->length);
    buf[len] = '\0';
    ObjString* result = takeString(buf, len);
    return OBJ_VAL(result);
}

// ─────────────────────────────────────────────
//  Function call handler
// ─────────────────────────────────────────────
static bool callFunction(VM* vm, ObjFunction* fn, int argc) {
    if (argc != fn->arity) {
        vmRuntimeError(vm,
            "Fungsi '%s' memerlukan %d argumen, tapi diberikan %d.",
            fn->name ? fn->name->chars : "<skrip>",
            fn->arity, argc);
        return false;
    }
    if (vm->frame_count >= NQ_FRAMES_MAX) {
        vmRuntimeError(vm, "Stack overflow — terlalu banyak pemanggilan fungsi bersarang.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->function  = fn;
    frame->ip        = fn->chunk->code;
    // slots[0] = function itself, slots[1..argc] = arguments
    frame->slots = vm->stack_top - argc - 1;
    return true;
}

// ─────────────────────────────────────────────
//  Main execution loop
// ─────────────────────────────────────────────
InterpretResult runVM(VM* vm, ObjFunction* script, const char* source_path) {
    vm->source_path = source_path;

    // Push the script function itself onto the stack
    push(vm, OBJ_VAL(script));
    callFunction(vm, script, 0);

    CallFrame* frame = &vm->frames[vm->frame_count - 1];

// Convenience macros for the dispatch loop
#define READ_BYTE()    (*frame->ip++)
#define READ_UINT16()  (frame->ip += 2, \
                        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONST()   (frame->function->chunk->constants.values[READ_BYTE()])

#define BINARY_NUM(vm_op) do { \
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
        vmRuntimeError(vm, \
            "Operasi ini hanya bisa digunakan pada angka. " \
            "Gunakan '..' untuk menggabungkan teks."); \
        return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = AS_NUMBER(pop(vm)); \
    double a = AS_NUMBER(pop(vm)); \
    push(vm, NUMBER_VAL(a vm_op b)); \
} while (0)

    for (;;) {
        uint8_t instruction = READ_BYTE();

        switch (instruction) {

            // ── Literals ────────────────────────
            case OP_CONST: push(vm, READ_CONST()); break;
            case OP_NIL:   push(vm, NIL_VAL);      break;
            case OP_TRUE:  push(vm, BOOL_VAL(true)); break;
            case OP_FALSE: push(vm, BOOL_VAL(false)); break;

            // ── Stack ───────────────────────────
            case OP_POP: pop(vm); break;
            case OP_DUP: push(vm, peek(vm, 0)); break;

            // ── Globals ─────────────────────────
            case OP_DEFINE_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                tableSet(&vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                Value val;
                if (!tableGet(&vm->globals, name, &val)) {
                    vmRuntimeError(vm,
                        "Variabel '%s' tidak ditemukan.\n"
                        "  Petunjuk: Apakah kamu sudah mendeklarasikannya dengan 'let %s = ...'?",
                        name->chars, name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, val);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                if (tableSet(&vm->globals, name, peek(vm, 0))) {
                    // tableSet returns true if it was a NEW key
                    tableDelete(&vm->globals, name);
                    vmRuntimeError(vm,
                        "Variabel '%s' belum dideklarasikan.\n"
                        "  Petunjuk: Gunakan 'let %s = ...' untuk mendeklarasikan terlebih dahulu.",
                        name->chars, name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                // Note: we don't pop — assignment is an expression in some contexts
                break;
            }

            // ── Locals ──────────────────────────
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            // ── Arithmetic ──────────────────────
            case OP_ADD: BINARY_NUM(+); break;
            case OP_SUB: BINARY_NUM(-); break;
            case OP_MUL: BINARY_NUM(*); break;
            case OP_DIV: {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
                    vmRuntimeError(vm, "Pembagian hanya bisa dilakukan pada angka.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                if (b == 0.0) {
                    vmRuntimeError(vm,
                        "Pembagian dengan nol tidak diizinkan!\n"
                        "  Petunjuk: Periksa nilai pembagi sebelum melakukan pembagian.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, NUMBER_VAL(a / b));
                break;
            }
            case OP_MOD: {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
                    vmRuntimeError(vm, "Modulo hanya bisa dilakukan pada angka.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                if (b == 0.0) {
                    vmRuntimeError(vm, "Modulo dengan nol tidak diizinkan.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, NUMBER_VAL(fmod(a, b)));
                break;
            }
            case OP_NEGATE: {
                Value v = peek(vm, 0);
                if (!checkNumber(vm, v, "Gunakan tanda minus hanya untuk angka. Contoh: -5")) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop(vm);
                push(vm, NUMBER_VAL(-AS_NUMBER(v)));
                break;
            }

            // ── String concat ────────────────────
            case OP_CONCAT: {
                Value b_val = pop(vm);
                Value a_val = pop(vm);
                // Auto-convert to string if needed
                ObjString* a = valueToString(a_val);
                ObjString* b = valueToString(b_val);
                push(vm, concatStrings(a, b));
                break;
            }

            // ── Comparison ──────────────────────
            case OP_EQ: {
                Value b = pop(vm), a = pop(vm);
                push(vm, BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_NEQ: {
                Value b = pop(vm), a = pop(vm);
                push(vm, BOOL_VAL(!valuesEqual(a, b)));
                break;
            }
            case OP_LT: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    vmRuntimeError(vm, "Perbandingan '<' hanya untuk angka.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                push(vm, BOOL_VAL(a < b));
                break;
            }
            case OP_GT: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    vmRuntimeError(vm, "Perbandingan '>' hanya untuk angka.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                push(vm, BOOL_VAL(a > b));
                break;
            }
            case OP_LTE: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    vmRuntimeError(vm, "Perbandingan '<=' hanya untuk angka.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                push(vm, BOOL_VAL(a <= b));
                break;
            }
            case OP_GTE: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    vmRuntimeError(vm, "Perbandingan '>=' hanya untuk angka.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                push(vm, BOOL_VAL(a >= b));
                break;
            }

            // ── Logical ─────────────────────────
            case OP_NOT: {
                Value v = pop(vm);
                push(vm, BOOL_VAL(!isTruthy(v)));
                break;
            }

            // ── I/O ─────────────────────────────
            case OP_PRINT: {
                printValue(pop(vm));
                printf("\n");
                break;
            }

            // ── Control flow ────────────────────
            case OP_JUMP: {
                uint16_t offset = READ_UINT16();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_UINT16();
                if (!isTruthy(peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_UINT16();
                frame->ip -= offset;
                break;
            }

            // ── Functions ───────────────────────
            case OP_CALL: {
                int argc = READ_BYTE();
                Value callee = peek(vm, argc);
                if (!IS_FUNCTION(callee)) {
                    vmRuntimeError(vm,
                        "Hanya fungsi yang bisa dipanggil, bukan %s.\n"
                        "  Petunjuk: Pastikan nama yang kamu panggil adalah sebuah fungsi.",
                        IS_STRING(callee) ? "teks" :
                        IS_NUMBER(callee) ? "angka" :
                        IS_BOOL(callee)   ? "boolean" :
                        IS_NIL(callee)    ? "nil" : "objek");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjFunction* fn = AS_FUNCTION(callee);
                if (!callFunction(vm, fn, argc)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                // Update frame pointer to new top frame
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;

                if (vm->frame_count == 0) {
                    // Returned from top-level script
                    pop(vm);
                    return INTERPRET_OK;
                }

                // Restore stack to before the call
                vm->stack_top = frame->slots;
                push(vm, result);

                // Restore frame
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_HALT:
                return INTERPRET_OK;

            default: {
                vmRuntimeError(vm,
                    "Instruksi bytecode tidak dikenal: %d. "
                    "Ini adalah bug internal — harap laporkan.", instruction);
                return INTERPRET_RUNTIME_ERROR;
            }
        }
    }

#undef READ_BYTE
#undef READ_UINT16
#undef READ_CONST
#undef BINARY_NUM
}
