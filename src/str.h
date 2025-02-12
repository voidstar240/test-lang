#ifndef STR_H
#define STR_H

#include <stddef.h>
#include <string.h>

#include "alloc.h"

typedef struct {
    const char* cstr; // this is null terminated
    size_t len; // not counting null terminator
} Str;

static inline Str str_lit(const char* cstr) {
    return (Str) {
        .cstr = cstr,
        .len = strlen(cstr),
    };
}

static inline Str str_alloc(Alloc* alloc, const char* bytes, size_t len) {
    char* alloc_bytes = (char*)alloc_alloc(alloc, len + 1);
    memcpy(alloc_bytes, bytes, len);
    alloc_bytes[len] = '\0';
    return (Str) {
        .cstr = alloc_bytes,
        .len = len,
    };
}

#endif
