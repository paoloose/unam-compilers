#include "automaton.h"

#include <stdio.h>

typedef struct first_context
{
	bool *first_table;
	bool *nullable;
	int epsilon_id;
} first_context;

typedef struct core_item
{
	int production_index;
	int dot_position;
} core_item;

typedef struct kernel_signature
{
	core_item *items;
	int count;
} kernel_signature;

static bool ensure_state_capacity(lr1_state *state, int min_capacity);
static int find_terminal_id(const grammar *g, const char *name);
static bool build_first_context(const grammar *g, first_context *ctx);
static void free_first_context(first_context *ctx);
static int get_item_rhs_symbol(const grammar *g, const lr1_item *item, int offset);
static int get_item_rhs_length(const grammar *g, const lr1_item *item);
static bool compute_first_of_suffix_with_lookahead(
	const grammar *g,
	const first_context *ctx,
	const lr1_item *item,
	int non_terminal_position,
	int eof_lookahead_id,
	bool *out_lookaheads,
	int lookahead_capacity);
static bool add_transition_unique(lr1_automaton *automaton, int from_state, int symbol_id, int to_state);
static bool ensure_states_capacity(lr1_automaton *automaton, int min_capacity);
static bool ensure_transitions_capacity(lr1_automaton *automaton, int min_capacity);
static int compare_lr1_items(const void *a, const void *b);
static int compare_core_items(const void *a, const void *b);
static void sort_state_items(lr1_state *state);
static bool states_equal(const lr1_state *left, const lr1_state *right);
static int find_state_index(const lr1_automaton *automaton, const lr1_state *state);
static bool append_state_copy(lr1_automaton *automaton, const lr1_state *state, int *out_index);
static bool collect_goto_symbols(const grammar *g, const lr1_state *state, bool *symbols_out, int symbols_count);
static bool build_kernel_signature(const lr1_state *state, kernel_signature *signature);
static void free_kernel_signature(kernel_signature *signature);
static bool equal_kernel_signatures(const kernel_signature *left, const kernel_signature *right);
static bool build_state_group_map(const lr1_automaton *lr1, int **out_state_group, int *out_group_count);
static bool merge_group_items(const lr1_automaton *lr1, const int *state_group, int group_id, lr1_state *out_state);
static bool symbol_is_terminal(const grammar *g, int encoded_symbol_id);
static const char *symbol_name(const grammar *g, int encoded_symbol_id);
static const char *lookahead_name(const lr1_automaton *automaton, int lookahead_id);

void init_lr1_state(lr1_state *state)
{
	if (state == NULL)
	{
		return;
	}

	state->items = NULL;
	state->num_items = 0;
	state->capacity = 0;
}

void free_lr1_state(lr1_state *state)
{
	if (state == NULL)
	{
		return;
	}

	free(state->items);
	state->items = NULL;
	state->num_items = 0;
	state->capacity = 0;
}

bool add_lr1_item_unique(lr1_state *state, lr1_item item)
{
	if (state == NULL)
	{
		return false;
	}

	for (int i = 0; i < state->num_items; i++)
	{
		const lr1_item existing = state->items[i];
		if (existing.production_index == item.production_index &&
			existing.dot_position == item.dot_position &&
			existing.lookahead_id == item.lookahead_id)
		{
			return true;
		}
	}

	if (!ensure_state_capacity(state, state->num_items + 1))
	{
		return false;
	}

	state->items[state->num_items++] = item;
	return true;
}

