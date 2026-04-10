#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int action_index(const parser_table *table, int state_id, int terminal_or_eof_id);
static int goto_index(const parser_table *table, int state_id, int non_terminal_id);
static parser_action make_error_action(void);
static parser_action make_shift_action(int target_state);
static parser_action make_reduce_action(int production_index);
static parser_action make_accept_action(void);
static bool parser_actions_equal(parser_action left, parser_action right);
static bool set_action_entry(parser_table *table, int state_id, int terminal_or_eof_id, parser_action action);
static bool set_goto_entry(parser_table *table, int state_id, int non_terminal_id, int target_state);
static bool symbol_is_terminal(const grammar *g, int encoded_symbol_id);
static const char *symbol_name(const grammar *g, int encoded_symbol_id);
static const char *lookahead_name(const parser_table *table, int terminal_or_eof_id);
static void print_action_cell(const parser_table *table, parser_action action);
static const char *action_type_name(parser_action_type type);
static bool has_suffix(const char *text, const char *suffix);
static void write_json_escaped(FILE *file, const char *text);

parser_table *build_lalr1_parser_table(const grammar *g, const lalr1_automaton *automaton)
{
    if (g == NULL || automaton == NULL)
    {
        return NULL;
    }

    parser_table *table = (parser_table *)calloc(1, sizeof(parser_table));
    if (table == NULL)
    {
        return NULL;
    }

    table->g = g;
    table->num_states = automaton->num_states;
    table->num_terminals_with_eof = g->num_terminals + 1;
    table->num_non_terminals = g->num_non_terminals;

    if (table->num_states <= 0 || table->num_terminals_with_eof <= 0 || table->num_non_terminals <= 0)
    {
        free_parser_table(table);
        return NULL;
    }

    table->action_table = (parser_action *)malloc(
        (size_t)table->num_states * (size_t)table->num_terminals_with_eof * sizeof(parser_action));
    table->goto_table = (int *)malloc((size_t)table->num_states * (size_t)table->num_non_terminals * sizeof(int));
    if (table->action_table == NULL || table->goto_table == NULL)
    {
        free_parser_table(table);
        return NULL;
    }

    for (int s = 0; s < table->num_states; s++)
    {
        for (int a = 0; a < table->num_terminals_with_eof; a++)
        {
            table->action_table[action_index(table, s, a)] = make_error_action();
        }

        for (int nt = 0; nt < table->num_non_terminals; nt++)
        {
            table->goto_table[goto_index(table, s, nt)] = -1;
        }
    }

    // 1) Fill SHIFT and GOTO from automaton transitions.
    for (int i = 0; i < automaton->num_transitions; i++)
    {
        lr1_transition transition = automaton->transitions[i];
        if (symbol_is_terminal(g, transition.symbol_id))
        {
            if (!set_action_entry(
                    table,
                    transition.from_state,
                    transition.symbol_id,
                    make_shift_action(transition.to_state)))
            {
                free_parser_table(table);
                return NULL;
            }
            continue;
        }

        int non_terminal_id = transition.symbol_id - g->num_terminals;
        if (!set_goto_entry(table, transition.from_state, non_terminal_id, transition.to_state))
        {
            free_parser_table(table);
            return NULL;
        }
    }

    // 2) Fill REDUCE and ACCEPT from completed items and lookahead symbols.
    for (int state_id = 0; state_id < automaton->num_states; state_id++)
    {
        const lr1_state *state = &automaton->states[state_id];
        for (int i = 0; i < state->num_items; i++)
        {
            lr1_item item = state->items[i];

            if (item.production_index < 0)
            {
                if (item.dot_position == 1 && item.lookahead_id == automaton->eof_lookahead_id)
                {
                    if (!set_action_entry(table, state_id, automaton->eof_lookahead_id, make_accept_action()))
                    {
                        free_parser_table(table);
                        return NULL;
                    }
                }
                continue;
            }

            if (item.production_index >= g->num_productions)
            {
                continue;
            }

            production p = g->productions[item.production_index];
            if (item.dot_position != p.production_length)
            {
                continue;
            }

            if (item.lookahead_id < 0 || item.lookahead_id >= table->num_terminals_with_eof)
            {
                continue;
            }

            if (!set_action_entry(table, state_id, item.lookahead_id, make_reduce_action(item.production_index)))
            {
                free_parser_table(table);
                return NULL;
            }
        }
    }

    return table;
}

