#ifndef ANALYZER_H
#define ANALYZER_H

#include "ast.h"

// Symbol table entry
typedef struct Symbol {
    char* name;
    ASTNode* type_node; // Type information
    int is_function;
    struct Symbol* next;
} Symbol;

// Scope structure
typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;

void analyze_semantics(ASTNode* root);

#endif
