#ifndef SYMTABLE_H
#define SYMTABLE_H

typedef struct SymOccurrence {
    int line, col;
    struct SymOccurrence* next;
} SymOccurrence;

typedef struct SymTableEntry {
    char *name;
    int  line;
    int  col;
    int n_occurrences;
    SymOccurrence* occurrences;
    struct SymTableEntry* next;
} SymTableEntry;

/* Insert 'name' if not already present; increment occurrences otherwise.
 * Calling this function with uninitialized tables is OK. */
void sym_insert(SymTableEntry** table, const char *name, int line, int col);

/* Look up 'name'; returns NULL if not found. */
SymTableEntry* sym_lookup(SymTableEntry* table, const char *name);

/* Print every entry in the table */
void sym_print_all(SymTableEntry* table);

/* Free all memory used by the table. */
void sym_free(SymTableEntry** table);

#endif /* SYMTABLE_H */
