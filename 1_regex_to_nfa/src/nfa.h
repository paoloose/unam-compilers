#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "result.h"
#include "regex.h"
#include "da.h"

// Global node counter for unique IDs
static int g_node_id = 0;

// Create a new NFA node
static inline nfa_node* create_nfa_node() {
    nfa_node* node = (nfa_node*)calloc(1, sizeof(nfa_node));
    node->id = g_node_id++;
    node->trans_count = 0;
    node->is_accept = false;
    return node;
}

// Add a transition from 'from' to 'to' on character 'on'
static inline void add_transition(nfa_node* from, nfa_node* to, char on) {
    if (from->trans_count >= MAX_TRANSITIONS) {
        return; // Error: too many transitions
    }
    from->trans[from->trans_count].on = on;
    from->trans[from->trans_count].target = to;
    from->trans_count++;
}

// Create a basic NFA fragment for a single character
static inline nfa_fragment create_atom(char c) {
    nfa_fragment frag = {0};
    frag.start = create_nfa_node();
    frag.accept = create_nfa_node();
    add_transition(frag.start, frag.accept, c);
    return frag;
}

// Concatenate two NFA fragments (AB)
static inline nfa_fragment concat(nfa_fragment a, nfa_fragment b) {
    nfa_fragment result = {0};
    a.accept->is_accept = false;
    add_transition(a.accept, b.start, TRANSITION_EPSILON);
    result.start = a.start;
    result.accept = b.accept;
    return result;
}

// Union two NFA fragments (A|B)
static inline nfa_fragment union_nfa(nfa_fragment a, nfa_fragment b) {
    nfa_fragment result = {0};
    result.start = create_nfa_node();
    result.accept = create_nfa_node();

    a.accept->is_accept = false;
    b.accept->is_accept = false;

    add_transition(result.start, a.start, TRANSITION_EPSILON);
    add_transition(result.start, b.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);
    add_transition(b.accept, result.accept, TRANSITION_EPSILON);

    return result;
}

// One or more (a+) - same as a concatenated with a*
static inline nfa_fragment plus(nfa_fragment a) {
    nfa_fragment result = {0};
    result.start = a.start;
    result.accept = create_nfa_node();

    a.accept->is_accept = false;

    add_transition(a.accept, a.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);

    return result;
}

// Zero or one (a?) - either the pattern or epsilon
static inline nfa_fragment question(nfa_fragment a) {
    nfa_fragment result = {0};
    result.start = create_nfa_node();
    result.accept = create_nfa_node();

    a.accept->is_accept = false;

    // Path through a
    add_transition(result.start, a.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);

    // Direct epsilon path (zero occurrences)
    add_transition(result.start, result.accept, TRANSITION_EPSILON);

    return result;
}

// Kleene star (A*)
static inline nfa_fragment kleene(nfa_fragment a) {
    nfa_fragment result = {0};
    result.start = create_nfa_node();
    result.accept = create_nfa_node();

    a.accept->is_accept = false;

    add_transition(result.start, a.start, TRANSITION_EPSILON);
    add_transition(result.start, result.accept, TRANSITION_EPSILON);
    add_transition(a.accept, a.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);
    // todo: write unit tests for this

    return result;
}

// Helper function to add a state and its epsilon closure
#define MAX_STATES 1024

static void add_state_with_closure(nfa_node* node, nfa_node** states, int* count) {
    if (*count >= MAX_STATES) return;

    // Check if already in states
    for (int i = 0; i < *count; i++) {
        if (states[i] == node) return;
    }

    states[(*count)++] = node;

    // Follow epsilon transitions
    for (int i = 0; i < node->trans_count; i++) {
        if (node->trans[i].on == TRANSITION_EPSILON) {
            add_state_with_closure(node->trans[i].target, states, count);
        }
    }
}

