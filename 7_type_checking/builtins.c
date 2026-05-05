#include "analyzer.h"

static SymbolTableEntry* cached_int = NULL;
static SymbolTableEntry* cached_float = NULL;
static SymbolTableEntry* cached_bool = NULL;
static SymbolTableEntry* cached_string = NULL;
static SymbolTableEntry* cached_list = NULL;
static SymbolTableEntry* cached_print = NULL;

SymbolTableEntry* make_builtin(const char* name, NodeType type) {
    ASTNode* node = calloc(1, sizeof *node);
    if (!node) return NULL;

    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    if (!sym) {
        free(node);
        return NULL;
    }

    node->type = type;
    node->as.type.name = name;

    sym->name = name;
    sym->node = node;

    return sym;
}

SymbolTableEntry* get_int_symbol() {
    if (!cached_int) cached_int = make_builtin("int", NODE_PLAIN_TYPE);
    return cached_int;
}
SymbolTableEntry* get_float_symbol() {
    if (!cached_float) cached_float = make_builtin("float", NODE_PLAIN_TYPE);
    return cached_float;
}
SymbolTableEntry* get_bool_symbol() {
    if (!cached_bool) cached_bool = make_builtin("bool", NODE_PLAIN_TYPE);
    return cached_bool;
}
SymbolTableEntry* get_string_symbol() {
    if (!cached_string) cached_string = make_builtin("string", NODE_PLAIN_TYPE);
    return cached_string;
}

SymbolTableEntry* get_list_symbol() {
    if (cached_list) return cached_list;
    // NOTE: this is equivalent to all files having declared List<T> {}
    ASTNode* list_decl = calloc(1, sizeof *list_decl);
    list_decl->type = NODE_STRUCT_DECL;
    list_decl->as.struct_decl.name = ast_strdup("List");

    ASTNode* t_param = calloc(1, sizeof *t_param);
    t_param->type = NODE_PLAIN_TYPE;
    t_param->as.type.name = ast_strdup("T");

    list_decl->as.struct_decl.generic_args = t_param;

    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("List");
    sym->node = list_decl;
    cached_list = sym;
    return sym;
}

SymbolTableEntry* get_print_symbol() {
    if (cached_print) return cached_print;
    ASTNode* print_func = calloc(1, sizeof *print_func);
    print_func->type = NODE_FUNCTION;
    print_func->as.function.name = ast_strdup("print");
    print_func->as.function.is_lambda = false;

    // Generic parameter T
    ASTNode* t_param = calloc(1, sizeof *t_param);
    t_param->type = NODE_PLAIN_TYPE;
    t_param->as.type.name = ast_strdup("T");
    print_func->as.function.generic_args = t_param;

    // Function parameter what: T
    ASTNode* what_param = calloc(1, sizeof * what_param);
    what_param->type = NODE_FUNC_PARAMETER;
    what_param->as.func_param.name = ast_strdup("what");
    ASTNode* what_type = calloc(1, sizeof * what_type);
    what_type->type = NODE_PLAIN_TYPE;
     what_type->as.type.name = ast_strdup("T");
    what_param->as.func_param.type_expr = what_type;
    print_func->as.function.params = what_param;

    // Signature
    ASTNode* sig = calloc(1, sizeof *sig);
    sig->type = NODE_SIGNATURE_TYPE;
    sig->as.signature.params = what_param;
    sig->as.signature.return_type = NULL; // returns void
    print_func->evaluates_to_type = sig;

    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("print");
    sym->node = print_func;
    cached_print = sym;
    return sym;
}

static void free_ast_recursive(ASTNode* node) {
    // Currently not implemented
    if (!node) return;
}

void cleanup_builtins() {
    // We'll leave the actual node freeing to a more global AST cleanup if possible
    // or just free the symbols themselves for now.
    if (cached_int) { free(cached_int->node); free(cached_int); }
    if (cached_float) { free(cached_float->node); free(cached_float); }
    if (cached_bool) { free(cached_bool->node); free(cached_bool); }
    if (cached_string) { free(cached_string->node); free(cached_string); }
    // List and print have more complex structures, skipping deep free for now to avoid complexity
    // but the caching already solves the massive leak problem.
    if (cached_list) free(cached_list);
    if (cached_print) free(cached_print);
}
