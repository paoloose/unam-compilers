#include "grammar.h"
#include "automaton.h"
#include "parser.h"
#include "scanner.h"

#include <errno.h>

static bool has_suffix(const char *text, const char *suffix);

extern int yylex(void);
extern char *yytext;
extern FILE *yyin;

typedef struct token_stream
{
    int lexer_token;
    int terminal_id;
    const char *lexeme;
} token_stream;

typedef struct parser_stack
{
    int *states;
    int size;
    int capacity;
} parser_stack;

/**
 * @brief Reads complete stdin content into a dynamically allocated buffer.
 * @return Null-terminated buffer on success, or NULL when stdin is empty or on allocation error.
 */
static char *read_file_all(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0)
    {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)length + 1);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length)
    {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

/**
 * @brief Looks up a terminal id by symbol text.
 * @param g Parsed grammar.
 * @param symbol_name Name of terminal symbol.
 * @return Terminal id or -1 when not found.
 */
static int find_terminal_id(const grammar *g, const char *symbol_name)
{
    if (g == NULL || symbol_name == NULL)
    {
        return -1;
    }

    for (int i = 0; i < g->num_terminals; i++)
    {
        if (strcmp(g->terminals[i].symbol, symbol_name) == 0)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Maps lexer token ids to grammar terminal ids.
 * @param g Parsed grammar.
 * @param lexer_token Token returned by yylex().
 * @param lexeme Lexeme text from yytext.
 * @return Terminal id, g->num_terminals for EOF, or -1 if unmapped.
 */
static int map_lexer_token_to_terminal_id(const grammar *g, int lexer_token, const char *lexeme)
{
    if (g == NULL)
    {
        return -1;
    }

    if (lexer_token == TOK_EOF)
    {
        return g->num_terminals;
    }

    if (lexer_token == TOK_ERROR)
    {
        return -1;
    }

    // If grammar terminals are literal lexemes, yytext may directly match.
    int terminal_id = find_terminal_id(g, lexeme);
    if (terminal_id >= 0)
    {
        return terminal_id;
    }

    switch (lexer_token)
    {
    case TOK_IDENTIFIER:
        terminal_id = find_terminal_id(g, "IDENTIFIER");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "ID");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "id");
        return terminal_id;
    case TOK_INT_LITERAL:
        terminal_id = find_terminal_id(g, "INT_LITERAL");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "INT");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "num");
        return terminal_id;
    case TOK_FLOAT_LITERAL:
        terminal_id = find_terminal_id(g, "FLOAT_LITERAL");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "FLOAT");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "num");
        return terminal_id;
    case TOK_STRING_LITERAL:
        terminal_id = find_terminal_id(g, "STRING_LITERAL");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "STRING");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "str");
        return terminal_id;
    case TOK_CHAR_LITERAL:
        terminal_id = find_terminal_id(g, "CHAR_LITERAL");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "CHAR");
        if (terminal_id < 0) terminal_id = find_terminal_id(g, "char_lit");
        return terminal_id;
    default:
        break;
    }

    // Fallback for lexers that return punctuation as character codes.
    if (lexer_token > 0 && lexer_token <= 127)
    {
        char one_char_symbol[2] = {(char)lexer_token, '\0'};
        terminal_id = find_terminal_id(g, one_char_symbol);
        if (terminal_id >= 0)
        {
            return terminal_id;
        }
    }

    return -1;
}

/**
 * @brief Initializes parser stack with state 0.
 * @param stack Target stack.
 * @return true on success, false on allocation failure.
 */
static bool init_parser_stack(parser_stack *stack)
{
    if (stack == NULL)
    {
        return false;
    }

    stack->capacity = 64;
    stack->size = 0;
    stack->states = (int *)malloc((size_t)stack->capacity * sizeof(int));
    if (stack->states == NULL)
    {
        stack->capacity = 0;
        return false;
    }

    stack->states[stack->size++] = 0;
    return true;
}

/**
 * @brief Releases parser stack memory.
 * @param stack Stack to free.
 * @return This function does not return a value.
 */
static void free_parser_stack(parser_stack *stack)
{
    if (stack == NULL)
    {
        return;
    }

    free(stack->states);
    stack->states = NULL;
    stack->size = 0;
    stack->capacity = 0;
}

/**
 * @brief Pushes one state to parser stack.
 * @param stack Parser stack.
 * @param state_id State to push.
 * @return true on success, false on allocation failure.
 */
