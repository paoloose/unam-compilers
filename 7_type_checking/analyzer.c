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
    if (!current_scope) {
        UNAM_ASSERT(false, "trying to pop null scope");
        return;
    }
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

ASTNode* find_nearest_function(const da_astnodes* contexts_stack) {
    size_t i = contexts_stack->length;
    while (i-->0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_FUNCTION) {
            return entry;
        }
    }
    return NULL;
}

ASTNode* find_nearest_breakable_context(const da_astnodes* contexts_stack) {
    size_t i = contexts_stack->length;
    ASTNode* breakable_stmt;
    while (i-->0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_FOR || entry->type == NODE_FOREACH || entry->type == NODE_LOOP) {
            breakable_stmt = entry;
            break;
        }
    }

    if (!breakable_stmt) return NULL;
    ASTNode* last_scope;

    i = contexts_stack->length;
    while (i-->0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_SCOPE) {
            last_scope = entry;
        }

        switch (breakable_stmt->type) {
            case NODE_FOR: {
                if (breakable_stmt->as.for_expr.body == last_scope || breakable_stmt->as.for_expr.else_body == last_scope) {
                    return last_scope;
                }
                break;
            };
            case NODE_FOREACH: {
                if (breakable_stmt->as.foreach_expr.body == last_scope || breakable_stmt->as.foreach_expr.else_body == last_scope) {
                    return last_scope;
                }
                break;
            };
            case NODE_LOOP: {
                if (breakable_stmt->as.loop_expr.body == last_scope || breakable_stmt->as.loop_expr.else_body == last_scope) {
                    return last_scope;
                }
                break;
            };
            default: UNAM_ASSERT(false, "unreachable");
        }
    }
    return NULL;
}

