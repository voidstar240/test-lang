#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

#define arrlist_get(arr, Type, index) (((Type*)(arr)->data)[index])
#define arrlist_init(Type) ((ArrayList){ .data = NULL, .len = 0, .cap = 0, .elem_size = sizeof(Type) })
#define arrlist_push(arr, Type, val) (*(Type*)arrlist_append((arr), 1) = val)

typedef struct {
    void* data;
    size_t len;
    size_t cap;
    size_t elem_size;
} ArrayList;

// Frees the underlying data allocation.
static inline void arrlist_deinit(ArrayList* arr) {
    free(arr->data);
}

// Reserves another count bytes at the end of the array.
void arrlist_reserve(ArrayList* arr, size_t count);

// Allocates elem_size * count more bytes to the end of the array and returns a
// pointer to the allocated data.
void* arrlist_append(ArrayList* arr, size_t count);

// Shrinks the capacity of the ArrayList to the smallest power of 2 larger then
// length.
void arrlist_shrink(ArrayList* arr);

// Removes count elements from the ArrayList starting at index.
void arrlist_remove(ArrayList* arr, size_t index, size_t count);

// Shrinks the allocation of the data to the same size as the length and returns
// a pointer to it.
static inline void* arrlist_to_array(ArrayList* arr) {
    return realloc(arr->data, arr->len * arr->elem_size);
}