static bool push_state(parser_stack *stack, int state_id)
{
    if (stack == NULL)
    {
        return false;
    }

    if (stack->size >= stack->capacity)
    {
        int new_capacity = stack->capacity * 2;
        int *resized = (int *)realloc(stack->states, (size_t)new_capacity * sizeof(int));
        if (resized == NULL)
        {
            return false;
        }
        stack->states = resized;
        stack->capacity = new_capacity;
    }

    stack->states[stack->size++] = state_id;
    return true;
}

/**
 * @brief Pops N states from parser stack.
 * @param stack Parser stack.
 * @param count Number of states to pop.
 * @return false if count is invalid, true otherwise.
 */
static bool pop_states(parser_stack *stack, int count)
{
    if (stack == NULL || count < 0 || stack->size - count <= 0)
    {
        return false;
    }

    stack->size -= count;
    return true;
}

/**
 * @brief Returns current top parser state.
 * @param stack Parser stack.
 * @return Top state id or -1 if stack is invalid/empty.
 */
static int top_state(const parser_stack *stack)
{
    if (stack == NULL || stack->size <= 0)
    {
        return -1;
    }

    return stack->states[stack->size - 1];
}

/**
 * @brief Computes how many stack states to pop for a reduction.
 * @param g Parsed grammar.
 * @param p Production to reduce by.
 * @return Number of states to pop.
 */
static int reduction_pop_count(const grammar *g, production p)
{
    int epsilon_id = find_terminal_id(g, "epsilon");
    int count = 0;

    for (int i = 0; i < p.production_length; i++)
    {
        int symbol_id = p.production_symbol_ids[i];
        if (symbol_id == epsilon_id)
        {
            continue;
        }
        count++;
    }

    return count;
}

/**
 * @brief Reads one token from yylex and maps it to parser terminal id.
 * @param g Parsed grammar.
 * @param out_token Output token descriptor.
 * @return true on success, false when token cannot be mapped.
 */
static bool next_token(const grammar *g, token_stream *out_token)
{
    if (g == NULL || out_token == NULL)
    {
        return false;
    }

    int lexer_token = yylex();
    const char *lexeme = yytext != NULL ? yytext : "";
    int terminal_id = map_lexer_token_to_terminal_id(g, lexer_token, lexeme);

    out_token->lexer_token = lexer_token;
    out_token->terminal_id = terminal_id;
    out_token->lexeme = lexeme;

    return terminal_id >= 0;
}

/**
 * @brief Runs an LALR shift-reduce parse against yylex token stream.
 * @param g Parsed grammar.
 * @param table ACTION/GOTO table.
 * @return true if input is accepted, false otherwise.
 */
static bool parse_token_stream(const grammar *g, const parser_table *table)
{
    if (g == NULL || table == NULL)
    {
        return false;
    }

    parser_stack stack;
    if (!init_parser_stack(&stack))
    {
        return false;
    }

    token_stream lookahead;
    if (!next_token(g, &lookahead))
    {
        fprintf(stderr, "Lexer token could not be mapped to grammar terminal: '%s' (token=%d)\n",
                yytext != NULL ? yytext : "",
                lookahead.lexer_token);
        free_parser_stack(&stack);
        return false;
    }

    while (true)
    {
        int state_id = top_state(&stack);
        parser_action action = get_parser_action(table, state_id, lookahead.terminal_id);

        if (action.type == PARSER_ACTION_SHIFT)
        {
            if (!push_state(&stack, action.value))
            {
                fprintf(stderr, "Parser stack overflow while shifting.\n");
                free_parser_stack(&stack);
                return false;
            }

            if (!next_token(g, &lookahead))
            {
                fprintf(stderr, "Lexer token could not be mapped to grammar terminal: '%s' (token=%d)\n",
                        yytext != NULL ? yytext : "",
                        lookahead.lexer_token);
                free_parser_stack(&stack);
                return false;
            }

            continue;
        }

        if (action.type == PARSER_ACTION_REDUCE)
        {
            if (action.value < 0 || action.value >= g->num_productions)
            {
                fprintf(stderr, "Invalid reduction production index: %d\n", action.value);
                free_parser_stack(&stack);
                return false;
            }

            production p = g->productions[action.value];
            int pop_count = reduction_pop_count(g, p);
            if (!pop_states(&stack, pop_count))
            {
                fprintf(stderr, "Invalid parser stack pop for production %d\n", action.value);
                free_parser_stack(&stack);
                return false;
            }

            int goto_from = top_state(&stack);
            int goto_state = get_parser_goto(table, goto_from, p.non_terminal_id);
            if (goto_state < 0)
            {
                fprintf(stderr,
                        "Missing GOTO[%d, %s] after reduction p%d\n",
                        goto_from,
                        g->non_terminals[p.non_terminal_id].symbol,
                        action.value);
                free_parser_stack(&stack);
                return false;
            }

            if (!push_state(&stack, goto_state))
            {
                fprintf(stderr, "Parser stack overflow after reduction.\n");
                free_parser_stack(&stack);
                return false;
            }

            continue;
        }

        if (action.type == PARSER_ACTION_ACCEPT)
        {
            free_parser_stack(&stack);
            return true;
        }

        fprintf(stderr,
                "Syntax error at token '%s' (lexer=%d, terminal=%d) in state %d\n",
                lookahead.lexeme,
                lookahead.lexer_token,
                lookahead.terminal_id,
                state_id);
        free_parser_stack(&stack);
        return false;
    }
}

