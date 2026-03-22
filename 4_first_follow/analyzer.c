#include "analyzer.h"
#include <assert.h>

/**
 * @brief Finds a terminal identifier by terminal name.
 * @param g Parsed grammar.
 * @param name Terminal name to search.
 * @return Terminal id if found, otherwise -1.
 */
static int find_terminal_id(const grammar *g, const char *name)
{
    if (g == NULL || name == NULL)
    {
        return -1;
    }

    for (int i = 0; i < g->num_terminals; i++)
    {
        if (strcmp(g->terminals[i].symbol, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Appends one symbol to a dynamically sized symbol array.
 * @param arr Target array pointer.
 * @param count Current element count; incremented on success.
 * @param text Symbol text to copy.
 * @param is_terminal Indicates terminal/non-terminal role.
 * @return true on success, false on allocation failure.
 */
static bool add_symbol_to_array(symbol **arr, int *count, const char *text, bool is_terminal)
{
    if (arr == NULL || count == NULL || text == NULL)
    {
        return false;
    }

    // we need to copy the text because it will be freed when the function returns
    char *text_copy = strdup(text);
    if (text_copy == NULL)
    {
        return false;
    }

    // the new size is the current size + 1
    symbol *resized = realloc(*arr, (*count + 1) * sizeof(symbol));
    if (resized == NULL)
    {
        // abort
        free(text_copy);
        return false;
    }

    // and we update the array
    resized[*count].symbol = text_copy;
    resized[*count].symbol_length = (int)strlen(text);
    resized[*count].is_terminal = is_terminal;

    *arr = resized;
    (*count)++;

    return true;
}

/**
 * @brief Builds FIRST and nullable tables for all non-terminals.
 *
 * The FIRST table is a flattened 2-D boolean array of dimensions
 * [num_non_terminals x num_terminals]. Entry [nt][t] is true when
 * terminal t can appear as the first token derived from non-terminal nt
 *
 * The nullable array is a 1-D boolean array of length num_non_terminals.
 * nullable[nt] is true when non-terminal nt can derive the empty string ε
 *
 * The algorithm uses fixed-point (iterative) propagation. That is, it keeps
 * scanning every production and updating the tables until no new entry
 * changes in a full pass
 *
 * @param g Parsed grammar.
 * @param first_table Output flattened table: non-terminal x terminal.
 * @param nullable Output nullable flags per non-terminal.
 * @param epsilon_id Output id of terminal "epsilon", or -1 if absent.
 * @return true when tables were built, false on invalid input or allocation error.
 */
static bool compute_first_tables(const grammar *g, bool **first_table, bool **nullable, int *epsilon_id)
{
    if (g == NULL || first_table == NULL || nullable == NULL || epsilon_id == NULL)
    {
        return false;
    }

    int N = g->num_non_terminals;
    int T = g->num_terminals;

    if (N <= 0 || T <= 0)
    {
        return false;
    }

    // From now on we can assume that the grammar is valid and non-empty

    // FIRST table is (N rows x T columns)
    bool *ft = calloc((N * T), sizeof(bool));
    if (ft == NULL)
    {
        return false;
    }

    // nullable flags is a 1-D array of length N
    bool *nl = calloc(N, sizeof(bool));
    if (nl == NULL)
    {
        free(ft);
        return false;
    }

    // Locate the epsilon terminal (may not exist)
    int eps = find_terminal_id(g, "epsilon");
    *epsilon_id = eps;

    // Fixed-point iteration: repeat until no change occurs
    bool changed = true;

    while (changed)
    {
        changed = false;

            
        for (int p = 0; p < g->num_productions; p++)
        {
            production *prod = &g->productions[p];
            int nt = prod->non_terminal_id;   // LHS non-terminal index

            if (nt < 0 || nt >= N)
            {
                continue;
            }

            // Walk each symbol in the RHS left-to-right.
            bool all_nullable = true;

            for (int s = 0; s < prod->production_length; s++)
            {
                int sym_id = prod->production_symbol_ids[s];
                bool sym_is_terminal = (sym_id >= 0 && sym_id < T);

                if (sym_is_terminal)
                {
                    /* A plain terminal contributes itself to FIRST,
                     * but only if it is not epsilon. */
                    if (sym_id != eps)
                    {
                        if (!ft[nt * T + sym_id])
                        {
                            ft[nt * T + sym_id] = true;
                            changed = true;
                        }
                        all_nullable = false;
                        break; // can't look past a real terminal
                    }
                    else
                    {
                        // epsilon in the RHS, the whole production is trivially nullable
                        // keep scanning, all_nullable stays true
                    }
                }
                else
                {
                    // The symbol is a non-terminal (encoded as T + nt_index).
                    int rhs_nt = sym_id - T;

                    assert(rhs_nt >= 0 && rhs_nt < N);

                    // Propagate FIRST(rhs_nt) - {epsilon} into FIRST(nt).
                    for (int t = 0; t < T; t++)
                    {
                        if (t == eps)
                        {
                            continue; // epsilon is handled via nullable
                        }
                        if (ft[rhs_nt * T + t] && !ft[nt * T + t])
                        {
                            ft[nt * T + t] = true;
                            changed = true;
                        }
                    }

                    // If this RHS non-terminal is NOT nullable we stop here
                    if (!nl[rhs_nt])
                    {
                        all_nullable = false;
                        break;
                    }
                    // Otherwise continue to the next symbol in the production,
                    // as the current may completely disappear (derive epsilon)
                }
            }

            // if every symbol in the RHS was nullable (or it was empty), the LHS is nullable too
            if (all_nullable && !nl[nt])
            {
                nl[nt] = true;
                changed = true;
            }
        }
    }

    // return the computed tables!!
    *first_table = ft;
    *nullable    = nl;

    return true;
}

/**
 * @brief Builds FOLLOW table for all non-terminals.
 *
 * The FOLLOW table is a flattened 2-D boolean array of dimensions
 * [num_non_terminals x (num_terminals + 1)]. The extra column (index num_terminals)
 * represents the end-of-input marker '$', that we denote as EOF.
 *
 * Entry [nt][t] is true when terminal t (or '$') can appear immediately
 * after some sentential form containing non-terminal nt
 *
 * Rules applied during fixed-point propagation:
 *   1. FOLLOW(start) contains '$'.
 *   2. For every production A -> α B β:
 *      - FIRST(β) - {ε}  ⊆  FOLLOW(B)
 *      - if β =>* ε then FOLLOW(A) ⊆ FOLLOW(B)
 *   3. For every production A -> α B (B at the end):
 *      - FOLLOW(A) ⊆ FOLLOW(B)
 *
 * @param g Parsed grammar.
 * @param first_table FIRST table from compute_first_tables.
 * @param nullable Nullable flags from compute_first_tables.
 * @param epsilon_id Terminal id for "epsilon", or -1.
 * @param out_follow Output flattened table: non-terminal x (terminals + '$').
 * @param out_follow_cols Output number of columns for out_follow.
 * @return true on success, false on allocation error or invalid input.
 */
static bool compute_follow_table(
    const grammar *g,
    const bool *first_table,
    const bool *nullable,
    int epsilon_id,
    bool **out_follow,
    int *out_follow_cols)
{
    if (g == NULL || first_table == NULL || nullable == NULL || out_follow == NULL || out_follow_cols == NULL) {
        return false;
    }

    int N = g->num_non_terminals;
    int T = g->num_terminals;

    if (N <= 0 || T <= 0) {
        return false;
    }

    // one column per terminal, plus one extra slot for '$'
    int cols = T + 1;
    int dollar_col = T; // last column holds '$'

    bool *fw = calloc((N * cols), sizeof(bool));
    if (fw == NULL)
    {
        return false;
    }

    // rule 1: start symbol always has '$' in its FOLLOW set
    fw[0 * cols + dollar_col] = true;

    // fixed-point loop
    bool changed = true;
    while (changed)
    {
        changed = false;

        for (int p = 0; p < g->num_productions; p++)
        {
            production *prod = &g->productions[p];
            int lhs = prod->non_terminal_id; // A (the LHS)

            if (lhs < 0 || lhs >= N)
            {
                continue;
            }

            /* For each symbol B in the RHS, compute what follows it. */
            for (int s = 0; s < prod->production_length; s++)
            {
                int sym_id = prod->production_symbol_ids[s];
                bool sym_is_terminal = (sym_id >= 0 && sym_id < T);

                // skip terminals, we only care about non-terminals here
                if (sym_is_terminal)
                {
                    continue;
                }

                int B = sym_id - T; // decode: non-terminal index

                if (B < 0 || B >= N)
                {
                    continue;
                }

                // beta = everything after B in this production
                bool beta_all_nullable = true;

                for (int k = s + 1; k < prod->production_length; k++)
                {
                    int beta_sym = prod->production_symbol_ids[k];
                    bool beta_is_terminal = (beta_sym >= 0 && beta_sym < T);

                    if (beta_is_terminal)
                    {
                        if (beta_sym != epsilon_id)
                        {
                            // rule 2a: real terminal after B, add it to FOLLOW(B)
                            if (!fw[B * cols + beta_sym])
                            {
                                fw[B * cols + beta_sym] = true;
                                changed = true;
                            }
                            beta_all_nullable = false;
                            break;
                        }
                        // epsilon token here, beta stays nullable so we keep going
                    }
                    else
                    {
                        int beta_nt = beta_sym - T;

                        if (beta_nt < 0 || beta_nt >= N)
                        {
                            beta_all_nullable = false;
                            break;
                        }

                        // rule 2a: FIRST(beta_nt) \ {epsilon} goes into FOLLOW(B)
                        for (int t = 0; t < T; t++)
                        {
                            if (t == epsilon_id)
                            {
                                continue;
                            }
                            if (first_table[beta_nt * T + t] && !fw[B * cols + t])
                            {
                                fw[B * cols + t] = true;
                                changed = true;
                            }
                        }

                        if (!nullable[beta_nt])
                        {
                            beta_all_nullable = false;
                            break;
                        }
                        // beta_nt is nullable, keep scanning right
                    }
                }

                // rule 2b/3: beta derived epsilon (or B was at the end), so FOLLOW(A) goes into FOLLOW(B)
                if (beta_all_nullable)
                {
                    for (int t = 0; t < cols; t++)
                    {
                        if (fw[lhs * cols + t] && !fw[B * cols + t])
                        {
                            fw[B * cols + t] = true;
                            changed = true;
                        }
                    }
                }
            }
        }
    }

    *out_follow      = fw;
    *out_follow_cols = cols;

    return true;
}

/**
 * @brief Collects FIRST symbols for one non-terminal from the computed table.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index.
 * @param first_table FIRST table.
 * @param nullable Nullable flags.
 * @param epsilon_id Terminal id for "epsilon", or -1.
 * @param out_first Output array with FIRST symbols.
 * @return Number of collected symbols, or 0 on error.
 */
static int collect_first_for_non_terminal(
    const grammar *g,
    int non_terminal_id,
    const bool *first_table,
    const bool *nullable,
    int epsilon_id,
    symbol **out_first)
{
    if (g == NULL || first_table == NULL || nullable == NULL || out_first == NULL)
    {
        return 0;
    }

    int N = g->num_non_terminals;
    int T = g->num_terminals;

    if (non_terminal_id < 0 || non_terminal_id >= N)
    {
        return 0;
    }

    symbol *result = NULL;
    int count = 0;

    // scan the row, skip epsilon (that's handled by nullable below)
    for (int t = 0; t < T; t++)
    {
        if (t == epsilon_id)
        {
            continue; // epsilon gets added separately via the nullable check
        }
        if (first_table[non_terminal_id * T + t])
        {
            if (!add_symbol_to_array(&result, &count, g->terminals[t].symbol, true))
            {
                free_symbol_array(result, count);
                return 0;
            }
        }
    }

    // nullable means epsilon is in FIRST too
    if (nullable[non_terminal_id] && epsilon_id >= 0)
    {
        if (!add_symbol_to_array(&result, &count, g->terminals[epsilon_id].symbol, true))
        {
            free_symbol_array(result, count);
            return 0;
        }
    }

    *out_first = result;
    return count;
}

/**
 * @brief Collects FOLLOW symbols for one non-terminal from the computed table.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index.
 * @param follow_table FOLLOW table.
 * @param follow_cols Number of columns in follow_table (terminals + 1 for '$').
 * @param out_follow Output array with FOLLOW symbols.
 * @return Number of collected symbols, or 0 on error.
 */
static int collect_follow_for_non_terminal(
    const grammar *g,
    int non_terminal_id,
    const bool *follow_table,
    int follow_cols,
    symbol **out_follow)
{
    if (g == NULL || follow_table == NULL || out_follow == NULL)
    {
        return 0;
    }

    int N = g->num_non_terminals;
    int T = g->num_terminals;

    if (non_terminal_id < 0 || non_terminal_id >= N)
    {
        return 0;
    }

    symbol *result = NULL;
    int count = 0;

    // columns 0..T-1 are real terminals, column T is '$'
    for (int t = 0; t < follow_cols; t++)
    {
        if (!follow_table[non_terminal_id * follow_cols + t])
        {
            continue;
        }

        const char *name;
        bool is_terminal;

        if (t < T)
        {
            name = g->terminals[t].symbol;
            is_terminal = true;
        }
        else
        {
            name = "$";
            is_terminal = false; // $ is a sentinel, not a real grammar terminal
        }

        if (!add_symbol_to_array(&result, &count, name, is_terminal))
        {
            free_symbol_array(result, count);
            return 0;
        }
    }

    *out_follow = result;
    return count;
}

// Public API

/**
 * @brief Computes FIRST set for one non-terminal by index.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index in g->non_terminals.
 * @param out_first Output array with FIRST symbols.
 * @return Number of symbols in out_first, or 0 on error.
 */
int compute_first_for_non_terminal(const grammar *g, int non_terminal_id, symbol **out_first)
{
    if (g == NULL || out_first == NULL)
    {
        return 0;
    }

    if (non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals)
    {
        return 0;
    }

    bool *first_table = NULL;
    bool *nullable    = NULL;
    int   epsilon_id  = -1;

    if (!compute_first_tables(g, &first_table, &nullable, &epsilon_id))
    {
        return 0;
    }

    int count = collect_first_for_non_terminal(
        g, non_terminal_id, first_table, nullable, epsilon_id, out_first);

    free(first_table);
    free(nullable);

    return count;
}

/**
 * @brief Computes FOLLOW set for one non-terminal by index.
 * @param g Parsed grammar.
 * @param non_terminal_id Non-terminal index in g->non_terminals.
 * @param out_follow Output array with FOLLOW symbols.
 * @return Number of symbols in out_follow, or 0 on error.
 */
int compute_follow_for_non_terminal(const grammar *g, int non_terminal_id, symbol **out_follow)
{
    if (g == NULL || out_follow == NULL)
    {
        return 0;
    }

    if (non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals)
    {
        return 0;
    }

    bool *first_table = NULL;
    bool *nullable    = NULL;
    int   epsilon_id  = -1;

    if (!compute_first_tables(g, &first_table, &nullable, &epsilon_id))
    {
        return 0;
    }

    bool *follow_table = NULL;
    int   follow_cols  = 0;

    if (!compute_follow_table(g, first_table, nullable, epsilon_id, &follow_table, &follow_cols))
    {
        free(first_table);
        free(nullable);
        return 0;
    }

    int count = collect_follow_for_non_terminal(
        g, non_terminal_id, follow_table, follow_cols, out_follow);

    free(first_table);
    free(nullable);
    free(follow_table);

    return count;
}

/**
 * @brief Computes FIRST set for the start symbol.
 * @param g Parsed grammar.
 * @param out_first Output array with FIRST(start) terminals.
 * @return Number of symbols in out_first, or 0 on error.
 */
int compute_first_for_start_symbol(const grammar *g, symbol **out_first)
{
    return compute_first_for_non_terminal(g, 0, out_first);
}

/**
 * @brief Computes FOLLOW set for the start symbol.
 * @param g Parsed grammar.
 * @param out_follow Output array with FOLLOW(start) terminals.
 * @return Number of symbols in out_follow, or 0 on error.
 */
int compute_follow_for_start_symbol(const grammar *g, symbol **out_follow)
{
    return compute_follow_for_non_terminal(g, 0, out_follow);
}

/**
 * @brief Frees a symbol array and each duplicated symbol string.
 * @param symbols Symbol array to release.
 * @param count Number of initialized entries.
 * @return This function does not return a value.
 */
void free_symbol_array(symbol *symbols, int count)
{
    if (symbols == NULL)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        free(symbols[i].symbol);
    }

    free(symbols);
}
