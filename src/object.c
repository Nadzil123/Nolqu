#include "object.h"
#include "memory.h"
#include "chunk.h"

// ─────────────────────────────────────────────
//  Global object tracking (for GC)
// ─────────────────────────────────────────────
Obj* nq_all_objects = NULL;

// ─────────────────────────────────────────────
//  String interning table (global singleton)
// ─────────────────────────────────────────────
StringTable nq_string_table;

static uint32_t hashString(const char* key, int length) {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

void initStringTable(StringTable* t) {
    t->entries  = NULL;
    t->count    = 0;
    t->capacity = 0;
}

void freeStringTable(StringTable* t) {
    FREE_ARRAY(ObjString*, t->entries, t->capacity);
    initStringTable(t);
}

ObjString* tableFindString(StringTable* t, const char* chars, int len, uint32_t hash) {
    if (t->capacity == 0) return NULL;
    uint32_t idx = hash % (uint32_t)t->capacity;
    for (;;) {
        ObjString* entry = t->entries[idx];
        if (entry == NULL)  return NULL;
        if (entry->length == len &&
            entry->hash   == hash &&
            memcmp(entry->chars, chars, (size_t)len) == 0) {
            return entry;
        }
        idx = (idx + 1) % (uint32_t)t->capacity;
    }
}

void tableAddString(StringTable* t, ObjString* s) {
    // Grow if needed
    if (t->count + 1 > t->capacity * 3 / 4) {
        int old_cap = t->capacity;
        t->capacity = GROW_CAPACITY(old_cap);
        ObjString** new_entries = NQ_ALLOC(ObjString*, t->capacity);
        memset(new_entries, 0, sizeof(ObjString*) * (size_t)t->capacity);
        // Rehash
        for (int i = 0; i < old_cap; i++) {
            if (t->entries[i]) {
                uint32_t idx = t->entries[i]->hash % (uint32_t)t->capacity;
                while (new_entries[idx]) {
                    idx = (idx + 1) % (uint32_t)t->capacity;
                }
                new_entries[idx] = t->entries[i];
            }
        }
        FREE_ARRAY(ObjString*, t->entries, old_cap);
        t->entries = new_entries;
    }

    uint32_t idx = s->hash % (uint32_t)t->capacity;
    while (t->entries[idx] && t->entries[idx] != s) {
        idx = (idx + 1) % (uint32_t)t->capacity;
    }
    if (!t->entries[idx]) {
        t->entries[idx] = s;
        t->count++;
    }
}

// ─────────────────────────────────────────────
//  Allocate a base Obj + extra bytes
// ─────────────────────────────────────────────
static Obj* allocObject(size_t size, ObjType type) {
    Obj* obj = (Obj*)nq_realloc(NULL, 0, size);
    obj->type   = type;
    obj->marked = false;
    // Prepend to global object list
    obj->next       = nq_all_objects;
    nq_all_objects  = obj;
    return obj;
}

#define ALLOC_OBJ(type, obj_type) \
    (type*)allocObject(sizeof(type), obj_type)

// ─────────────────────────────────────────────
//  String creation
// ─────────────────────────────────────────────
static ObjString* allocString(char* chars, int length, uint32_t hash) {
    // Check interning table first
    ObjString* interned = tableFindString(&nq_string_table, chars, length, hash);
    if (interned) {
        // Already interned — free the incoming buffer if we own it
        free(chars);
        return interned;
    }

    ObjString* s = ALLOC_OBJ(ObjString, OBJ_STRING);
    s->chars  = chars;
    s->length = length;
    s->hash   = hash;
    tableAddString(&nq_string_table, s);
    return s;
}

// copyString — we copy the characters (we don't own the source)
ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    // Check if already interned before allocating
    ObjString* interned = tableFindString(&nq_string_table, chars, length, hash);
    if (interned) return interned;

    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(length + 1));
    memcpy(buf, chars, (size_t)length);
    buf[length] = '\0';
    return allocString(buf, length, hash);
}

// takeString — we take ownership of the character buffer
ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    return allocString(chars, length, hash);
}

// ─────────────────────────────────────────────
//  Function creation
// ─────────────────────────────────────────────
ObjFunction* newFunction(void) {
    ObjFunction* fn = ALLOC_OBJ(ObjFunction, OBJ_FUNCTION);
    fn->arity = 0;
    fn->name  = NULL;
    fn->chunk = NQ_ALLOC(Chunk, 1);
    initChunk(fn->chunk);
    return fn;
}

// ─────────────────────────────────────────────
//  printObject
// ─────────────────────────────────────────────
void printObject(Value v) {
    switch (OBJ_TYPE(v)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(v));
            break;
        case OBJ_FUNCTION: {
            ObjFunction* fn = AS_FUNCTION(v);
            if (fn->name) {
                printf("<fungsi %s>", fn->name->chars);
            } else {
                printf("<skrip>");
            }
            break;
        }
    }
}

// ─────────────────────────────────────────────
//  Object deallocation
// ─────────────────────────────────────────────
void freeObject(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* s = (ObjString*)obj;
            nq_realloc(s->chars, (size_t)(s->length + 1), 0);
            nq_realloc(obj, sizeof(ObjString), 0);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            freeChunk(fn->chunk);
            nq_realloc(fn->chunk, sizeof(Chunk), 0);
            nq_realloc(obj, sizeof(ObjFunction), 0);
            break;
        }
    }
}

void freeAllObjects(void) {
    Obj* obj = nq_all_objects;
    while (obj) {
        Obj* next = obj->next;
        freeObject(obj);
        obj = next;
    }
    nq_all_objects = NULL;
}

const char* objectTypeName(ObjType t) {
    switch (t) {
        case OBJ_STRING:   return "teks";
        case OBJ_FUNCTION: return "fungsi";
        default:           return "objek";
    }
}
