#pragma once

#include "ast.h"

typedef enum EnnuyeuxType {
    TYPE_BUILTIN_SIMPLE,
    TYPE_BUILTIN_LIST,
    TYPE_STRUCT,
    TYPE_FUNCTION,
    TYPE_GENERIC,
} EnnuyeuxType;

typedef enum SymbolKind {
    SYMBOL_KIND_VARIABLE,
    SYMBOL_KIND_CONSTANT,
    SYMBOL_KIND_TYPENAME,
    SYMBOL_KIND_FUNCTION,
    SYMBOL_KIND_PARAMETER,
    SYMBOL_KIND_TYPE_ARGUMENT,
} SymbolKind;

typedef struct StructField {
    char* field_name;
    char* type_name;
    struct StructField* next;
} StructField;

// typedef struct SymbolTypeDataArray {
//     SymbolTypeData data;
//     SymbolTypeData* next;
// } SymbolTypeDataArray;

// // Flexible data structure that may be:
// //   TYPE_BUILTIN_SIMPLE ->
// //   TYPE_BUILTIN_LIST   ->
// //   TYPE_STRUCT         ->
// //   TYPE_FUNCTION       ->
// //   TYPE_GENERIC        ->
// typedef struct SymbolTypeData {
//     char* name;
//     EnnuyeuxType type;

//     SymbolTypeDataArray* generic_args;
//     SymbolTypeDataArray* args;

//     StructField* fields;   // struct members
// } SymbolTypeData;

typedef enum {
    PHASE_ENTER,
    PHASE_MID,
    PHASE_EXIT
} VisitPhase;

typedef struct {
    ASTNode* node;
    VisitPhase phase;
} AnalyzeFrame;

// Symbol table entry
typedef struct SymbolTableEntry {
    const char* name;
    // SymbolKind kind;
    // SymbolTypeData type_data;

    // struct SymbolTableEntry* next;

    ASTNode* node; // type information
    struct SymbolTableEntry* next;
} SymbolTableEntry;

// Scope that we can push and pop from
typedef struct Scope {
    SymbolTableEntry* symbols;
    struct Scope* parent;
} Scope;

bool analyze_semantics(ASTNode* root);
