#pragma once
#include "ast.h"
#include "analyzer.h"

/*
    Usage:

    create_da(int, int)

    da_int arr = {0};
    da_int_init(&arr, 24);

    if (!da_int_append(&arr, 42)) {
        // handle OOM
    }

    int value;
    if (da_int_pop(&arr, &value)) {
        printf("%d\n", value);
    }

    da_free(&arr);
*/

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define DA_INITIAL_CAPACITY 8

#define create_da(TYPE, NAME)                                                   \
typedef struct {                                                                \
    size_t capacity;                                                            \
    size_t length;                                                              \
    size_t reverse_start;                                                       \
    bool is_reverse_mode;                                                       \
    TYPE *data;                                                                 \
} da_##NAME;                                                                    \
                                                                                \
static inline bool da_##NAME##_reserve(da_##NAME *da, size_t new_cap) {         \
    if (new_cap <= da->capacity)                                                \
        return true;                                                            \
                                                                                \
    if (new_cap > SIZE_MAX / sizeof(TYPE))                                      \
        return false;                                                           \
                                                                                \
    TYPE *tmp;                                                                  \
    if (da->data == NULL) {                                                     \
        tmp = calloc(new_cap, sizeof(TYPE));                                    \
    } else {                                                                    \
        tmp = (TYPE *)realloc(da->data, new_cap * sizeof(TYPE));                \
    }                                                                           \
    if (!tmp)                                                                   \
        return false;                                                           \
                                                                                \
    da->data = tmp;                                                             \
    da->capacity = new_cap;                                                     \
    return true;                                                                \
}                                                                               \
                                                                                \
static inline void da_##NAME##_init(da_##NAME *da, size_t cap) {                \
    da->capacity = 0;                                                           \
    da->length = 0;                                                             \
    da->reverse_start = 0;                                                      \
    da->is_reverse_mode = false;                                                \
    da->data = NULL;                                                            \
    da_##NAME##_reserve(da, cap);                                               \
}                                                                               \
                                                                                \
static inline bool da_##NAME##_grow(da_##NAME *da) {                            \
    size_t new_cap = da->capacity ? da->capacity * 2 : DA_INITIAL_CAPACITY;     \
                                                                                \
    if (new_cap < da->capacity)                                                 \
        return false; /* overflow */                                            \
                                                                                \
    return da_##NAME##_reserve(da, new_cap);                                    \
}                                                                               \
                                                                                \
static inline bool da_##NAME##_append(da_##NAME *da, TYPE item) {               \
    if (da->length == da->capacity) {                                           \
        if (!da_##NAME##_grow(da))                                              \
            return false;                                                       \
    }                                                                           \
                                                                                \
    da->data[da->length++] = item;                                              \
    return true;                                                                \
}                                                                               \
                                                                                \
static inline void da_##NAME##_reverse_mode_start(da_##NAME *da) {              \
    if (!da->is_reverse_mode) {                                                 \
        da->is_reverse_mode = true;                                             \
        da->reverse_start = da->length;                                         \
    }                                                                           \
}                                                                               \
                                                                                \
static inline void da_##NAME##_reverse_mode_end(da_##NAME *da) {                \
    if (da->is_reverse_mode) {                                                  \
        size_t start = da->reverse_start;                                       \
        size_t end = da->length > 0 ? da->length - 1 : 0;                       \
        while (start < end) {                                                   \
            TYPE temp = da->data[start];                                        \
            da->data[start] = da->data[end];                                    \
            da->data[end] = temp;                                               \
            start++;                                                            \
            end--;                                                              \
        }                                                                       \
        da->is_reverse_mode = false;                                            \
    }                                                                           \
}                                                                               \
                                                                                \
static inline TYPE *da_##NAME##_peek_last(da_##NAME *da) {                      \
    if (!da || da->length == 0)                                                 \
        return NULL;                                                            \
                                                                                \
    return &da->data[da->length - 1];                                           \
}                                                                               \
                                                                                \
static inline bool da_##NAME##_pop(da_##NAME *da, TYPE *out) {                  \
    if (!da || da->length == 0)                                                 \
        return false;                                                           \
    da->length--;                                                               \
    if (out) *out = da->data[da->length];                                       \
    return true;                                                                \
}                                                                               \
                                                                                \
static inline void da_##NAME##_clear(da_##NAME *da) {                           \
    if (da)                                                                     \
        da->length = 0;                                                         \
}                                                                               \

#define da_free(DA_PTR) \
do { \
    if ((DA_PTR) != NULL) { \
        free((DA_PTR)->data); \
        (DA_PTR)->data = NULL; \
        (DA_PTR)->length = 0; \
        (DA_PTR)->capacity = 0; \
        (DA_PTR)->is_reverse_mode = false; \
        (DA_PTR)->reverse_start = 0; \
    } \
} while (0)

/* Generic helper macros */
#define da_empty(DA_PTR) ((DA_PTR)->length == 0)
#define da_len(DA_PTR)   ((DA_PTR)->length)
#define da_cap(DA_PTR)   ((DA_PTR)->capacity)

/* Generate common types */
create_da(int, int)
create_da(float, float)
create_da(double, double)
create_da(char, char)
create_da(char*, cstr)
create_da(void*, pointer)
create_da(ASTNode*, astnodes)
create_da(AnalyzeFrame, analyze_frames)
create_da(SemanticDiagnostic, diagnostics)
