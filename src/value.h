#ifndef NQ_VALUE_H
#define NQ_VALUE_H

#include "common.h"

// ─────────────────────────────────────────────
//  Forward declarations for heap-allocated types
// ─────────────────────────────────────────────
typedef struct Obj        Obj;
typedef struct ObjString  ObjString;
typedef struct ObjFunction ObjFunction;

// ─────────────────────────────────────────────
//  Value type tags
// ─────────────────────────────────────────────
typedef enum {
    VAL_NIL,      // nil
    VAL_BOOL,     // true / false
    VAL_NUMBER,   // 64-bit float (all numbers in Nolqu)
    VAL_OBJ,      // heap-allocated object (string, function, ...)
} ValueType;

// ─────────────────────────────────────────────
//  The Value — a tagged union
// ─────────────────────────────────────────────
typedef struct {
    ValueType type;
    union {
        bool   boolean;
        double number;
        Obj*   obj;
    } as;
} Value;

// ─────────────────────────────────────────────
//  Type check macros
// ─────────────────────────────────────────────
#define IS_NIL(v)       ((v).type == VAL_NIL)
#define IS_BOOL(v)      ((v).type == VAL_BOOL)
#define IS_NUMBER(v)    ((v).type == VAL_NUMBER)
#define IS_OBJ(v)       ((v).type == VAL_OBJ)

// ─────────────────────────────────────────────
//  Unbox macros
// ─────────────────────────────────────────────
#define AS_BOOL(v)      ((v).as.boolean)
#define AS_NUMBER(v)    ((v).as.number)
#define AS_OBJ(v)       ((v).as.obj)

// ─────────────────────────────────────────────
//  Box macros — create a Value from a raw type
// ─────────────────────────────────────────────
#define NIL_VAL           ((Value){VAL_NIL,    {.number = 0}})
#define BOOL_VAL(b)       ((Value){VAL_BOOL,   {.boolean = (b)}})
#define NUMBER_VAL(n)     ((Value){VAL_NUMBER, {.number  = (n)}})
#define OBJ_VAL(o)        ((Value){VAL_OBJ,    {.obj = (Obj*)(o)}})

// ─────────────────────────────────────────────
//  Resizable array of Values (used for constants)
// ─────────────────────────────────────────────
typedef struct {
    int    capacity;
    int    count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* arr);
void writeValueArray(ValueArray* arr, Value v);
void freeValueArray(ValueArray* arr);

void  printValue(Value v);
bool  valuesEqual(Value a, Value b);
bool  isTruthy(Value v);
const char* valueTypeName(ValueType t);

#endif // NQ_VALUE_H
