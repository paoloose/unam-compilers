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

da_diagnostics diagnostics;

static void report_diagnostic(ASTNode* node, Severity severity, const char* format, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    char* message = malloc(length + 1);
    vsnprintf(message, length + 1, format, args);

    SemanticDiagnostic diagnostic = {
        .loc = node ? node->loc : (SourceLoc){0, 0, 0},
        .message = message,
        .severity = severity
    };

    da_diagnostics_append(&diagnostics, diagnostic);
}

void report_error(ASTNode* node, const char* format, ...) {
    va_list args;
    va_start(args, format);
    report_diagnostic(node, SEVERITY_ERROR, format, args);
    va_end(args);
}

void report_warning(ASTNode* node, const char* format, ...) {
    va_list args;
    va_start(args, format);
    report_diagnostic(node, SEVERITY_WARNING, format, args);
    va_end(args);
}

static ASTNode* get_function_signature(ASTNode* func) {
    ASTNode* sig = create_node(NODE_SIGNATURE_TYPE);
    ASTNode* p = func->as.function.params;
    ASTNode* params_head = NULL;
    ASTNode* params_tail = NULL;
    while (p) {
        ASTNode* p_type = create_node(p->as.func_param.type_expr->type);
        p_type->as = p->as.func_param.type_expr->as;

        if (!params_head) params_head = p_type;
        else params_tail->next = p_type;
        params_tail = p_type;

        p = p->next;
    }
    sig->as.signature.params = params_head;
    // Use the explicit return type or the body's evaluated type if not yet set
    sig->as.signature.return_type = func->as.function.return_type;
    if (!sig->as.signature.return_type && func->as.function.body) {
        sig->as.signature.return_type = func->as.function.body->evaluates_to_type;
    }
    return sig;
}

void infer_specializations(ASTNode* expected, ASTNode* actual, da_astnodes* types, da_astnodes* values) {
    if (!expected || !actual) return;
    if (expected->type == NODE_PLAIN_TYPE) {
        for (size_t i = 0; i < types->length; i++) {
            if (strcmp(types->data[i]->as.type.name, expected->as.type.name) == 0) return;
        }
        da_astnodes_append(types, expected);
        da_astnodes_append(values, actual);
        // Link them for types_match (which expects a linked list)
        if (values->length > 1) {
            values->data[values->length - 2]->next = actual;
        }
        actual->next = NULL;
    } else if (expected->type == NODE_SIGNATURE_TYPE && actual->type == NODE_SIGNATURE_TYPE) {
        ASTNode* ep = expected->as.signature.params;
        ASTNode* ap = actual->as.signature.params;
        while (ep && ap) {
            infer_specializations(ep, ap, types, values);
            ep = ep->next;
            ap = ap->next;
        }
        infer_specializations(expected->as.signature.return_type, actual->as.signature.return_type, types, values);
    }
}


ASTNode* specialize_type(ASTNode* type, ASTNode* decl_gen, ASTNode* lit_gen) {
    if (!type || !decl_gen || !lit_gen) return type;

    if (type->type == NODE_PLAIN_TYPE) {
        ASTNode* dg = decl_gen;
        ASTNode* lg = lit_gen;
        while (dg && lg) {
            if (strcmp(dg->as.type.name, type->as.type.name) == 0) {
                return lg;
            }
            dg = dg->next;
            lg = lg->next;
        }
    } else if (type->type == NODE_SIGNATURE_TYPE) {
        ASTNode* new_sig = create_node(NODE_SIGNATURE_TYPE);
        ASTNode* p = type->as.signature.params;
        ASTNode* head = NULL;
        ASTNode* tail = NULL;
        while (p) {
            ASTNode* sp = specialize_type(p, decl_gen, lit_gen);
            ASTNode* cp = create_node(sp->type);
            cp->as = sp->as;
            if (!head) head = cp;
            else tail->next = cp;
            tail = cp;
            p = p->next;
        }
        new_sig->as.signature.params = head;
        new_sig->as.signature.return_type = specialize_type(type->as.signature.return_type, decl_gen, lit_gen);
        new_sig->evaluates_to_type = new_sig;
        return new_sig;
    }
    return type;
}

