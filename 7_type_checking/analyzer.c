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
        int len = snprintf(NULL, 0, "[%d:%d] %s", node->loc.line, node->loc.col, message);
        final_message = malloc(len + 1);
        snprintf(final_message, len + 1, "[%d:%d] %s", node->loc.line, node->loc.col, message);
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

    const char* type1_name = type1->as.type.name;
    const char* type2_name = type2->as.type.name;

    // First check lexemes
    if (type1_name == NULL && type2_name != NULL) {
        return false;
    }
    if (type1_name != NULL && type2_name == NULL) {
        return false;
    }
    if (type1_name && type2_name && strcmp(type1_name, type2_name) != 0) {
        return false;
    }

    // Then check generic args
    ASTNode* g1 = type1->as.type.generic_args;
    ASTNode* g2 = type2->as.type.generic_args;
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
    UNAM_ASSERT(false, "find_nearest_function shit is broken....");
    Scope* s = current_scope;
    SymbolTableEntry* last_func = NULL;

    while (s) {
        SymbolTableEntry* sym = s->symbols;
        while (sym) {
            if (sym->node->type == NODE_FUNCTION) {
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
    sym->node = type_node;
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
            analyze_node(current_scope, node->as.program.body);
            pop_scope(&current_scope);
            break;
        };
        case NODE_FUNCTION: {
            char* func_name = node->as.function.name;
            UNAM_DEBUG("function '%s'\n", func_name);
            // The function is available in the current scope, and creates another
            if (!add_symbol_unshadowed(current_scope, func_name, node)) {
                report_error(node, "Trying to name a function '%s', but it's already defined", func_name);
            }
            push_scope(&current_scope);

            // For each function type argument, if any
            ASTNode* generic_arg = node->as.function.generic_args;
            while (generic_arg) {
                // We add the generic type as symbol, and then analyze the node
                UNAM_ASSERT(generic_arg->type == NODE_CONCRETE_TYPE, "generic arguments must be concrete types");
                // And we promote them to generic types, as they appear in the generic type parameters list
                generic_arg->type = NODE_GENERIC_TYPE;
                add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg);
                analyze_node(current_scope, generic_arg);
                generic_arg = generic_arg->next;
            }

            // For each function parameter, if any
            ASTNode* param = node->as.function.params;
            while (param) {
                ASTNode* param_type = param->as.func_param.type_expr;
                UNAM_ASSERT(param->type == NODE_FUNC_PARAMETER, "function parameter must have NODE_FUNC_PARAMETER type");
                UNAM_ASSERT(param_type != NULL && (param_type->type == NODE_CONCRETE_TYPE || param_type->type == NODE_GENERIC_TYPE), "invalid arg->right type");
                // First we analyze the type of the parameter
                analyze_node(current_scope, param_type);
                // Parameter name
                const char* parameter_name = param->as.func_param.name;
                // Parameter type
                const char* type_name = param_type->as.type.name;
                // We add the parameter to the symbols table, with the given type
                UNAM_DEBUG("  arg name=%s\n", parameter_name);
                SymbolTableEntry* parameter_type = find_symbol(current_scope, type_name);
                if (!parameter_type || !parameter_type->node) {
                    report_error(param, "Function parameter '%s' is using a type '%s' that doesn't exist", parameter_name, type_name);
                } else {
                    // This overrides the concrete type with a generic type, as this param is referecing a generic types in the symbol table
                    if (parameter_type->node->type == NODE_GENERIC_TYPE) {
                        param_type->type = NODE_GENERIC_TYPE;
                    }
                    param->evaluates_to_type = parameter_type->node;
                    // We add this parameter to the current scope
                    add_symbol_unchecked(current_scope, parameter_name, param);
                }
                param = param->next;
            }
            ASTNode* func_return_type = node->as.function.return_type;
            if (node->as.function.return_type) {
                UNAM_DEBUG("  ->\n");
                analyze_node(current_scope, func_return_type);
            }
            UNAM_DEBUG("body ->\n");
            analyze_node(current_scope, node->as.function.body);
            UNAM_DEBUG("<- end body\n");
            pop_scope(&current_scope);
            break;
        };
        case NODE_CALL: {
            // Function call
            ASTNode* func = node->as.call.callee;
            char* func_name = node->as.call.debug_name;
            UNAM_DEBUG("call: %s\n", func_name);

            SymbolTableEntry* resolved_func = NULL;

            // If we are calling an identifier, we only validate that the function exists in the symbol table
            if (func->type == NODE_IDENTIFIER) {
                resolved_func = find_symbol(current_scope, func_name);
                if (!resolved_func) {
                    report_error(func, "Function is not defined: '%s'", func_name);
                    break;
                }
                if (resolved_func->node->type != NODE_FUNCTION && resolved_func->node->type != NODE_LAMBDA) {
                    report_error(func, "Trying to call a non function '%s'", node_repr(func->evaluates_to_type));
                    break;
                }
            }
            // And in this case, we must evaluate what's on the left and make sure it evaluates to a function!
            else {
                // We first analyze the left node so that `evaluates_to_type` gets populated
                analyze_node(current_scope, func);
                resolved_func = find_symbol(current_scope, func->evaluates_to_type->as.type.name);
                if (resolved_func == NULL || (resolved_func->node->type != NODE_FUNCTION && resolved_func->node->type != NODE_LAMBDA)) {
                    report_error(func, "Trying to call a non function '%s'", node_repr(func->evaluates_to_type));
                    break;
                }
            }

            // And now that we now a function is being called, we validate its arguments!
            ASTNode* calling_function = resolved_func->node;

            ASTNode* expected_param = calling_function->as.function.params;
            ASTNode* passed_arg = node->as.call.args;

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
                ASTNode* param_type = expected_param->as.func_param.type_expr;
                passed_args_len++;
                // Check that types of arguments match
                UNAM_ASSERT(expected_param->type == NODE_FUNC_PARAMETER, "function arguments must be of type NODE_FUNC_PARAMETER");
                UNAM_ASSERT(expected_param->as.func_param.type_expr, "function arguments must be typed");
                int param_node_type = param_type->type;
                UNAM_ASSERT(param_node_type == NODE_CONCRETE_TYPE || param_node_type == NODE_GENERIC_TYPE, "function argument must be of correct type");

                // And we evaluate the passed arguments to see what type it resolves to
                analyze_node(current_scope, passed_arg);

                if (passed_arg->evaluates_to_type == NULL) {
                    report_error(passed_arg, "Passed argument #%d to function '%s' evalutes to void", passed_args_len, node_repr(node));
                    goto outer_loop;
                }

                SymbolTableEntry* returned_type = find_symbol(current_scope, passed_arg->evaluates_to_type->as.type.name);
                UNAM_ASSERT(returned_type != NULL, "function must return a type at this point");
                if (param_type->type == NODE_GENERIC_TYPE) {
                    bool already_specialized = false;
                    int i = 0;
                    for (; i < specialized_params_types.length; i++) {
                        if (types_are_equal(specialized_params_types.data[i], param_type)) {
                            already_specialized = true;
                        }
                    }
                    if (already_specialized) {
                        ASTNode* specialized_type = specialized_params_types.data[i-1];
                        ASTNode* specialized_to = specialized_params_values.data[i-1];
                        if (!types_are_equal(passed_arg->evaluates_to_type, specialized_to)) {
                            report_error(
                                passed_arg,
                                "Generic type '%s' has already been specialized to type '%s', but passed type '%s'",
                                node_repr(specialized_type), node_repr(specialized_to), node_repr(passed_arg->evaluates_to_type)
                            );
                        }
                    } else {
                        // register the specialization
                        da_astnodes_append(&specialized_params_types, param_type);
                        UNAM_ASSERT(passed_arg->evaluates_to_type->type == NODE_CONCRETE_TYPE, "specialization of type arguments must be done to a CONCRETE_TYPE");
                        da_astnodes_append(&specialized_params_values, passed_arg->evaluates_to_type);
                    }
                } else if (!types_are_equal(passed_arg->evaluates_to_type, param_type)) {
                    report_error(passed_arg, "Expected type '%s', but passed type '%s'", node_repr(param_type), node_repr(passed_arg->evaluates_to_type));
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
            ASTNode* let_type = node->as.let.declared_type;
            char* let_name = node->as.let.name;
            ASTNode* let_value = node->as.let.value;
            analyze_node(current_scope, let_value);
            ASTNode* evaluated_type_node_right = let_value->evaluates_to_type;
            if (!evaluated_type_node_right) {
                report_error(let_value, "Trying to asign a void value to variable '%s'", let_name);
                break;
            }

            // If explicitely type, we check
            // - if exists in the symbols table
            // - if matches the right-hand-side
            if (let_type) {
                const char* type_name = let_type->as.type.name;
                UNAM_DEBUG("let %s: %s\n", let_name, type_name);
                SymbolTableEntry* explicit_type = find_symbol(current_scope, type_name);
                if (explicit_type == NULL) {
                    report_error(let_type, "Used explicit type '%s' that does not exist", type_name);
                    break;
                }
                if (!types_are_equal(explicit_type->node, evaluated_type_node_right)) {
                    report_error(let_value, "Cannot bind variable: types don't match");
                    break;
                }
                add_symbol_unshadowed(current_scope, let_name, let_type);
            }
            else {
                // node->left;
                UNAM_DEBUG("let %s: (inferred %s)\n", let_name, node_repr(evaluated_type_node_right));
                add_symbol_unshadowed(current_scope, let_name, evaluated_type_node_right);
            }
            break;
        };
        case NODE_ASSIGN: {
            UNAM_DEBUG("NODE_ASSIGN not implemented");
            exit(69);
            // analyze_node(current_scope, node->as.assign.value);
            // if (node->left && node->left->lexeme) {
            //     printf("Assigning variable: %s (via %s)\n", node->left->lexeme, node->lexeme);
            //     if (!find_symbol(current_scope, node->left->lexeme)) {
            //         fprintf(stderr, "Error: Semantic error at line %d: Undefined variable '%s'\n", 0, node->left->lexeme);
            //     }
            // }
            break;
        };
        case NODE_IF: {
            ASTNode* if_cond = node->as.if_expr.cond;
            analyze_node(current_scope, if_cond);
            ASTNode* condition_type = if_cond->evaluates_to_type;
            if (strcmp(condition_type->as.type.name, "bool") != 0) {
                report_error(if_cond, "if condition must evaluate to a bool");
            }
            analyze_node(current_scope, node->as.if_expr.then_body);
            analyze_node(current_scope, node->as.if_expr.else_body);
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
            if (!bound_function || !bound_function->node) {
                report_error(node, "Used return but no function is in context");
                break;
            }
            // The funciton->return_type property store its explicit return type, if specified
            ASTNode* actual_return_type = bound_function->node->as.function.return_type;
            UNAM_DEBUG("  return:\n");

            // The `return <thing>;` value
            ASTNode* returning_type = node->as.return_stmt.value;
            if (returning_type) {
                analyze_node(current_scope, returning_type);
                ASTNode* evaluated_type = returning_type->evaluates_to_type;

                if (actual_return_type) {
                    // Check against it
                    if (!types_are_equal(actual_return_type, evaluated_type)) {
                        report_error(
                            returning_type, "Type of return '%s' and function signature '%s' don't match",
                            node_repr(evaluated_type), node_repr(actual_return_type)
                        );
                    }
                } else {
                    UNAM_DEBUG("no explicit return type found");
                    // Function has no return type, so try to infer it by mutating the function node
                    bound_function->node->as.function.return_type = evaluated_type;
                }
            } else {
                // Used didn't returned something, but function was expected the opposite
                if (actual_return_type != NULL) {
                    report_error(node, "Expected to return a type '%s', but returned nothing", node_repr(actual_return_type));
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
            const char* op = node->as.binop.op;
            int initial_errors = semantic_errors.length;
            analyze_node(current_scope, node->as.binop.left);
            analyze_node(current_scope, node->as.binop.right);
            if (semantic_errors.length > initial_errors) {
                break;
            }

            ASTNode* type_left = node->as.binop.left->evaluates_to_type;
            ASTNode* type_right = node->as.binop.right->evaluates_to_type;

            if (!type_left) {
                report_error(node->as.binop.left, "Left side of the operation '%s' is void", op);
                break;
            }
            if (!type_right) {
                report_error(node->as.binop.right, "Right side of the operation is void");
                break;
            }

            const char* type_left_name = type_left->as.type.name;
            const char* type_right_name = type_left->as.type.name;

            if (strcmp(op, "+") == 0) {
                bool any_is_string = strcmp(type_left_name, "string") == 0 || strcmp(type_right_name, "string") == 0;
                if (any_is_string) {
                    // string concatenation, allow anything (for now...)
                    node->evaluates_to_type = find_symbol(current_scope, "string")->node;
                    break;
                }
                bool left_is_int = strcmp(type_left_name, "int") == 0;
                bool left_is_float = strcmp(type_left_name, "float") == 0;
                bool right_is_int = strcmp(type_right_name, "int") == 0;
                bool right_is_float = strcmp(type_right_name, "float") == 0;

                bool is_int_sum = left_is_int && right_is_int;
                bool is_float_sum = (left_is_int && right_is_float) || (left_is_float && right_is_int);

                if (is_int_sum) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->node;
                } else if (is_float_sum) {
                    node->evaluates_to_type = find_symbol(current_scope, "float")->node;
                } else {
                    report_error(node, "Trying to add non-numeric types: '%s' and '%s'", type_left_name, type_right_name);
                }
            } else if (strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
                bool left_is_int = strcmp(type_left_name, "int") == 0;
                bool left_is_float = strcmp(type_left_name, "float") == 0;
                bool right_is_int = strcmp(type_right_name, "int") == 0;
                bool right_is_float = strcmp(type_right_name, "float") == 0;

                bool is_int_diff = left_is_int && right_is_int;
                bool is_float_diff = (left_is_int && right_is_float) || (left_is_float && right_is_int);

                if (is_int_diff) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->node;
                } else if (is_float_diff) {
                    node->evaluates_to_type = find_symbol(current_scope, "float")->node;
                } else {
                    report_error(node, "Arithmetic operator %s can only be used for int types", op);
                }
            } else if (strcmp(op, "%") == 0 || strcmp(op, "**") == 0 || strcmp(op, "|") == 0 || strcmp(op, "&") == 0 || strcmp(op, "^") == 0 || strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) {
                bool left_is_int = strcmp(type_left_name, "int") == 0;
                bool right_is_int = strcmp(type_right_name, "int") == 0;

                bool is_int_op = left_is_int && right_is_int;

                if (is_int_op) {
                    node->evaluates_to_type = find_symbol(current_scope, "int")->node;
                } else {
                    report_error(node, "Operator %s can only be used for int types", op);
                }
            } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                bool left_is_builtin = is_builting_literal(type_left_name);
                bool right_is_builtin = is_builting_literal(type_right_name);

                if (left_is_builtin && right_is_builtin) {
                    node->evaluates_to_type = find_symbol(current_scope, "bool")->node;
                } else {
                    report_error(node, "Boolean operator %s can only be used for builtin types", op);
                }
            } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                bool left_is_int = strcmp(type_left_name, "int") == 0;
                bool left_is_float = strcmp(type_left_name, "float") == 0;
                bool right_is_int = strcmp(type_right_name, "int") == 0;
                bool right_is_float = strcmp(type_right_name, "float") == 0;

                if (left_is_int || left_is_float || right_is_int || right_is_float) {
                    // ok
                } else {
                    report_error(node, "Comparison operator %s must only be used for numeric types", op);
                }
                node->evaluates_to_type = find_symbol(current_scope, "bool")->node;

            } else if (strcmp(op, "[]") == 0) {
                bool left_is_list = strcmp(type_right_name, "list") == 0;;
                bool right_is_int = strcmp(type_right_name, "bool") == 0;;

                if (left_is_list && right_is_int) {
                    // And the result of this index access is the first generic argument of what we assume is a list
                    UNAM_ASSERT(type_left->as.type.generic_args->type, "function from index access operator must have a generic type");
                    node->evaluates_to_type = type_left->as.type.generic_args;
                } else {
                    report_error(node, "The indexing operator [] can only be used for list[integer]", op);
                }
            } else {
                UNAM_ASSERT(false, "got undefined binary operator");
            }
            break;
        };
        case NODE_UNARY_OP: {
            const char* op = node->as.unary.op;
            ASTNode* operand = node->as.unary.operand;
            analyze_node(current_scope, operand);
            ASTNode* operand_type = operand->evaluates_to_type;
            if (!operand_type) {
                report_error(operand, "Trying to apply unary operator '%s' to void type", op);
                break;
            }

            const char* operand_type_name = operand_type->as.type.name;

            bool operand_is_int = strcmp(operand_type_name, "int") == 0;
            bool operand_is_float = strcmp(operand_type_name, "float") == 0;
            bool operand_is_bool = strcmp(operand_type_name, "bool") == 0;

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
            UNAM_DEBUG("  type name=%s", node_repr(node));
            SymbolTableEntry* found = find_symbol(current_scope, node->as.type.name);
            if (!found) {
                report_error(node, "Referenced type '%s' is not defined", node_repr(node));
            }
            // No need to check anything
            ASTNode* generic_arg = node->as.type.generic_args;
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
            SymbolTableEntry* found = find_symbol(current_scope, node_repr(node));
            if (!found) {
                report_error(node, "Referenced generic type '%s' is not defined", node_repr(node));
            }
            break;
        };
        case NODE_IDENTIFIER: {
            SymbolTableEntry* identifier = find_symbol(current_scope, node_repr(node));
            if (!identifier) {
                report_error(node, "Referencing identifier '%s' doesn't exists", node_repr(node));
                break;
            }
            node->evaluates_to_type = identifier->node;
            break;
        };
        case NODE_INT_LITERAL: {
            SymbolTableEntry* int_type = find_symbol(current_scope, "int");
            UNAM_ASSERT(int_type != NULL, "int type is not defined");
            UNAM_DEBUG("  int = %d\n", node->as.int_lit.value);
            node->evaluates_to_type = int_type->node;
            break;
        };
        case NODE_FLOAT_LITERAL: {
            SymbolTableEntry* float_type = find_symbol(current_scope, "float");
            UNAM_ASSERT(float_type != NULL, "float type is not defined");
            node->evaluates_to_type = float_type->node;
            break;
        };
        case NODE_BOOL_LITERAL: {
            SymbolTableEntry* bool_type = find_symbol(current_scope, "bool");
            UNAM_ASSERT(bool_type != NULL, "bool type is not defined");
            node->evaluates_to_type = bool_type->node;
            break;
        };
        case NODE_STRING_LITERAL: {
            SymbolTableEntry* string_type = find_symbol(current_scope, "string");
            UNAM_ASSERT(string_type != NULL, "string type is not defined");
            node->evaluates_to_type = string_type->node;
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
            const char* struct_name = node->as.struct_decl.name;
            UNAM_DEBUG("struct name=%s\n", struct_name);
            if (!add_symbol_unshadowed(current_scope, struct_name, node)) {
                report_error(node, "The type name for struct '%s' already exists", struct_name);
            }

            // TODO, this function generates errors in the inside, and we must move it to the outside
            push_scope(&current_scope);

            ASTNode* generic_arg = node->as.struct_decl.generic_args;
            while (generic_arg) {
                add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg);
                generic_arg = generic_arg->next;
            }

            // Check for struct fields
            // We only check that struct field names are not repeated and their types exists
            ASTNode* start_struct_field = node->as.struct_decl.fields;
            ASTNode* struct_field = node->as.struct_decl.fields;
            int struct_field_idx = 0;

            while (struct_field) {
                UNAM_DEBUG("  field name=%s\n", node_repr(struct_field));

                int search_struct_field_idx = 0;
                ASTNode* sf = start_struct_field;
                while (sf) {
                    if (strcmp(sf->as.struct_field.name, struct_field->as.struct_field.name) == 0 && struct_field_idx != search_struct_field_idx) {
                        report_error(struct_field, "Duplicated field name '%s' for struct", node_repr(struct_field));
                        break;
                    }
                    search_struct_field_idx++;
                    sf = sf->next;
                }

                // check for type
                UNAM_ASSERT(struct_field->as.struct_field.value != NULL, "struct_field->right must not be null");
                analyze_node(current_scope, struct_field->as.struct_field.value);
                ASTNode* field_type = struct_field->as.struct_field.value;
                const char* field_type_name = field_type->as.type.name;
                SymbolTableEntry* found = find_symbol(current_scope, field_type_name);
                if (!found) {
                    report_error(struct_field, "Type '%s' for struct field '%s' is not defined", field_type_name, node_repr(struct_field));
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
            ASTNode* element = node->as.list_lit.items;
            ASTNode* last_element_type = NULL;
            UNAM_DEBUG("  list: \n");

            while (element) {
                analyze_node(current_scope, element);
                if (!last_element_type) {
                    last_element_type = element->evaluates_to_type;
                } else {
                    // check if this element has the same type of the rest
                    if (!types_are_equal(last_element_type, element->evaluates_to_type)) {
                        report_error(
                            element,
                            "Types of elements of list are not the same. Expected '%s' but found '%s'",
                            node_repr(last_element_type), node_repr(element->evaluates_to_type)
                        );
                    }
                }
                element = element->next;
            }

            SymbolTableEntry* list_type = find_symbol(current_scope, "List");
            UNAM_ASSERT(list_type != NULL, "List type is not defined");
            node->evaluates_to_type = list_type->node;
            node->evaluates_to_type->as.type.generic_args = last_element_type;
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
            ASTNode* arg = node->as.lambda.params;
            while (arg) {
                if (arg->type == NODE_FUNC_PARAMETER) {
                    const char* type_name = arg->as.func_param.name;
                    printf("  Parameter: %s : %s\n", node_repr(arg), type_name);
                    add_symbol_unshadowed(current_scope, node_repr(arg), arg->as.func_param.type_expr);
                }
                else {
                    add_symbol_unshadowed(current_scope, node_repr(arg), NULL);
                }
                arg = arg->next;
            }
            analyze_node(current_scope, node->as.lambda.body);
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