bool lr1_closure(const grammar *g, lr1_state *state, int eof_lookahead_id)
{
	if (g == NULL || state == NULL)
	{
		return false;
	}

	first_context ctx = {0};
	if (!build_first_context(g, &ctx))
	{
		return false;
	}

	const int lookahead_count = g->num_terminals + 1;
	bool *lookahead_buffer = (bool *)calloc((size_t)lookahead_count, sizeof(bool));
	if (lookahead_buffer == NULL)
	{
		free_first_context(&ctx);
		return false;
	}

	bool changed = true;
	while (changed)
	{
		changed = false;

		// Snapshot current size because closure expansion can append new items.
		const int base_count = state->num_items;
		for (int item_index = 0; item_index < base_count; item_index++)
		{
			lr1_item item = state->items[item_index];

			int next_symbol = get_item_rhs_symbol(g, &item, item.dot_position);
			if (next_symbol < g->num_terminals)
			{
				continue;
			}

			int non_terminal_id = next_symbol - g->num_terminals;
			if (non_terminal_id < 0 || non_terminal_id >= g->num_non_terminals)
			{
				continue;
			}

			memset(lookahead_buffer, 0, (size_t)lookahead_count * sizeof(bool));
			if (!compute_first_of_suffix_with_lookahead(
					g,
					&ctx,
					&item,
					item.dot_position,
					eof_lookahead_id,
					lookahead_buffer,
					lookahead_count))
			{
				free(lookahead_buffer);
				free_first_context(&ctx);
				return false;
			}

			for (int production_index = 0; production_index < g->num_productions; production_index++)
			{
				if (g->productions[production_index].non_terminal_id != non_terminal_id)
				{
					continue;
				}

				for (int la = 0; la < lookahead_count; la++)
				{
					if (!lookahead_buffer[la])
					{
						continue;
					}

					const int before_count = state->num_items;
					lr1_item new_item;
					new_item.production_index = production_index;
					new_item.dot_position = 0;
					new_item.lookahead_id = la;

					if (!add_lr1_item_unique(state, new_item))
					{
						free(lookahead_buffer);
						free_first_context(&ctx);
						return false;
					}

					if (state->num_items > before_count)
					{
						changed = true;
					}
				}
			}
		}
	}

	sort_state_items(state);
	free(lookahead_buffer);
	free_first_context(&ctx);
	return true;
}

bool lr1_goto(
	const grammar *g,
	const lr1_state *from_state,
	int symbol_id,
	int eof_lookahead_id,
	lr1_state *out_state)
{
	if (g == NULL || from_state == NULL || out_state == NULL)
	{
		return false;
	}

	free_lr1_state(out_state);
	init_lr1_state(out_state);

	for (int i = 0; i < from_state->num_items; i++)
	{
		lr1_item item = from_state->items[i];
		int next_symbol = get_item_rhs_symbol(g, &item, item.dot_position);
		if (next_symbol != symbol_id)
		{
			continue;
		}

		item.dot_position++;
		if (!add_lr1_item_unique(out_state, item))
		{
			free_lr1_state(out_state);
			return false;
		}
	}

	if (out_state->num_items == 0)
	{
		return true;
	}

	return lr1_closure(g, out_state, eof_lookahead_id);
}

lr1_automaton *build_lr1_automaton(const grammar *g)
{
	if (g == NULL || g->num_non_terminals <= 0)
	{
		return NULL;
	}

	lr1_automaton *automaton = (lr1_automaton *)calloc(1, sizeof(lr1_automaton));
	if (automaton == NULL)
	{
		return NULL;
	}

	automaton->g = g;
	automaton->eof_lookahead_id = g->num_terminals;

	lr1_state start_state;
	init_lr1_state(&start_state);

	lr1_item start_item;
	start_item.production_index = -1;
	start_item.dot_position = 0;
	start_item.lookahead_id = automaton->eof_lookahead_id;

	if (!add_lr1_item_unique(&start_state, start_item) ||
		!lr1_closure(g, &start_state, automaton->eof_lookahead_id))
	{
		free_lr1_state(&start_state);
		free_lr1_automaton(automaton);
		return NULL;
	}

	int initial_index = -1;
	if (!append_state_copy(automaton, &start_state, &initial_index))
	{
		free_lr1_state(&start_state);
		free_lr1_automaton(automaton);
		return NULL;
	}
	free_lr1_state(&start_state);

	const int symbols_count = g->num_terminals + g->num_non_terminals;
	bool *symbols = (bool *)calloc((size_t)symbols_count, sizeof(bool));
	if (symbols == NULL)
	{
		free_lr1_automaton(automaton);
		return NULL;
	}

	for (int state_id = 0; state_id < automaton->num_states; state_id++)
	{
		memset(symbols, 0, (size_t)symbols_count * sizeof(bool));
		if (!collect_goto_symbols(g, &automaton->states[state_id], symbols, symbols_count))
		{
			free(symbols);
			free_lr1_automaton(automaton);
			return NULL;
		}

		for (int symbol_id = 0; symbol_id < symbols_count; symbol_id++)
		{
			if (!symbols[symbol_id])
			{
				continue;
			}

			lr1_state goto_state;
			init_lr1_state(&goto_state);

			if (!lr1_goto(g, &automaton->states[state_id], symbol_id, automaton->eof_lookahead_id, &goto_state))
			{
				free_lr1_state(&goto_state);
				free(symbols);
				free_lr1_automaton(automaton);
				return NULL;
			}

			if (goto_state.num_items == 0)
			{
				free_lr1_state(&goto_state);
				continue;
			}

			int target_id = find_state_index(automaton, &goto_state);
			if (target_id < 0)
			{
				if (!append_state_copy(automaton, &goto_state, &target_id))
				{
					free_lr1_state(&goto_state);
					free(symbols);
					free_lr1_automaton(automaton);
					return NULL;
				}
			}

			if (!add_transition_unique(automaton, state_id, symbol_id, target_id))
			{
				free_lr1_state(&goto_state);
				free(symbols);
				free_lr1_automaton(automaton);
				return NULL;
			}

			free_lr1_state(&goto_state);
		}
	}

	free(symbols);
	return automaton;
}

