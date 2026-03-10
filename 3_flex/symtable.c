#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "symtable.h"

void sym_insert(SymTableEntry** table, const char *name, int line, int col) {
    assert(table != NULL);

    SymTableEntry* curr_sym = *table;
    SymOccurrence* occ = malloc(sizeof(SymOccurrence));
    occ->line = line;
    occ->col = col;

    while (curr_sym) {
        if (strcmp(curr_sym->name, name) == 0) {
            // Found! We create a new ocurrence entry for this symbol
            SymOccurrence* curr_occ = curr_sym->occurrences;
            while (curr_occ) {
                if (curr_occ->next == NULL) break;
                curr_occ = curr_occ->next;
            }
            curr_occ->next = occ;
            curr_sym->n_occurrences++;
            return;
        }

        if (curr_sym->next == NULL) break;
        curr_sym = curr_sym->next;
    }

    // Not found. This is either a new name, or an unitialized table
    // Either way, we allocate memory for this new entry

    SymTableEntry* new_entry = malloc(sizeof(SymTableEntry));
    new_entry->col = col;
    new_entry->line = line;
    new_entry->name = strdup(name);
    new_entry->next = NULL;
    new_entry->n_occurrences = 1;
    new_entry->occurrences = occ;

    if (curr_sym == NULL) {
        *table = new_entry;
    } else {
        curr_sym->next = new_entry;
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
        printf("\n%s: %d occurrences\n", last_entry->name, last_entry->n_occurrences);
        for (SymOccurrence* occ = last_entry->occurrences; occ != NULL; occ = occ->next) {
            printf(" - line: %d, col: %d\n", occ->line, occ->col);
        }
        last_entry = last_entry->next;
    }
}

void sym_free(SymTableEntry** table) {
    assert(table != NULL);

    SymTableEntry* last_entry = *table;

    while (last_entry) {
        SymTableEntry* next_entry = last_entry->next;

        SymOccurrence* occ = last_entry->occurrences;
        while (occ) {
            SymOccurrence* next_ocurrence = occ->next;
            free(occ);
            occ = next_ocurrence;
        }

        free(last_entry->name);
        free(last_entry);
        last_entry = next_entry;
    }

    *table = NULL;
}
