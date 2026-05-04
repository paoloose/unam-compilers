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

SymbolTableEntry* get_int_symbol()    { return make_builtin("int", NODE_CONCRETE_TYPE); }
SymbolTableEntry* get_float_symbol()  { return make_builtin("float", NODE_CONCRETE_TYPE); }
SymbolTableEntry* get_bool_symbol()   { return make_builtin("bool", NODE_CONCRETE_TYPE); }
SymbolTableEntry* get_string_symbol() { return make_builtin("string", NODE_CONCRETE_TYPE); }
SymbolTableEntry* get_list_symbol()   { return make_builtin("List", NODE_CONCRETE_TYPE); }
