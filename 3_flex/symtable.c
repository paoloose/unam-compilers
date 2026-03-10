#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "symtable.h"

void sym_insert(SymTableEntry** table, const char *name, int line, int col) {
    assert(table != NULL);

    SymTableEntry* curr = *table;

    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->occurrences++;
            return;
        }

        if (curr->next == NULL) break;
        curr = curr->next;
    }

    SymTableEntry* new_entry = malloc(sizeof(SymTableEntry));
    new_entry->col = col;
    new_entry->line = line;
    new_entry->name = strdup(name);
    new_entry->occurrences = 1;
    new_entry->next = NULL;

    if (curr == NULL) {
        *table = new_entry;
    } else {
        curr->next = new_entry;
    }
}

SymTableEntry* sym_lookup(SymTableEntry* table, const char *name) {
    assert(table != NULL);

    SymTableEntry* last_entry = table;

    while (last_entry) {
        if (strcmp(last_entry->name, name) == 0) {
            return last_entry;
        }
        last_entry = last_entry->next;
    }

    return NULL;
}

void sym_print_all(SymTableEntry* table) {
    assert(table != NULL);

    SymTableEntry* last_entry = table;

    printf("\n");

    printf("--------------\n");
    printf("Symbols table\n");
    printf("--------------\n");

    while (last_entry) {
        printf("%s: %d occurrences\n", last_entry->name, last_entry->occurrences);
        last_entry = last_entry->next;
    }
}

void sym_free(SymTableEntry** table) {
    assert(table != NULL);

    SymTableEntry* last_entry = *table;

    while (last_entry) {
        SymTableEntry* next_direction = last_entry->next;
        free(last_entry->name);
        free(last_entry);
        last_entry = next_direction;
    }

    *table = NULL;
}
