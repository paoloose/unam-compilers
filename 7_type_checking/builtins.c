#include "analyzer.h"

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

SymbolTableEntry* get_int_symbol()    { return make_builtin("int", NODE_PLAIN_TYPE); }
SymbolTableEntry* get_float_symbol()  { return make_builtin("float", NODE_PLAIN_TYPE); }
SymbolTableEntry* get_bool_symbol()   { return make_builtin("bool", NODE_PLAIN_TYPE); }
SymbolTableEntry* get_string_symbol() { return make_builtin("string", NODE_PLAIN_TYPE); }

SymbolTableEntry* get_list_symbol()   {
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
    return sym;
}
SymbolTableEntry* get_print_symbol() {
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
    ASTNode* what_param = calloc(1, sizeof *what_param);
    what_param->type = NODE_FUNC_PARAMETER;
    what_param->as.func_param.name = ast_strdup("what");
    ASTNode* what_type = calloc(1, sizeof *what_type);
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
    return sym;
}
