#ifndef NQ_OBJECT_H
#define NQ_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

// ─────────────────────────────────────────────
//  Object type tags
// ─────────────────────────────────────────────
typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,     // built-in native function (new in 0.2.0)
} ObjType;

// ─────────────────────────────────────────────
//  Base object header
// ─────────────────────────────────────────────
struct Obj {
    ObjType     type;
    struct Obj* next;
    bool        marked;
};

// ─────────────────────────────────────────────
//  ObjString — interned immutable string
// ─────────────────────────────────────────────
struct ObjString {
    Obj      obj;
    int      length;
    char*    chars;
    uint32_t hash;
};

// ─────────────────────────────────────────────
//  ObjFunction — compiled Nolqu function
// ─────────────────────────────────────────────
typedef struct ObjFunction {
    Obj        obj;
    int        arity;
    Chunk*     chunk;
    ObjString* name;
} ObjFunction;

// ─────────────────────────────────────────────
//  ObjNative — built-in C function (new in 0.2.0)
// ─────────────────────────────────────────────
typedef Value (*NativeFn)(int argc, Value* args);

typedef struct {
    Obj      obj;
    NativeFn fn;
    const char* name;
    int      arity;  // -1 = variadic
} ObjNative;

// ─────────────────────────────────────────────
//  Type check macros
// ─────────────────────────────────────────────
#define OBJ_TYPE(v)       (AS_OBJ(v)->type)
#define IS_STRING(v)      (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_STRING)
#define IS_FUNCTION(v)    (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_FUNCTION)
#define IS_NATIVE(v)      (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_NATIVE)

#define AS_STRING(v)      ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v)     (((ObjString*)AS_OBJ(v))->chars)
#define AS_FUNCTION(v)    ((ObjFunction*)AS_OBJ(v))
#define AS_NATIVE(v)      ((ObjNative*)AS_OBJ(v))

// ─────────────────────────────────────────────
//  Global object list (for GC)
// ─────────────────────────────────────────────
extern Obj* nq_all_objects;

// ─────────────────────────────────────────────
//  String interning table
// ─────────────────────────────────────────────
typedef struct {
    ObjString** entries;
    int         count;
    int         capacity;
} StringTable;

extern StringTable nq_string_table;

void initStringTable(StringTable* t);
void freeStringTable(StringTable* t);
ObjString* tableFindString(StringTable* t, const char* chars, int len, uint32_t hash);
void tableAddString(StringTable* t, ObjString* s);

// ─────────────────────────────────────────────
//  Object creation
// ─────────────────────────────────────────────
ObjString*   copyString(const char* chars, int length);
ObjString*   takeString(char* chars, int length);
ObjFunction* newFunction(void);
ObjNative*   newNative(NativeFn fn, const char* name, int arity);

// ─────────────────────────────────────────────
//  Utilities
// ─────────────────────────────────────────────
void printObject(Value v);
void freeObject(Obj* obj);
void freeAllObjects(void);
const char* objectTypeName(ObjType t);

#endif // NQ_OBJECT_H
