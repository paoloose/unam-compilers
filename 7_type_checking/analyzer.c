#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "analyzer.h"
#include "ast.h"
#include "debug.h"
#include "darray.h"
#include "builtins.h"

da_cstr semantic_errors;

void report_error(ASTNode* node, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    char* message = malloc(length + 1);
    va_start(args, format);
    vsnprintf(message, length + 1, format, args);
    va_end(args);

    char* final_message;
    if (node) {
        int len = snprintf(NULL, 0, "[%d:%d] %s", node->line, node->col, message);
        final_message = malloc(len + 1);
        snprintf(final_message, len + 1, "[%d:%d] %s", node->line, node->col, message);
        free(message);
    } else {
        final_message = message;
    }

    da_cstr_append(&semantic_errors, final_message);
}

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
    UNAM_ASSERT(type1->type == NODE_CONCRETE_TYPE || type1->type == NODE_GENERIC_TYPE || type2->type == NODE_CONCRETE_TYPE || type2->type == NODE_GENERIC_TYPE, "tried to compare non type nodes");

    // First check lexemes
    if (type1->lexeme == NULL && type2->lexeme != NULL) {
        return false;
    }
    if (type1->lexeme != NULL && type2->lexeme == NULL) {
        return false;
    }
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

bool is_builting_literal(const char* lexeme) {
    bool is_bool = strcmp(lexeme, "bool") == 0;
    bool is_int = strcmp(lexeme, "int") == 0;
    bool is_float = strcmp(lexeme, "float") == 0;
    bool is_string = strcmp(lexeme, "string") == 0;
    return is_bool || is_int || is_float || is_string;
}

SymbolTableEntry* find_symbol(Scope* current_scope, const char* name) {
    // builtins
    if (strcmp(name, "int")    == 0) return get_int_symbol();
    if (strcmp(name, "float")  == 0) return get_float_symbol();
    if (strcmp(name, "bool")   == 0) return get_bool_symbol();
    if (strcmp(name, "string") == 0) return get_string_symbol();
    if (strcmp(name, "List")   == 0) return get_list_symbol();
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

void add_symbol_unchecked(Scope* current_scope, const char* name, ASTNode* type_node) {
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
        case NODE_CONCRETE_TYPE:
        case NODE_GENERIC_TYPE: {
            break;
        }
        default: {
            report_error(type_node, "unreachable: node type (%d) can\'t be treated as symbol", type_node->type);
            break;
        }
    }
}

