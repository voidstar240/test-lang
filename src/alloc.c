#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "alloc.h"

static void* malloc_wrapper(Alloc* alloc, size_t size) {
    return malloc(size);
}

static void* realloc_wrapper(Alloc* alloc, void* old, size_t old_size, size_t new_size) {
    return realloc(old, new_size);
}

static void free_wrapper(Alloc* alloc, void* ptr, size_t size) {
    free(ptr);
}

Alloc alloc_sys_init(void) {
    return (Alloc) {
        .alloc = malloc_wrapper,
        .realloc = realloc_wrapper,
        .dealloc = free_wrapper,
    };
}

static inline bool arena_is_last(ArenaAlloc* arena, void* ptr, size_t size) {
    return ((char*)ptr + size) >= (char*)arena->next;
}

static void* arena_alloc_inner(Alloc* alloc, size_t size) {
    ArenaAlloc* arena = (ArenaAlloc*)alloc;
    const size_t used = (size_t)((char*)arena->next - (char*)arena->start);
    assert((used + size) <= arena->capacity); // TODO cant fit return null and report error
    void* block = arena->next;
    arena->next = (void*)((char*)arena->next + size);
    return block;
}

static void* arena_realloc_inner(Alloc* alloc, void* old, size_t old_size, size_t new_size) {
    ArenaAlloc* arena = (ArenaAlloc*)alloc;
    if (arena_is_last(arena, old, old_size)) {
        // allocation is at the end it can be extended if there is space
        const size_t used = (size_t)((char*)old - (char*)arena->start);
        assert((used + new_size) <= arena->capacity); // TODO cant fit return null and report error
        return old;
    }
    void* new_block = arena_alloc_inner(alloc, new_size); // TODO check if null
    memcpy(new_block, old, old_size);
    return new_block;
}

static void arena_dealloc_inner(Alloc* alloc, void* ptr, size_t size) {
    ArenaAlloc* arena = (ArenaAlloc*)alloc;
    if (!arena_is_last(arena, ptr, size)) { return; }
    // allocation is at the end it can be reused
    arena->next = ptr;
}

ArenaAlloc alloc_arena_create(Alloc* alloc, size_t capacity) {
    void* block = alloc->alloc(alloc, capacity);
    return (ArenaAlloc) {
        ._alloc = (Alloc) {
            .alloc = arena_alloc_inner,
            .realloc = arena_realloc_inner,
            .dealloc = arena_dealloc_inner,
        },
        .parent = alloc,
        .start = block,
        .next = block,
        .capacity = capacity,
    };
}

void alloc_arena_destroy(ArenaAlloc* arena) {
    arena->parent->dealloc(arena->parent, arena->start, arena->capacity);
}