lalr1_automaton *build_lalr1_automaton(const grammar *g)
{
	lr1_automaton *lr1 = build_lr1_automaton(g);
	if (lr1 == NULL)
	{
		return NULL;
	}

	int *state_group = NULL;
	int group_count = 0;
	if (!build_state_group_map(lr1, &state_group, &group_count))
	{
		free_lr1_automaton(lr1);
		return NULL;
	}

	lalr1_automaton *lalr = (lalr1_automaton *)calloc(1, sizeof(lalr1_automaton));
	if (lalr == NULL)
	{
		free(state_group);
		free_lr1_automaton(lr1);
		return NULL;
	}

	lalr->g = g;
	lalr->eof_lookahead_id = lr1->eof_lookahead_id;

	if (!ensure_states_capacity(lalr, group_count))
	{
		free(state_group);
		free_lr1_automaton(lr1);
		free_lalr1_automaton(lalr);
		return NULL;
	}

	for (int group = 0; group < group_count; group++)
	{
		init_lr1_state(&lalr->states[group]);
		if (!merge_group_items(lr1, state_group, group, &lalr->states[group]))
		{
			free(state_group);
			free_lr1_automaton(lr1);
			free_lalr1_automaton(lalr);
			return NULL;
		}
	}
	lalr->num_states = group_count;

	for (int i = 0; i < lr1->num_transitions; i++)
	{
		lr1_transition t = lr1->transitions[i];
		int merged_from = state_group[t.from_state];
		int merged_to = state_group[t.to_state];

		if (!add_transition_unique(lalr, merged_from, t.symbol_id, merged_to))
		{
			free(state_group);
			free_lr1_automaton(lr1);
			free_lalr1_automaton(lalr);
			return NULL;
		}
	}

	free(state_group);
	free_lr1_automaton(lr1);
	return lalr;
}

void free_lr1_automaton(lr1_automaton *automaton)
{
	if (automaton == NULL)
	{
		return;
	}

	for (int i = 0; i < automaton->num_states; i++)
	{
		free_lr1_state(&automaton->states[i]);
	}

	free(automaton->states);
	free(automaton->transitions);
	free(automaton);
}

void free_lalr1_automaton(lalr1_automaton *automaton)
{
	free_lr1_automaton(automaton);
}

