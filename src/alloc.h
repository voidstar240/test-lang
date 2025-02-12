#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

// allocator interface
typedef struct Alloc {
    void* (*alloc)   (struct Alloc* alloc, size_t size);
    void* (*realloc) (struct Alloc* alloc, void* old, size_t old_size, size_t new_size);
    void  (*dealloc) (struct Alloc* alloc, void* ptr, size_t size);
} Alloc;

static inline void* alloc_alloc(Alloc* alloc, size_t size) {
    return alloc->alloc(alloc, size);
}

static inline void* alloc_realloc(Alloc* alloc, void* old, size_t old_size, size_t new_size) {
    return alloc->realloc(alloc, old, old_size, new_size);
}

static inline void alloc_dealloc(Alloc* alloc, void* ptr, size_t size) {
    alloc->dealloc(alloc, ptr, size);
}

Alloc alloc_sys_init(void);

typedef struct {
    Alloc _alloc;
    Alloc* parent;
    void* start;
    void* next;
    size_t capacity;
} ArenaAlloc;

ArenaAlloc alloc_arena_init(Alloc* alloc, size_t capacity);
void alloc_arena_deinit(ArenaAlloc* arena);

#endif
