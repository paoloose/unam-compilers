#include "grammar.h"

#include <ctype.h>

static symbol *get_symbols_from_line(const char *symbols_line, int *symbols_count, bool is_terminal);
static production get_production_from_line(const char *production_line, grammar *g);

/**
 * @brief Trims leading and trailing whitespace from a mutable token.
 * @param text Token text to trim in-place.
 * @return Pointer to the first non-whitespace character in text.
 */
static char *trim_token(char *text)
{
    if (text == NULL)
    {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text))
    {
        text++;
    }

    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1]))
    {
        text[--length] = '\0';
    }

    return text;
}

/**
 * @brief Computes a deterministic hash for a symbol string.
 * @param str Input string.
 * @return Hash value for lookup indexing.
 */
static unsigned long hash_symbol(const char *str)
{
    // Using the djb2 hash function for strings
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != 0)
    {
        hash = ((hash << 5) + hash) + (unsigned long)c;
    }

    return hash;
}

/**
 * @brief Computes the next power of two greater than or equal to value.
 * @param value Minimum required capacity.
 * @return Power-of-two capacity.
 */
static int next_power_of_two(int value)
{
    int capacity = 1;

    while (capacity < value)
    {
        capacity <<= 1;
    }

    return capacity;
}

/**
 * @brief Creates an open-addressed hash table for fast symbol lookup.
 * @param symbols Symbol array to index.
 * @param symbols_count Number of symbols.
 * @return Initialized hash table; empty table on error.
 */
static symbol_hash_table create_symbol_hash_table(symbol *symbols, int symbols_count)
{
    symbol_hash_table table;
    table.entries = NULL;
    table.capacity = 0;

    if (symbols == NULL || symbols_count <= 0)
    {
        return table;
    }

    int capacity = next_power_of_two(symbols_count * 2);
    table.entries = (symbol_hash_entry *)calloc((size_t)capacity, sizeof(symbol_hash_entry));
    table.capacity = capacity;

    if (table.entries == NULL)
    {
        table.capacity = 0;
        return table;
    }

    for (int i = 0; i < symbols_count; i++)
    {
        unsigned long hash = hash_symbol(symbols[i].symbol);
        int index = (int)(hash & (unsigned long)(capacity - 1));

        while (table.entries[index].occupied)
        {
            index = (index + 1) & (capacity - 1);
        }

        table.entries[index].key = symbols[i].symbol;
        table.entries[index].id = i;
        table.entries[index].occupied = true;
    }

    return table;
}

/**
 * @brief Resolves a symbol id using a prebuilt hash table.
 * @param symbol_str Symbol text.
 * @param table Hash table containing symbol->id mapping.
 * @return Symbol id if found, otherwise -1.
 */
static int get_symbol_id_from_hash(const char *symbol_str, const symbol_hash_table *table)
{
    if (symbol_str == NULL || table == NULL || table->entries == NULL || table->capacity <= 0)
    {
        return -1;
    }

    int capacity = table->capacity;
    unsigned long hash = hash_symbol(symbol_str);
    int index = (int)(hash & (unsigned long)(capacity - 1));
    int start_index = index;

    while (table->entries[index].occupied)
    {
        if (strcmp(symbol_str, table->entries[index].key) == 0)
        {
            return table->entries[index].id;
        }

        index = (index + 1) & (capacity - 1);
        if (index == start_index)
        {
            break;
        }
    }

    return -1;
}

/**
 * @brief Parses a textual grammar into symbols, indices, and productions.
 * @param grammar_file_content Full grammar text.
 * @return Allocated grammar instance, or NULL on failure.
 */