ASTNode* find_nearest_scope(const da_astnodes* contexts_stack) {
    size_t i = contexts_stack->length;
    while (i-->0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_SCOPE) {
            return entry;
        }
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

int calls = 0;

void semantic_analyze(Scope* initial_scope, ASTNode* root) {
    UNAM_ASSERT(++calls == 1, "analyze_node must NOT be called recursively");
    Scope* current_scope = initial_scope;

    da_astnodes contexts_stack;
    da_astnodes_init(&contexts_stack, 32);

    da_analyze_frames stack;
    da_analyze_frames_init(&stack, 128);
    da_analyze_frames_append(&stack, (AnalyzeFrame){ root, PHASE_ENTER });

    AnalyzeFrame frame;

    while (da_analyze_frames_pop(&stack, &frame)) {
        ASTNode* node = frame.node;
        VisitPhase phase = frame.phase;
        if (!node) continue;

        if (node->next && phase == PHASE_ENTER) {
            da_analyze_frames_append(&stack, (AnalyzeFrame){ node->next, PHASE_ENTER });
        }

        switch (node->type) {
            case NODE_PROGRAM: {
                if (phase == PHASE_ENTER) {
                    push_scope(&current_scope);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.program.body, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    pop_scope(&current_scope);
                }
                break;
            };
            case NODE_SCOPE: {
                if (phase == PHASE_ENTER) {
                    push_scope(&current_scope);
                    da_astnodes_append(&contexts_stack, node);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    if (node->as.scope.body) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.scope.body, PHASE_ENTER });
                    }
                } else if (phase == PHASE_EXIT) {
                    ASTNode* last_stmt = node->as.scope.body;
                    if (last_stmt) {
                        while (last_stmt->next) {
                            last_stmt = last_stmt->next;
                        }
                        if (last_stmt->type == NODE_RETURN && !last_stmt->as.return_stmt.is_explicit) {
                            node->evaluates_to_type = last_stmt->as.return_stmt.value ? last_stmt->as.return_stmt.value->evaluates_to_type : NULL;
                        }
                    }
                    da_astnodes_pop(&contexts_stack, NULL);
                    pop_scope(&current_scope);
                }
                break;
            };
            case NODE_FUNC_PARAMETER: {
                ASTNode* param_type = node->as.func_param.type_expr;
                // First we analyze the type of the parameter
                const char* parameter_name = node->as.func_param.name;
                const char* parameter_type_name = param_type->as.type.name;

                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("  arg name=%s\n", parameter_name);
                    // We add the parameter to the symbols table, with the given type
                    add_symbol_shadowed(current_scope, parameter_name, node);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.func_param.type_expr, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    SymbolTableEntry* parameter_type = find_symbol(current_scope, parameter_type_name);
                    if (!parameter_type || !parameter_type->node) {
                        report_error(node, "Function parameter '%s' is using a type '%s' that doesn't exist", parameter_name, parameter_type_name);
                    } else {
                        // This overrides the concrete type with a generic type, as this param is referecing a generic types in the symbol table
                        if (parameter_type->node->type == NODE_GENERIC_TYPE) {
                            param_type->type = NODE_GENERIC_TYPE;
                        }
                        node->evaluates_to_type = param_type;
                    }
                }
                break;
            };
            case NODE_FUNCTION: {
                char* func_name = node->as.function.name;
                bool is_lambda = node->as.function.is_lambda;
                if (phase == PHASE_ENTER) {
                    da_astnodes_append(&contexts_stack, node);

                    UNAM_DEBUG("function '%s'\n", func_name);
                    // Named functions bind to the current scope; lambdas do not
                    if (!is_lambda) {
                        if (!add_symbol_unshadowed(current_scope, func_name, node)) {
                            report_error(node, "Trying to name a function '%s', but it's already defined", func_name);
                        }
                    }
                    push_scope(&current_scope);

                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    if (node->as.function.body) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.function.body, PHASE_ENTER });
                    }
                    if (node->as.function.return_type) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.function.return_type, PHASE_ENTER });
                    }

                    // For each function type param, if any
                    da_analyze_frames_reverse_mode_start(&stack);
                    ASTNode* param = node->as.function.params;
                    while (param) {
                        ASTNode* param_type = param->as.func_param.type_expr;
                        UNAM_ASSERT(param->type == NODE_FUNC_PARAMETER, "function parameter must have NODE_FUNC_PARAMETER type");
                        UNAM_ASSERT(param_type != NULL && (param_type->type == NODE_CONCRETE_TYPE || param_type->type == NODE_GENERIC_TYPE), "invalid func_param type");
                        // First we analyze the type of the parameter
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ param, PHASE_ENTER });
                        param = param->next;
                    }
                    da_analyze_frames_reverse_mode_end(&stack);

                    // For each function type param, if any
                    ASTNode* generic_arg = node->as.function.generic_args;
                    while (generic_arg) {
                        // We add the generic type as symbol, and then analyze the node
                        UNAM_ASSERT(generic_arg->type == NODE_CONCRETE_TYPE, "generic type params must initially be concrete types");
                        // And we promote them to generic types, as they appear in the generic type parameters list
                        generic_arg->type = NODE_GENERIC_TYPE;
                        // add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg);
                        add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg);
                        da_analyze_frames_append(&stack, (AnalyzeFrame){generic_arg, PHASE_ENTER});
                        generic_arg = generic_arg->next;
                    }
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);
                    UNAM_DEBUG("<- end %s\n", func_name);

                    // Infer return type from body if not explicitly specified
                    if (node->as.function.body && node->as.function.body->evaluates_to_type) {
                        ASTNode* body_type = node->as.function.body->evaluates_to_type;
                        if (node->as.function.return_type) {
                            if (!types_are_equal(node->as.function.return_type, body_type)) {
                                report_error(
                                    node, "Function '%s' return type '%s' doesn't match body type '%s'",
                                    func_name, node_repr(node->as.function.return_type), node_repr(body_type)
                                );
                            }
                        } else {
                            node->as.function.return_type = body_type;
                        }
                    }

                    // The function node itself evaluates to itself (first-class value)
                    node->evaluates_to_type = node;
                    pop_scope(&current_scope);
                }
                break;
            };
            case NODE_CALL: {
                // Function call
                ASTNode* func = node->as.call.callee;
                char* func_name = node->as.call.debug_name;
                UNAM_DEBUG("call: %s\n", func_name);

                if (phase == PHASE_ENTER) {
                    // If we are calling an identifier, we only validate that the function exists in the symbol table
                    if (func->type == NODE_IDENTIFIER) {
                        SymbolTableEntry* resolved_func = find_symbol(current_scope, func_name);
                        if (!resolved_func || !resolved_func->node) {
                            report_error(func, "Function is not defined: '%s'", func_name);
                            break;
                        }
                        if (resolved_func->node->type != NODE_FUNCTION) {
                            report_error(func, "Trying to call a non function '%s'", node_repr(func->evaluates_to_type));
                            break;
                        }
                    }

                    // We are free of basic errors!
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    ASTNode* passed_arg = node->as.call.args;

                    // We add our arguments so we evaluate them before ocoming back to this node
                    da_analyze_frames_reverse_mode_start(&stack);
                    while (passed_arg) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ passed_arg, PHASE_ENTER });
                        passed_arg = passed_arg->next;
                    }
                    da_analyze_frames_reverse_mode_end(&stack);

                    // This means that the calling function is dynamic, so our priority is to evaluate it first!
                    if (func->type != NODE_IDENTIFIER) {
                        // We first analyze the function so that `evaluates_to_type` gets populated
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ func, PHASE_ENTER });
                    }
                } else if (phase == PHASE_EXIT) {
                    ASTNode* calling_function = NULL;

                    if (func->type == NODE_IDENTIFIER) {
                        SymbolTableEntry* func_symbol = find_symbol(current_scope, func_name);
                        UNAM_ASSERT(func_symbol != NULL && func_symbol->node != NULL, "Exit phase for NODE_CALL should have not been called");
                        calling_function = func_symbol->node;
                    }
                    // And in this case, we must evaluate what's on the left and make sure it evaluates to a function!
                    else {
                        // TODO(NOTE): write a test dynamic function calls
                        calling_function = func->evaluates_to_type;
                        if (calling_function == NULL || calling_function->type != NODE_FUNCTION) {
                            report_error(func, "Trying to call a non function '%s'", node_repr(func->evaluates_to_type));
                        }
                    }
                    // And now that we now a function is being called, we validate its arguments!

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

                        if (passed_arg->evaluates_to_type == NULL) {
                            report_error(passed_arg, "Passed argument #%d to function '%s' evalutes to void", passed_args_len, node_repr(node));
                            goto outer_loop;
                        }

                        if (passed_arg->evaluates_to_type->type != NODE_CONCRETE_TYPE && passed_arg->evaluates_to_type->type != NODE_GENERIC_TYPE) {
                            report_error(passed_arg, "Passed argument #%d to function '%s' evaluates to %s, which is not a type", passed_args_len, node_repr(node), node_repr(passed_arg->evaluates_to_type));
                            goto outer_loop;
                        }

                        UNAM_ASSERT(find_symbol(current_scope, passed_arg->evaluates_to_type->as.type.name) != NULL, "function must return a type at this point");
                        if (param_type->type == NODE_GENERIC_TYPE) {
                            bool already_specialized = false;
                            size_t i = 0;
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

                    // Set the call's evaluated type to the function's return type
                    node->evaluates_to_type = calling_function->as.function.return_type;
                }
                outer_loop:
                break;
            };

            case NODE_LET: {
                // First we evaluate what we got at the right side
                ASTNode* let_type = node->as.let.declared_type;
                char* let_name = node->as.let.name;
                ASTNode* let_value = node->as.let.value;

                if (phase == PHASE_ENTER) {
                    if (let_type) {
                        UNAM_DEBUG("let %s: %s\n", let_name, let_type->as.type.name);
                    } else {
                        UNAM_DEBUG("let %s\n", let_name);
                    }
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ let_value, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    ASTNode* evaluated_type_node_right = let_value->evaluates_to_type;
                    if (!evaluated_type_node_right) {
                        report_error(let_value, "Trying to assign a void value to variable '%s'", let_name);
                        break;
                    }

                    // If explicitely type, we check if matches the right-hand-side
                    if (let_type) {
                        if (!types_are_equal(let_type, evaluated_type_node_right)) {
                            report_error(node, "Cannot bind variable: types don't match. Expected %s, got %s", node_repr(let_type), node_repr(evaluated_type_node_right));
                            break;
                        }
                    }
                    add_symbol_unshadowed(current_scope, let_name, evaluated_type_node_right);
                }
                break;
            };
            case NODE_ASSIGN: {
                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.assign.value, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.assign.target, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    ASTNode* assign_value = node->as.assign.value;
                    if (!assign_value->evaluates_to_type) {
                        report_error(assign_value, "Right hand side of an assignment must evaluate to something");
                        break;
                    }
                    const char* operand_type_name = assign_value->evaluates_to_type->as.type.name;
                    const char* op = node->as.assign.op;

                    if (strcmp(op, "=") == 0) {}
                    else {
                        bool operand_is_int = strcmp(operand_type_name, "int") == 0;
                        bool operand_is_float = strcmp(operand_type_name, "float") == 0;

                        if (!operand_is_int && !operand_is_float) {
                            report_error(node, "Using %s operator with non-numerical type", op);
                            break;
                        }

                        if (operand_is_int) {
                            node->evaluates_to_type = find_symbol(current_scope, "int")->node;
                        } else {
                            node->evaluates_to_type = find_symbol(current_scope, "float")->node;
                        }
                    }
                }
                break;
            };
            case NODE_IF: {
                ASTNode* if_cond = node->as.if_expr.cond;

                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.if_expr.else_body, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.if_expr.then_body, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ if_cond, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    ASTNode* condition_type = if_cond->evaluates_to_type;
                    if (!condition_type) {
                        report_error(if_cond, "if condition does not evaluate to a type");
                    } else if (strcmp(condition_type->as.type.name, "bool") != 0) {
                        report_error(if_cond, "if condition must evaluate to a bool, got '%s'", node_repr(condition_type));
                    }
                    // Because if's can evaluate to a value, let's check that
                    ASTNode* then_body_type = node->as.if_expr.then_body->evaluates_to_type;

                    if (node->as.if_expr.else_body) {
                        ASTNode* else_body_type = node->as.if_expr.else_body->evaluates_to_type;

                        if (!types_are_equal(then_body_type, else_body_type)) {
                            report_error(node, "if branches must evaluate to the same type, got %s and %s", node_repr(then_body_type), node_repr(else_body_type));
                            break;
                        }
                    }
                    node->evaluates_to_type = then_body_type;
                }
                break;
            };
            case NODE_FOR: {
                if (phase == PHASE_ENTER) {
                    da_astnodes_append(&contexts_stack, node);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.for_expr.else_body, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.for_expr.body, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.for_expr.step, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.for_expr.cond, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.for_expr.init, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);
                    ASTNode* for_else_body = node->as.for_expr.else_body;
                    ASTNode* for_body = node->as.for_expr.body;
                    ASTNode* for_cond = node->as.for_expr.cond;

                    if (for_cond) {
                        if (for_cond->evaluates_to_type) {
                            ASTNode* type_bool = find_symbol(current_scope, "bool")->node;
                            if (!types_are_equal(for_cond->evaluates_to_type, type_bool)) {
                                report_error(for_cond, "Expected for condition to evaluate to bool type, got %s type instead", node_repr(for_cond->evaluates_to_type));
                            }
                        } else {
                            // TODO: warning, your condition doesn't evaluate to anything
                        }
                    }
                    UNAM_ASSERT(for_body, "for_body must be defined");
                    ASTNode* body_return_type = for_body->evaluates_to_type;

                    if (for_else_body) {
                        ASTNode* else_body_return_type = for_else_body->evaluates_to_type;
                        if (!else_body_return_type) {
                            report_error(for_else_body, "If you define an else in your for loop, then it should evaluate to something");
                            break;
                        }
                        if (!body_return_type) {
                            report_error(for_body, "If you define an else in your for loop, then your body should evaluate to something");
                            break;
                        }
                        if (!types_are_equal(body_return_type, else_body_return_type)) {
                            report_error(
                                node, "Your for loops have two branches (then, else) that evaluate to different types: '%s' and '%s'",
                                node_repr(for_body->evaluates_to_type), node_repr(for_else_body->evaluates_to_type)
                            );
                            break;
                        }
                    }
                    node->evaluates_to_type = body_return_type;
                }
                break;
            };
            case NODE_FOREACH: {
                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("foreach %s\n", node->as.foreach_expr.binded_term);
                    da_astnodes_append(&contexts_stack, node);

                    // Push a scope for the loop variable
                    push_scope(&current_scope);

                    // After iterator is analyzed, we'll bind the loop variable in PHASE_MID
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_MID });
                    // Iterator is analyzed first so we can infer the loop variable type
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.foreach_expr.iterator, PHASE_ENTER });
                } else if (phase == PHASE_MID) {
                    // Iterator has been analyzed, now bind the loop variable before analyzing the body
                    ASTNode* foreach_iterator = node->as.foreach_expr.iterator;
                    const char* binded_term = node->as.foreach_expr.binded_term;

                    if (foreach_iterator && foreach_iterator->evaluates_to_type) {
                        ASTNode* iter_type = foreach_iterator->evaluates_to_type;

                        // If iterating over a List<T>, the loop variable is of type T
                        if (iter_type->as.type.name && strcmp(iter_type->as.type.name, "List") == 0) {
                            ASTNode* element_type = iter_type->as.type.generic_args;
                            if (element_type) {
                                add_symbol_shadowed(current_scope, binded_term, element_type);
                            } else {
                                report_error(node, "Cannot infer element type of list for foreach variable '%s'", binded_term);
                                add_symbol_shadowed(current_scope, binded_term, NULL);
                            }
                        }
                        // If iterating over a Range, the loop variable is int
                        else if (iter_type->as.type.name && strcmp(iter_type->as.type.name, "int") == 0) {
                            add_symbol_shadowed(current_scope, binded_term, iter_type);
                        }
                        else {
                            // For other iterables, use the iterator's type directly
                            add_symbol_shadowed(current_scope, binded_term, iter_type);
                        }
                    } else {
                        report_error(foreach_iterator ? foreach_iterator : node, "foreach iterator does not evaluate to a type");
                        add_symbol_shadowed(current_scope, binded_term, NULL);
                    }

                    // Now schedule body and else_body for analysis
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.foreach_expr.else_body, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.foreach_expr.body, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);

                    ASTNode* foreach_body = node->as.foreach_expr.body;
                    ASTNode* foreach_else_body = node->as.foreach_expr.else_body;

                    UNAM_ASSERT(foreach_body, "foreach_body must be defined");
                    ASTNode* body_return_type = foreach_body->evaluates_to_type;

                    if (foreach_else_body) {
                        ASTNode* else_body_return_type = foreach_else_body->evaluates_to_type;
                        if (!else_body_return_type) {
                            report_error(foreach_else_body, "If you define an else in your foreach, then it should evaluate to something");
                            break;
                        }
                        if (!body_return_type) {
                            report_error(foreach_body, "If you define an else in your foreach, then your body should evaluate to something");
                            break;
                        }
                        if (!types_are_equal(body_return_type, else_body_return_type)) {
                            report_error(
                                node, "Your foreach has two branches (then, else) that evaluate to different types: '%s' and '%s'",
                                node_repr(foreach_body->evaluates_to_type), node_repr(foreach_else_body->evaluates_to_type)
                            );
                            break;
                        }
                    }
                    node->evaluates_to_type = body_return_type;
                    pop_scope(&current_scope);
                }
                break;
            };
            case NODE_LOOP: {
                if (phase == PHASE_ENTER) {
                    da_astnodes_append(&contexts_stack, node);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.loop_expr.else_body, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.loop_expr.body, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);

                    ASTNode* loop_body = node->as.loop_expr.body;
                    ASTNode* loop_else_body = node->as.loop_expr.else_body;

                    UNAM_ASSERT(loop_body, "loop_body must be defined");
                    ASTNode* body_return_type = loop_body->evaluates_to_type;

                    if (loop_else_body) {
                        ASTNode* else_body_return_type = loop_else_body->evaluates_to_type;
                        if (!else_body_return_type) {
                            report_error(loop_else_body, "If you define an else in your for loop, then it should evaluate to something");
                            break;
                        }
                        if (!body_return_type) {
                            report_error(loop_body, "If you define an else in your for loop, then your body should evaluate to something");
                            break;
                        }
                        if (!types_are_equal(body_return_type, else_body_return_type)) {
                            report_error(
                                node, "Your for loops have two branches (then, else) that evaluate to different types: '%s' and '%s'",
                                node_repr(loop_body->evaluates_to_type), node_repr(loop_else_body->evaluates_to_type)
                            );
                            break;
                        }
                    }
                    node->evaluates_to_type = body_return_type;
                }
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
                if (phase == PHASE_ENTER) {
                    if (node->as.return_stmt.is_explicit) {
                        ASTNode* bound_function = find_nearest_function(&contexts_stack);
                        if (!bound_function) {
                            report_error(node, "Used return but no function is in context");
                            break;
                        }
                    }

                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    UNAM_DEBUG("  return:\n");

                    // The `return <thing>;` value
                    ASTNode* returning_type = node->as.return_stmt.value;
                    if (returning_type) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ returning_type, PHASE_ENTER });
                    }
                } else if (phase == PHASE_EXIT) {
                    if (node->as.return_stmt.is_explicit) {
                        ASTNode* bound_function = find_nearest_function(&contexts_stack);
                        UNAM_ASSERT(bound_function, "At this point, a function must exist");

                        // The funciton->return_type property store its explicit return type, if specified
                        ASTNode* actual_return_type = bound_function->as.function.return_type;

                        // The `return <thing>;` value
                        ASTNode* returning_type = node->as.return_stmt.value;
                        if (returning_type) {
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
                                UNAM_DEBUG("no explicit return type found\n");
                                // Function has no return type, so try to infer it by mutating the function node
                                bound_function->as.function.return_type = evaluated_type;
                            }
                        } else {
                            // Used didn't returned something, but function was expected the opposite
                            if (actual_return_type != NULL) {
                                report_error(node, "Expected to return a type '%s', but returned nothing", node_repr(actual_return_type));
                            }
                        }
                    } else {
                        ASTNode* bound_scope = find_nearest_scope(&contexts_stack);
                        if (bound_scope) {
                            ASTNode* returning_type = node->as.return_stmt.value;
                            bound_scope->evaluates_to_type = returning_type;
                        }
                    }
                }
                break;
            };
            case NODE_BREAK: {
                ASTNode* to_break = find_nearest_breakable_context(&contexts_stack);

                if (phase == PHASE_ENTER) {
                    if (!to_break) {
                        report_error(node, "Nothing to break. You can only use break within loop");
                        break;
                    }
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    if (node->as.break_stmt.value) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.break_stmt.value, PHASE_ENTER });
                    }
                } else if (phase == PHASE_EXIT) {
                    UNAM_ASSERT(to_break, "must have something to break here");
                    ASTNode* break_arg = node->as.break_stmt.value;
                    ASTNode* to_break_type = to_break->evaluates_to_type;

                    if (break_arg) {
                        ASTNode* break_arg_type = break_arg->evaluates_to_type;

                        // If the block doesn't evaluate to a type yet, but user is using break,
                        // then the break type will be used as the block type
                        if (!to_break_type && break_arg_type) {
                            to_break->evaluates_to_type = break_arg_type;
                            to_break_type = break_arg_type;
                        }

                        if (!types_are_equal(break_arg_type, to_break_type)) {
                            report_error(node, "Trying to break with type %s, but block is of type %s", node_repr(break_arg_type), node_repr(to_break_type));
                        }
                    } else {
                        if (to_break_type) {
                            report_error(node, "Trying to break with no type, but type %s was expected for this block", node_repr(to_break_type));
                        }
                    }
                }
                break;
            };
            case NODE_IDENT_LIST: {
                UNAM_DEBUG("NODE_IDENT_LIST not implemented");
                exit(69);
                break;
            };
            case NODE_BINARY_OP: {
                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.binop.right, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.binop.left, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    const char* op = node->as.binop.op;

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
                    const char* type_right_name = type_right->as.type.name;

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
                            report_error(node, "Arithmetic operator %s can only be used for numeric types", op);
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
                        bool left_is_list = strcmp(type_left_name, "List") == 0;;
                        bool right_is_int = strcmp(type_right_name, "int") == 0;;

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
                }
                break;
            };
            case NODE_UNARY_OP: {
                const char* op = node->as.unary.op;
                ASTNode* operand = node->as.unary.operand;
                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ operand, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
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
                }
                break;
            };
            case NODE_CONCRETE_TYPE:
            case NODE_GENERIC_TYPE: {
                UNAM_DEBUG("  type name=%s", node_repr(node));
                if (!find_symbol(current_scope, node->as.type.name)) {
                    report_error(node, "Referenced type '%s' is not defined", node_repr(node));
                }

                // No need to check anything
                ASTNode* generic_arg = node->as.type.generic_args;
                if (generic_arg) UNAM_DEBUG_PLAIN(", with args:");

                da_analyze_frames_reverse_mode_start(&stack);
                while (generic_arg) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ generic_arg, PHASE_ENTER });
                    generic_arg = generic_arg->next;
                }
                da_analyze_frames_reverse_mode_start(&stack);
                UNAM_DEBUG_PLAIN("\n");
                break;
            };
            case NODE_IDENTIFIER: {
                SymbolTableEntry* identifier = find_symbol(current_scope, node->as.ident.name);
                if (!identifier) {
                    report_error(node, "Referencing identifier '%s' doesn't exists", node_repr(node));
                    break;
                }
                if (identifier->node) {
                    if (identifier->node->evaluates_to_type) {
                        node->evaluates_to_type = identifier->node->evaluates_to_type;
                    } else {
                        node->evaluates_to_type = identifier->node;
                    }
                }
                break;
            };
            case NODE_INT_LITERAL: {
                SymbolTableEntry* int_type = find_symbol(current_scope, "int");
                UNAM_ASSERT(int_type != NULL, "int type is not defined");
                UNAM_DEBUG("int (%d)\n", node->as.int_lit.value);
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

                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("struct name=%s\n", struct_name);
                    if (!add_symbol_unshadowed(current_scope, struct_name, node)) {
                        report_error(node, "The type name for struct '%s' already exists", struct_name);
                    }

                    push_scope(&current_scope);

                    // Register generic type parameters in the new scope
                    ASTNode* generic_arg = node->as.struct_decl.generic_args;
                    while (generic_arg) {
                        UNAM_ASSERT(generic_arg->type == NODE_CONCRETE_TYPE, "generic type params must initially be concrete types");
                        generic_arg->type = NODE_GENERIC_TYPE;
                        add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg);
                        generic_arg = generic_arg->next;
                    }

                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    // Schedule field type expressions for analysis (in reverse so they're processed in order)
                    ASTNode* struct_field = node->as.struct_decl.fields;
                    da_analyze_frames_reverse_mode_start(&stack);
                    while (struct_field) {
                        UNAM_ASSERT(struct_field->as.struct_field.value != NULL, "struct_field type_expr must not be null");
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ struct_field->as.struct_field.value, PHASE_ENTER });
                        struct_field = struct_field->next;
                    }
                    da_analyze_frames_reverse_mode_end(&stack);
                } else if (phase == PHASE_EXIT) {
                    // Validate: no duplicate field names, and each field's type exists
                    ASTNode* start_struct_field = node->as.struct_decl.fields;
                    ASTNode* struct_field = node->as.struct_decl.fields;
                    int struct_field_idx = 0;

                    while (struct_field) {
                        UNAM_DEBUG("  field name=%s\n", node_repr(struct_field));

                        // Check for duplicate field names
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

                        // Verify that the field's type exists in scope
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
                }
                break;
            };
            case NODE_STRUCT_FIELD: {
                UNAM_DEBUG("NODE_STRUCT_FIELD not implemented");
                exit(69);
                break;
            };
            case NODE_LIST_LITERAL: {
                ASTNode* element = node->as.list_lit.items;

                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("  list: \n");
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    da_analyze_frames_reverse_mode_start(&stack);
                    while (element) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ element, PHASE_ENTER });
                        element = element->next;
                    }
                    da_analyze_frames_reverse_mode_end(&stack);
                } else if (phase == PHASE_EXIT) {
                    // We iter over the evaluated types of each of its items to make sure they are the same
                    ASTNode* last_element_type = NULL;

                    while (element) {
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
                }
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
                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    UNAM_ASSERT(node->as.range.end && node->as.range.start, "both sides of the range must be defined");
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.range.end, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.range.start, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    ASTNode* start_type = node->as.range.start->evaluates_to_type;
                    ASTNode* end_type = node->as.range.end->evaluates_to_type;

                    if (!start_type || !end_type) {
                        report_error(node, "Range bounds must evaluate to a type");
                        break;
                    }

                    bool start_is_int = strcmp(start_type->as.type.name, "int") == 0;
                    bool end_is_int = strcmp(end_type->as.type.name, "int") == 0;

                    if (!start_is_int || !end_is_int) {
                        report_error(
                            node, "Both sides of a range expression must be of int type, got '%s' and '%s'",
                            node_repr(start_type), node_repr(end_type)
                        );
                    }

                    // A range evaluates to int (the element type)
                    node->evaluates_to_type = find_symbol(current_scope, "int")->node;
                }
                break;
            };

            default: {
                UNAM_DEBUG("Unknown type %d", node->type);
                exit(69);
            }
        }
    }

}

bool analyze_semantics(ASTNode* root) {
    printf("\n🪰 Starting Semantic Analysis...\n");
    da_cstr_init(&semantic_errors, 16);
    Scope* current_scope = NULL;
    push_scope(&current_scope);

    semantic_analyze(current_scope, root);
    pop_scope(&current_scope);

    bool success = true;
    if (semantic_errors.length > 0) {
        success = false;
        fprintf(stderr, UNAM_RED "\nErrors:\n" UNAM_RESET);
        for (size_t i = 0; i < semantic_errors.length; i++) {
            fprintf(stderr, "  At %s\n", semantic_errors.data[i]);
            free(semantic_errors.data[i]);
        }
    }
    da_free(&semantic_errors);
    return success;
}
