#pragma once

#include "analyzer.h"

SymbolTableEntry* get_int_symbol();
SymbolTableEntry* get_float_symbol();
SymbolTableEntry* get_bool_symbol();
SymbolTableEntry* get_string_symbol();
SymbolTableEntry* get_list_symbol();
SymbolTableEntry* get_pixel_symbol();
SymbolTableEntry* get_arr_symbol();
SymbolTableEntry* get_arr_get_symbol();
SymbolTableEntry* get_arr_set_symbol();

void cleanup_builtins();
