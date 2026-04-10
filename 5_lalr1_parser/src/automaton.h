#ifndef AUTOMATON_H
#define AUTOMATON_H

#include <stdbool.h>
#include "grammar.h"

typedef struct lr1_item
{
	int production_index;
	int dot_position;
	int lookahead_id;
} lr1_item;

typedef struct lr1_state
{
	lr1_item *items;
	int num_items;
	int capacity;
} lr1_state;

typedef struct lr1_transition
{
	int from_state;
	int symbol_id;
	int to_state;
} lr1_transition;

typedef struct lr1_automaton
{
	const grammar *g;
	int eof_lookahead_id;
	lr1_state *states;
	int num_states;
	int states_capacity;
	lr1_transition *transitions;
	int num_transitions;
	int transitions_capacity;
} lr1_automaton;

typedef lr1_automaton lalr1_automaton;

/**
 * @brief Initializes an empty LR(1) state.
 * @param state State object to initialize.
 * @return This function does not return a value.
 */
void init_lr1_state(lr1_state *state);

/**
 * @brief Releases all memory owned by one LR(1) state.
 * @param state State object to release.
 * @return This function does not return a value.
 */
void free_lr1_state(lr1_state *state);

/**
 * @brief Adds one item to a state if it does not already exist.
 * @param state Destination state.
 * @param item Item to append.
 * @return true when the item exists after the call, false on allocation failure.
 */
bool add_lr1_item_unique(lr1_state *state, lr1_item item);

/**
 * @brief Computes LR(1) closure(I) for a set of items in-place.
 * @param g Parsed grammar.
 * @param state Input/output item set.
 * @param eof_lookahead_id Lookahead id used as EOF marker.
 * @return true on success, false on invalid input or allocation error.
 */
bool lr1_closure(const grammar *g, lr1_state *state, int eof_lookahead_id);

/**
 * @brief Computes LR(1) GOTO(I, X) for one grammar symbol.
 * @param g Parsed grammar.
 * @param from_state Source item set I.
 * @param symbol_id Encoded grammar symbol X.
 * @param eof_lookahead_id Lookahead id used as EOF marker.
 * @param out_state Output state, overwritten by the result.
 * @return true on success, false on allocation error.
 */
bool lr1_goto(
	const grammar *g,
	const lr1_state *from_state,
	int symbol_id,
	int eof_lookahead_id,
	lr1_state *out_state);

/**
 * @brief Builds the canonical LR(1) automaton for the grammar.
 * @param g Parsed grammar.
 * @return Newly allocated automaton, or NULL on failure.
 */
lr1_automaton *build_lr1_automaton(const grammar *g);

/**
 * @brief Builds an LALR(1) automaton by merging LR(1) states with equal kernels.
 * @param g Parsed grammar.
 * @return Newly allocated LALR(1) automaton, or NULL on failure.
 */
lalr1_automaton *build_lalr1_automaton(const grammar *g);

/**
 * @brief Releases all memory owned by an LR(1) automaton object.
 * @param automaton Automaton to free.
 * @return This function does not return a value.
 */
void free_lr1_automaton(lr1_automaton *automaton);

/**
 * @brief Releases all memory owned by an LALR(1) automaton object.
 * @param automaton Automaton to free.
 * @return This function does not return a value.
 */
void free_lalr1_automaton(lalr1_automaton *automaton);

/**
 * @brief Prints one automaton using a compact item/transition format.
 * @param automaton Automaton to print.
 * @return This function does not return a value.
 */
void print_lr1_automaton(const lr1_automaton *automaton);

/**
 * @brief Prints one LALR(1) automaton.
 * @param automaton Automaton to print.
 * @return This function does not return a value.
 */
void print_lalr1_automaton(const lalr1_automaton *automaton);

#endif // AUTOMATON_H
