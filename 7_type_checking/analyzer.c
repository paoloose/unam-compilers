#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "analyzer.h"
#include "ast.h"
#include "debug.h"

static void push_scope(Scope** current_scope) {
    Scope* scope = calloc(1, sizeof(Scope));
    scope->parent = *current_scope;
    *current_scope = scope;
}

static void pop_scope(Scope** current_scope) {
    if (!current_scope) return;
    Scope* parent = (*current_scope)->parent;
    // Note: In a real compiler, we'd free the symbols here but we'll keep them for now
    *current_scope = parent;
}

bool types_are_equal(ASTNode* type1, ASTNode* type2) {
    if (!type1 && !type2) return true;
    if (!type1 || !type2) return false;
    UNAM_ASSERT(type1->type == NODE_TYPE_IDENTIFIER || type1->type == NODE_GENERIC_TYPE || type2->type == NODE_TYPE_IDENTIFIER || type2->type == NODE_GENERIC_TYPE, "tried to compare non type nodes");

    // First check lexemes
    if (type1->lexeme == NULL && type2->lexeme != NULL) return false;
    if (type1->lexeme != NULL && type2->lexeme == NULL) return false;
    if (type1->lexeme && type2->lexeme && strcmp(type1->lexeme, type2->lexeme) != 0) {
        return false;
    }

    // Then check generic args
    ASTNode* g1 = type1->generic_args;
    ASTNode* g2 = type2->generic_args;
    while (g1 && g2) {
        if (!types_are_equal(g1, g2)) {
            return false;
        }
        g1 = g1->next;
        g2 = g2->next;
    }

    // If one has more generic args than the other
    if (g1 || g2) return false;
    return true;
}

SymbolTableEntry* find_symbol(Scope* current_scope, char* name) {
    extern SymbolTableEntry int_symbol;
    extern SymbolTableEntry float_symbol;
    extern SymbolTableEntry bool_symbol;
    extern SymbolTableEntry string_symbol;
    extern SymbolTableEntry list_symbol;

    // builtins
    if (strcmp(name, "int") == 0) {
        return &int_symbol;
    }
    if (strcmp(name, "float") == 0) {
        return &float_symbol;
    }
    if (strcmp(name, "bool") == 0) {
        return &bool_symbol;
    }
    if (strcmp(name, "string") == 0) {
        return &string_symbol;
    }
    if (strcmp(name, "List") == 0) {
        return &list_symbol;
    }
    Scope* s = current_scope;
    while (s) {
        SymbolTableEntry* sym = s->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        s = s->parent;
    }
    return NULL;
}

SymbolTableEntry* find_nearest_function(Scope* current_scope) {
    Scope* s = current_scope;
    SymbolTableEntry* last_func = NULL;

    while (s) {
        SymbolTableEntry* sym = s->symbols;
        while (sym) {
            if (sym->type_node->type == NODE_FUNCTION) {
                last_func = sym;
            }
            sym = sym->next;
        }
        if (last_func) {
            return last_func;
        }
        s = s->parent;
    }
    return NULL;
}

void add_symbol_unchecked(Scope* current_scope, char* name, ASTNode* type_node) {
    if (!current_scope) return;
    SymbolTableEntry* sym = calloc(1, sizeof(SymbolTableEntry));
    sym->name = ast_strdup(name);
    sym->type_node = type_node;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;

    if (!type_node) return;

    switch (type_node->type) {
        case NODE_STRUCT_DECL: {
            // sym->kind = SYMBOL_KIND_TYPENAME;
            // sym->next = current_scope->symbols;
            // sym->type_data.name = sym->name;
            // sym->type_data.type = TYPE_STRUCT;
            // ASTNode* garg = type_node->generic_args;
            // while (garg) {
            //     SymbolTableEntry* sym = calloc(1, sizeof(SymbolTableEntry));
            //     garg = garg->next;
            // }
            // current_scope->symbols = sym;
            break;
        };
        case NODE_ENUM_DECL: {
            break;
        };
        case NODE_FUNCTION: {
            break;
        };
        case NODE_FUNC_PARAMETER: {
            break;
        };
        case NODE_LET: {
            break;
        };
        case NODE_TYPE_IDENTIFIER:
        case NODE_GENERIC_TYPE: {
            break;
        }
        default: {
            UNAM_DEBUG("unreachable: node type (%d) can't be treated as symbol", type_node->type);
            exit(69);
        }
    }
}

bool add_symbol_unshadowed(Scope* current_scope, char* name, ASTNode* type_node) {
    if (!current_scope) return false;

    SymbolTableEntry* found_symbol = find_symbol(current_scope, name);
    if (found_symbol) {
        return false;
    }

    add_symbol_unchecked(current_scope, name, type_node);
    return true;
}

