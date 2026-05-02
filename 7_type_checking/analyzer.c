#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "analyzer.h"
#include "ast.h"
#include "debug.h"

#define IDENTIFIER_MAX_LEN 128

static void get_type_name(ASTNode* node, char* buffer, size_t size) {
    UNAM_ASSERT(node != NULL, "get_type_name must not be called for null nodes");
    if (node->type == NODE_IDENTIFIER) {
        UNAM_ASSERT(node->lexeme != NULL, "identifiers must have a lexeme set");
        strncpy(buffer, node->lexeme, size);
    }
    else if (node->type == NODE_GENERIC_TYPE) {
        UNAM_ASSERT(node->lexeme != NULL, "types must have a lexeme set");
        snprintf(buffer, size, "%s<...>", node->lexeme ? node->lexeme : "?");
    }
    else {
        UNAM_ASSERT(false, "tried to get type name for unknown lexeme, maybe define this branch?");
    }
}

void push_scope(Scope* current_scope) {
    Scope* scope = (Scope*)calloc(1, sizeof(Scope));
    scope->parent = current_scope;
    current_scope = scope;
}

void pop_scope(Scope* current_scope) {
    if (!current_scope) return;
    Scope* parent = current_scope->parent;
    // Note: In a real compiler, we'd free the symbols here but we'll keep them for now
    current_scope = parent;
}

void add_symbol(Scope* current_scope, char* name, ASTNode* type_node) {
    if (!current_scope) return;
    Symbol* sym = (Symbol*)calloc(1, sizeof(Symbol));
    sym->name = ast_strdup(name);
    sym->type_node = type_node;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;
}