void print_lr1_automaton(const lr1_automaton *automaton)
{
	if (automaton == NULL || automaton->g == NULL)
	{
		return;
	}

	printf("States: %d\n", automaton->num_states);
	for (int state_id = 0; state_id < automaton->num_states; state_id++)
	{
		const lr1_state *state = &automaton->states[state_id];
		printf("I%d:\n", state_id);

		for (int i = 0; i < state->num_items; i++)
		{
			const lr1_item item = state->items[i];

			if (item.production_index < 0)
			{
				printf("  S' -> ");
				if (item.dot_position == 0)
				{
					printf(". ");
				}
				printf("%s", automaton->g->non_terminals[0].symbol);
				if (item.dot_position == 1)
				{
					printf(" .");
				}
				printf(", %s\n", lookahead_name(automaton, item.lookahead_id));
				continue;
			}

			const production p = automaton->g->productions[item.production_index];
			printf("  %s ->", automaton->g->non_terminals[p.non_terminal_id].symbol);
			for (int j = 0; j <= p.production_length; j++)
			{
				if (j == item.dot_position)
				{
					printf(" .");
				}
				if (j < p.production_length)
				{
					int encoded = p.production_symbol_ids[j];
					printf(" %s", symbol_name(automaton->g, encoded));
				}
			}
			printf(", %s\n", lookahead_name(automaton, item.lookahead_id));
		}
	}

	printf("Transitions:\n");
	for (int i = 0; i < automaton->num_transitions; i++)
	{
		lr1_transition t = automaton->transitions[i];
		printf("  I%d -- %s --> I%d\n", t.from_state, symbol_name(automaton->g, t.symbol_id), t.to_state);
	}
}

void print_lalr1_automaton(const lalr1_automaton *automaton)
{
	print_lr1_automaton(automaton);
}

static bool ensure_state_capacity(lr1_state *state, int min_capacity)
{
	if (state->capacity >= min_capacity)
	{
		return true;
	}

	int new_capacity = state->capacity == 0 ? 8 : state->capacity;
	while (new_capacity < min_capacity)
	{
		new_capacity *= 2;
	}

	lr1_item *resized = (lr1_item *)realloc(state->items, (size_t)new_capacity * sizeof(lr1_item));
	if (resized == NULL)
	{
		return false;
	}

	state->items = resized;
	state->capacity = new_capacity;
	return true;
}

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

static bool build_first_context(const grammar *g, first_context *ctx)
{
	if (g == NULL || ctx == NULL)
	{
		return false;
	}

	const int nt_count = g->num_non_terminals;
	const int t_count = g->num_terminals;
	if (nt_count <= 0 || t_count <= 0)
	{
		return false;
	}

	ctx->first_table = (bool *)calloc((size_t)nt_count * (size_t)t_count, sizeof(bool));
	ctx->nullable = (bool *)calloc((size_t)nt_count, sizeof(bool));
	if (ctx->first_table == NULL || ctx->nullable == NULL)
	{
		free_first_context(ctx);
		return false;
	}

	ctx->epsilon_id = find_terminal_id(g, "epsilon");

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
				if (!ctx->nullable[A])
				{
					ctx->nullable[A] = true;
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
					if (t == ctx->epsilon_id)
					{
						continue;
					}

					if (t >= 0 && t < t_count && !ctx->first_table[A * t_count + t])
					{
						ctx->first_table[A * t_count + t] = true;
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
					if (t == ctx->epsilon_id)
					{
						continue;
					}

					if (ctx->first_table[B * t_count + t] && !ctx->first_table[A * t_count + t])
					{
						ctx->first_table[A * t_count + t] = true;
						changed = true;
					}
				}

				if (!ctx->nullable[B])
				{
					derives_epsilon = false;
					break;
				}
			}

			if (derives_epsilon && !ctx->nullable[A])
			{
				ctx->nullable[A] = true;
				changed = true;
			}
		}
	}

	return true;
}

static void free_first_context(first_context *ctx)
{
	if (ctx == NULL)
	{
		return;
	}

	free(ctx->first_table);
	free(ctx->nullable);
	ctx->first_table = NULL;
	ctx->nullable = NULL;
	ctx->epsilon_id = -1;
}

static int get_item_rhs_symbol(const grammar *g, const lr1_item *item, int offset)
{
	if (g == NULL || item == NULL)
	{
		return -1;
	}

	if (item->production_index < 0)
	{
		if (offset == 0)
		{
			return g->num_terminals;
		}
		return -1;
	}

	if (item->production_index >= g->num_productions)
	{
		return -1;
	}

	production p = g->productions[item->production_index];
	if (offset < 0 || offset >= p.production_length)
	{
		return -1;
	}

	return p.production_symbol_ids[offset];
}

