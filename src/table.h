#ifndef NQ_TABLE_H
#define NQ_TABLE_H

#include "common.h"
#include "value.h"
#include "object.h"

// ─────────────────────────────────────────────
//  Hash table entry (open addressing)
// ─────────────────────────────────────────────
typedef struct {
    ObjString* key;    // NULL = empty slot
    Value      value;
} Entry;

// ─────────────────────────────────────────────
//  Hash table
// ─────────────────────────────────────────────
typedef struct {
    int    count;
    int    capacity;
    Entry* entries;
} Table;

void  initTable(Table* t);
void  freeTable(Table* t);
bool  tableGet(Table* t, ObjString* key, Value* out);
bool  tableSet(Table* t, ObjString* key, Value value);
bool  tableDelete(Table* t, ObjString* key);
void  tableCopy(Table* from, Table* to);

#endif // NQ_TABLE_H