Symbol* find_symbol(Scope* current_scope, char* name) {
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

void analyze_node(Scope* current_scope, ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            UNAM_DEBUG("NODE_PROGRAM not implemented");
            exit(69);
        };
        case NODE_FUNCTION: {
            // The function is available in the current scope, and creates another
            add_symbol(current_scope, node->lexeme, node);
            push_scope(current_scope);
            ASTNode* arg = node->args;
            while (arg) {
                UNAM_ASSERT(arg->type == NODE_FUNC_PARAMETER, "function argument must have NODE_FUNC_PARAMETER type");
                // Parameter name
                char type_buf[IDENTIFIER_MAX_LEN];
                get_type_name(arg->left, type_buf, sizeof(type_buf));
                add_symbol(current_scope, arg->lexeme, arg->left);
                // TODO: check if argument type exists!!
                arg = arg->next;
            }
            analyze_node(current_scope, node->body);
            pop_scope(current_scope);
            break;
        };
        case NODE_CALL: {
            if (node->type == NODE_IDENTIFIER) {}
            UNAM_DEBUG("NODE_CALL not implemented");
            exit(69);
        };
        case NODE_STMT_LIST: {
            UNAM_DEBUG("NODE_STMT_LIST not implemented");
            exit(69);
        };
        case NODE_LET: {
            // First we evaluate what we got at the right side
            analyze_node(current_scope, node->right);
            if (node->left) {
                char type_buf[IDENTIFIER_MAX_LEN];
                get_type_name(node->left, type_buf, sizeof(type_buf));
                printf("Defining variable: %s : %s\n", node->lexeme, type_buf);
                add_symbol(current_scope, node->lexeme, node->left);
            }
            else {
                printf("Defining variable: %s (inferred)\n", node->lexeme);
                add_symbol(current_scope, node->lexeme, NULL);
            }
            break;
        };
        case NODE_ASSIGN: {
            UNAM_DEBUG("NODE_ASSIGN not implemented");
            exit(69);
            analyze_node(current_scope, node->right);
            if (node->left && node->left->lexeme) {
                printf("Assigning variable: %s (via %s)\n", node->left->lexeme, node->lexeme);
                if (!find_symbol(current_scope, node->left->lexeme)) {
                    fprintf(stderr, "Error: Semantic error at line %d: Undefined variable '%s'\n", 0, node->left->lexeme);
                }
            }
            break;
        };
        case NODE_IF: {
            UNAM_DEBUG("NODE_IF not implemented");
            exit(69);
        };
        case NODE_FOR: {
            UNAM_DEBUG("NODE_FOR not implemented");
            exit(69);
        };
        case NODE_LOOP: {
            UNAM_DEBUG("NODE_LOOP not implemented");
            exit(69);
        };
        case NODE_MATCH: {
            UNAM_DEBUG("NODE_MATCH not implemented");
            exit(69);
        };
        case NODE_MATCH_ARM: {
            UNAM_DEBUG("NODE_MATCH_ARM not implemented");
            exit(69);
        };
        case NODE_RETURN: {
            UNAM_DEBUG("NODE_RETURN not implemented");
            exit(69);
        };
        case NODE_BREAK: {
            UNAM_DEBUG("NODE_BREAK not implemented");
            exit(69);
        };
        case NODE_IDENT_LIST: {
            UNAM_DEBUG("NODE_IDENT_LIST not implemented");
            exit(69);
        };
        case NODE_FUNC_PARAMETER: {
            UNAM_DEBUG("NODE_FUNC_PARAMETER not implemented");
            exit(69);
        };
        case NODE_BINARY_OP: {
            UNAM_DEBUG("NODE_BINARY_OP not implemented");
            exit(69);
        };
        case NODE_UNARY_OP: {
            UNAM_DEBUG("NODE_UNARY_OP not implemented");
            exit(69);
        };
        case NODE_IDENTIFIER: {
            UNAM_DEBUG("NODE_IDENTIFIER not implemented");
            exit(69);
            if (!find_symbol(current_scope, node->lexeme)) {
                // Not all identifiers are variables (some might be types or tags),
                // but we'll flag it for now if we expect a variable
            }
            break;
        };
        case NODE_INT_LITERAL: {
            UNAM_DEBUG("NODE_INT_LITERAL not implemented");
            exit(69);
        };
        case NODE_FLOAT_LITERAL: {
            UNAM_DEBUG("NODE_FLOAT_LITERAL not implemented");
            exit(69);
        };
        case NODE_BOOL_LITERAL: {
            UNAM_DEBUG("NODE_BOOL_LITERAL not implemented");
            exit(69);
        };
        case NODE_STRING_LITERAL: {
            UNAM_DEBUG("NODE_STRING_LITERAL not implemented");
            exit(69);
        };
        case NODE_ENUM_DECL: {
            UNAM_DEBUG("NODE_ENUM_DECL not implemented");
            exit(69);
        };
        case NODE_ENUM_VARIANT: {
            UNAM_DEBUG("NODE_ENUM_VARIANT not implemented");
            exit(69);
        };
        case NODE_STRUCT_DECL: {
            UNAM_DEBUG("NODE_STRUCT_DECL not implemented");
            exit(69);
        };
        case NODE_STRUCT_FIELD: {
            UNAM_DEBUG("NODE_STRUCT_FIELD not implemented");
            exit(69);
        };
        case NODE_LIST_LITERAL: {
            UNAM_DEBUG("NODE_LIST_LITERAL not implemented");
            exit(69);
        };
        case NODE_PIPELINE: {
            UNAM_DEBUG("NODE_PIPELINE not implemented");
            exit(69);
        };
        case NODE_PLACEHOLDER: {
            UNAM_DEBUG("NODE_PLACEHOLDER not implemented");
            exit(69);
        };
        case NODE_MEMBER_ACCESS: {
            UNAM_DEBUG("NODE_MEMBER_ACCESS not implemented");
            exit(69);
        };
        case NODE_RANGE: {
            UNAM_DEBUG("NODE_RANGE not implemented");
            exit(69);
        };
        case NODE_GENERIC_TYPE: {
            UNAM_DEBUG("NODE_GENERIC_TYPE not implemented");
            exit(69);
        };
        case NODE_LAMBDA: {
            UNAM_DEBUG("NODE_LAMBDA not implemented");
            exit(69);
            push_scope(current_scope);
            printf("Lambda expression:\n");
            ASTNode* arg = node->args;
            while (arg) {
                if (arg->type == NODE_FUNC_PARAMETER) {
                    char type_buf[IDENTIFIER_MAX_LEN];
                    get_type_name(arg->left, type_buf, sizeof(type_buf));
                    printf("  Parameter: %s : %s\n", arg->lexeme, type_buf);
                    add_symbol(current_scope, arg->lexeme, arg->left);
                }
                else {
                    add_symbol(current_scope, arg->lexeme, NULL);
                }
                arg = arg->next;
            }
            analyze_node(current_scope, node->body);
            pop_scope(current_scope);
            break;
        };
        default: {
            UNAM_DEBUG("Unknown type %d", node->type);
            exit(69);
        }
    }

    analyze_node(current_scope, node->next);
}

void analyze_semantics(ASTNode* root) {
    printf("\n🪰 Starting Semantic Analysis...\n");
    Scope* current_scope = NULL;
    push_scope(current_scope);

    analyze_node(current_scope, root);
    pop_scope(current_scope);
}
