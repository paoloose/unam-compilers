#pragma once

#include "analyzer.h"

ASTNode int_astnode = {
    .lexeme = "int",
};

SymbolTableEntry int_symbol = {
    .name = "int",
    .type_node = &int_astnode,
    .next = NULL,
};

ASTNode float_astnode = {
    .lexeme = "float",
};

SymbolTableEntry float_symbol = {
    .name = "float",
    .type_node = &float_astnode,
    .next = NULL,
};

ASTNode bool_astnode = {
    .lexeme = "bool",
};

SymbolTableEntry bool_symbol = {
    .name = "bool",
    .type_node = &bool_astnode,
    .next = NULL,
};

ASTNode string_astnode = {
    .lexeme = "string",
};

SymbolTableEntry string_symbol = {
    .name = "string",
    .type_node = &string_astnode,
    .next = NULL,
};
