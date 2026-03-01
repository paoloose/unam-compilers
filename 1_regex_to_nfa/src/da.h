#pragma once
#include <stdlib.h>
#include <string.h>
#include "result.h"
#include "types.h"

#define DA_INITIAL_CAPACITY 8

#define create_da(TYPE, NAME) \
typedef struct { \
    int capacity; \
    int length; \
    TYPE* data; \
} da_##NAME; \
\
static inline void da_##NAME##_init(da_##NAME* da, int initial_capacity) { \
    if (da->data) { \
        free(da->data); \
    } \
    da->data = (TYPE*)calloc(initial_capacity, sizeof(TYPE)); \
    da->capacity = initial_capacity; \
    da->length = 0; \
} \
\
static inline void da_##NAME##_append(da_##NAME* da, TYPE item) { \
    if (!da->data) { \
        da_##NAME##_init(da, DA_INITIAL_CAPACITY); \
    } \
    \
    if (da->capacity == da->length) { \
        TYPE* old_data = da->data; \
        TYPE* new_data = (TYPE*)calloc(da->capacity * 2, sizeof(TYPE)); \
        memcpy(new_data, old_data, sizeof(TYPE) * da->length); \
        free(old_data); \
        da->capacity *= 2; \
        da->data = new_data; \
    } \
    \
    da->data[da->length++] = item; \
} \
\
static inline TYPE* da_##NAME##_peek_last(da_##NAME* da) { \
    if (!da->data || da->length == 0) return NULL; \
    return &da->data[da->length - 1]; \
} \
\
static inline TYPE* da_##NAME##_pop(da_##NAME* da) { \
    if (!da->data || da->length == 0) return NULL; \
    da->length--; \
    return &da->data[da->length]; \
} \
\
static inline void da_##NAME##_free(da_##NAME* da) { \
    if (da->data) { \
        free(da->data); \
        da->data = NULL; \
        da->capacity = 0; \
        da->length = 0; \
    } \
}

// Generate implementations for common types
create_da(int, int)
create_da(float, float)
create_da(double, double)
create_da(char, char)
create_da(nfa_fragment, nfa_fragment)
