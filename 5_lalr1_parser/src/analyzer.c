#include "analyzer.h"

#pragma region First_Helpers
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
	symbol *resized = (symbol *)realloc(*arr, (size_t)(*count + 1) * sizeof(symbol));
	if (resized == NULL)
	{
		return false;
	}

	*arr = resized;
	(*arr)[*count].symbol = strdup(text);
	if ((*arr)[*count].symbol == NULL)
	{
		return false;
	}

	(*arr)[*count].symbol_length = (int)strlen(text);
	(*arr)[*count].is_terminal = is_terminal;
	(*count)++;

	return true;
}

/**
 * @brief Builds FIRST and nullable tables for all non-terminals.
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
	if (g->num_non_terminals <= 0 || g->num_terminals <= 0)
	{
		return false;
	}

	int nt_count = g->num_non_terminals;
	int t_count = g->num_terminals;

	*first_table = (bool *)calloc((size_t)nt_count * (size_t)t_count, sizeof(bool));
	*nullable = (bool *)calloc((size_t)nt_count, sizeof(bool));
	if (*first_table == NULL || *nullable == NULL)
	{
		free(*first_table);
		free(*nullable);
		*first_table = NULL;
		*nullable = NULL;
		return false;
	}

	*epsilon_id = find_terminal_id(g, "epsilon");

	// Fixed-point iteration until no FIRST/nullable entry changes.
	bool changed = true;
	while (changed)
	{
		changed = false;

		for (int p = 0; p < g->num_productions; p++)
		{
			production prod = g->productions[p];
			int A = prod.non_terminal_id;
			bool derives_epsilon = true;

			if (A < 0 || A >= nt_count)
			{
				continue;
			}

			if (prod.production_length == 0)
			{
				if (!(*nullable)[A])
				{
					(*nullable)[A] = true;
					changed = true;
				}
				continue;
			}

			for (int i = 0; i < prod.production_length; i++)
			{
				int encoded = prod.production_symbol_ids[i];

				if (encoded < g->num_terminals)
				{
					int t = encoded;
					if (t == *epsilon_id)
					{
						continue;
					}

					if (t >= 0 && t < t_count && !(*first_table)[A * t_count + t])
					{
						(*first_table)[A * t_count + t] = true;
						changed = true;
					}

					derives_epsilon = false;
					break;
				}

				int B = encoded - g->num_terminals;
				if (B < 0 || B >= nt_count)
				{
					derives_epsilon = false;
					break;
				}

				for (int t = 0; t < t_count; t++)
				{
					if (t == *epsilon_id)
					{
						continue;
					}

					if ((*first_table)[B * t_count + t] && !(*first_table)[A * t_count + t])
					{
						(*first_table)[A * t_count + t] = true;
						changed = true;
					}
				}

				if (!(*nullable)[B])
				{
					derives_epsilon = false;
					break;
				}
			}

			if (derives_epsilon && !(*nullable)[A])
			{
				(*nullable)[A] = true;
				changed = true;
			}
		}
	}

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
	*out_first = NULL;
	int count = 0;

	for (int t = 0; t < g->num_terminals; t++)
	{
		if (first_table[non_terminal_id * g->num_terminals + t])
		{
			if (!add_symbol_to_array(out_first, &count, g->terminals[t].symbol, true))
			{
				free_symbol_array(*out_first, count);
				*out_first = NULL;
				return 0;
			}
		}
	}

	if (nullable[non_terminal_id] && epsilon_id >= 0)
	{
		if (!add_symbol_to_array(out_first, &count, g->terminals[epsilon_id].symbol, true))
		{
			free_symbol_array(*out_first, count);
			*out_first = NULL;
			return 0;
		}
	}

	return count;
}

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
	bool *nullable = NULL;
	int epsilon_id = -1;
	if (!compute_first_tables(g, &first_table, &nullable, &epsilon_id))
	{
		return 0;
	}

	int count = collect_first_for_non_terminal(g, non_terminal_id, first_table, nullable, epsilon_id, out_first);

	free(first_table);
	free(nullable);
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

#pragma endregion

