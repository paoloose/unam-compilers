#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "result.h"
#include "regex.h"
#include "da.h"

static int global_node_id = 0;

static void free_nfa_node(nfa_node* n, da_pointer* visited);

static void free_fragment(nfa_fragment* f) {
    da_pointer visited_nodes = {0};
    da_pointer_init(&visited_nodes, 16);

    free_nfa_node(f->start, &visited_nodes);

    bool end_was_freed = false;

    for (int j = 0; j < visited_nodes.length; j++) {
        if (visited_nodes.data[j] == (void*)f->accept) {
            end_was_freed = true;
        }
    }

    must(be_true(end_was_freed), "Tried to free a malformed fragment: start and end were not connected");

    da_free(&visited_nodes);
}

static void free_nfa_node(nfa_node* n, da_pointer* visited) {
    must(be_not_null(n), "Trying to free a null fragment");

    // checking if it was already freed
    for (int j = 0; j < visited->length; j++) {
        if (visited->data[j] == (void*)n) {
            return; // means already freed
        }
    }

    da_pointer_append(visited, n);

    for (int i = 0; i < n->trans_count; i++) {
        free_nfa_node(n->trans[i].target, visited);
    }

    free(n);
}

static nfa_node* alloc_nfa_node() {
    nfa_node* n = calloc(sizeof(nfa_node), 1);
    must(be_not_null(n));
    n->id = global_node_id++;
    n->trans_count = 0;
    n->is_accept = false;
    return n;
}

static void add_transition(nfa_node* from, nfa_node* to, char on) {
    must(be_less_equal_than(from->trans_count, MAX_TRANSITIONS), "Tried to add more transitions than supported");

    from->trans[from->trans_count].on = on;
    from->trans[from->trans_count].target = to;
    from->trans_count++;
}

static nfa_fragment fragment_atom(char symbol) {
    nfa_fragment f = { .start = alloc_nfa_node(), .accept = alloc_nfa_node() };
    add_transition(f.start, f.accept, symbol);
    return f;
}

static nfa_fragment fragment_kleene_star(nfa_fragment fparam) {
    nfa_fragment f = { .start = alloc_nfa_node(), .accept = alloc_nfa_node() };
    add_transition(f.start, fparam.start, TRANSITION_EPSILON);
    add_transition(f.start, f.accept, TRANSITION_EPSILON);
    add_transition(fparam.accept, fparam.start, TRANSITION_EPSILON);
    add_transition(fparam.accept, f.accept, TRANSITION_EPSILON);
    return f;
}

static nfa_fragment fragment_plus(nfa_fragment fparam) {
    nfa_fragment f = { .start = fparam.start, .accept = alloc_nfa_node() };

    add_transition(fparam.accept, fparam.start, TRANSITION_EPSILON);
    add_transition(fparam.accept, f.accept, TRANSITION_EPSILON);
    return f;
}

// This equivalence applies for the optional operator: a? = (a | epsilon)
static nfa_fragment fragment_union(nfa_fragment f1, nfa_fragment f2) {
    nfa_fragment f = { .start = alloc_nfa_node(), .accept = alloc_nfa_node() };

    add_transition(f.start, f1.start, TRANSITION_EPSILON);
    add_transition(f1.accept, f.accept, TRANSITION_EPSILON);
    add_transition(f.start, f2.start, TRANSITION_EPSILON);
    add_transition(f2.accept, f.accept, TRANSITION_EPSILON);
    return f;
}

// This equivalence applies for the optional operator: a? = (a | epsilon)
static nfa_fragment fragment_optional(nfa_fragment fparam) {
    return fragment_union(fparam, fragment_atom(TRANSITION_EPSILON));
}

static nfa_fragment fragment_concat(nfa_fragment f1, nfa_fragment f2) {
    nfa_fragment f = { .start = f1.start, .accept = f2.accept };
    add_transition(f1.accept, f2.start, TRANSITION_EPSILON);
    return f;
}

static void print_fragments(da_nfa_fragment* frags) {
    UNAM_DEBUG("[");
    for (int i = 0; i < frags->length; i++) {
        UNAM_DEBUG("%p, ", &frags->data[i]);
    }
    UNAM_DEBUG("]\n");
}

static void _traverse_nfa(nfa_node* node, da_nfa_node* visited_nodes) {
    for (int i = 0; i < visited_nodes->length; i++) {
        if (node == visited_nodes->data[i]) {
            return;
        }
    }

    da_nfa_node_append(visited_nodes, node);

    for (int t = 0; t < node->trans_count; t++) {
        _traverse_nfa(node->trans[t].target, visited_nodes);
    }
}

static void print_nfa(nfa automata) {
    da_nfa_node nodes = {0};
    da_nfa_node_init(&nodes, automata.node_count);

    _traverse_nfa(automata.origin, &nodes);

    for (int n = 0; n < nodes.length; n++) {
        nfa_node* node = nodes.data[n];
        UNAM_DEBUG("node(%d)%s\n", node->id, node->is_accept ? " ⭐" : "");
        for (int t = 0; t < node->trans_count; t++) {
            transition trans = node->trans[t];
            UNAM_DEBUG(" -%c->(%d)\n", trans.on, trans.target->id);
        }
    }
}

