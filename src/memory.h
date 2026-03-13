#ifndef NQ_MEMORY_H
#define NQ_MEMORY_H

#include "common.h"

// Core allocator — all memory goes through here.
// new_size == 0 means free.
void* nq_realloc(void* ptr, size_t old_size, size_t new_size);

// Bytes currently allocated (tracked for GC)
extern size_t nq_bytes_allocated;
extern size_t nq_gc_threshold;

// Convenience macros
#define NQ_ALLOC(type, count) \
    (type*)nq_realloc(NULL, 0, sizeof(type) * (count))

#define NQ_FREE(type, ptr) \
    nq_realloc((ptr), sizeof(type), 0)

#endif // NQ_MEMORY_H