// Simulate NFA matching
static inline bool match_nfa(nfa n, char* buf, int buflen) {
    if (!n.origin) return false;

    nfa_node* current_states[MAX_STATES];
    int current_count = 0;

    nfa_node* next_states[MAX_STATES];
    int next_count = 0;

    // Start with epsilon closure of origin
    add_state_with_closure(n.origin, current_states, &current_count);

    // Process each character in buffer
    for (int i = 0; i < buflen; i++) {
        char c = buf[i];
        next_count = 0;

        // For each current state, follow transitions on character c
        for (int j = 0; j < current_count; j++) {
            nfa_node* state = current_states[j];

            for (int k = 0; k < state->trans_count; k++) {
                if (state->trans[k].on == c) {
                    add_state_with_closure(state->trans[k].target, next_states, &next_count);
                }
            }
        }

        // Swap current and next
        for (int j = 0; j < next_count; j++) {
            current_states[j] = next_states[j];
        }
        current_count = next_count;
    }

    // Check if any current state is accepting
    bool accepted = false;
    for (int i = 0; i < current_count; i++) {
        if (current_states[i]->is_accept) {
            accepted = true;
            break;
        }
    }

    return accepted;
}

// Convert regex to NFA using Thompson algorithm
static inline result(nfa) regex_to_nfa(regex r) {
    result(nfa) res = {0};

    if (r.size == 0) {
        res.err = create_error("Empty regex");
        return res;
    }

    // Use a stack to build fragments
    da_nfa_fragment stack = {0};
    da_nfa_fragment_init(&stack, 64);

    // Process postfix regex tokens
    for (int i = 0; i < r.size; i++) {
        char token = r.items[i].value;

        if (token == '|') {
            // Pop two fragments and union them
            if (stack.length < 2) {
                res.err = create_error("Invalid regex: not enough operands for |");
                da_nfa_fragment_free(&stack);
                return res;
            }

            nfa_fragment b = *da_nfa_fragment_pop(&stack);
            nfa_fragment a = *da_nfa_fragment_pop(&stack);

            nfa_fragment result_frag = union_nfa(a, b);
            da_nfa_fragment_append(&stack, result_frag);
        }
        else if (token == '.') {
            // Pop two fragments and concatenate them
            if (stack.length < 2) {
                res.err = create_error("Invalid regex: not enough operands for .");
                da_nfa_fragment_free(&stack);
                return res;
            }

            nfa_fragment b = *da_nfa_fragment_pop(&stack);
            nfa_fragment a = *da_nfa_fragment_pop(&stack);

            nfa_fragment result_frag = concat(a, b);
            da_nfa_fragment_append(&stack, result_frag);
        }
        else if (token == '*') {
            // Pop one fragment and apply kleene star
            if (stack.length < 1) {
                res.err = create_error("Invalid regex: not enough operands for *");
                da_nfa_fragment_free(&stack);
                return res;
            }

            nfa_fragment a = *da_nfa_fragment_pop(&stack);
            nfa_fragment result_frag = kleene(a);
            da_nfa_fragment_append(&stack, result_frag);
        }
        else if (token == '+') {
            // Pop one fragment and apply plus (one or more)
            if (stack.length < 1) {
                res.err = create_error("Invalid regex: not enough operands for +");
                da_nfa_fragment_free(&stack);
                return res;
            }

            nfa_fragment a = *da_nfa_fragment_pop(&stack);
            nfa_fragment result_frag = plus(a);
            da_nfa_fragment_append(&stack, result_frag);
        }
        else if (token == '?') {
            // Pop one fragment and apply question mark (zero or one) (is this working?)
            if (stack.length < 1) {
                res.err = create_error("Invalid regex: not enough operands for ?");
                da_nfa_fragment_free(&stack);
                return res;
            }

            nfa_fragment a = *da_nfa_fragment_pop(&stack);
            nfa_fragment result_frag = question(a);
            da_nfa_fragment_append(&stack, result_frag);
        }
        else {
            // Operand: create atom
            nfa_fragment atom = create_atom(token);
            da_nfa_fragment_append(&stack, atom);
        }
    }

    // Should have exactly one fragment on stack!!!!
    if (stack.length != 1) {
        res.err = create_error("Invalid regex: incorrect number of operands");
        da_nfa_fragment_free(&stack);
        return res;
    }

    nfa_fragment final_frag = stack.data[0];
    final_frag.accept->is_accept = true;

    res.val.origin = final_frag.start;
    res.val.node_count = g_node_id;

    da_nfa_fragment_free(&stack);

    return res;
}