grammar *create_grammar(const char *grammar_file_content)
{
    if (grammar_file_content == NULL)
    {
        return NULL;
    }

    // strtok modifies the input buffer, so parse from a mutable copy.
    char *grammar_copy = strdup(grammar_file_content);
    if (grammar_copy == NULL)
    {
        return NULL;
    }

    // Split the grammar file content into lines
    char *lines[MAX_PRODUCTIONS + 2];
    int num_lines = 0;
    char *line = strtok(grammar_copy, "\n");
    while (line != NULL && num_lines < MAX_PRODUCTIONS + 2)
    {
        lines[num_lines++] = line;
        line = strtok(NULL, "\n");
    }

    // Create a grammar object
    grammar *g = (grammar *)calloc(1, sizeof(grammar));
    if (g == NULL)
    {
        free(grammar_copy);
        return NULL;
    }

    if (num_lines < 2)
    {
        free(grammar_copy);
        return g;
    }

    // Get non-terminals
    g->non_terminals = get_symbols_from_line(lines[0], &g->num_non_terminals, false);
    // Get terminals
    g->terminals = get_symbols_from_line(lines[1], &g->num_terminals, true);

    // Build hash tables for O(1) average symbol lookup.
    g->non_terminal_index = create_symbol_hash_table(g->non_terminals, g->num_non_terminals);
    g->terminal_index = create_symbol_hash_table(g->terminals, g->num_terminals);

    // Get productions
    g->productions = (production *)malloc(MAX_PRODUCTIONS * sizeof(production));
    g->num_productions = 0;
    for (int i = 2; i < num_lines && g->num_productions < MAX_PRODUCTIONS; i++)
    {
        g->productions[g->num_productions] = get_production_from_line(lines[i], g);
        g->num_productions++;
    }

    free(grammar_copy);

    return g;
}

/**
 * @brief Parses a symbol declaration line into a symbol array.
 * @param symbols_line Source line (for example terminals/non-terminals line).
 * @param symbols_count Output number of parsed symbols.
 * @param is_terminal Terminal flag to assign to parsed symbols.
 * @return Allocated symbol array, or NULL on failure.
 */
static symbol *get_symbols_from_line(const char *symbols_line, int *symbols_count, bool is_terminal)
{
    if (symbols_line == NULL || symbols_count == NULL)
    {
        return NULL;
    }

    // Copy the line for counting tokens.
    char *count_copy = strdup(symbols_line);
    if (count_copy == NULL)
    {
        return NULL;
    }

    // Split the symbols line into symbols
    char *token = strtok(count_copy, " ");
    int count = 0;

    // Count the number of symbols
    while (token != NULL)
    {
        char *trimmed = trim_token(token);
        if (trimmed != NULL && trimmed[0] != '\0')
        {
            count++;
        }
        token = strtok(NULL, " ");
    }

    free(count_copy);

    // Allocate memory for the symbols array
    *symbols_count = count - 1; // Subtract 1 for the "Non-terminals:" or "Terminals:" prefix
    if (*symbols_count < 0)
    {
        *symbols_count = 0;
    }
    symbol *symbols = (symbol *)malloc(count * sizeof(symbol));
    if (symbols == NULL)
    {
        *symbols_count = 0;
        return NULL;
    }

    // Copy the line again because strtok already modified the first copy.
    char *fill_copy = strdup(symbols_line);
    if (fill_copy == NULL)
    {
        free(symbols);
        *symbols_count = 0;
        return NULL;
    }

    token = strtok(fill_copy, " ");
    int symbol_index = 0;
    int non_empty_index = 0;

    // Fill the symbols array skipping the first token which is the prefix
    while (token != NULL)
    {
        char *trimmed = trim_token(token);
        if (trimmed != NULL && trimmed[0] != '\0')
        {
            // Skip first non-empty token (header prefix) and keep the rest as symbols.
            if (non_empty_index > 0 && symbol_index < *symbols_count)
            {
                symbols[symbol_index].symbol = strdup(trimmed);
                symbols[symbol_index].symbol_length = (int)strlen(trimmed);
                symbols[symbol_index].is_terminal = is_terminal;
                symbol_index++;
            }
            non_empty_index++;
        }

        token = strtok(NULL, " ");
    }

    free(fill_copy);

    return symbols;
}

/**
 * @brief Parses one production line into encoded production ids.
 * @param production_line Source production line.
 * @param g Grammar context for symbol resolution.
 * @return Parsed production structure.
 */
