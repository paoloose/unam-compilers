#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "analyzer.h"

static Scope* current_scope = NULL;

void push_scope() {
    Scope* scope = (Scope*)calloc(1, sizeof(Scope));
    scope->parent = current_scope;
    current_scope = scope;
}

void pop_scope() {
    if (!current_scope) return;
    Scope* parent = current_scope->parent;
    // Note: In a real compiler, we'd free the symbols here but we'll keep them for now
    current_scope = parent;
}

void add_symbol(char* name, ASTNode* type_node, int is_function) {
    if (!current_scope) return;
    Symbol* sym = (Symbol*)calloc(1, sizeof(Symbol));
    sym->name = ast_strdup(name);
    sym->type_node = type_node;
    sym->is_function = is_function;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;
}

Symbol* find_symbol(char* name) {
    Scope* s = current_scope;
    while (s) {
        Symbol* sym = s->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        s = s->parent;
    }
    return NULL;
}

void analyze_node(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_FUNCTION:
            add_symbol(node->lexeme, NULL, 1);
            push_scope();
            // Process parameters
            ASTNode* arg = node->args;
            while (arg) {
                add_symbol(arg->lexeme, NULL, 0);
                arg = arg->next;
            }
            analyze_node(node->body);
            pop_scope();
            break;

        case NODE_LET:
            analyze_node(node->right);
            printf("Defining variable: %s\n", node->lexeme);
            add_symbol(node->lexeme, NULL, 0);
            break;

        case NODE_ASSIGN:
            analyze_node(node->right);
            if (!find_symbol(node->lexeme)) {
                fprintf(stderr, "Error: Semantic error at line %d: Undefined variable '%s'\n", 0, node->lexeme);
            }
            break;

        case NODE_IDENTIFIER:
            if (!find_symbol(node->lexeme)) {
                // Not all identifiers are variables (some might be types or tags),
                // but we'll flag it for now if we expect a variable
            }
            break;

        case NODE_CALL:
            analyze_node(node->args);
            break;

        default:
            analyze_node(node->left);
            analyze_node(node->right);
            analyze_node(node->cond);
            analyze_node(node->body);
            analyze_node(node->else_branch);
            analyze_node(node->args);
            break;
    }

    analyze_node(node->next);
}

void analyze_semantics(ASTNode* root) {
    printf("Starting Semantic Analysis...\n");
    push_scope();
    analyze_node(root);
    pop_scope();
    printf("Semantic Analysis completed.\n");
}
