#include "memory.h"

size_t nq_bytes_allocated = 0;
size_t nq_gc_threshold    = 1024 * 1024; // 1 MB initial threshold

void* nq_realloc(void* ptr, size_t old_size, size_t new_size) {
    nq_bytes_allocated += new_size;
    if (old_size <= nq_bytes_allocated)
        nq_bytes_allocated -= old_size;
    else
        nq_bytes_allocated = 0;

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* result = realloc(ptr, new_size);
    if (result == NULL) {
        fprintf(stderr,
            NQ_COLOR_RED "[nolqu] " NQ_COLOR_RESET
            "Out of memory! Could not allocate %zu bytes.\n",
            new_size);
        exit(1);
    }
    return result;
}