void free_parser_table(parser_table *table)
{
    if (table == NULL)
    {
        return;
    }

    free(table->action_table);
    free(table->goto_table);
    free(table);
}

parser_action get_parser_action(const parser_table *table, int state_id, int terminal_or_eof_id)
{
    if (table == NULL)
    {
        return make_error_action();
    }

    int idx = action_index(table, state_id, terminal_or_eof_id);
    if (idx < 0)
    {
        return make_error_action();
    }

    return table->action_table[idx];
}

int get_parser_goto(const parser_table *table, int state_id, int non_terminal_id)
{
    if (table == NULL)
    {
        return -1;
    }

    int idx = goto_index(table, state_id, non_terminal_id);
    if (idx < 0)
    {
        return -1;
    }

    return table->goto_table[idx];
}

void print_parser_table(const parser_table *table)
{
    if (table == NULL || table->g == NULL)
    {
        return;
    }

    printf("Parser table: states=%d, terminals(with $)=%d, non-terminals=%d\n",
           table->num_states,
           table->num_terminals_with_eof,
           table->num_non_terminals);

    if (table->has_conflicts)
    {
        printf("Conflicts detected: %d\n", table->num_conflicts);
    }
    else
    {
        printf("Conflicts detected: 0\n");
    }

    printf("\nACTION\n");
    for (int state_id = 0; state_id < table->num_states; state_id++)
    {
        printf("state %d:\n", state_id);
        for (int a = 0; a < table->num_terminals_with_eof; a++)
        {
            parser_action action = get_parser_action(table, state_id, a);
            if (action.type == PARSER_ACTION_ERROR)
            {
                continue;
            }

            printf("  on %s -> ", lookahead_name(table, a));
            print_action_cell(table, action);
            printf("\n");
        }
    }

    printf("\nGOTO\n");
    for (int state_id = 0; state_id < table->num_states; state_id++)
    {
        printf("state %d:\n", state_id);
        for (int nt = 0; nt < table->num_non_terminals; nt++)
        {
            int target = get_parser_goto(table, state_id, nt);
            if (target < 0)
            {
                continue;
            }

            printf("  on %s -> %d\n", table->g->non_terminals[nt].symbol, target);
        }
    }
}

static int action_index(const parser_table *table, int state_id, int terminal_or_eof_id)
{
    if (table == NULL)
    {
        return -1;
    }
    if (state_id < 0 || state_id >= table->num_states)
    {
        return -1;
    }
    if (terminal_or_eof_id < 0 || terminal_or_eof_id >= table->num_terminals_with_eof)
    {
        return -1;
    }

    return state_id * table->num_terminals_with_eof + terminal_or_eof_id;
}

static int goto_index(const parser_table *table, int state_id, int non_terminal_id)
{
    if (table == NULL)
    {
        return -1;
    }
    if (state_id < 0 || state_id >= table->num_states)
    {
        return -1;
    }
    if (non_terminal_id < 0 || non_terminal_id >= table->num_non_terminals)
    {
        return -1;
    }

    return state_id * table->num_non_terminals + non_terminal_id;
}

static parser_action make_error_action(void)
{
    parser_action action;
    action.type = PARSER_ACTION_ERROR;
    action.value = -1;
    return action;
}

static parser_action make_shift_action(int target_state)
{
    parser_action action;
    action.type = PARSER_ACTION_SHIFT;
    action.value = target_state;
    return action;
}

static parser_action make_reduce_action(int production_index)
{
    parser_action action;
    action.type = PARSER_ACTION_REDUCE;
    action.value = production_index;
    return action;
}

static parser_action make_accept_action(void)
{
    parser_action action;
    action.type = PARSER_ACTION_ACCEPT;
    action.value = 0;
    return action;
}

static bool parser_actions_equal(parser_action left, parser_action right)
{
    return left.type == right.type && left.value == right.value;
}

