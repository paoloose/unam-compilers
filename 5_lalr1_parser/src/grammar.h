#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_PRODUCTIONS 100

typedef struct symbol
{
    char* symbol;
    int symbol_length;
    bool is_terminal;
} symbol;

typedef struct production
{
    int non_terminal_id;
    int* production_symbol_ids;
    int production_length;
} production;

typedef struct symbol_hash_entry
{
    const char* key;
    int id;
    bool occupied;
} symbol_hash_entry;

typedef struct symbol_hash_table
{
    symbol_hash_entry* entries;
    int capacity;
} symbol_hash_table;

typedef struct grammar
{
    symbol* non_terminals;
    symbol* terminals;
    production* productions;
    symbol_hash_table non_terminal_index;
    symbol_hash_table terminal_index;
    int num_non_terminals;
    int num_terminals;
    int num_productions;
} grammar;

/**
 * @brief Parses raw grammar text and builds an in-memory grammar structure.
 * @param grammar_file_content Full grammar file content as a null-terminated string.
 * @return Pointer to a newly allocated grammar object, or NULL on error.
 */
grammar* create_grammar(const char* grammar_file_content);

/**
 * @brief Prints grammar symbols and productions to stdout.
 * @param g Grammar to print.
 * @return This function does not return a value.
 */
void print_grammar(const grammar* g);

#endif // GRAMMAR_H
