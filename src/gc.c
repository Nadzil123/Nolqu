#include "gc.h"
#include "vm.h"
#include "object.h"
#include "memory.h"
#include "table.h"
#include <stdio.h>

// ─────────────────────────────────────────────
//  Mark phase — recursively mark reachable objects
// ─────────────────────────────────────────────

static void markObject(Obj* obj) {
    if (obj == NULL || obj->marked) return;
    obj->marked = true;

    // Recursively mark children
    switch (obj->type) {
        case OBJ_STRING:
            // Strings have no children
            break;
        case OBJ_NATIVE:
            // Natives have no heap children
            break;
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            if (fn->name) markObject((Obj*)fn->name);
            // Mark all constants in the function's chunk
            for (int i = 0; i < fn->chunk->constants.count; i++) {
                Value v = fn->chunk->constants.values[i];
                if (IS_OBJ(v)) markObject(AS_OBJ(v));
            }
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)obj;
            for (int i = 0; i < arr->count; i++)
                if (IS_OBJ(arr->items[i])) markObject(AS_OBJ(arr->items[i]));
            break;
        }
    }
}

static void markValue(Value v) {
    if (IS_OBJ(v)) markObject(AS_OBJ(v));
}

static void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* e = &table->entries[i];
        if (e->key == NULL) continue;
        markObject((Obj*)e->key);
        markValue(e->value);
    }
}

static void markRoots(VM* vm) {
    // 1. Value stack
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++)
        markValue(*slot);

    // 2. Call frames — each frame's function
    for (int i = 0; i < vm->frame_count; i++)
        markObject((Obj*)vm->frames[i].function);

    // 3. Try handler stack — saved stack tops don't need marking
    //    (those values are already covered by the stack scan above)

    // 4. Global variables
    markTable(&vm->globals);

    // 5. Pending thrown value
    markValue(vm->thrown);
}

// ─────────────────────────────────────────────
//  Sweep phase — free all unmarked objects
// ─────────────────────────────────────────────

static void sweep(void) {
    Obj** obj = &nq_all_objects;
    while (*obj) {
        if ((*obj)->marked) {
            // Survived — reset mark for next cycle
            (*obj)->marked = false;
            obj = &(*obj)->next;
        } else {
            // Unreachable — free it
            Obj* garbage = *obj;
            *obj = garbage->next;
            freeObject(garbage);
        }
    }
}

// ─────────────────────────────────────────────
//  Remove dead interned strings from the string table
// ─────────────────────────────────────────────

static void removeWhiteStrings(void) {
    StringTable* t = &nq_string_table;
    for (int i = 0; i < t->capacity; i++) {
        ObjString* s = t->entries[i];
        if (s != NULL && !s->obj.marked) {
            // Clear the slot — the string itself will be freed by sweep()
            t->entries[i] = NULL;
            t->count--;
        }
    }
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────

void nq_collect(VM* vm) {
    size_t before = nq_bytes_allocated;

    markRoots(vm);
    // Prune dead interned strings before sweep so sweep can safely free them
    removeWhiteStrings();
    sweep();

    // Grow threshold: 2x bytes surviving after collection
    nq_gc_threshold = nq_bytes_allocated * 2;
    if (nq_gc_threshold < 1024 * 1024)
        nq_gc_threshold = 1024 * 1024; // minimum 1 MB

    (void)before; // suppress unused warning in release builds
}

void nq_gc_maybe(VM* vm) {
    if (vm == NULL) return;
    if (nq_bytes_allocated > nq_gc_threshold)
        nq_collect(vm);
}
