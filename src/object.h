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
} ObjType;

// ─────────────────────────────────────────────
//  Base object header — every heap object starts with this
// ─────────────────────────────────────────────
struct Obj {
    ObjType   type;
    struct Obj* next;  // intrusive linked list for GC tracking
    bool      marked;  // GC mark bit
};

// ─────────────────────────────────────────────
//  ObjString — interned immutable string
// ─────────────────────────────────────────────
struct ObjString {
    Obj     obj;
    int     length;
    char*   chars;
    uint32_t hash;
};

// ─────────────────────────────────────────────
//  ObjFunction — compiled function
// ─────────────────────────────────────────────
typedef struct ObjFunction {
    Obj        obj;
    int        arity;     // number of parameters
    Chunk*     chunk;     // bytecode for this function
    ObjString* name;      // function name (NULL for script)
} ObjFunction;

// ─────────────────────────────────────────────
//  Type check macros for objects
// ─────────────────────────────────────────────
#define OBJ_TYPE(v)       (AS_OBJ(v)->type)
#define IS_STRING(v)      (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_STRING)
#define IS_FUNCTION(v)    (IS_OBJ(v) && OBJ_TYPE(v) == OBJ_FUNCTION)

#define AS_STRING(v)      ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v)     (((ObjString*)AS_OBJ(v))->chars)
#define AS_FUNCTION(v)    ((ObjFunction*)AS_OBJ(v))

// ─────────────────────────────────────────────
//  Global object list head (for GC)
// ─────────────────────────────────────────────
extern Obj* nq_all_objects;

// ─────────────────────────────────────────────
//  String interning table (global)
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
ObjString*   takeString(char* chars, int length);  // takes ownership
ObjFunction* newFunction(void);

// ─────────────────────────────────────────────
//  Object utilities
// ─────────────────────────────────────────────
void printObject(Value v);
void freeObject(Obj* obj);
void freeAllObjects(void);

const char* objectTypeName(ObjType t);

#endif // NQ_OBJECT_H
