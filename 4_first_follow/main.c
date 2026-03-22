#include "analyzer.h"

/**
 * @brief Reads complete stdin content into a dynamically allocated buffer.
 * @return Null-terminated buffer on success, or NULL when stdin is empty or on allocation error.
 */
static char *read_stdin_all(void)
{
    size_t capacity = 1024;
    size_t length = 0;
    char *buffer = (char *)malloc(capacity);
    if (buffer == NULL)
    {
        return NULL;
    }

    int ch;
    while ((ch = getchar()) != EOF)
    {
        if (length + 1 >= capacity)
        {
            size_t new_capacity = capacity * 2;
            char *resized = (char *)realloc(buffer, new_capacity);
            if (resized == NULL)
            {
                free(buffer);
                return NULL;
            }
            buffer = resized;
            capacity = new_capacity;
        }

        buffer[length++] = (char)ch;
    }

    if (length == 0)
    {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

/**
 * @brief Prints one FIRST/FOLLOW set row using a common display format.
 * @param prefix Label prefix such as "First" or "Follow".
 * @param non_terminal Non-terminal name that owns the set.
 * @param symbols Computed symbols for this set.
 * @param count Number of symbols in the array.
 * @return This function does not return a value.
 */
static void print_named_set(const char *prefix, const char *non_terminal, const symbol *symbols, int count)
{
    printf("%s(%s): {", prefix, non_terminal);

    bool first_item = true;
    for (int i = 0; i < count; i++)
    {
        if (!first_item)
        {
            printf(", ");
        }
        printf("%s", symbols[i].symbol);
        first_item = false;
    }

    printf("}\n");
}

/**
 * @brief Prints FIRST/FOLLOW sets for all non-terminals.
 * @param g Parsed grammar.
 * @return This function does not return a value.
 */
static void print_all_first_follow(const grammar *g)
{
    if (g == NULL || g->num_non_terminals <= 0)
    {
        return;
    }

    for (int i = 0; i < g->num_non_terminals; i++)
    {
        symbol *first_symbols = NULL;
        symbol *follow_symbols = NULL;

        int first_count = compute_first_for_non_terminal(g, i, &first_symbols);
        int follow_count = compute_follow_for_non_terminal(g, i, &follow_symbols);

        print_named_set("First", g->non_terminals[i].symbol, first_symbols, first_count);
        print_named_set("Follow", g->non_terminals[i].symbol, follow_symbols, follow_count);

        free_symbol_array(first_symbols, first_count);
        free_symbol_array(follow_symbols, follow_count);
    }
}

/**
 * @brief Program entry point. Reads grammar text from stdin and prints FIRST/FOLLOW sets.
 * @return 0 on success, non-zero on input or parsing failure.
 */
int main()
{
    char *grammar_file_content = read_stdin_all();
    if (grammar_file_content == NULL)
    {
        fprintf(stderr, "No grammar input provided on stdin.\n");
        return 1;
    }

    grammar *g = create_grammar(grammar_file_content);
    free(grammar_file_content);

    if (g == NULL)
    {
        fprintf(stderr, "Failed to create grammar.\n");
        return 1;
    }

    print_all_first_follow(g);

    // Grammar-free routine is not available in this codebase yet.

    return 0;
}
