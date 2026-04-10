#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "automaton.h"

typedef enum parser_action_type
{
    PARSER_ACTION_ERROR = 0,
    PARSER_ACTION_SHIFT,
    PARSER_ACTION_REDUCE,
    PARSER_ACTION_ACCEPT
} parser_action_type;

typedef struct parser_action
{
    parser_action_type type;
    int value;
} parser_action;

typedef struct parser_table
{
    const grammar *g;
    int num_states;
    int num_terminals_with_eof;
    int num_non_terminals;
    parser_action *action_table;
    int *goto_table;
    bool has_conflicts;
    int num_conflicts;
} parser_table;

/**
 * @brief Builds ACTION and GOTO tables from a ready LALR(1) automaton.
 * @param g Parsed grammar used by the automaton.
 * @param automaton LALR(1) automaton with merged kernels and lookaheads.
 * @return Allocated parser table, or NULL on allocation/input error.
 */
parser_table *build_lalr1_parser_table(const grammar *g, const lalr1_automaton *automaton);

/**
 * @brief Frees all memory owned by a parser table.
 * @param table Parser table to release.
 * @return This function does not return a value.
 */
void free_parser_table(parser_table *table);

/**
 * @brief Reads one ACTION entry.
 * @param table Parser table.
 * @param state_id State index.
 * @param terminal_or_eof_id Terminal id or EOF id.
 * @return Action entry, PARSER_ACTION_ERROR on invalid indexes.
 */
parser_action get_parser_action(const parser_table *table, int state_id, int terminal_or_eof_id);

/**
 * @brief Reads one GOTO entry.
 * @param table Parser table.
 * @param state_id State index.
 * @param non_terminal_id Non-terminal id in [0, num_non_terminals).
 * @return Target state or -1 when not defined/invalid indexes.
 */
int get_parser_goto(const parser_table *table, int state_id, int non_terminal_id);

/**
 * @brief Prints ACTION and GOTO tables in a compact text format.
 * @param table Parser table to print.
 * @return This function does not return a value.
 */
void print_parser_table(const parser_table *table);

/**
 * @brief Saves parser table in CSV format.
 * @param table Parser table to serialize.
 * @param output_path Destination CSV path.
 * @return true on success, false on I/O or invalid input error.
 */
bool save_parser_table_csv(const parser_table *table, const char *output_path);

/**
 * @brief Saves parser table in JSON format.
 * @param table Parser table to serialize.
 * @param output_path Destination JSON path.
 * @return true on success, false on I/O or invalid input error.
 */
bool save_parser_table_json(const parser_table *table, const char *output_path);

/**
 * @brief Saves parser table as CSV or JSON based on file extension.
 * @param table Parser table to serialize.
 * @param output_path Destination path (.csv or .json).
 * @return true on success, false on unsupported extension or I/O error.
 */
bool save_parser_table(const parser_table *table, const char *output_path);

#endif // PARSER_H