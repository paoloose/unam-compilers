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
    if (da->capacity <= da->length) { \
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
static inline TYPE* da_##NAME##_peek_last(const da_##NAME* da) { \
    if (!da->data || da->length == 0) return NULL; \
    return &da->data[da->length - 1]; \
} \
\
static inline TYPE* da_##NAME##_pop(da_##NAME* da) { \
    if (!da->data || da->length == 0) return NULL; \
    da->length--; \
    return &da->data[da->length]; \
}

#define da_free(__DA) \
    { \
        if ((__DA)->data) { \
            free((__DA)->data); \
            (__DA)->data = NULL; \
            (__DA)->capacity = 0; \
            (__DA)->length = 0; \
        } \
    }

#define da_empty(__DA) ((__DA)->length == 0)

#define da_drain(__DA) {(__DA)->length = 0;}

// Generate implementations for common types
create_da(int, int)
create_da(float, float)
create_da(double, double)
create_da(char, char)
create_da(nfa_fragment, nfa_fragment)
create_da(nfa_node*, nfa_node)
create_da(input_op, input_op)
create_da(regex_item, regex_item)
create_da(void*, pointer)