bool add_symbol_unshadowed(Scope* current_scope, const char* name, ASTNode* type_node) {
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
bool add_symbol_shadowed(Scope* current_scope, const char* name, ASTNode* type_node) {
    if (!current_scope) return false;

    SymbolTableEntry* sym = current_scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) return false;
        sym = sym->next;
    }

    add_symbol_unchecked(current_scope, name, type_node);
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
                report_error(node, "Trying to name a function '%s', but it's already defined", node->lexeme);
            }
            push_scope(&current_scope);

            // For each function type argument, if any
            ASTNode* generic_arg = node->generic_args;
            while (generic_arg) {
                // We add the generic type as symbol, and then analyze the node
                UNAM_ASSERT(generic_arg->type == NODE_CONCRETE_TYPE, "generic arguments must be concrete types");
                // And we promote them to generic types, as they appear in the generic type parameters list
                generic_arg->type = NODE_GENERIC_TYPE;
                add_symbol_unshadowed(current_scope, generic_arg->lexeme, generic_arg);
                analyze_node(current_scope, generic_arg);
                generic_arg = generic_arg->next;
            }

            // For each function parameter, if any
            ASTNode* param = node->args;
            while (param) {
                UNAM_ASSERT(param->type == NODE_FUNC_PARAMETER, "function parameter must have NODE_FUNC_PARAMETER type");
                UNAM_ASSERT(param->right != NULL && (param->right->type == NODE_CONCRETE_TYPE || param->right->type == NODE_GENERIC_TYPE), "invalid arg->right type");
                // First we analyze the type of the parameter
                analyze_node(current_scope, param->right);
                // Parameter name
                const char* parameter_name = param->lexeme;
                // Parameter type
                const char* type_name = param->right->lexeme;
                // We add the parameter to the symbols table, with the given type
                UNAM_DEBUG("  arg name=%s\n", parameter_name);
                SymbolTableEntry* parameter_type = find_symbol(current_scope, type_name);
                if (!parameter_type || !parameter_type->type_node) {
                    report_error(param, "Function parameter '%s' is using a type '%s' that doesn't exist", parameter_name, type_name);
                } else {
                    // This overrides the concrete type with a generic type, as this param is referecing a generic types in the symbol table
                    if (parameter_type->type_node->type == NODE_GENERIC_TYPE) {
                        param->right->type = NODE_GENERIC_TYPE;
                    }
                    param->evaluates_to_type = parameter_type->type_node;
                    // We add this parameter to the current scope
                    add_symbol_unchecked(current_scope, parameter_name, param);
                }
                param = param->next;
            }
            if (node->return_type) {
                UNAM_DEBUG("  ->\n");
                analyze_node(current_scope, node->return_type);
            }
            UNAM_DEBUG("body ->\n");
            analyze_node(current_scope, node->body);
            UNAM_DEBUG("<- end body\n");
            pop_scope(&current_scope);
            break;
        };
        case NODE_CALL: {
            // Normal function call
            ASTNode* func = node->left;
            UNAM_DEBUG("call: %s\n", func->lexeme);
            SymbolTableEntry* resolved_func = NULL;

            if (func->type == NODE_IDENTIFIER) {
                // We only validate that the function exists in the symbol table
                resolved_func = find_symbol(current_scope, func->lexeme);
                if (!resolved_func) {
                    report_error(func, "Function is not defined: '%s'", func->lexeme);
                    break;
                }
                if (resolved_func->type_node->type != NODE_FUNCTION && resolved_func->type_node->type != NODE_LAMBDA) {
                    report_error(func, "Trying to call a non function '%s'", func->evaluates_to_type->lexeme);
                    break;
                }
            }
            // And in this case, we must evaluate what's on the left and make sure it evaluates to a function!
            else {
                // We first analyze the left node so that `evaluates_to_type` gets populated
                analyze_node(current_scope, func);
                resolved_func = find_symbol(current_scope, func->evaluates_to_type->lexeme);
                if (resolved_func == NULL || (resolved_func->type_node->type != NODE_FUNCTION && resolved_func->type_node->type != NODE_LAMBDA)) {
                    report_error(func, "Trying to call a non function '%s'", func->evaluates_to_type->lexeme);
                    break;
                }
            }

            // And now that we now a function is being called, we validate its arguments!
            ASTNode* calling_function = resolved_func->type_node;

            ASTNode* expected_param = calling_function->args;
            ASTNode* passed_arg = node->args;

            da_astnodes specialized_params_types;  // generic types that are now specialized (NODE_GENERIC_TYPE)
            da_astnodes_init(&specialized_params_types, 4);
            da_astnodes specialized_params_values; // the actual types they have been specialized too (ANY)
            da_astnodes_init(&specialized_params_values, 4);

            int passed_args_len = 0;
            while (expected_param) {
                if (passed_arg == NULL) {
                    report_error(node, "Expected argument, but none passed");
                    break;
                }
                passed_args_len++;
                // Check that types of arguments match
                UNAM_ASSERT(expected_param->type == NODE_FUNC_PARAMETER, "function arguments must be of type NODE_FUNC_PARAMETER");
                UNAM_ASSERT(expected_param->right, "function arguments must be typed");
                UNAM_ASSERT(expected_param->right->type == NODE_CONCRETE_TYPE || expected_param->right->type == NODE_GENERIC_TYPE, "function argument must be of correct type");

                // And we evaluate the passed arguments to see what type it resolves to
                analyze_node(current_scope, passed_arg);

                if (passed_arg->evaluates_to_type == NULL) {
                    report_error(passed_arg, "Passed argument #%d to function '%s' evalutes to void", passed_args_len, node->lexeme);
                    goto outer_loop;
                }

                SymbolTableEntry* returned_type = find_symbol(current_scope, passed_arg->evaluates_to_type->lexeme);
                UNAM_ASSERT(returned_type != NULL, "function must return a type at this point");
                if (expected_param->right->type == NODE_GENERIC_TYPE) {
                    bool already_specialized = false;
                    int i = 0;
                    for (; i < specialized_params_types.length; i++) {
                        if (types_are_equal(specialized_params_types.data[i], expected_param->right)) {
                            already_specialized = true;
                        }
                    }
                    if (already_specialized) {
                        ASTNode* specialized_type = specialized_params_types.data[i-1];
                        ASTNode* specialized_to = specialized_params_values.data[i-1];
                        if (!types_are_equal(passed_arg->evaluates_to_type, specialized_to)) {
                            report_error(passed_arg, "Generic type '%s' has already been specialized to type '%s', but passed type '%s'", specialized_type->lexeme, specialized_to->lexeme, passed_arg->evaluates_to_type->lexeme);
                        }
                    } else {
                        // register the specialization
                        da_astnodes_append(&specialized_params_types, expected_param->right);
                        UNAM_ASSERT(passed_arg->evaluates_to_type->type == NODE_CONCRETE_TYPE, "specialization of type arguments must be done to a CONCRETE_TYPE");
                        da_astnodes_append(&specialized_params_values, passed_arg->evaluates_to_type);
                    }
                } else if (!types_are_equal(passed_arg->evaluates_to_type, expected_param->right)) {
                    report_error(passed_arg, "Expected type '%s', but passed type '%s'", expected_param->right->lexeme, passed_arg->evaluates_to_type->lexeme);
                }
                // Then it's okay
                passed_arg = passed_arg->next;
                expected_param = expected_param->next;
            }
            if (passed_arg != NULL) {
                report_error(node, "Passing more arguments than expected");
            }
            outer_loop:
            break;
        };
        case NODE_STMT_LIST: {
            UNAM_DEBUG("NODE_STMT_LIST not implemented");
            exit(69);
            break;
        };
        case NODE_LET: {
            // First we evaluate what we got at the right side
            analyze_node(current_scope, node->right);
            ASTNode* evaluated_type_node_right = node->right->evaluates_to_type;
            if (!evaluated_type_node_right) {
                report_error(node->right, "Trying to asign a void value to variable '%s'", node->lexeme);
                break;
            }

            // If explicitely type, we check
            // - if exists in the symbols table
            // - if matches the right-hand-side
            if (node->left) {
                const char* type_name = node->left->lexeme;
                UNAM_DEBUG("let %s: %s\n", node->lexeme, type_name);
                SymbolTableEntry* explicit_type = find_symbol(current_scope, type_name);
                if (explicit_type == NULL) {
                    report_error(node->left, "Used explicit type '%s' that does not exist", type_name);
                    break;
                }
                if (!types_are_equal(explicit_type->type_node, evaluated_type_node_right)) {
                    report_error(node->right, "Cannot bind variable: types don't match");
                    break;
                }
                add_symbol_unshadowed(current_scope, node->lexeme, node->left);
            }
            else {
                // node->left;
                UNAM_DEBUG("let %s: (inferred %s)\n", node->lexeme, evaluated_type_node_right->lexeme);
                add_symbol_unshadowed(current_scope, node->lexeme, evaluated_type_node_right);
            }
            break;
        };
        case NODE_ASSIGN: {
            UNAM_DEBUG("NODE_ASSIGN not implemented");
            exit(69);
            break;
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
            break;
        };
        case NODE_FOR: {
            UNAM_DEBUG("NODE_FOR not implemented");
            exit(69);
            break;
        };
        case NODE_LOOP: {
            UNAM_DEBUG("NODE_LOOP not implemented");
            exit(69);
            break;
        };
        case NODE_MATCH: {
            UNAM_DEBUG("NODE_MATCH not implemented");
            exit(69);
            break;
        };
        case NODE_MATCH_ARM: {
            UNAM_DEBUG("NODE_MATCH_ARM not implemented");
            exit(69);
            break;
        };
        case NODE_RETURN: {
            SymbolTableEntry* bound_function = find_nearest_function(current_scope);
            if (!bound_function || !bound_function->type_node) {
                report_error(node, "Used return but no function is in context");
                break;
            }
            // The funciton->return_type property store its explicit return type, if specified
            ASTNode* actual_return_type = bound_function->type_node->return_type;
            UNAM_DEBUG("  return:\n");

            // The `return <thing>;` value
            ASTNode* returning_type = node->right;
            if (returning_type) {
                analyze_node(current_scope, returning_type);
                ASTNode* evaluated_type = returning_type->evaluates_to_type;

                if (actual_return_type) {
                    // Check against it
                    if (!types_are_equal(actual_return_type, evaluated_type)) {
                        report_error(
                            returning_type, "Type of return '%s' and function signature '%s' don't match",
                            evaluated_type->lexeme, actual_return_type->lexeme
                        );
                    }
                } else {
                    UNAM_DEBUG("no explicit return type found");
                    // Function has no return type, so try to infer it by mutating the function node
                    bound_function->type_node->return_type = evaluated_type;
                }
            } else {
                // Used didn't returned something, but function was expected the opposite
                if (bound_function->type_node->return_type != NULL) {
                    report_error(node, "Expected to return a type '%s', but returned nothing", bound_function->type_node->return_type->lexeme);
                }
            }
            break;
        };
        case NODE_BREAK: {
            UNAM_DEBUG("NODE_BREAK not implemented");
            exit(69);
            break;
        };
        case NODE_IDENT_LIST: {
            UNAM_DEBUG("NODE_IDENT_LIST not implemented");
            exit(69);
            break;
        };
        case NODE_FUNC_PARAMETER: {
            UNAM_DEBUG("NODE_FUNC_PARAMETER not implemented");
            exit(69);
            break;
        };
        case NODE_BINARY_OP: {
            const char* op = node->lexeme;
            int initial_errors = semantic_errors.length;
            analyze_node(current_scope, node->left);
            analyze_node(current_scope, node->right);
            if (semantic_errors.length > initial_errors) {
                break;
            }

            ASTNode* type_left = node->left->evaluates_to_type;
            ASTNode* type_right = node->right->evaluates_to_type;

            if (!type_left) {
                report_error(node->left, "Left side of the operation '%s' is void", op);
                break;
            }
            if (!type_right) {
                report_error(node->right, "Right side of the operation is void");
                break;
            }

            if (strcmp(op, "+") == 0) {
                bool any_is_string = strcmp(type_left->lexeme, "string") == 0 || strcmp(type_right->lexeme, "string") == 0;
                if (any_is_string) {
                    // string concatenation, allow anything (for now...)
                    node->evaluates_to_type = find_symbol(current_scope, "string")->type_node;
                    break;
                }
                bool left_is_int = strcmp(type_left->lexeme, "int") == 0;
                bool left_is_float = strcmp(type_left->lexeme, "float") == 0;
                bool right_is_int = strcmp(type_right->lexeme, "int") == 0;
                bool right_is_float = strcmp(type_right->lexeme, "float") == 0;

                bool is_int_sum = left_is_int && right_is_int;
                bool is_float_sum = (left_is_int && right_is_float) || (left_is_float && right_is_int);

                if (is_int_sum) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->type_node;
                } else if (is_float_sum) {
                    node->evaluates_to_type = find_symbol(current_scope, "float")->type_node;
                } else {
                    report_error(node, "Trying to add non-numeric types: '%s' and '%s'", type_left->lexeme, type_right->lexeme);
                }
            } else if (strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
                bool left_is_int = strcmp(type_left->lexeme, "int") == 0;
                bool left_is_float = strcmp(type_left->lexeme, "float") == 0;
                bool right_is_int = strcmp(type_right->lexeme, "int") == 0;
                bool right_is_float = strcmp(type_right->lexeme, "float") == 0;

                bool is_int_diff = left_is_int && right_is_int;
                bool is_float_diff = (left_is_int && right_is_float) || (left_is_float && right_is_int);

                if (is_int_diff) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->type_node;
                } else if (is_float_diff) {
                    node->evaluates_to_type = find_symbol(current_scope, "float")->type_node;
                } else {
                    report_error(node, "Arithmetic operator %s can only be used for int types", op);
                }
            } else if (strcmp(op, "%") == 0 || strcmp(op, "**") == 0 || strcmp(op, "|") == 0 || strcmp(op, "&") == 0 || strcmp(op, "^") == 0 || strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) {
                bool left_is_int = strcmp(type_left->lexeme, "int") == 0;
                bool right_is_int = strcmp(type_right->lexeme, "int") == 0;

                bool is_int_op = left_is_int && right_is_int;

                if (is_int_op) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->type_node;
                } else {
                    report_error(node, "Operator %s can only be used for int types", op);
                }
            } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                bool left_is_builtin = is_builting_literal(type_left->lexeme);
                bool right_is_builtin = is_builting_literal(type_right->lexeme);

                if (left_is_builtin && right_is_builtin) {
                    node->evaluates_to_type = find_symbol(current_scope, "bool")->type_node;
                } else {
                    report_error(node, "Boolean operator %s can only be used for builtin types", op);
                }
            } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                bool left_is_int = strcmp(type_left->lexeme, "int") == 0;
                bool left_is_float = strcmp(type_left->lexeme, "float") == 0;
                bool right_is_int = strcmp(type_right->lexeme, "int") == 0;
                bool right_is_float = strcmp(type_right->lexeme, "float") == 0;

                bool is_int_div = left_is_int && right_is_int;
                bool is_float_div = (left_is_int && right_is_float) || (left_is_float && right_is_int);

                if (is_int_div) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->type_node;
                } else if (is_float_div) {
                    node->evaluates_to_type = find_symbol(current_scope, "float")->type_node;
                } else {
                    report_error(node, "Trying to use comparison operator for non-numeric types: '%s' and '%s'", type_left->lexeme, type_right->lexeme);
                }
            } else if (strcmp(op, "[]") == 0) {
                bool left_is_list = strcmp(type_right->lexeme, "list") == 0;;
                bool right_is_int = strcmp(type_right->lexeme, "bool") == 0;;

                if (left_is_list && right_is_int) {
                    node->evaluates_to_type = node->left->args->evaluates_to_type;
                } else {
                    report_error(node, "The indexing operator [] can only be used for list[integer]", op);
                }
            } else {
                UNAM_ASSERT(false, "got undefined binary operator");
            }
            break;
        };
        case NODE_UNARY_OP: {
            const char* op = node->lexeme;
            ASTNode* operand = node->right;
            analyze_node(current_scope, operand);
            ASTNode* operand_type = node->right->evaluates_to_type;
            if (!operand_type) {
                report_error(node->right, "Trying to apply unary operator '%s' to void type", op);
                break;
            }

            bool operand_is_int = strcmp(operand_type->lexeme, "int");
            bool operand_is_float = strcmp(operand_type->lexeme, "float");
            bool operand_is_bool = strcmp(operand_type->lexeme, "bool");

            if (strcmp(op, "++") == 0) {
                if (!operand_is_int && !operand_is_float) {
                    report_error(node, "Using ++ operator with non-numerical type");
                }
            } else if (strcmp(op, "--") == 0) {
                if (!operand_is_int && !operand_is_float) {
                    report_error(node, "Using -- operator with non-numerical type");
                }
            } else if (strcmp(op, "!") == 0) {
                // TODO: boolean cast maybe?
                if (!operand_is_bool) {
                    report_error(node, "Using ! operator with non-bool type");
                }
            } else if (strcmp(op, "~") == 0) {
                if (!operand_is_int) {
                    report_error(node, "The ~ operator is only available for integers");
                }
            } else if (strcmp(op, "-") == 0) {
                if (!operand_is_int && !operand_is_float) {
                    report_error(node, "Using - operator with non-numerical type");
                }
            } else {
                UNAM_ASSERT(false, "unknown unary operator");
            }
            node->evaluates_to_type = operand_type;
            break;
        };
        case NODE_CONCRETE_TYPE: {
            UNAM_DEBUG("  type name=%s", node->lexeme);
            SymbolTableEntry* found = find_symbol(current_scope, node->lexeme);
            if (!found) {
                report_error(node, "Referenced type '%s' is not defined", node->lexeme);
            }
            // No need to check anything
            ASTNode* generic_arg = node->generic_args;
            if (generic_arg) {
                UNAM_DEBUG_PLAIN(", with args:");
            }
            while (generic_arg) {
                analyze_node(current_scope, generic_arg);
                generic_arg = generic_arg->next;
            }
            UNAM_DEBUG_PLAIN("\n");
            break;
        };
        case NODE_GENERIC_TYPE: {
            SymbolTableEntry* found = find_symbol(current_scope, node->lexeme);
            if (!found) {
                report_error(node, "Referenced generic type '%s' is not defined", node->lexeme);
            }
            break;
        };
        case NODE_IDENTIFIER: {
            SymbolTableEntry* identifier = find_symbol(current_scope, node->lexeme);
            if (!identifier) {
                report_error(node, "Referencing identifier '%s' doesn't exists", node->lexeme);
                break;
            }
            node->evaluates_to_type = identifier->type_node;
            break;
        };
        case NODE_INT_LITERAL: {
            SymbolTableEntry* int_type = find_symbol(current_scope, "int");
            UNAM_ASSERT(int_type != NULL, "int type is not defined");
            UNAM_DEBUG("  int = %d\n", node->int_val);
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
            break;
        };
        case NODE_ENUM_VARIANT: {
            UNAM_DEBUG("NODE_ENUM_VARIANT not implemented");
            exit(69);
            break;
        };
        case NODE_STRUCT_DECL: {
            const char* struct_name = node->lexeme;
            UNAM_DEBUG("struct name=%s\n", struct_name);
            if (!add_symbol_unshadowed(current_scope, struct_name, node)) {
                report_error(node, "The type name for struct '%s' already exists", struct_name);
            }

            // TODO, this function generates errors in the inside, and we must move it to the outside
            push_scope(&current_scope);

            ASTNode* generic_arg = node->generic_args;
            while (generic_arg) {
                add_symbol_unshadowed(current_scope, generic_arg->lexeme, generic_arg);
                generic_arg = generic_arg->next;
            }

            // Check for struct fields
            // We only check that struct field names are not repeated and their types exists
            ASTNode* start_struct_field = node->args;
            ASTNode* struct_field = node->args;
            int struct_field_idx = 0;

            while (struct_field) {
                UNAM_DEBUG("  field name=%s\n", struct_field->lexeme);

                int search_struct_field_idx = 0;
                ASTNode* sf = start_struct_field;
                while (sf) {
                    if (strcmp(sf->lexeme, struct_field->lexeme) == 0 && struct_field_idx != search_struct_field_idx) {
                        report_error(struct_field, "Duplicated field name '%s' for struct", struct_field->lexeme);
                        break;
                    }
                    search_struct_field_idx++;
                    sf = sf->next;
                }

                // check for type
                UNAM_ASSERT(struct_field->right != NULL, "struct_field->right must not be null");
                analyze_node(current_scope, struct_field->right);
                ASTNode* field_type = struct_field->right;
                const char* field_type_name = field_type->lexeme;
                SymbolTableEntry* found = find_symbol(current_scope, field_type_name);
                if (!found) {
                    report_error(struct_field->right, "Type '%s' for struct field '%s' is not defined", field_type_name, struct_field->lexeme);
                }

                struct_field_idx++;
                struct_field = struct_field->next;
            }

            pop_scope(&current_scope);
            break;
        };
        case NODE_STRUCT_FIELD: {
            UNAM_DEBUG("NODE_STRUCT_FIELD not implemented");
            exit(69);
            break;
        };
        case NODE_LIST_LITERAL: {
            ASTNode* element = node->args;
            ASTNode* last_element_type = NULL;
            UNAM_DEBUG("  list: \n");

            while (element) {
                analyze_node(current_scope, element);
                if (!last_element_type) {
                    last_element_type = element->evaluates_to_type;
                } else {
                    // check if this element has the same type of the rest
                    if (!types_are_equal(last_element_type, element->evaluates_to_type)) {
                        report_error(element, "Types of elements of list are not the same. Expected '%s' but found '%s'", last_element_type->lexeme, element->evaluates_to_type->lexeme);
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
            break;
        };
        case NODE_PIPELINE: {
            UNAM_DEBUG("NODE_PIPELINE not implemented");
            exit(69);
            break;
        };
        case NODE_PLACEHOLDER: {
            UNAM_DEBUG("NODE_PLACEHOLDER not implemented");
            exit(69);
            break;
        };
        case NODE_MEMBER_ACCESS: {
            UNAM_DEBUG("NODE_MEMBER_ACCESS not implemented");
            exit(69);
            break;
        };
        case NODE_RANGE: {
            UNAM_DEBUG("NODE_RANGE not implemented");
            exit(69);
            break;
        };
        case NODE_LAMBDA: {
            UNAM_DEBUG("NODE_LAMBDA not implemented");
            exit(69);
            break;
            push_scope(&current_scope);
            printf("Lambda expression:\n");
            ASTNode* arg = node->args;
            while (arg) {
                if (arg->type == NODE_FUNC_PARAMETER) {
                    const char* type_name = arg->lexeme;
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

bool analyze_semantics(ASTNode* root) {
    printf("\n🪰 Starting Semantic Analysis...\n");
    da_cstr_init(&semantic_errors, 16);
    Scope* current_scope = NULL;
    push_scope(&current_scope);

    analyze_node(current_scope, root);
    pop_scope(&current_scope);

    bool success = true;
    if (semantic_errors.length > 0) {
        success = false;
        fprintf(stderr, UNAM_RED "\nErrors:\n" UNAM_RESET);
        for (int i = 0; i < semantic_errors.length; i++) {
            fprintf(stderr, "  At %s\n", semantic_errors.data[i]);
            free(semantic_errors.data[i]);
        }
    }
    da_free(&semantic_errors);
    return success;
}
