#pragma once

typedef struct {
    char value;
} regex_item;

typedef struct {
    int size;
    regex_item* items;
} regex;

regex parse_regex(const char* str) {
    regex r = {0};
    return r;
}
