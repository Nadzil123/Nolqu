#ifndef NQ_GC_H
#define NQ_GC_H

// Forward declaration — gc.c depends on VM for roots
struct VM;

// Run a full mark-and-sweep collection.
void nq_collect(struct VM* vm);

// Called from the allocator — trigger GC if threshold exceeded.
void nq_gc_maybe(struct VM* vm);

#endif // NQ_GC_H
