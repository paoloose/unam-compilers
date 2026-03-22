#ifndef ANALYZER_H
#define ANALYZER_H

#include "grammar.h"

/**
 * @brief Computes FIRST set for one non-terminal by index.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index in g->non_terminals.
 * @param out_first Output array with FIRST symbols. Caller owns the returned array.
 * @return Number of symbols written to out_first, or 0 on error.
 */
int compute_first_for_non_terminal(const grammar *g, int non_terminal_id, symbol **out_first);

/**
 * @brief Computes FOLLOW set for one non-terminal by index.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index in g->non_terminals.
 * @param out_follow Output array with FOLLOW symbols. Caller owns the returned array.
 * @return Number of symbols written to out_follow, or 0 on error.
 */
int compute_follow_for_non_terminal(const grammar *g, int non_terminal_id, symbol **out_follow);

/**
 * @brief Computes FIRST set for the start symbol.
 * @param g Parsed grammar.
 * @param out_first Output array with FIRST symbols. Caller owns the returned array.
 * @return Number of symbols written to out_first, or 0 on error.
 */
int compute_first_for_start_symbol(const grammar *g, symbol **out_first);

/**
 * @brief Computes FOLLOW set for the start symbol.
 * @param g Parsed grammar.
 * @param out_follow Output array with FOLLOW symbols. Caller owns the returned array.
 * @return Number of symbols written to out_follow, or 0 on error.
 */
int compute_follow_for_start_symbol(const grammar *g, symbol **out_follow);

/**
 * @brief Releases a symbol array returned by analyzer helpers.
 * @param symbols Array to release.
 * @param count Number of valid symbols in the array.
 * @return This function does not return a value.
 */
void free_symbol_array(symbol *symbols, int count);

#endif // ANALYZER_H