static production get_production_from_line(const char *production_line, grammar *g)
{
    // Copy the production line to avoid modifying the original string
    char *production_line_duplicate = strdup(production_line);

    // Split the production line into parts
    char *token = strtok((char *)production_line_duplicate, " ");
    if (token == NULL)
    {
        production empty = {0};
        empty.non_terminal_id = -1;
        empty.production_symbol_ids = (int *)calloc(1, sizeof(int));
        empty.production_length = 0;
        free(production_line_duplicate);
        return empty;
    }

    char *lhs = trim_token(token);
    int non_terminal_id = get_symbol_id_from_hash(lhs, &g->non_terminal_index);

    // Production symbols are stored as encoded ids:
    // terminals [0..T-1], non-terminals [T..T+N-1].
    int *production_symbol_ids = (int *)malloc(MAX_PRODUCTIONS * sizeof(int));
    int production_length = 0;

    // Get the production symbols
    token = strtok(NULL, " ");
    while (token != NULL)
    {
        char *trimmed = trim_token(token);
        if (trimmed == NULL || trimmed[0] == '\0')
        {
            token = strtok(NULL, " ");
            continue;
        }

        if (strcmp(trimmed, "->") == 0)
        {
            token = strtok(NULL, " ");
            continue;
        }

        int symbol_id = get_symbol_id_from_hash(trimmed, &g->terminal_index);
        if (symbol_id != -1)
        {
            production_symbol_ids[production_length++] = symbol_id;
        }
        else
        {
            symbol_id = get_symbol_id_from_hash(trimmed, &g->non_terminal_index);
            if (symbol_id != -1)
            {
                // Non-terminals are encoded after terminals.
                production_symbol_ids[production_length++] = g->num_terminals + symbol_id;
            }
        }
        token = strtok(NULL, " ");
    }

    // Create a production object
    production p;
    p.non_terminal_id = non_terminal_id;
    p.production_symbol_ids = production_symbol_ids;
    p.production_length = production_length;

    // Free the copied line
    free(production_line_duplicate);

    return p;
}

/**
 * @brief Finds a symbol id by linear scan.
 * @param symbol_str Symbol text to locate.
 * @param symbols Symbol array to search.
 * @param symbols_count Number of symbols in the array.
 * @return Symbol id if found, otherwise -1.
 */
int get_symbol_id(const char *symbol_str, symbol *symbols, int symbols_count)
{
    if (symbol_str == NULL || symbols == NULL || symbols_count <= 0)
    {
        return -1;
    }

    size_t symbol_len = strlen(symbol_str);
    char first_char = symbol_str[0];

    for (int i = 0; i < symbols_count; i++)
    {
        if ((size_t)symbols[i].symbol_length != symbol_len)
        {
            continue;
        }
        if (symbols[i].symbol[0] != first_char)
        {
            continue;
        }
        if (strcmp(symbol_str, symbols[i].symbol) == 0)
        {
            return i;
        }
    }

    return -1; // Symbol not found
}

/**
 * @brief Prints non-terminals, terminals, and productions.
 * @param g Grammar instance to print.
 * @return This function does not return a value.
 */
void print_grammar(const grammar *g)
{
    if (g == NULL)
    {
        return;
    }

    printf("Non-terminals:\n");
    for (int i = 0; i < g->num_non_terminals; i++)
    {
        printf("  %s\n", g->non_terminals[i].symbol);
    }

    printf("Terminals:\n");
    for (int i = 0; i < g->num_terminals; i++)
    {
        printf("  %s\n", g->terminals[i].symbol);
    }

    printf("Productions:\n");
    for (int i = 0; i < g->num_productions; i++)
    {
        production p = g->productions[i];
        printf("  %s ->", g->non_terminals[p.non_terminal_id].symbol);
        for (int j = 0; j < p.production_length; j++)
        {
            int symbol_id = p.production_symbol_ids[j];
            if (symbol_id >= 0 && symbol_id < g->num_terminals)
            {
                printf(" %s", g->terminals[symbol_id].symbol);
            }
            else if (symbol_id >= g->num_terminals && symbol_id < g->num_terminals + g->num_non_terminals)
            {
                printf(" %s", g->non_terminals[symbol_id - g->num_terminals].symbol);
            }
        }
        printf("\n");
    }
}