/**
 * @brief Program entry point. Builds LALR table and parses yylex token stream.
 * @param argc CLI argument count.
 * @param argv CLI argument vector.
 * @return 0 on accepted input, non-zero on error/reject.
 */
int main(int argc, char **argv)
{
    const char *source_path = NULL;
    const char *table_output_path = "parse_table.csv";

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <grammar_file> [source_file] [table_output.(csv|json)]\n", argv[0]);
        return 1;
    }

    if (argc >= 3)
    {
        if (has_suffix(argv[2], ".csv") || has_suffix(argv[2], ".json"))
        {
            table_output_path = argv[2];
        }
        else
        {
            source_path = argv[2];
        }
    }

    if (argc >= 4)
    {
        table_output_path = argv[3];
    }

    if (argc > 4)
    {
        fprintf(stderr, "Usage: %s <grammar_file> [source_file] [table_output.(csv|json)]\n", argv[0]);
        return 1;
    }

    char *grammar_file_content = read_file_all(argv[1]);
    if (grammar_file_content == NULL)
    {
        fprintf(stderr, "Failed to read grammar file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    grammar *g = create_grammar(grammar_file_content);
    free(grammar_file_content);
    if (g == NULL)
    {
        fprintf(stderr, "Failed to create grammar from '%s'.\n", argv[1]);
        return 1;
    }

    print_grammar(g);
    printf("----------------\n\n");

    if (source_path != NULL)
    {
        yyin = fopen(source_path, "r");
        if (yyin == NULL)
        {
            fprintf(stderr, "Failed to open source file '%s': %s\n", source_path, strerror(errno));
            return 1;
        }
    }

    lalr1_automaton *automaton = build_lalr1_automaton(g);
    if (automaton == NULL)
    {
        fprintf(stderr, "Failed to build LALR(1) automaton.\n");
        return 1;
    }

    print_lr1_automaton(automaton);
    printf("----------------\n\n");

    parser_table *table = build_lalr1_parser_table(g, automaton);
    if (table == NULL)
    {
        fprintf(stderr, "Failed to build parsing table.\n");
        free_lalr1_automaton(automaton);
        return 1;
    }

    if (table->has_conflicts)
    {
        fprintf(stderr, "Warning: parser table has %d conflicts.\n", table->num_conflicts);
    }

    print_parser_table(table);
    printf("----------------\n\n");

    if (!save_parser_table(table, table_output_path))
    {
        fprintf(stderr, "Failed to save parsing table to '%s'. Use .csv or .json extension.\n", table_output_path);
        free_parser_table(table);
        free_lalr1_automaton(automaton);
        if (source_path != NULL && yyin != NULL)
        {
            fclose(yyin);
        }
        return 1;
    }
    printf("Parsing table written to %s\n", table_output_path);

    bool accepted = parse_token_stream(g, table);
    if (accepted)
    {
        printf("Input accepted.\n");
    }
    else
    {
        printf("Input rejected.\n");
    }

    free_parser_table(table);
    free_lalr1_automaton(automaton);

    if (source_path != NULL && yyin != NULL)
    {
        fclose(yyin);
    }

    return accepted ? 0 : 2;
}

static bool has_suffix(const char *text, const char *suffix)
{
    if (text == NULL || suffix == NULL)
    {
        return false;
    }

    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > text_len)
    {
        return false;
    }

    return strcmp(text + (text_len - suffix_len), suffix) == 0;
}
