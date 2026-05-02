#pragma once

#include "analyzer.h"

ASTNode int_astnode = {
    .type = NODE_TYPE_IDENTIFIER,
    .lexeme = "int",
};

SymbolTableEntry int_symbol = {
    .name = "int",
    .type_node = &int_astnode,
    .next = NULL,
};

ASTNode float_astnode = {
    .type = NODE_TYPE_IDENTIFIER,
    .lexeme = "float",
};

SymbolTableEntry float_symbol = {
    .name = "float",
    .type_node = &float_astnode,
    .next = NULL,
};

ASTNode bool_astnode = {
    .type = NODE_TYPE_IDENTIFIER,
    .lexeme = "bool",
};

SymbolTableEntry bool_symbol = {
    .name = "bool",
    .type_node = &bool_astnode,
    .next = NULL,
};

ASTNode string_astnode = {
    .type = NODE_TYPE_IDENTIFIER,
    .lexeme = "string",
};

SymbolTableEntry string_symbol = {
    .name = "string",
    .type_node = &string_astnode,
    .next = NULL,
};

ASTNode list_astnode = {
    .type = NODE_TYPE_IDENTIFIER,
    .lexeme = "List",
};

SymbolTableEntry list_symbol = {
    .name = "List",
    .type_node = &list_astnode,
    .next = NULL,
};
