#include "memory.h"

void* nq_realloc(void* ptr, size_t old_size, size_t new_size) {
    (void)old_size; // tracked for future GC

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* result = realloc(ptr, new_size);
    if (result == NULL) {
        fprintf(stderr,
            NQ_COLOR_RED "[nolqu] " NQ_COLOR_RESET
            "Keluar memori! Tidak dapat mengalokasikan %zu bytes.\n",
            new_size);
        exit(1);
    }
    return result;
}
