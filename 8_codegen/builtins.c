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
    what_type->as.type.is_generic = true;
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

static SymbolTableEntry* cached_pixel = NULL;

SymbolTableEntry* get_pixel_symbol() {
    if (cached_pixel) return cached_pixel;
    ASTNode* pixel_func = calloc(1, sizeof *pixel_func);
    pixel_func->type = NODE_FUNCTION;
    pixel_func->as.function.name = ast_strdup("pixel");
    pixel_func->as.function.is_lambda = false;

    // Function parameter x: int
    ASTNode* x_param = calloc(1, sizeof *x_param);
    x_param->type = NODE_FUNC_PARAMETER;
    x_param->as.func_param.name = ast_strdup("x");
    ASTNode* int_type1 = calloc(1, sizeof *int_type1);
    int_type1->type = NODE_PLAIN_TYPE;
    int_type1->as.type.name = ast_strdup("int");
    x_param->as.func_param.type_expr = int_type1;

    // Function parameter y: int
    ASTNode* y_param = calloc(1, sizeof *y_param);
    y_param->type = NODE_FUNC_PARAMETER;
    y_param->as.func_param.name = ast_strdup("y");
    ASTNode* int_type2 = calloc(1, sizeof *int_type2);
    int_type2->type = NODE_PLAIN_TYPE;
    int_type2->as.type.name = ast_strdup("int");
    y_param->as.func_param.type_expr = int_type2;
    x_param->next = y_param;

    // Function parameter color: int (or bool)
    ASTNode* color_param = calloc(1, sizeof *color_param);
    color_param->type = NODE_FUNC_PARAMETER;
    color_param->as.func_param.name = ast_strdup("color");
    ASTNode* int_type3 = calloc(1, sizeof *int_type3);
    int_type3->type = NODE_PLAIN_TYPE;
    int_type3->as.type.name = ast_strdup("int"); // using int for colors to be safer (could be 0 or 1 for now)
    color_param->as.func_param.type_expr = int_type3;
    y_param->next = color_param;

    pixel_func->as.function.params = x_param;

    // Signature
    ASTNode* sig = calloc(1, sizeof *sig);
    sig->type = NODE_SIGNATURE_TYPE;
    sig->as.signature.params = x_param;
    sig->as.signature.return_type = NULL; // returns void
    pixel_func->evaluates_to_type = sig;

    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("pixel");
    sym->node = pixel_func;
    cached_pixel = sym;
    return sym;
}

static SymbolTableEntry* cached_input = NULL;

SymbolTableEntry* get_input_symbol() {
    if (cached_input) return cached_input;
    ASTNode* func = calloc(1, sizeof *func);
    func->type = NODE_FUNCTION;
    func->as.function.name = ast_strdup("input");
    func->as.function.is_lambda = false;
    func->as.function.params = NULL;
    ASTNode* sig = calloc(1, sizeof *sig);
    sig->type = NODE_SIGNATURE_TYPE;
    sig->as.signature.params = NULL;
    ASTNode* ret_type = calloc(1, sizeof *ret_type);
    ret_type->type = NODE_PLAIN_TYPE;
    ret_type->as.type.name = ast_strdup("int");
    sig->as.signature.return_type = ret_type;
    func->evaluates_to_type = sig;
    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("input");
    sym->node = func;
    cached_input = sym;
    return sym;
}

static SymbolTableEntry* cached_arr = NULL;
static SymbolTableEntry* cached_arr_get = NULL;
static SymbolTableEntry* cached_arr_set = NULL;

SymbolTableEntry* get_arr_symbol() {
    if (cached_arr) return cached_arr;
    ASTNode* func = calloc(1, sizeof *func);
    func->type = NODE_FUNCTION;
    func->as.function.name = ast_strdup("arr");
    func->as.function.is_lambda = false;
    ASTNode* x_param = calloc(1, sizeof *x_param);
    x_param->type = NODE_FUNC_PARAMETER;
    x_param->as.func_param.name = ast_strdup("size");
    ASTNode* int_type = calloc(1, sizeof *int_type);
    int_type->type = NODE_PLAIN_TYPE;
    int_type->as.type.name = ast_strdup("int");
    x_param->as.func_param.type_expr = int_type;
    func->as.function.params = x_param;
    ASTNode* sig = calloc(1, sizeof *sig);
    sig->type = NODE_SIGNATURE_TYPE;
    sig->as.signature.params = x_param;
    ASTNode* ret_type = calloc(1, sizeof *ret_type);
    ret_type->type = NODE_PLAIN_TYPE;
    ret_type->as.type.name = ast_strdup("arr"); // Arrays represent as arr type
    sig->as.signature.return_type = ret_type;
    func->evaluates_to_type = sig;
    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("arr");
    sym->node = func;
    cached_arr = sym;
    return sym;
}