/*
 * Succees if the symbol to add is not defined in the current scope only.
 * It may be defined in parent scopes, but shadowing will allow that!
 */
bool add_symbol_shadowed(Scope* current_scope, char* name, ASTNode* type_node) {
    if (!current_scope) return false;

    SymbolTableEntry* sym = current_scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) return false;
        sym = sym->next;
    }

    add_symbol_unchecked(current_scope, name, type_node);
    return true;
}

static bool generic_args_are_valid(ASTNode* generic_args) {
    ASTNode* start_generic_args = generic_args;
    ASTNode* generic_arg = generic_args;

    while (generic_arg) {
        ASTNode* g = start_generic_args;
        int duplicated = 0;
        while (g) {
            if (strcmp(g->lexeme, generic_arg->lexeme) == 0) {
                duplicated++;
            }
            g = g->next;
        }
        if (duplicated > 1) {
            UNAM_ASSERT(false, "TODO return error, duplicated field name in struct");
            return false;
        }
        generic_arg = generic_arg->next;
    }
    return true;
}

void analyze_node(Scope* current_scope, ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            push_scope(&current_scope);
            analyze_node(current_scope, node->body);
            pop_scope(&current_scope);
            break;
        };
        case NODE_FUNCTION: {
            UNAM_DEBUG("function '%s'\n", node->lexeme);
            // The function is available in the current scope, and creates another
            if (!add_symbol_unshadowed(current_scope, node->lexeme, node)) {
                UNAM_ASSERT(false, "TODO return error, creating a function that already exists");
            }
            push_scope(&current_scope);
            ASTNode* arg = node->args;
            while (arg) {
                UNAM_ASSERT(arg->type == NODE_FUNC_PARAMETER, "function argument must have NODE_FUNC_PARAMETER type");
                UNAM_ASSERT(arg->right != NULL && arg->right->type == NODE_IDENTIFIER, "NODE_FUNC_PARAMETER must set self->right to the IDENTIFIER");
                // Parameter name
                char* parameter_name = arg->lexeme;
                // Parameter type
                char* type_name = arg->right->lexeme;
                // We add the parameter to the symbols table, with the given type
                UNAM_DEBUG(" arg: name=%s, ", parameter_name);
                SymbolTableEntry* parameter_type = find_symbol(current_scope, type_name);
                if (parameter_type == NULL) {
                    UNAM_ASSERT(false, "TODO return error, function argument is using a type that doesn't exists");
                }
                // New scope, so no need to check
                add_symbol_unchecked(current_scope, parameter_name, parameter_type->type_node);

                arg = arg->next;
                UNAM_DEBUG_PLAIN("type=%s\n", parameter_type->name);
            }
            if (node->return_type) {
                UNAM_DEBUG("  return_type=%s\n", node->return_type->lexeme);
                analyze_node(current_scope, node->return_type);
            }
            analyze_node(current_scope, node->body);
            pop_scope(&current_scope);
            break;
        };
        case NODE_CALL: {
            // Normal function call
            ASTNode* func = node->left;
            if (func->type == NODE_IDENTIFIER) {
                // TODO: check symbol table
            }
            // And in this case, we must evaluate what's on the left and make sure it evaluates to a function!
            else {
                // We first analyze the left node so that `evaluates_to_type` gets populated
                analyze_node(current_scope, func);
                SymbolTableEntry* returned_type = find_symbol(current_scope, func->evaluates_to_type->lexeme);
                if (returned_type == NULL) {
                    UNAM_ASSERT(false, "TODO return error, call made to a non-function");
                }
                if (returned_type->type_node->type != NODE_FUNCTION || returned_type->type_node->type != NODE_LAMBDA) {
                    UNAM_ASSERT(false, "TODO return error, call made to a non-function");
                }
                // And now that we now a function is being called, we validate its arguments!
                ASTNode* calling_function = returned_type->type_node;

                ASTNode* expected_arg = calling_function->args;
                ASTNode* passed_arg = node->args;

                int passed_args_len = 0;
                while (expected_arg) {
                    if (passed_arg == NULL) {
                        UNAM_ASSERT(false, "TODO return error, expected argument, but none passed");
                    }
                    passed_args_len++;
                    // Check that types of arguments match
                    UNAM_ASSERT(expected_arg->type == NODE_FUNC_PARAMETER, "function arguments must be of type NODE_FUNC_PARAMETER");
                    UNAM_ASSERT(expected_arg->right != NULL && expected_arg->right->type == NODE_IDENTIFIER, "NODE_FUNC_PARAMETER must set self->right to the IDENTIFIER");
                    // TODO: get from symbols table and check if exists
                    // TODO: note that if this function is already in the symbols table, then its type must exist
                    analyze_node(current_scope, passed_arg);
                    SymbolTableEntry* returned_type = find_symbol(current_scope, passed_arg->evaluates_to_type->lexeme);
                    if (returned_type == NULL) {
                        UNAM_ASSERT(false, "TODO return error, expected type, but it evaluates to void");
                    }

                    if (!types_are_equal(passed_arg->evaluates_to_type, expected_arg->right)) {
                        UNAM_ASSERT(false, "TODO return error, expected type A, but passed type B");
                    }
                    // Then it's okay

                    passed_arg = passed_arg->next;
                    expected_arg = expected_arg->next;
                }
                if (passed_arg != NULL) {
                    UNAM_ASSERT(false, "TODO return error, passing more arguments than expected");
                }
            }
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
            ASTNode* evaluated_type_node_right = node->right->evaluates_to_type;
            SymbolTableEntry* evaluated_type_right = find_symbol(current_scope, evaluated_type_node_right->lexeme);
            UNAM_ASSERT(evaluated_type_right != NULL, "evaluated type must exist in the symbols table");

            // If explicitely type, we check
            // - if exists in the symbols table
            // - if matches the right-hand-side
            if (node->left) {
                char* type_name = node->left->lexeme;
                UNAM_DEBUG("let %s: %s\n", node->lexeme, type_name);
                SymbolTableEntry* explicit_type = find_symbol(current_scope, type_name);
                if (explicit_type == NULL) {
                    UNAM_ASSERT(false, "TODO return error, used explicit type that does not exist");
                }
                add_symbol_unshadowed(current_scope, node->lexeme, node->left);
            }
            else {
                // node->left;
                UNAM_DEBUG("let %s: (inferred %s)\n", node->lexeme, node->right->evaluates_to_type->lexeme);
                add_symbol_unshadowed(current_scope, node->lexeme, NULL);
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
            SymbolTableEntry* bound_function = find_nearest_function(current_scope);
            if (!bound_function || !bound_function->type_node) {
                UNAM_ASSERT(false, "TODO return error, used return but no function is in context");
            }
            // The funciton->return_type property store its explicit return type, if specified
            ASTNode* explicit_return_type = bound_function->type_node->return_type;
            if (explicit_return_type) {
                analyze_node(current_scope, explicit_return_type);
            }

            // The `return <thing>;` value
            bool is_returning_something = node->right;
            if (is_returning_something) {
                analyze_node(current_scope, node->right);
                ASTNode* evaluated_type = node->right->evaluates_to_type;

                if (explicit_return_type) {
                    UNAM_DEBUG("explicit_return_type=%s, returning_type=%s\n", explicit_return_type->lexeme, evaluated_type->lexeme);
                    // Check against it
                    if (!types_are_equal(explicit_return_type, evaluated_type)) {
                        UNAM_ASSERT(false, "TODO return error, type of return and function signature don't match");
                    }
                } else {
                    UNAM_DEBUG("no explicit return type found");
                    // Function has no return type, so try to infer it by mutating the function node
                    bound_function->type_node->return_type = evaluated_type;
                }
            } else {
                // Used didn't returned something, but function was expected the opposite
                if (bound_function->type_node->return_type != NULL) {
                    UNAM_ASSERT(false, "TODO return error, expected to return a type, but returned nothing");
                }
            }
            break;
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
        case NODE_TYPE_IDENTIFIER: {
            SymbolTableEntry* found = find_symbol(current_scope, node->lexeme);
            if (!found) {
                UNAM_ASSERT(false, "TODO return error, referenced type is not defined");
            }
            // No need to check anything
            break;
        };
        case NODE_GENERIC_TYPE: {
            SymbolTableEntry* found = find_symbol(current_scope, node->lexeme);
            if (!found) {
                UNAM_ASSERT(false, "TODO return error, referenced type is not defined");
            }

            ASTNode* generic_arg = node->generic_args;
            while (generic_arg) {
                analyze_node(current_scope, generic_arg);
                generic_arg = generic_arg->next;
            }
            break;
        };
        case NODE_IDENTIFIER: {
            // No need to check anything
            break;
        };
        case NODE_INT_LITERAL: {
            SymbolTableEntry* int_type = find_symbol(current_scope, "int");
            UNAM_ASSERT(int_type != NULL, "int type is not defined");
            node->evaluates_to_type = int_type->type_node;
            break;
        };
        case NODE_FLOAT_LITERAL: {
            SymbolTableEntry* float_type = find_symbol(current_scope, "float");
            UNAM_ASSERT(float_type != NULL, "float type is not defined");
            node->evaluates_to_type = float_type->type_node;
            break;
        };
        case NODE_BOOL_LITERAL: {
            SymbolTableEntry* bool_type = find_symbol(current_scope, "bool");
            UNAM_ASSERT(bool_type != NULL, "bool type is not defined");
            node->evaluates_to_type = bool_type->type_node;
            break;
        };
        case NODE_STRING_LITERAL: {
            SymbolTableEntry* string_type = find_symbol(current_scope, "string");
            UNAM_ASSERT(string_type != NULL, "string type is not defined");
            node->evaluates_to_type = string_type->type_node;
            break;
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
            char* struct_name = node->lexeme;
            UNAM_DEBUG("struct name=%s\n", struct_name);
            if (!add_symbol_unshadowed(current_scope, struct_name, node)) {
                UNAM_ASSERT(false, "TODO return error, the type name for struct already exists");
            }

            // TODO, this function generates errors in the inside, and we must move it to the outside
            push_scope(&current_scope);
            if (!generic_args_are_valid(node->generic_args)) {
                UNAM_ASSERT(false, "TODO return error, invalid generic args for struct");
            }

            ASTNode* generic_arg = node->generic_args;
            while (generic_arg) {
                add_symbol_unchecked(current_scope, generic_arg->lexeme, generic_arg);
                generic_arg = generic_arg->next;
            }

            // Check for struct fields
            // We only check that struct field names are not repeated and their types exists
            ASTNode* start_struct_field = node->args;
            ASTNode* struct_field = node->args;
            int struct_field_idx = 0;

            while (struct_field) {
                UNAM_DEBUG("  field name=%s, ", struct_field->lexeme);

                int search_struct_field_idx = 0;
                ASTNode* sf = start_struct_field;
                while (sf) {
                    if (strcmp(sf->lexeme, struct_field->lexeme) == 0 && struct_field_idx != search_struct_field_idx) {
                        UNAM_ASSERT(false, "TODO return error, duplicated field name for struct");
                        break;
                    }
                    search_struct_field_idx++;
                    sf = sf->next;
                }

                // check for type
                UNAM_ASSERT(struct_field->right != NULL, "NODE_FUNC_PARAMETER must set self->right to the IDENTIFIER");
                analyze_node(current_scope, struct_field->right);
                ASTNode* field_type = struct_field->right;
                char* field_type_name = field_type->lexeme;
                SymbolTableEntry* found = find_symbol(current_scope, field_type_name);
                if (!found) {
                    UNAM_ASSERT(false, "TODO return error, type for struct is not defined");
                }
                UNAM_DEBUG_PLAIN("type=%s\n", found->name);

                struct_field_idx++;
                struct_field = struct_field->next;
            }

            pop_scope(&current_scope);
            break;
        };
        case NODE_STRUCT_FIELD: {
            UNAM_DEBUG("NODE_STRUCT_FIELD not implemented");
            exit(69);
        };
        case NODE_LIST_LITERAL: {
            ASTNode* element = node->args;
            ASTNode* last_element_type = NULL;

            while (element) {
                analyze_node(current_scope, element);
                if (!last_element_type) {
                    last_element_type = element->evaluates_to_type;
                } else {
                    // check if this element has the same type of the rest
                    if (!types_are_equal(last_element_type, element->evaluates_to_type)) {
                        UNAM_ASSERT(false, "TODO return error, types of elements of list are not the same");
                    }
                }
                element = element->next;
            }

            SymbolTableEntry* list_type = find_symbol(current_scope, "List");
            UNAM_ASSERT(list_type != NULL, "List type is not defined");
            node->evaluates_to_type = list_type->type_node;
            node->evaluates_to_type->generic_args = last_element_type;
            break;
        };
        case NODE_LIST_PATTERN: {
            UNAM_DEBUG("NODE_LIST_PATTERN not implemented");
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
        case NODE_LAMBDA: {
            UNAM_DEBUG("NODE_LAMBDA not implemented");
            exit(69);
            push_scope(&current_scope);
            printf("Lambda expression:\n");
            ASTNode* arg = node->args;
            while (arg) {
                if (arg->type == NODE_FUNC_PARAMETER) {
                    char* type_name = arg->lexeme;
                    printf("  Parameter: %s : %s\n", arg->lexeme, type_name);
                    add_symbol_unshadowed(current_scope, arg->lexeme, arg->right);
                }
                else {
                    add_symbol_unshadowed(current_scope, arg->lexeme, NULL);
                }
                arg = arg->next;
            }
            analyze_node(current_scope, node->body);
            pop_scope(&current_scope);
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
    push_scope(&current_scope);

    analyze_node(current_scope, root);
    pop_scope(&current_scope);
}
