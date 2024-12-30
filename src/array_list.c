#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "array_list.h"

static void realloc_cap(ArrayList* arr, size_t cap) {
    // will always reserve a power of 2 >= cap
    uint8_t log = 0;
    while (cap > 1) {
        cap /= 2;
        log++;
    }
    cap = (size_t)1 << log;
    arr->cap = cap;
    arr->data = realloc(arr->data, arr->cap * arr->elem_size);
}

void arrlist_reserve(ArrayList* arr, size_t count) {
    realloc_cap(arr, arr->len + count);
}

void* arrlist_append(ArrayList* arr, size_t count) {
    if (arr->len + count > arr->cap) {
        if (arr->len + count < 16) {
            realloc_cap(arr, 16);
        } else {
            realloc_cap(arr, arr->len + count);
        }
    }
    void* out = (char*)arr->data + (arr->len * arr->elem_size);
    arr->len += count; 
    return out;
}

void arrlist_shrink(ArrayList* arr) {
    if (arr->len == 0) {
        arr->cap = 0;
        free(arr->data);
        return;
    }
    realloc_cap(arr, arr->len);
}

void arrlist_remove(ArrayList* arr, size_t start, size_t count) {
    if (count == 0) { return; }
    if (start >= arr->len) { return; }

    if (start + count >= arr->len) {
        arr->len = start;
        return;
    }

    char* start_addr = (char*)arr->data + (start * arr->elem_size);
    const char* end_addr = (char*)arr->data + (start + count) * arr->elem_size;
    const size_t num_bytes = (arr->len - start - count) * arr->elem_size;
    memmove(start_addr, end_addr, num_bytes);
    arr->len -= count;
}