static int get_item_rhs_length(const grammar *g, const lr1_item *item)
{
	if (g == NULL || item == NULL)
	{
		return 0;
	}

	if (item->production_index < 0)
	{
		return 1;
	}

	if (item->production_index >= g->num_productions)
	{
		return 0;
	}

	return g->productions[item->production_index].production_length;
}

static bool compute_first_of_suffix_with_lookahead(
	const grammar *g,
	const first_context *ctx,
	const lr1_item *item,
	int non_terminal_position,
	int eof_lookahead_id,
	bool *out_lookaheads,
	int lookahead_capacity)
{
	if (g == NULL || ctx == NULL || item == NULL || out_lookaheads == NULL)
	{
		return false;
	}

	const int rhs_length = get_item_rhs_length(g, item);
	bool all_nullable = true;

	for (int index = non_terminal_position + 1; index < rhs_length; index++)
	{
		int encoded = get_item_rhs_symbol(g, item, index);

		if (encoded < g->num_terminals)
		{
			if (encoded != ctx->epsilon_id)
			{
				if (encoded >= 0 && encoded < lookahead_capacity)
				{
					out_lookaheads[encoded] = true;
				}
				all_nullable = false;
				break;
			}

			continue;
		}

		int non_terminal = encoded - g->num_terminals;
		if (non_terminal < 0 || non_terminal >= g->num_non_terminals)
		{
			all_nullable = false;
			break;
		}

		for (int terminal = 0; terminal < g->num_terminals; terminal++)
		{
			if (terminal == ctx->epsilon_id)
			{
				continue;
			}

			if (ctx->first_table[non_terminal * g->num_terminals + terminal])
			{
				out_lookaheads[terminal] = true;
			}
		}

		if (!ctx->nullable[non_terminal])
		{
			all_nullable = false;
			break;
		}
	}

	if (all_nullable)
	{
		if (item->lookahead_id == eof_lookahead_id)
		{
			out_lookaheads[eof_lookahead_id] = true;
		}
		else if (item->lookahead_id >= 0 && item->lookahead_id < g->num_terminals)
		{
			out_lookaheads[item->lookahead_id] = true;
		}
	}

	return true;
}

static bool add_transition_unique(lr1_automaton *automaton, int from_state, int symbol_id, int to_state)
{
	if (automaton == NULL)
	{
		return false;
	}

	for (int i = 0; i < automaton->num_transitions; i++)
	{
		lr1_transition existing = automaton->transitions[i];
		if (existing.from_state == from_state && existing.symbol_id == symbol_id && existing.to_state == to_state)
		{
			return true;
		}
	}

	if (!ensure_transitions_capacity(automaton, automaton->num_transitions + 1))
	{
		return false;
	}

	lr1_transition transition;
	transition.from_state = from_state;
	transition.symbol_id = symbol_id;
	transition.to_state = to_state;

	automaton->transitions[automaton->num_transitions++] = transition;
	return true;
}

static bool ensure_states_capacity(lr1_automaton *automaton, int min_capacity)
{
	if (automaton->states_capacity >= min_capacity)
	{
		return true;
	}

	int new_capacity = automaton->states_capacity == 0 ? 8 : automaton->states_capacity;
	while (new_capacity < min_capacity)
	{
		new_capacity *= 2;
	}

	lr1_state *resized = (lr1_state *)realloc(automaton->states, (size_t)new_capacity * sizeof(lr1_state));
	if (resized == NULL)
	{
		return false;
	}

	for (int i = automaton->states_capacity; i < new_capacity; i++)
	{
		init_lr1_state(&resized[i]);
	}

	automaton->states = resized;
	automaton->states_capacity = new_capacity;
	return true;
}

static bool ensure_transitions_capacity(lr1_automaton *automaton, int min_capacity)
{
	if (automaton->transitions_capacity >= min_capacity)
	{
		return true;
	}

	int new_capacity = automaton->transitions_capacity == 0 ? 16 : automaton->transitions_capacity;
	while (new_capacity < min_capacity)
	{
		new_capacity *= 2;
	}

	lr1_transition *resized =
		(lr1_transition *)realloc(automaton->transitions, (size_t)new_capacity * sizeof(lr1_transition));
	if (resized == NULL)
	{
		return false;
	}

	automaton->transitions = resized;
	automaton->transitions_capacity = new_capacity;
	return true;
}

