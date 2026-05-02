#pragma once

#include "analyzer.h"

ASTNode int_astnode = {
    .lexeme = "int",
};

SymbolTableEntry int_symbol = {
    .name = "int",
    .next = NULL,
    .type_node = &int_astnode,
};