void push_scope(Scope** current_scope) {
    Scope* scope = calloc(1, sizeof(Scope));
    scope->parent = *current_scope;
    scope->depth = (*current_scope) ? (*current_scope)->depth + 1 : 0;
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

bool types_match(ASTNode* expected, ASTNode* actual, ASTNode* decl_gen, ASTNode* lit_gen) {
    if (!expected && !actual) return true;
    if (!expected || !actual) return false;

    // Handle function signatures
    if (expected->type == NODE_SIGNATURE_TYPE) {
        if (actual->type != NODE_SIGNATURE_TYPE) return false;
        ASTNode* ep = expected->as.signature.params;
        ASTNode* ap = actual->as.signature.params;
        while (ep && ap) {
            if (!types_match(ep, ap, decl_gen, lit_gen)) return false;
            ep = ep->next;
            ap = ap->next;
        }
        if (ep || ap) return false;
        return types_match(expected->as.signature.return_type, actual->as.signature.return_type, decl_gen, lit_gen);
    }
    if (actual->type == NODE_SIGNATURE_TYPE) return false; // Expected was not a signature

    // Handle generic specialization for the expected type
    // If it's a plain type, it might be a generic parameter name
    if (expected->type == NODE_PLAIN_TYPE) {
        ASTNode* dg = decl_gen;
        ASTNode* lg = lit_gen;
        while (dg && lg) {
            if (dg->type == NODE_PLAIN_TYPE && strcmp(dg->as.type.name, expected->as.type.name) == 0) {
                expected = lg;
                break;
            }
            dg = dg->next;
            lg = lg->next;
        }
    }

    const char* expected_name = NULL;
    ASTNode* expected_args = NULL;
    if (expected->type == NODE_PLAIN_TYPE) {
        expected_name = expected->as.type.name;
        expected_args = expected->as.type.generic_args;
    } else if (expected->type == NODE_ENUM_DECL) {
        expected_name = expected->as.enum_decl.name;
        expected_args = expected->as.enum_decl.generic_args;
    } else if (expected->type == NODE_STRUCT_DECL) {
        expected_name = expected->as.struct_decl.name;
        expected_args = expected->as.struct_decl.generic_args;
    }

    const char* actual_name = NULL;
    ASTNode* actual_args = NULL;
    if (actual->type == NODE_PLAIN_TYPE) {
        actual_name = actual->as.type.name;
        actual_args = actual->as.type.generic_args;
    } else if (actual->type == NODE_ENUM_DECL) {
        actual_name = actual->as.enum_decl.name;
        actual_args = actual->as.enum_decl.generic_args;
    } else if (actual->type == NODE_STRUCT_DECL) {
        actual_name = actual->as.struct_decl.name;
        actual_args = actual->as.struct_decl.generic_args;
    }

    if (!expected_name || !actual_name || strcmp(expected_name, actual_name) != 0) return false;

    // Deep compare generic args
    ASTNode* eg = expected_args;
    ASTNode* ag = actual_args;
    while (eg && ag) {
        if (!types_match(eg, ag, decl_gen, lit_gen)) return false;
        eg = eg->next;
        ag = ag->next;
    }
    return eg == NULL && ag == NULL;
}

bool types_are_equal(ASTNode* type1, ASTNode* type2) {
    return types_match(type1, type2, NULL, NULL);
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
    ASTNode* breakable_stmt = NULL;
    size_t i = contexts_stack->length;
    while (i-->0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_FOR || entry->type == NODE_FOREACH || entry->type == NODE_LOOP) {
            breakable_stmt = entry;
            break;
        }
    }

    return breakable_stmt;
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
    sym->depth = current_scope->depth;
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
        case NODE_ENUM_VARIANT: {
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
        case NODE_PLAIN_TYPE:
        case NODE_SIGNATURE_TYPE: {
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

ASTNode* find_nearest_match(const da_astnodes* contexts_stack) {
    size_t i = contexts_stack->length;
    while (i-->0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_MATCH) {
            return entry;
        }
    }
    return NULL;
}

void analyze_pattern(ASTNode* pattern, ASTNode* subject_type, Scope* current_scope) {
    if (!pattern || !subject_type) return;

    if (pattern->type == NODE_IDENTIFIER) {
        if (strcmp(pattern->as.ident.name, "_") != 0) {
            add_symbol_shadowed(current_scope, pattern->as.ident.name, subject_type);
        }
        pattern->evaluates_to_type = subject_type;
    } else if (pattern->type == NODE_INT_LITERAL || pattern->type == NODE_FLOAT_LITERAL ||
               pattern->type == NODE_BOOL_LITERAL || pattern->type == NODE_STRING_LITERAL) {
        ASTNode* lit_type = NULL;
        if (pattern->type == NODE_INT_LITERAL) lit_type = find_symbol(current_scope, "int")->node;
        else if (pattern->type == NODE_FLOAT_LITERAL) lit_type = find_symbol(current_scope, "float")->node;
        else if (pattern->type == NODE_BOOL_LITERAL) lit_type = find_symbol(current_scope, "bool")->node;
        else if (pattern->type == NODE_STRING_LITERAL) lit_type = find_symbol(current_scope, "string")->node;

        if (lit_type && !types_are_equal(lit_type, subject_type)) {
            report_error(pattern, "Pattern type %s does not match subject type %s", node_repr(lit_type), node_repr(subject_type));
        }
        pattern->evaluates_to_type = lit_type;
    }
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
                        } else {
                            node->evaluates_to_type = last_stmt->evaluates_to_type;
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
                const char* parameter_type_name = (param_type && param_type->type == NODE_PLAIN_TYPE) ? param_type->as.type.name : NULL;

                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("  arg name=%s\n", parameter_name);
                    // We add the parameter to the symbols table, with the given type
                    if (!add_symbol_unshadowed(current_scope, parameter_name, node)) {
                        report_error(node, "Function parameter name '%s' is already defined", parameter_name);
                        break;
                    }
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.func_param.type_expr, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    if (param_type->type != NODE_PLAIN_TYPE) {
                        node->evaluates_to_type = param_type;
                    } else {
                        SymbolTableEntry* parameter_type = find_symbol(current_scope, parameter_type_name);
                        if (!parameter_type || !parameter_type->node) {
                            report_error(node, "Function parameter '%s' is using a type '%s' that doesn't exist", parameter_name, parameter_type_name);
                        } else {
                            node->evaluates_to_type = param_type;
                        }
                    }
                }
                break;
            };
            case NODE_FUNCTION: {
                char* func_name = node->as.function.name;
                bool is_lambda = node->as.function.is_lambda;

                if (phase == PHASE_ENTER) {
                    node->evaluates_to_type = node;
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

                    // Schedule body for analysis after signature
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.function.body, PHASE_ENTER });

                    if (node->as.function.return_type) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.function.return_type, PHASE_ENTER });
                    }

                    // For each function param
                    ASTNode* param = node->as.function.params;
                    while (param) {
                        ASTNode* param_type = param->as.func_param.type_expr;
                        UNAM_ASSERT(param->type == NODE_FUNC_PARAMETER, "function parameter must have NODE_FUNC_PARAMETER type");
                        UNAM_ASSERT(param_type != NULL && (param_type->type == NODE_PLAIN_TYPE || param_type->type == NODE_SIGNATURE_TYPE || param_type->type == NODE_STRUCT_DECL || param_type->type == NODE_ENUM_DECL), "invalid func_param type");
                        param = param->next;
                    }

                    // For each generic type param
                    ASTNode* generic_arg = node->as.function.generic_args;
                    while (generic_arg) {
                        generic_arg->type = NODE_PLAIN_TYPE;
                        if (!add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg)) {
                            report_error(generic_arg, "Trying to name a generic type '%s', but it's already defined", generic_arg->as.type.name);
                            break;
                        }
                        generic_arg = generic_arg->next;
                    }

                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.function.params, PHASE_ENTER });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.function.generic_args, PHASE_ENTER });

                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);
                    pop_scope(&current_scope);
                    UNAM_DEBUG("<- end %s\n", func_name);

                    // Infer return type from body if not explicitly specified
                    ASTNode* body_type = node->as.function.body->evaluates_to_type;
                    if (body_type) {
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
                    } else {
                        // Means a type was expected, but didn't return anything
                        if (node->as.function.return_type) {
                            report_error(
                                node, "Function '%s' expected a return type '%s', but body returned void instead",
                                func_name, node_repr(node->as.function.return_type)
                            );
                        }
                    }
                    node->evaluates_to_type = get_function_signature(node);
                }
                break;
            };
            case NODE_CALL: {
                // Function call
                ASTNode* func = node->as.call.callee;
                char* func_name = node->as.call.debug_name;
                UNAM_DEBUG("call: %s\n", func_name);

                bool is_recursive = false;

                if (phase == PHASE_ENTER) {
                    // If we are calling an identifier, we only validate that the function exists in the symbol table
                    SymbolTableEntry* resolved_func;
                    if (func->type == NODE_IDENTIFIER) {
                        resolved_func = find_symbol(current_scope, func_name);
                        if (!resolved_func || !resolved_func->node) {
                            report_error(func, "Function is not defined: '%s'", func_name);
                            break;
                        }
                        if (resolved_func->node->type != NODE_FUNCTION && resolved_func->node->type != NODE_FUNC_PARAMETER && resolved_func->node->type != NODE_ENUM_VARIANT) {
                            report_error(func, "Trying to call a non function '%s'", node_repr(func->evaluates_to_type));
                            break;
                        }

                        ASTNode* parent_function = find_nearest_function(&contexts_stack);
                        if (parent_function) {
                            is_recursive = strcmp(func_name, parent_function->as.function.name) == 0;
                        }
                        if (is_recursive && !resolved_func->node->as.function.return_type) {
                            report_error(parent_function, "Recursive functions MUST specify their return types");
                            break;
                        }
                    }

                    // We are free of basic errors!
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    // We add our arguments so we evaluate them before coming back to this node
                    if (node->as.call.args) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.call.args, PHASE_ENTER });
                    }

                    // We first analyze the function so that `evaluates_to_type` gets populated
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ func, PHASE_ENTER });
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
                        if (calling_function == NULL || (calling_function->type != NODE_FUNCTION && calling_function->type != NODE_ENUM_VARIANT && calling_function->type != NODE_SIGNATURE_TYPE)) {
                            report_error(func, "Trying to call a non function '%s'", node_repr(func->evaluates_to_type));
                        }
                    }
                    da_astnodes specialized_params_types;  // generic types that are now specialized (NODE_PLAIN_TYPE)
                    da_astnodes_init(&specialized_params_types, 4);
                    da_astnodes specialized_params_values; // the actual types they have been specialized too (ANY)
                    da_astnodes_init(&specialized_params_values, 4);

                    ASTNode* signature = NULL;
                    if (calling_function->type == NODE_SIGNATURE_TYPE) {
                        signature = calling_function;
                    } else if (calling_function->type == NODE_FUNCTION) {
                        signature = calling_function->evaluates_to_type;
                        if (signature->type != NODE_SIGNATURE_TYPE) {
                             signature = get_function_signature(calling_function);
                             calling_function->evaluates_to_type = signature;
                        }
                    } else if (calling_function->type == NODE_ENUM_VARIANT) {
                        // A variant call evaluates to the enum type itself
                        node->evaluates_to_type = calling_function->evaluates_to_type;
                        signature = NULL; // Special case for variants
                    } else if (calling_function->evaluates_to_type && calling_function->evaluates_to_type->type == NODE_SIGNATURE_TYPE) {
                        signature = calling_function->evaluates_to_type;
                    }

                    // specialization darrays moved inside argument loop and return type specialization

                    // And now that we now a function is being called, we validate its arguments!
                    ASTNode* expected_param = NULL;
                    if (signature) {
                        expected_param = signature->as.signature.params;
                    } else if (calling_function->type == NODE_ENUM_VARIANT) {
                        expected_param = calling_function->as.enum_variant.payload_types;
                    }

                    ASTNode* passed_arg = node->as.call.args;

                    int passed_args_len = 0;
                    while (expected_param) {
                        if (passed_arg == NULL) {
                            report_error(node, "Expected argument, but none passed");
                            break;
                        }
                        // Check that types of arguments match
                        ASTNode* param_type = NULL;
                        if (expected_param->type == NODE_FUNC_PARAMETER) {
                            param_type = expected_param->as.func_param.type_expr;
                        } else {
                            // For enum variants, payload_types is just a list of types
                            param_type = expected_param;
                        }

                        passed_args_len++;
                        int param_node_type = param_type->type;
                        UNAM_ASSERT(param_node_type == NODE_PLAIN_TYPE || param_node_type == NODE_SIGNATURE_TYPE || param_node_type == NODE_STRUCT_DECL || param_node_type == NODE_ENUM_DECL, "function argument must be of correct type");

                        if (passed_arg->evaluates_to_type == NULL) {
                            report_error(passed_arg, "Passed argument #%d to function '%s' evalutes to void", passed_args_len, node_repr(node));
                            goto outer_loop;
                        }

                        if (passed_arg->evaluates_to_type->type != NODE_PLAIN_TYPE &&
                            passed_arg->evaluates_to_type->type != NODE_STRUCT_DECL &&
                            passed_arg->evaluates_to_type->type != NODE_ENUM_DECL &&
                            passed_arg->evaluates_to_type->type != NODE_SIGNATURE_TYPE) {
                            report_error(passed_arg, "Passed argument #%d to function '%s' evaluates to %s, which is not a type", passed_args_len, node_repr(node), node_repr(passed_arg->evaluates_to_type));
                            goto outer_loop;
                        }

                        if (passed_arg->evaluates_to_type->type == NODE_PLAIN_TYPE) {
                             UNAM_ASSERT(find_symbol(current_scope, passed_arg->evaluates_to_type->as.type.name) != NULL, "function must return a type at this point");
                        }
                        infer_specializations(param_type, passed_arg->evaluates_to_type, &specialized_params_types, &specialized_params_values);

                        ASTNode* dg = specialized_params_types.length > 0 ? specialized_params_types.data[0] : NULL;
                        ASTNode* lg = specialized_params_values.length > 0 ? specialized_params_values.data[0] : NULL;
                        if (!types_match(param_type, passed_arg->evaluates_to_type, dg, lg)) {
                             report_error(passed_arg, "Expected type '%s', but passed type '%s'", node_repr(param_type), node_repr(passed_arg->evaluates_to_type));
                        }
                        // Then it's okay
                        passed_arg = passed_arg->next;
                        expected_param = expected_param->next;
                    }
                    if (passed_arg != NULL) {
                        report_error(node, "Passing more arguments than expected (passed %d)", passed_args_len);
                    }

                    if (signature) {
                        ASTNode* dg = specialized_params_types.length > 0 ? specialized_params_types.data[0] : NULL;
                        ASTNode* lg = specialized_params_values.length > 0 ? specialized_params_values.data[0] : NULL;
                        node->evaluates_to_type = specialize_type(signature->as.signature.return_type, dg, lg);
                    }
                    da_free(&specialized_params_types);
                    da_free(&specialized_params_values);
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
                        node->evaluates_to_type = then_body_type;
                    }
                }
                break;
            };
            case NODE_FOR: {
                if (phase == PHASE_ENTER) {
                    da_astnodes_append(&contexts_stack, node);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    // Push a scope for the for loop (for the init variable)
                    push_scope(&current_scope);

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
                            report_warning(for_cond, "The condition of this for loop evaluates to void");
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
                        if (body_return_type && !types_are_equal(body_return_type, else_body_return_type)) {
                            report_error(
                                node, "Your for loop branches evaluate to different types: body is %s, else is %s",
                                node_repr(body_return_type), node_repr(else_body_return_type)
                            );
                        }
                        if (!node->evaluates_to_type) {
                            node->evaluates_to_type = else_body_return_type;
                        } else if (!types_are_equal(node->evaluates_to_type, else_body_return_type)) {
                            report_error(node, "Loop results are inconsistent: break returns %s but else returns %s", node_repr(node->evaluates_to_type), node_repr(else_body_return_type));
                        }
                    } else {
                        if (!node->evaluates_to_type) {
                            node->evaluates_to_type = body_return_type;
                        } else if (body_return_type && !types_are_equal(node->evaluates_to_type, body_return_type)) {
                            report_error(node, "Loop results are inconsistent: break returns %s but body returns %s", node_repr(node->evaluates_to_type), node_repr(body_return_type));
                        }
                    }
                    pop_scope(&current_scope);
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
                        if (body_return_type && !types_are_equal(body_return_type, else_body_return_type)) {
                            report_error(
                                node, "Your foreach branches evaluate to different types: body is %s, else is %s",
                                node_repr(body_return_type), node_repr(else_body_return_type)
                            );
                        }
                        if (!node->evaluates_to_type) {
                            node->evaluates_to_type = else_body_return_type;
                        } else if (!types_are_equal(node->evaluates_to_type, else_body_return_type)) {
                            report_error(node, "Foreach results are inconsistent: break returns %s but else returns %s", node_repr(node->evaluates_to_type), node_repr(else_body_return_type));
                        }
                    } else {
                        if (!node->evaluates_to_type) {
                            node->evaluates_to_type = body_return_type;
                        } else if (body_return_type && !types_are_equal(node->evaluates_to_type, body_return_type)) {
                            report_error(node, "Foreach results are inconsistent: break returns %s but body returns %s", node_repr(node->evaluates_to_type), node_repr(body_return_type));
                        }
                    }
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
                            report_error(loop_else_body, "If you define an else in your loop, then it should evaluate to something");
                            break;
                        }
                        if (body_return_type && !types_are_equal(body_return_type, else_body_return_type)) {
                            report_error(
                                node, "Your loop branches evaluate to different types: body is %s, else is %s",
                                node_repr(body_return_type), node_repr(else_body_return_type)
                            );
                        }
                        if (!node->evaluates_to_type) {
                            node->evaluates_to_type = else_body_return_type;
                        } else if (!types_are_equal(node->evaluates_to_type, else_body_return_type)) {
                            report_error(node, "Loop results are inconsistent: break returns %s but else returns %s", node_repr(node->evaluates_to_type), node_repr(else_body_return_type));
                        }
                    } else {
                        if (!node->evaluates_to_type) {
                            node->evaluates_to_type = body_return_type;
                        } else if (body_return_type && !types_are_equal(node->evaluates_to_type, body_return_type)) {
                            report_error(node, "Loop results are inconsistent: break returns %s but body returns %s", node_repr(node->evaluates_to_type), node_repr(body_return_type));
                        }
                    }
                }
                break;
            };
            case NODE_MATCH: {
                if (phase == PHASE_ENTER) {
                    da_astnodes_append(&contexts_stack, node);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_MID });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.match_expr.subject, PHASE_ENTER });
                } else if (phase == PHASE_MID) {
                    ASTNode* arm = node->as.match_expr.arms;
                    // Push arms in reverse order to analyze them in original order
                    da_astnodes arm_list;
                    da_astnodes_init(&arm_list, 0);
                    while (arm) {
                        da_astnodes_append(&arm_list, arm);
                        arm = arm->next;
                    }
                    for (int i = arm_list.length - 1; i >= 0; i--) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ arm_list.data[i], PHASE_ENTER });
                    }
                    da_free(&arm_list);
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);
                    ASTNode* arm = node->as.match_expr.arms;
                    ASTNode* first_arm_type = NULL;
                    while (arm) {
                        if (arm->evaluates_to_type) {
                            if (!first_arm_type) first_arm_type = arm->evaluates_to_type;
                            else if (!types_are_equal(first_arm_type, arm->evaluates_to_type)) {
                                report_error(arm, "Match arm evaluates to %s, but previous arms evaluate to %s", node_repr(arm->evaluates_to_type), node_repr(first_arm_type));
                            }
                        }
                        arm = arm->next;
                    }
                    node->evaluates_to_type = first_arm_type;
                }
                break;
            };
            case NODE_MATCH_ARM: {
                if (phase == PHASE_ENTER) {
                    ASTNode* match_expr = find_nearest_match(&contexts_stack);
                    UNAM_ASSERT(match_expr, "Match arm must be within a match expression");
                    ASTNode* subject_type = match_expr->as.match_expr.subject->evaluates_to_type;

                    push_scope(&current_scope);
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.match_arm.body, PHASE_ENTER });
                    if (node->as.match_arm.guard) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.match_arm.guard, PHASE_ENTER });
                    }
                    analyze_pattern(node->as.match_arm.pattern, subject_type, current_scope);
                } else if (phase == PHASE_EXIT) {
                    node->evaluates_to_type = node->as.match_arm.body->evaluates_to_type;
                    pop_scope(&current_scope);
                }
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
                        ASTNode* returning_value = node->as.return_stmt.value;

                        if (returning_value) {
                            ASTNode* returning_type = returning_value->evaluates_to_type;

                            if (actual_return_type) {
                                // Check against it
                                if (!types_are_equal(actual_return_type, returning_type)) {
                                    report_error(
                                        returning_value, "Type of return '%s' and function signature '%s' don't match",
                                        node_repr(returning_type), node_repr(actual_return_type)
                                    );
                                }
                            } else {
                                // Function has no return type, so try to infer it by mutating the function node
                                // So this return will actually set its type
                            }
                            bound_function->as.function.body->evaluates_to_type = returning_type;
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
                        node->evaluates_to_type = break_arg_type;
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
                        report_error(node->as.binop.left, "Left side of the '%s' operation is void", op);
                        break;
                    }
                    if (!type_right) {
                        report_error(node->as.binop.right, "Right side of the '%s' operation is void", op);
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
                        bool is_float_sum = (left_is_float && right_is_float) || (left_is_int && right_is_float) || (left_is_float && right_is_int);

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
                        bool is_float_diff = (left_is_float && right_is_float) || (left_is_int && right_is_float) || (left_is_float && right_is_int);

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
            case NODE_PLAIN_TYPE: {
                UNAM_DEBUG("  type name=%s", node_repr(node));
                 SymbolTableEntry* sym = find_symbol(current_scope, node->as.type.name);
                 if (!sym) {
                     report_error(node, "Referenced type '%s' is not defined", node_repr(node));
                 }

                // Analyze generic arguments if provided
                if (node->as.type.generic_args) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.type.generic_args, PHASE_ENTER });
                }
                UNAM_DEBUG_PLAIN("\n");
                break;
            };
            case NODE_SIGNATURE_TYPE: {
                if (phase == PHASE_ENTER) {
                    node->evaluates_to_type = node;
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    if (node->as.signature.params) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.signature.params, PHASE_ENTER });
                    }
                    if (node->as.signature.return_type) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.signature.return_type, PHASE_ENTER });
                    }
                }
                break;
            };
            case NODE_IDENTIFIER: {
                SymbolTableEntry* identifier = find_symbol(current_scope, node->as.ident.name);
                if (!identifier) {
                    report_error(node, "Identifier '%s' was not declared in this scope", node_repr(node));
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
                const char* enum_name = node->as.enum_decl.name;

                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("enum name=%s\n", enum_name);
                    da_astnodes_append(&contexts_stack, node);

                    if (!add_symbol_unshadowed(current_scope, enum_name, node)) {
                        report_error(node, "The type name for enum '%s' already exists", enum_name);
                    }

                    push_scope(&current_scope);

                    // Register generic type parameters in the new scope
                    ASTNode* enum_generic_arg = node->as.enum_decl.generic_args;
                    while (enum_generic_arg) {
                        enum_generic_arg->type = NODE_PLAIN_TYPE;
                        add_symbol_unshadowed(current_scope, enum_generic_arg->as.type.name, enum_generic_arg);
                        enum_generic_arg = enum_generic_arg->next;
                    }
                    if (node->as.enum_decl.generic_args) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.enum_decl.generic_args, PHASE_ENTER });
                    }

                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    // Schedule variants for analysis after sealing the type in PHASE_MID
                    if (node->as.enum_decl.variants) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.enum_decl.variants, PHASE_ENTER });
                    }

                    // PHASE_MID will seal the type so variants can reference it
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_MID });
                } else if (phase == PHASE_MID) {
                    node->evaluates_to_type = node;
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);
                    pop_scope(&current_scope);
                }
                break;
            };
            case NODE_ENUM_VARIANT: {
                const char* variant_name = node->as.enum_variant.name;
                UNAM_DEBUG("  variant: %s\n", variant_name);

                // We add the variant to the current scope (which should be the enum's scope)
                if (!add_symbol_unshadowed(current_scope, variant_name, node)) {
                    report_error(node, "Variant '%s' is already defined in this enum", variant_name);
                }

                // A variant always evaluates to its parent enum type
                size_t i = contexts_stack.length;
                ASTNode* enum_node = NULL;
                while (i-- > 0) {
                    if (contexts_stack.data[i]->type == NODE_ENUM_DECL) {
                        enum_node = contexts_stack.data[i];
                        break;
                    }
                }
                node->evaluates_to_type = enum_node;
                break;
            };
            case NODE_STRUCT_DECL: {
                const char* struct_name = node->as.struct_decl.name;

                if (phase == PHASE_ENTER) {
                    node->evaluates_to_type = node;
                    UNAM_DEBUG("struct name=%s\n", struct_name);
                    da_astnodes_append(&contexts_stack, node);

                    if (!add_symbol_unshadowed(current_scope, struct_name, node)) {
                        report_error(node, "The type name for struct '%s' already exists", struct_name);
                    }

                    push_scope(&current_scope);

                    // Register generic type parameters in the new scope
                    ASTNode* generic_arg = node->as.struct_decl.generic_args;
                    while (generic_arg) {
                        generic_arg->type = NODE_PLAIN_TYPE;
                        add_symbol_unshadowed(current_scope, generic_arg->as.type.name, generic_arg);
                        generic_arg = generic_arg->next;
                    }
                    if (node->as.struct_decl.generic_args) {
                        // Mark them as evaluated to themselves
                        ASTNode* g = node->as.struct_decl.generic_args;
                        while (g) {
                            g->evaluates_to_type = g;
                            g = g->next;
                        }
                    }

                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    if (node->as.struct_decl.fields) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.struct_decl.fields, PHASE_ENTER });
                    }
                } else if (phase == PHASE_EXIT) {
                    da_astnodes_pop(&contexts_stack, NULL);
                    // Validate: no duplicate field names
                    ASTNode* start_struct_field = node->as.struct_decl.fields;
                    ASTNode* struct_field = node->as.struct_decl.fields;
                    int i = 0;

                    while (struct_field) {
                        int j = 0;
                        ASTNode* sf = start_struct_field;
                        while (sf) {
                            if (strcmp(sf->as.struct_field.name, struct_field->as.struct_field.name) == 0 && i != j) {
                                report_error(struct_field, "Duplicated field name '%s' for struct", struct_field->as.struct_field.name);
                                break;
                            }
                            j++;
                            sf = sf->next;
                        }

                        // Verify that the field's type exists
                        ASTNode* field_type = struct_field->as.struct_field.value;
                        if (field_type && field_type->as.type.name) {
                            if (!find_symbol(current_scope, field_type->as.type.name)) {
                                report_error(struct_field, "Type '%s' for struct field '%s' is not defined", node_repr(field_type), node_repr(struct_field));
                            }
                        }

                        i++;
                        struct_field = struct_field->next;
                    }

                    pop_scope(&current_scope);
                }
                break;
            };
            case NODE_STRUCT_LITERAL: {
                const char* struct_name = node->as.struct_lit.name;
                SymbolTableEntry* struct_sym = find_symbol(current_scope, struct_name);

                if (!struct_sym || struct_sym->node->type != NODE_STRUCT_DECL) {
                    report_error(node, "Type '%s' is not a struct to instantiate", struct_name);
                    break;
                }

                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    // Analyze generic arguments if provided
                    if (node->as.struct_lit.generic_args) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.struct_lit.generic_args, PHASE_ENTER });
                    }

                    // Analyze provided fields
                    if (node->as.struct_lit.fields) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.struct_lit.fields, PHASE_ENTER });
                    }
                } else if (phase == PHASE_EXIT) {
                    ASTNode* struct_decl = struct_sym->node;
                    node->evaluates_to_type = struct_decl;

                    // 1. Validate that all required fields from the declaration are present
                    ASTNode* struct_field = struct_decl->as.struct_decl.fields;
                    while (struct_field) {
                        const char* field_name = struct_field->as.struct_field.name;
                        ASTNode* lit_field = node->as.struct_lit.fields;
                        bool found = false;
                        while (lit_field) {
                            if (strcmp(lit_field->as.struct_field.name, field_name) == 0) {
                                found = true;
                                break;
                            }
                            lit_field = lit_field->next;
                        }

                        if (!found) {
                            report_error(node, "Missing required field '%s' for struct '%s'", field_name, struct_name);
                        }
                        struct_field = struct_field->next;
                    }

                    // 2. Validate that each provided field matches the declaration's type
                    ASTNode* lit_field = node->as.struct_lit.fields;
                    while (lit_field) {
                        const char* field_name = lit_field->as.struct_field.name;
                        ASTNode* struct_field = struct_decl->as.struct_decl.fields;
                        ASTNode* target_struct_field = NULL;

                        while (struct_field) {
                            if (strcmp(struct_field->as.struct_field.name, field_name) == 0) {
                                target_struct_field = struct_field;
                                break;
                            }
                            struct_field = struct_field->next;
                        }

                        if (target_struct_field) {
                            ASTNode* expected_type = target_struct_field->as.struct_field.value;
                            ASTNode* actual_type = lit_field->as.struct_field.value->evaluates_to_type;

                            if (!types_match(expected_type, actual_type, struct_decl->as.struct_decl.generic_args, node->as.struct_lit.generic_args)) {
                                report_error(lit_field, "Type mismatch for field '%s'. Expected '%s', but found '%s'",
                                    field_name, node_repr(expected_type), node_repr(actual_type));
                            }
                        } else {
                            report_error(lit_field, "Field '%s' does not exist in struct '%s'", field_name, struct_name);
                        }
                        lit_field = lit_field->next;
                    }
                }
                break;
            };
            case NODE_STRUCT_FIELD: {
                da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.struct_field.value, PHASE_ENTER });
                break;
            };
            case NODE_LIST_LITERAL: {
                ASTNode* element = node->as.list_lit.items;

                if (phase == PHASE_ENTER) {
                    UNAM_DEBUG("  list: \n");
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });

                    // Schedule items for analysis
                    if (node->as.list_lit.items) {
                        da_analyze_frames_append(&stack, (AnalyzeFrame){ node->as.list_lit.items, PHASE_ENTER });
                    }
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

                    SymbolTableEntry* list_symbol = find_symbol(current_scope, "List");
                    UNAM_ASSERT(list_symbol != NULL, "List type is not defined");

                    // Create a NEW type node to avoid mutating the shared builtin
                    ASTNode* list_type = create_node(NODE_PLAIN_TYPE);
                    list_type->as.type.name = ast_strdup("List");
                    list_type->as.type.generic_args = last_element_type;
                    node->evaluates_to_type = list_type;
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
                ASTNode* object = node->as.member.object;
                ASTNode* member = node->as.member.member;
                const char* op = node->as.member.op;

                if (phase == PHASE_ENTER) {
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
                    da_analyze_frames_append(&stack, (AnalyzeFrame){ object, PHASE_ENTER });
                } else if (phase == PHASE_EXIT) {
                    SymbolTableEntry* found_type_symbol = find_symbol(current_scope, object->evaluates_to_type->as.type.name);
                    UNAM_ASSERT(found_type_symbol, "The object type should exist in the symbols table");
                    ASTNode* object_type = found_type_symbol->node;

                    if (!object_type) {
                        report_error(object, "Cannot access member of void type");
                        break;
                    }

                    const char* member_name = member->as.ident.name;

                    if (strcmp(op, "::") == 0) {
                        // Scoped access (::) is only for Enums
                        if (object_type->type != NODE_ENUM_DECL) {
                            report_error(object, "Operator :: can only be used with Enums, but got %s", node_repr(object_type));
                            break;
                        }

                        // Search in variants
                        ASTNode* variant = object_type->as.enum_decl.variants;
                        ASTNode* found = NULL;
                        while (variant) {
                            if (strcmp(variant->as.enum_variant.name, member_name) == 0) {
                                found = variant;
                                break;
                            }
                            variant = variant->next;
                        }

                        if (!found) {
                            report_error(member, "Variant '%s' not found in enum %s", member_name, object_type->as.enum_decl.name);
                            break;
                        }

                        // We evaluate to the variant node itself if it's "callable" (has payloads)
                        // otherwise we evaluate to the enum type (object_type)
                        if (found->as.enum_variant.payload_types) {
                            node->evaluates_to_type = found;
                        } else {
                            node->evaluates_to_type = object_type;
                        }
                    } else if (strcmp(op, ".") == 0) {
                        // Instance access (.) is only for Struct instances
                        if (object_type->type != NODE_STRUCT_DECL) {
                            report_error(object, "Operator . can only be used with Struct instances, but got %s", node_repr(object_type));
                            break;
                        }

                        // Search in fields
                        ASTNode* field = object_type->as.struct_decl.fields;
                        ASTNode* found = NULL;
                        while (field) {
                            if (strcmp(field->as.struct_field.name, member_name) == 0) {
                                found = field;
                                break;
                            }
                            field = field->next;
                        }

                        if (!found) {
                            report_error(member, "Field '%s' not found in struct %s", member_name, object_type->as.struct_decl.name);
                            break;
                        }
                        // The access evaluates to the field's type
                        UNAM_ASSERT(found->type == NODE_STRUCT_FIELD, "matched field should be an struct field");
                        node->evaluates_to_type = found->as.struct_field.value;
                    } else {
                        UNAM_ASSERT(false, "unreachable");
                    }
                }
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
    da_diagnostics_init(&diagnostics, 16);
    Scope* current_scope = NULL;
    push_scope(&current_scope);

    semantic_analyze(current_scope, root);
    pop_scope(&current_scope);

    int error_count = 0;
    int warning_count = 0;
    for (size_t i = 0; i < diagnostics.length; i++) {
        if (diagnostics.data[i].severity == SEVERITY_ERROR) error_count++;
        else warning_count++;
    }

    if (warning_count > 0) {
        fprintf(stderr, UNAM_YELLOW "\nWarnings:\n" UNAM_RESET);
        for (size_t i = 0; i < diagnostics.length; i++) {
            if (diagnostics.data[i].severity == SEVERITY_WARNING) {
                fprintf(stderr, "  [%d:%d] %s\n",
                    diagnostics.data[i].loc.line, diagnostics.data[i].loc.col, diagnostics.data[i].message);
            }
        }
    }

    if (error_count > 0) {
        fprintf(stderr, UNAM_RED "\nErrors:\n" UNAM_RESET);
        for (size_t i = 0; i < diagnostics.length; i++) {
            if (diagnostics.data[i].severity == SEVERITY_ERROR) {
                fprintf(stderr, "  [%d:%d] %s\n",
                    diagnostics.data[i].loc.line, diagnostics.data[i].loc.col, diagnostics.data[i].message);
            }
        }
    }

    for (size_t i = 0; i < diagnostics.length; i++) {
        free(diagnostics.data[i].message);
    }
    da_free(&diagnostics);

    return error_count == 0;
}