// Convert regex to NFA using Thompson algorithm
static result(nfa) regex_to_nfa(regex r) {
    result(nfa) res = {0};

    if (r.size == 0) {
        res.err = create_error("Empty regex");
        return res;
    }

    // Start popping the operands and evaluating them

    da_nfa_fragment fragments = {0};
    da_nfa_fragment_init(&fragments, r.size);

    UNAM_DEBUG("=== Building NFA ===\n");

    for (int i = 0; i < r.size; i++) {
        char symbol = r.items[i].value;

        result(input_op) operator = get_operator_from_symbol(symbol);

        if (is_ok(operator)) {
            UNAM_DEBUG("Got operator: %c\n", operator.val.symbol);

            if (operator.val.arguments == 1) {
                nfa_fragment* f1 = da_nfa_fragment_pop(&fragments);
                must(be_not_null(f1));
                switch (symbol) {
                case '*': {
                    da_nfa_fragment_append(&fragments, fragment_kleene_star(*f1));
                    break;
                }
                case '+': {
                    da_nfa_fragment_append(&fragments, fragment_plus(*f1));
                    break;
                }
                case '?': {
                    da_nfa_fragment_append(&fragments, fragment_optional(*f1));
                    break;
                }
                default: {
                    return_err(nfa, "Got invalid unary operator: '%c'", symbol);
                }
                }
            }
            else if (operator.val.arguments == 2) {
                nfa_fragment* f2 = da_nfa_fragment_pop(&fragments);
                nfa_fragment* f1 = da_nfa_fragment_pop(&fragments);
                must(be_not_null(f1));
                must(be_not_null(f2));
                switch (symbol) {
                case '.': {
                    da_nfa_fragment_append(&fragments, fragment_concat(*f1, *f2));
                    break;
                }
                case '|': {
                    da_nfa_fragment_append(&fragments, fragment_union(*f1, *f2));
                    break;
                }
                default: {
                    return_err(nfa, "Got invalid binary operator: '%c'", symbol);
                }
                }
            }
        } else {
            nfa_fragment frag = fragment_atom(symbol);
            UNAM_DEBUG("Got operand: %c\n", symbol);
            da_nfa_fragment_append(&fragments, frag);
        }
    }

    if (fragments.length != 1) {
        da_free(&fragments);
        for (int i = 0; i < fragments.length; i++) {
            free_fragment(&fragments.data[i]);
        }
        return_err(nfa, "Postfix regex is malformed: the number of operators didn't match the operant's")
    }

    fragments.data[0].accept->is_accept = true;

    res.val.origin = fragments.data[0].start;
    res.val.node_count = global_node_id;
    da_free(&fragments);

    UNAM_DEBUGP("\n");
    UNAM_DEBUG("== Parsed NFA\n");
    print_nfa(res.val);
    UNAM_DEBUG("\n");

    return res;
}

static void move_over_closures(nfa_node* node, da_nfa_node* visited_nodes) {
    for (int i = 0; i < visited_nodes->length; i++) {
        if (visited_nodes->data[i] == node) {
            return;
        }
    }
    UNAM_DEBUG("ε closure for node %d:\n", node->id);

    da_nfa_node_append(visited_nodes, node);

    for (int t = 0; t < node->trans_count; t++) {
        transition* trans = &node->trans[t];
        UNAM_DEBUG(" - transition #%d: (%d) -%c-> (%d)\n", t, node->id, trans->on, trans->target->id);
    }

    for (int t = 0; t < node->trans_count; t++) {
        transition* trans = &node->trans[t];
        if (node->trans[t].on == TRANSITION_EPSILON) {
            move_over_closures(node->trans[t].target, visited_nodes);
        }
    }
}

// Simulate NFA matching
static bool match_nfa(nfa n, char* buf, int buflen) {
    if (!n.origin) return false;
    int capacity = buflen > 0 ? buflen : 4;

    da_nfa_node current_states = {0};
    da_nfa_node new_states = {0};
    da_nfa_node_init(&current_states, capacity);
    da_nfa_node_init(&new_states, capacity);

    nfa_node* origin_node = n.origin;
    move_over_closures(origin_node, &current_states);

    for (int i = 0; i < buflen; i++) {
        char c = buf[i];
        UNAM_DEBUGP("\n");
        UNAM_DEBUG("== Processing character %c\n", c);

        UNAM_DEBUG(" current states: ");
        for (int i = 0; i < current_states.length; i++) {
            nfa_node* node = current_states.data[i];
            UNAM_DEBUGP("%d, ", node->id);
        }
        UNAM_DEBUGP("\n");

        for (int s = 0; s < current_states.length; s++) {
            nfa_node* node = current_states.data[s];
            UNAM_DEBUG(" visiting node %d:\n", node->id);

            for (int t = 0; t < node->trans_count; t++) {
                transition* trans = &node->trans[t];
                UNAM_DEBUG("  - transition #%d: (%d) -%c-> (%d) ", t, node->id, trans->on, trans->target->id);
                if (trans->on == c) {
                    UNAM_DEBUGP("✅");
                    da_nfa_node_append(&new_states, trans->target);
                } else {
                    UNAM_DEBUGP("❌");
                }
                UNAM_DEBUGP("\n");
            }
        }

        // Now we will temporarily switch roles between the new and the current
        da_drain(&current_states);

        // We move over epsilon transitions and store it in the current
        for (int i = 0; i < new_states.length; i++) {
            nfa_node* node = new_states.data[i];
            move_over_closures(node, &current_states);
        }

        // And empty out the new for the next iteration
        da_drain(&new_states);
    }

    // Check if we landed in an accepted state
    for (int i = 0; i < current_states.length; i++) {
        if (current_states.data[i]->is_accept) {
            return true;
        }
    }

    return false;
}