static int compare_lr1_items(const void *a, const void *b)
{
	const lr1_item *left = (const lr1_item *)a;
	const lr1_item *right = (const lr1_item *)b;

	if (left->production_index != right->production_index)
	{
		return left->production_index - right->production_index;
	}
	if (left->dot_position != right->dot_position)
	{
		return left->dot_position - right->dot_position;
	}
	return left->lookahead_id - right->lookahead_id;
}

static int compare_core_items(const void *a, const void *b)
{
	const core_item *left = (const core_item *)a;
	const core_item *right = (const core_item *)b;

	if (left->production_index != right->production_index)
	{
		return left->production_index - right->production_index;
	}
	return left->dot_position - right->dot_position;
}

static void sort_state_items(lr1_state *state)
{
	if (state == NULL || state->num_items <= 1)
	{
		return;
	}

	qsort(state->items, (size_t)state->num_items, sizeof(lr1_item), compare_lr1_items);
}

static bool states_equal(const lr1_state *left, const lr1_state *right)
{
	if (left == NULL || right == NULL)
	{
		return false;
	}

	if (left->num_items != right->num_items)
	{
		return false;
	}

	for (int i = 0; i < left->num_items; i++)
	{
		lr1_item li = left->items[i];
		lr1_item ri = right->items[i];
		if (li.production_index != ri.production_index ||
			li.dot_position != ri.dot_position ||
			li.lookahead_id != ri.lookahead_id)
		{
			return false;
		}
	}

	return true;
}

static int find_state_index(const lr1_automaton *automaton, const lr1_state *state)
{
	if (automaton == NULL || state == NULL)
	{
		return -1;
	}

	for (int i = 0; i < automaton->num_states; i++)
	{
		if (states_equal(&automaton->states[i], state))
		{
			return i;
		}
	}

	return -1;
}

static bool append_state_copy(lr1_automaton *automaton, const lr1_state *state, int *out_index)
{
	if (automaton == NULL || state == NULL || out_index == NULL)
	{
		return false;
	}

	if (!ensure_states_capacity(automaton, automaton->num_states + 1))
	{
		return false;
	}

	lr1_state *destination = &automaton->states[automaton->num_states];
	free_lr1_state(destination);
	init_lr1_state(destination);

	if (state->num_items > 0)
	{
		destination->items = (lr1_item *)malloc((size_t)state->num_items * sizeof(lr1_item));
		if (destination->items == NULL)
		{
			return false;
		}
		memcpy(destination->items, state->items, (size_t)state->num_items * sizeof(lr1_item));
		destination->num_items = state->num_items;
		destination->capacity = state->num_items;
	}

	*out_index = automaton->num_states;
	automaton->num_states++;
	return true;
}

static bool collect_goto_symbols(const grammar *g, const lr1_state *state, bool *symbols_out, int symbols_count)
{
	if (g == NULL || state == NULL || symbols_out == NULL)
	{
		return false;
	}

	for (int i = 0; i < state->num_items; i++)
	{
		const lr1_item item = state->items[i];
		int next = get_item_rhs_symbol(g, &item, item.dot_position);
		if (next >= 0 && next < symbols_count)
		{
			symbols_out[next] = true;
		}
	}

	return true;
}

static bool build_kernel_signature(const lr1_state *state, kernel_signature *signature)
{
	if (state == NULL || signature == NULL)
	{
		return false;
	}

	signature->items = NULL;
	signature->count = 0;

	for (int i = 0; i < state->num_items; i++)
	{
		lr1_item item = state->items[i];
		bool in_kernel = (item.production_index < 0) || (item.dot_position > 0);
		if (!in_kernel)
		{
			continue;
		}

		core_item core;
		core.production_index = item.production_index;
		core.dot_position = item.dot_position;

		core_item *resized =
			(core_item *)realloc(signature->items, (size_t)(signature->count + 1) * sizeof(core_item));
		if (resized == NULL)
		{
			free_kernel_signature(signature);
			return false;
		}
		signature->items = resized;
		signature->items[signature->count++] = core;
	}

	if (signature->count <= 1)
	{
		return true;
	}

	qsort(signature->items, (size_t)signature->count, sizeof(core_item), compare_core_items);

	int write_index = 1;
	for (int read_index = 1; read_index < signature->count; read_index++)
	{
		core_item prev = signature->items[write_index - 1];
		core_item cur = signature->items[read_index];
		if (prev.production_index == cur.production_index && prev.dot_position == cur.dot_position)
		{
			continue;
		}
		signature->items[write_index++] = cur;
	}
	signature->count = write_index;

	return true;
}

