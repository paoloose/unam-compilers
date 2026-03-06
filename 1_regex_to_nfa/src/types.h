#pragma once
#include <stdbool.h>

// -> Regex types

// Represent a single regex parsed token
typedef struct {
    char value;
} regex_item;

// A regex expression in postfix format
typedef struct {
    int size;
    regex_item* items;
} regex;

// Represents a supported operation by our regex implementation
typedef struct {
    char symbol;
    int precedence;
    int arguments;
} input_op;

// -> NFA types

#define TRANSITION_EPSILON '='
#define MAX_TRANSITIONS 2  // For epsilon closure, we have at most 2 transitions per state

typedef struct nfa_node nfa_node;

typedef struct {
    char on;           // character triggering this transition (TRANSITION_EPSILON for epsilon)
    nfa_node* target;  // target node
} transition;

typedef struct nfa_node {
    int id;                         // unique identifier for debugging
    transition trans[MAX_TRANSITIONS];  // transitions from this node
    int trans_count;                // number of transitions
    bool is_accept;                 // whether this is an accepting state
} nfa_node;

// Thompson NFA fragment - used during construction
typedef struct {
    nfa_node* start;
    nfa_node* accept;
} nfa_fragment;

// A graph representing an NFA graph
typedef struct {
    nfa_node* origin;
    int node_count;                 // total number of nodes created
} nfa;