SymbolTableEntry* get_arr_get_symbol() {
    if (cached_arr_get) return cached_arr_get;
    ASTNode* func = calloc(1, sizeof *func);
    func->type = NODE_FUNCTION;
    func->as.function.name = ast_strdup("arr_get");
    func->as.function.is_lambda = false;
    ASTNode* arr_param = calloc(1, sizeof *arr_param);
    arr_param->type = NODE_FUNC_PARAMETER;
    arr_param->as.func_param.name = ast_strdup("arr");
    ASTNode* t1 = calloc(1, sizeof *t1); t1->type = NODE_PLAIN_TYPE; t1->as.type.name = ast_strdup("arr");
    arr_param->as.func_param.type_expr = t1;
    ASTNode* idx_param = calloc(1, sizeof *idx_param);
    idx_param->type = NODE_FUNC_PARAMETER;
    idx_param->as.func_param.name = ast_strdup("idx");
    ASTNode* t2 = calloc(1, sizeof *t2); t2->type = NODE_PLAIN_TYPE; t2->as.type.name = ast_strdup("int");
    idx_param->as.func_param.type_expr = t2;
    arr_param->next = idx_param;
    func->as.function.params = arr_param;
    ASTNode* sig = calloc(1, sizeof *sig); sig->type = NODE_SIGNATURE_TYPE;
    sig->as.signature.params = arr_param;
    ASTNode* r = calloc(1, sizeof *r); r->type = NODE_PLAIN_TYPE; r->as.type.name = ast_strdup("int");
    sig->as.signature.return_type = r;
    func->evaluates_to_type = sig;
    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("arr_get");
    sym->node = func;
    cached_arr_get = sym;
    return sym;
}

SymbolTableEntry* get_arr_set_symbol() {
    if (cached_arr_set) return cached_arr_set;
    ASTNode* func = calloc(1, sizeof *func);
    func->type = NODE_FUNCTION;
    func->as.function.name = ast_strdup("arr_set");
    func->as.function.is_lambda = false;
    ASTNode* arr_param = calloc(1, sizeof *arr_param);
    arr_param->type = NODE_FUNC_PARAMETER; arr_param->as.func_param.name = ast_strdup("arr");
    ASTNode* t1 = calloc(1, sizeof *t1); t1->type = NODE_PLAIN_TYPE; t1->as.type.name = ast_strdup("arr");
    arr_param->as.func_param.type_expr = t1;
    ASTNode* idx_param = calloc(1, sizeof *idx_param);
    idx_param->type = NODE_FUNC_PARAMETER; idx_param->as.func_param.name = ast_strdup("idx");
    ASTNode* t2 = calloc(1, sizeof *t2); t2->type = NODE_PLAIN_TYPE; t2->as.type.name = ast_strdup("int");
    idx_param->as.func_param.type_expr = t2;
    ASTNode* val_param = calloc(1, sizeof *val_param);
    val_param->type = NODE_FUNC_PARAMETER; val_param->as.func_param.name = ast_strdup("val");
    ASTNode* t3 = calloc(1, sizeof *t3); t3->type = NODE_PLAIN_TYPE; t3->as.type.name = ast_strdup("int");
    val_param->as.func_param.type_expr = t3;
    arr_param->next = idx_param; idx_param->next = val_param;
    func->as.function.params = arr_param;
    ASTNode* sig = calloc(1, sizeof *sig); sig->type = NODE_SIGNATURE_TYPE;
    sig->as.signature.params = arr_param;
    sig->as.signature.return_type = NULL;
    func->evaluates_to_type = sig;
    SymbolTableEntry* sym = calloc(1, sizeof *sym);
    sym->name = ast_strdup("arr_set");
    sym->node = func;
    cached_arr_set = sym;
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
    if (cached_pixel) free(cached_pixel);
    if (cached_input) free(cached_input);
    if (cached_arr) free(cached_arr);
    if (cached_arr_get) free(cached_arr_get);
    if (cached_arr_set) free(cached_arr_set);
}