static bool set_action_entry(parser_table *table, int state_id, int terminal_or_eof_id, parser_action action)
{
    int idx = action_index(table, state_id, terminal_or_eof_id);
    if (idx < 0)
    {
        return false;
    }

    parser_action previous = table->action_table[idx];
    if (previous.type == PARSER_ACTION_ERROR)
    {
        table->action_table[idx] = action;
        return true;
    }

    if (parser_actions_equal(previous, action))
    {
        return true;
    }

    // Keep the first action inserted and just report conflict.
    table->has_conflicts = true;
    table->num_conflicts++;
    return true;
}

static bool set_goto_entry(parser_table *table, int state_id, int non_terminal_id, int target_state)
{
    int idx = goto_index(table, state_id, non_terminal_id);
    if (idx < 0)
    {
        return false;
    }

    int previous = table->goto_table[idx];
    if (previous == -1 || previous == target_state)
    {
        table->goto_table[idx] = target_state;
        return true;
    }

    table->has_conflicts = true;
    table->num_conflicts++;
    return true;
}

static bool symbol_is_terminal(const grammar *g, int encoded_symbol_id)
{
    return g != NULL && encoded_symbol_id >= 0 && encoded_symbol_id < g->num_terminals;
}

static const char *symbol_name(const grammar *g, int encoded_symbol_id)
{
    if (g == NULL)
    {
        return "?";
    }

    if (symbol_is_terminal(g, encoded_symbol_id))
    {
        return g->terminals[encoded_symbol_id].symbol;
    }

    int non_terminal_id = encoded_symbol_id - g->num_terminals;
    if (non_terminal_id >= 0 && non_terminal_id < g->num_non_terminals)
    {
        return g->non_terminals[non_terminal_id].symbol;
    }

    return "?";
}

static const char *lookahead_name(const parser_table *table, int terminal_or_eof_id)
{
    if (table == NULL || table->g == NULL)
    {
        return "?";
    }

    if (terminal_or_eof_id == table->g->num_terminals)
    {
        return "$";
    }

    return symbol_name(table->g, terminal_or_eof_id);
}

static void print_action_cell(const parser_table *table, parser_action action)
{
    (void)table;

    switch (action.type)
    {
    case PARSER_ACTION_SHIFT:
        printf("shift %d", action.value);
        break;
    case PARSER_ACTION_REDUCE:
        printf("reduce p%d", action.value);
        break;
    case PARSER_ACTION_ACCEPT:
        printf("accept");
        break;
    case PARSER_ACTION_ERROR:
    default:
        printf("error");
        break;
    }
}

bool save_parser_table_csv(const parser_table *table, const char *output_path)
{
    if (table == NULL || table->g == NULL || output_path == NULL)
    {
        return false;
    }

    FILE *file = fopen(output_path, "w");
    if (file == NULL)
    {
        return false;
    }

    // Long-form CSV keeps both ACTION and GOTO in one uniform schema.
    fprintf(file, "section,state,symbol,entry_type,value\n");

    for (int state_id = 0; state_id < table->num_states; state_id++)
    {
        for (int terminal = 0; terminal < table->num_terminals_with_eof; terminal++)
        {
            parser_action action = get_parser_action(table, state_id, terminal);
            if (action.type == PARSER_ACTION_ERROR)
            {
                continue;
            }

            fprintf(file,
                    "ACTION,%d,\"%s\",%s,%d\n",
                    state_id,
                    lookahead_name(table, terminal),
                    action_type_name(action.type),
                    action.value);
        }

        for (int non_terminal = 0; non_terminal < table->num_non_terminals; non_terminal++)
        {
            int target = get_parser_goto(table, state_id, non_terminal);
            if (target < 0)
            {
                continue;
            }

            fprintf(file,
                    "GOTO,%d,\"%s\",GOTO,%d\n",
                    state_id,
                    table->g->non_terminals[non_terminal].symbol,
                    target);
        }
    }

    if (fclose(file) != 0)
    {
        return false;
    }

    return true;
}

