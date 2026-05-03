#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "debug.h"

// Heap allocates a copy of a given string
static inline char* ast_strdup(const char* s) {
    if (!s) return NULL;
    char* d = (char*) malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

typedef enum {
    /* 0*/ NODE_PROGRAM,
    /* 1*/ NODE_FUNCTION,
    /* 2*/ NODE_STMT_LIST,
    /* 3*/ NODE_LET,
    /* 4*/ NODE_ASSIGN,
    /* 5*/ NODE_IF,
    /* 6*/ NODE_FOR,
    /* 7*/ NODE_LOOP,
    /* 8*/ NODE_MATCH,
    /* 9*/ NODE_MATCH_ARM,
    /*10*/ NODE_RETURN,
    /*11*/ NODE_BREAK,
    /*12*/ NODE_IDENT_LIST,
    /*13*/ NODE_FUNC_PARAMETER,
    /*14*/ NODE_BINARY_OP,
    /*15*/ NODE_UNARY_OP,
    /*16*/ NODE_IDENTIFIER,
    /*17*/ NODE_CONCRETE_TYPE,
    /*18*/ NODE_GENERIC_TYPE,
    /*19*/ NODE_INT_LITERAL,
    /*20*/ NODE_FLOAT_LITERAL,
    /*21*/ NODE_BOOL_LITERAL,
    /*22*/ NODE_STRING_LITERAL,
    /*23*/ NODE_CALL,
    /*24*/ NODE_ENUM_DECL,
    /*25*/ NODE_ENUM_VARIANT,
    /*26*/ NODE_STRUCT_DECL,
    /*27*/ NODE_STRUCT_LITERAL,
    /*28*/ NODE_STRUCT_FIELD,
    /*29*/ NODE_LIST_LITERAL,
    /*30*/ NODE_LIST_PATTERN,
    /*31*/ NODE_PIPELINE,
    /*32*/ NODE_PLACEHOLDER, // '_'
    /*33*/ NODE_MEMBER_ACCESS, // '.'
    /*34*/ NODE_RANGE,
    /*35*/ NODE_LAMBDA,
} NodeType;

// note: I may use a union in the future to save some memory
typedef struct ASTNode {
    NodeType type;
    int line;
    int col;
    const char* lexeme;
    int int_val;
    double float_val;
    int bool_val;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* next; // for lists/sequences
    struct ASTNode* cond;
    struct ASTNode* body;
    struct ASTNode* else_branch;
    struct ASTNode* generic_args; // for generig paramters
    struct ASTNode* args; // for calls
    // Populated in the semantic analysis phase
    // That is, if this node evaluates to a type, then after calling `analyze_node`,
    // this value must've been populated
    struct ASTNode* evaluates_to_type;
    // For functions
    struct ASTNode* return_type;
} ASTNode;

extern int yylineno;
extern int yycolumn;

static inline ASTNode* create_node(NodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = yylineno;
    node->col = yycolumn;
    return node;
}

static inline ASTNode* create_leaf_id(char* lexeme) {
    ASTNode* node = create_node(NODE_IDENTIFIER);
    node->lexeme = ast_strdup(lexeme);
    return node;
}

static inline ASTNode* create_leaf_int(int val) {
    ASTNode* node = create_node(NODE_INT_LITERAL);
    node->int_val = val;
    return node;
}

static inline ASTNode* create_leaf_float(double val) {
    ASTNode* node = create_node(NODE_FLOAT_LITERAL);
    node->float_val = val;
    return node;
}

static inline ASTNode* create_leaf_bool(int val) {
    ASTNode* node = create_node(NODE_BOOL_LITERAL);
    node->bool_val = val;
    return node;
}

static inline ASTNode* create_leaf_str(char* str) {
    ASTNode* node = create_node(NODE_STRING_LITERAL);
    node->lexeme = ast_strdup(str);
    return node;
}

#define PRINT_INDEN(indent) for (int i = 0; i <= (indent); i++) printf("  ");

static inline void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    PRINT_INDEN(indent - 1);

    // note: the order depends on the NodeType enum
    const char* type_names[] = {
        "NODE_PROGRAM", "NODE_FUNCTION", "NODE_STMT_LIST", "NODE_LET", "NODE_ASSIGN",
        "NODE_IF", "NODE_FOR", "NODE_LOOP", "NODE_MATCH",
        "NODE_MATCH_ARM", "NODE_RETURN", "NODE_BREAK", "NODE_IDENT_LIST", "NODE_FUNC_PARAMETER",
        "NODE_BINARY_OP", "NODE_UNARY_OP", "NODE_IDENTIFIER", "NODE_CONCRETE_TYPE", "NODE_GENERIC_TYPE",
        "NODE_INT_LITERAL", "NODE_FLOAT_LITERAL", "NODE_BOOL_LITERAL", "NODE_STRING_LITERAL", "NODE_CALL",
        "NODE_ENUM_DECL", "NODE_ENUM_VARIANT", "NODE_STRUCT_DECL", "NODE_STRUCT_LITERAL", "NODE_STRUCT_FIELD",
        "NODE_LIST_LITERAL", "NODE_LIST_PATTERN", "NODE_PIPELINE", "NODE_PLACEHOLDER", "NODE_MEMBER_ACCESS",
        "NODE_RANGE", "NODE_LAMBDA"
    };

    printf("[%s]", type_names[node->type]);
    if (node->lexeme) printf(" lexeme: %s", node->lexeme);
    if (node->type == NODE_INT_LITERAL) printf(" val: %d", node->int_val);
    if (node->type == NODE_FLOAT_LITERAL) printf(" val: %f", node->float_val);
    if (node->type == NODE_BOOL_LITERAL) printf(" val: %s", node->bool_val ? "true" : "false");
    printf("\n");

    if (node->type == NODE_GENERIC_TYPE) {
        ASTNode* garg = node->generic_args;
        while (garg) {
            print_ast(garg, indent);
            garg = garg->next;
        }
    }
    if (node->left) print_ast(node->left, indent + 1);
    if (node->right) print_ast(node->right, indent + 1);
    if (node->cond) {
        PRINT_INDEN(indent);
        printf("(cond)\n");
        print_ast(node->cond, indent + 2);
    }
    if (node->args) {
        PRINT_INDEN(indent);
        printf("(args)\n");
        print_ast(node->args, indent + 2);
    }
    if (node->return_type) {
        PRINT_INDEN(indent);
        printf("(return_type)\n");
        print_ast(node->return_type, indent + 2);
    }
    if (node->body) {
        PRINT_INDEN(indent);
        printf("(body)\n");
        print_ast(node->body, indent + 2);
    }
    if (node->else_branch) {
        PRINT_INDEN(indent);
        printf("(else)\n");
        print_ast(node->else_branch, indent + 2);
    }
    if (node->next) print_ast(node->next, indent);
}
