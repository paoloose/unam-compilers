#ifndef ANALYZER_H
#define ANALYZER_H

#include "ast.h"

// Symbol table entry
typedef struct Symbol {
    char* name;
    ASTNode* type_node; // type information
    int is_function;
    struct Symbol* next;
} Symbol;

// Scope that we can push and pop from
typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;

void analyze_semantics(ASTNode* root);

#endif