bool save_parser_table_json(const parser_table *table, const char *output_path)
{
    if (table == NULL || table->g == NULL || output_path == NULL)
    {
        return false;
    }

    FILE *file = fopen(output_path, "w");
    if (file == NULL)
    {
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"numStates\": %d,\n", table->num_states);
    fprintf(file, "  \"numTerminalsWithEof\": %d,\n", table->num_terminals_with_eof);
    fprintf(file, "  \"numNonTerminals\": %d,\n", table->num_non_terminals);
    fprintf(file, "  \"hasConflicts\": %s,\n", table->has_conflicts ? "true" : "false");
    fprintf(file, "  \"numConflicts\": %d,\n", table->num_conflicts);

    fprintf(file, "  \"terminals\": [");
    for (int terminal = 0; terminal < table->g->num_terminals; terminal++)
    {
        if (terminal > 0)
        {
            fprintf(file, ", ");
        }
        fprintf(file, "\"");
        write_json_escaped(file, table->g->terminals[terminal].symbol);
        fprintf(file, "\"");
    }
    if (table->g->num_terminals > 0)
    {
        fprintf(file, ", ");
    }
    fprintf(file, "\"$\"],\n");

    fprintf(file, "  \"nonTerminals\": [");
    for (int non_terminal = 0; non_terminal < table->g->num_non_terminals; non_terminal++)
    {
        if (non_terminal > 0)
        {
            fprintf(file, ", ");
        }
        fprintf(file, "\"");
        write_json_escaped(file, table->g->non_terminals[non_terminal].symbol);
        fprintf(file, "\"");
    }
    fprintf(file, "],\n");

    fprintf(file, "  \"action\": [\n");
    for (int state_id = 0; state_id < table->num_states; state_id++)
    {
        fprintf(file, "    [");
        for (int terminal = 0; terminal < table->num_terminals_with_eof; terminal++)
        {
            parser_action action = get_parser_action(table, state_id, terminal);
            if (terminal > 0)
            {
                fprintf(file, ", ");
            }
            fprintf(file,
                    "{\"type\":\"%s\",\"value\":%d}",
                    action_type_name(action.type),
                    action.value);
        }
        fprintf(file, "]");
        if (state_id < table->num_states - 1)
        {
            fprintf(file, ",");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "  ],\n");

    fprintf(file, "  \"goto\": [\n");
    for (int state_id = 0; state_id < table->num_states; state_id++)
    {
        fprintf(file, "    [");
        for (int non_terminal = 0; non_terminal < table->num_non_terminals; non_terminal++)
        {
            int target = get_parser_goto(table, state_id, non_terminal);
            if (non_terminal > 0)
            {
                fprintf(file, ", ");
            }
            fprintf(file, "%d", target);
        }
        fprintf(file, "]");
        if (state_id < table->num_states - 1)
        {
            fprintf(file, ",");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "  ]\n");

    fprintf(file, "}\n");

    if (fclose(file) != 0)
    {
        return false;
    }

    return true;
}

bool save_parser_table(const parser_table *table, const char *output_path)
{
    if (output_path == NULL)
    {
        return false;
    }

    if (has_suffix(output_path, ".json"))
    {
        return save_parser_table_json(table, output_path);
    }
    if (has_suffix(output_path, ".csv"))
    {
        return save_parser_table_csv(table, output_path);
    }

    return false;
}

static const char *action_type_name(parser_action_type type)
{
    switch (type)
    {
    case PARSER_ACTION_SHIFT:
        return "SHIFT";
    case PARSER_ACTION_REDUCE:
        return "REDUCE";
    case PARSER_ACTION_ACCEPT:
        return "ACCEPT";
    case PARSER_ACTION_ERROR:
    default:
        return "ERROR";
    }
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

static void write_json_escaped(FILE *file, const char *text)
{
    if (file == NULL || text == NULL)
    {
        return;
    }

    for (const char *cursor = text; *cursor != '\0'; cursor++)
    {
        unsigned char ch = (unsigned char)*cursor;
        if (ch == '\\' || ch == '"')
        {
            fputc('\\', file);
            fputc((int)ch, file);
            continue;
        }
        if (ch == '\n')
        {
            fputs("\\n", file);
            continue;
        }
        if (ch == '\r')
        {
            fputs("\\r", file);
            continue;
        }
        if (ch == '\t')
        {
            fputs("\\t", file);
            continue;
        }

        fputc((int)ch, file);
    }
}