static void free_kernel_signature(kernel_signature *signature)
{
	if (signature == NULL)
	{
		return;
	}

	free(signature->items);
	signature->items = NULL;
	signature->count = 0;
}

static bool equal_kernel_signatures(const kernel_signature *left, const kernel_signature *right)
{
	if (left == NULL || right == NULL)
	{
		return false;
	}

	if (left->count != right->count)
	{
		return false;
	}

	for (int i = 0; i < left->count; i++)
	{
		core_item l = left->items[i];
		core_item r = right->items[i];
		if (l.production_index != r.production_index || l.dot_position != r.dot_position)
		{
			return false;
		}
	}

	return true;
}

static bool build_state_group_map(const lr1_automaton *lr1, int **out_state_group, int *out_group_count)
{
	if (lr1 == NULL || out_state_group == NULL || out_group_count == NULL)
	{
		return false;
	}

	int *state_group = (int *)malloc((size_t)lr1->num_states * sizeof(int));
	kernel_signature *group_signatures = (kernel_signature *)calloc((size_t)lr1->num_states, sizeof(kernel_signature));
	if (state_group == NULL || group_signatures == NULL)
	{
		free(state_group);
		free(group_signatures);
		return false;
	}

	int group_count = 0;
	for (int s = 0; s < lr1->num_states; s++)
	{
		kernel_signature sig;
		if (!build_kernel_signature(&lr1->states[s], &sig))
		{
			for (int i = 0; i < group_count; i++)
			{
				free_kernel_signature(&group_signatures[i]);
			}
			free(group_signatures);
			free(state_group);
			return false;
		}

		int assigned_group = -1;
		for (int g = 0; g < group_count; g++)
		{
			if (equal_kernel_signatures(&sig, &group_signatures[g]))
			{
				assigned_group = g;
				break;
			}
		}

		if (assigned_group < 0)
		{
			assigned_group = group_count;
			group_signatures[group_count] = sig;
			group_count++;
		}
		else
		{
			free_kernel_signature(&sig);
		}

		state_group[s] = assigned_group;
	}

	for (int i = 0; i < group_count; i++)
	{
		free_kernel_signature(&group_signatures[i]);
	}
	free(group_signatures);

	*out_state_group = state_group;
	*out_group_count = group_count;
	return true;
}

static bool merge_group_items(const lr1_automaton *lr1, const int *state_group, int group_id, lr1_state *out_state)
{
	if (lr1 == NULL || state_group == NULL || out_state == NULL)
	{
		return false;
	}

	free_lr1_state(out_state);
	init_lr1_state(out_state);

	for (int s = 0; s < lr1->num_states; s++)
	{
		if (state_group[s] != group_id)
		{
			continue;
		}

		const lr1_state *source = &lr1->states[s];
		for (int i = 0; i < source->num_items; i++)
		{
			if (!add_lr1_item_unique(out_state, source->items[i]))
			{
				return false;
			}
		}
	}

	sort_state_items(out_state);
	return true;
}

static bool symbol_is_terminal(const grammar *g, int encoded_symbol_id)
{
	return encoded_symbol_id >= 0 && encoded_symbol_id < g->num_terminals;
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

static const char *lookahead_name(const lr1_automaton *automaton, int lookahead_id)
{
	if (automaton == NULL || automaton->g == NULL)
	{
		return "?";
	}

	if (lookahead_id == automaton->eof_lookahead_id)
	{
		return "$";
	}

	if (lookahead_id >= 0 && lookahead_id < automaton->g->num_terminals)
	{
		return automaton->g->terminals[lookahead_id].symbol;
	}

	return "?";
}
