#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Heap allocates a copy of a given string
static inline char* ast_strdup(const char* s) {
    if (!s) return NULL;
    char* d = (char*) malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION,
    NODE_STMT_LIST,
    NODE_LET,
    NODE_ASSIGN,
    NODE_IF,
    NODE_FOR,
    NODE_LOOP,
    NODE_MATCH,
    NODE_MATCH_ARM,
    NODE_RETURN,
    NODE_BREAK,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_IDENTIFIER,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_BOOL_LITERAL,
    NODE_STRING_LITERAL,
    NODE_CALL,
    NODE_ENUM_DECL,
    NODE_ENUM_VARIANT,
    NODE_STRUCT_DECL,
    NODE_STRUCT_FIELD,
    NODE_LIST_LITERAL,
    NODE_PIPELINE,
    NODE_PLACEHOLDER, // '_'
    NODE_MEMBER_ACCESS, // '.'
    NODE_RANGE,
} NodeType;

typedef struct ASTNode {
    NodeType type;
    char* lexeme;
    int int_val;
    double float_val;
    int bool_val;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* next; // for lists/sequences
    struct ASTNode* cond;
    struct ASTNode* body;
    struct ASTNode* else_branch;
    struct ASTNode* args; // for calls
} ASTNode;

static inline ASTNode* create_node(NodeType type) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = type;
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

static inline void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    
    const char* type_names[] = {
        "NODE_PROGRAM", "NODE_FUNCTION", "NODE_STMT_LIST", "NODE_LET", "NODE_ASSIGN",
        "NODE_IF", "NODE_FOR", "NODE_LOOP", "NODE_MATCH",
        "NODE_MATCH_ARM", "NODE_RETURN", "NODE_BREAK",
        "NODE_BINARY_OP", "NODE_UNARY_OP", "NODE_IDENTIFIER", "NODE_INT_LITERAL",
        "NODE_FLOAT_LITERAL", "NODE_BOOL_LITERAL", "NODE_STRING_LITERAL", "NODE_CALL",
        "NODE_ENUM_DECL", "NODE_ENUM_VARIANT", "NODE_STRUCT_DECL", "NODE_STRUCT_FIELD",
        "NODE_LIST_LITERAL", "NODE_PIPELINE", "NODE_PLACEHOLDER", "NODE_MEMBER_ACCESS",
        "NODE_RANGE"
    };

    printf("[%s]", type_names[node->type]);
    if (node->lexeme) printf(" lexeme: %s", node->lexeme);
    if (node->type == NODE_INT_LITERAL) printf(" val: %d", node->int_val);
    if (node->type == NODE_FLOAT_LITERAL) printf(" val: %f", node->float_val);
    if (node->type == NODE_BOOL_LITERAL) printf(" val: %s", node->bool_val ? "true" : "false");
    printf("\n");

    if (node->left) print_ast(node->left, indent + 1);
    if (node->right) print_ast(node->right, indent + 1);
    if (node->cond) {
        for (int i = 0; i <= indent; i++) printf("  ");
        printf("(cond)\n");
        print_ast(node->cond, indent + 2);
    }
    if (node->body) {
        for (int i = 0; i <= indent; i++) printf("  ");
        printf("(body)\n");
        print_ast(node->body, indent + 2);
    }
    if (node->args) {
        for (int i = 0; i <= indent; i++) printf("  ");
        printf("(args)\n");
        print_ast(node->args, indent + 2);
    }
    if (node->else_branch) {
        for (int i = 0; i <= indent; i++) printf("  ");
        printf("(else)\n");
        print_ast(node->else_branch, indent + 2);
    }
    if (node->next) print_ast(node->next, indent);
}

#endif
