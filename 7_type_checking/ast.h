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
    /* 2*/ NODE_LET,
    /* 3*/ NODE_ASSIGN,
    /* 4*/ NODE_IF,
    /* 5*/ NODE_FOR,
    /* 6*/ NODE_LOOP,
    /* 7*/ NODE_MATCH,
    /* 8*/ NODE_MATCH_ARM,
    /* 9*/ NODE_RETURN,
    /*10*/ NODE_BREAK,
    /*11*/ NODE_IDENT_LIST,
    /*12*/ NODE_FUNC_PARAMETER,
    /*13*/ NODE_BINARY_OP,
    /*14*/ NODE_UNARY_OP,
    /*15*/ NODE_IDENTIFIER,
    /*16*/ NODE_PLAIN_TYPE,
    /*18*/ NODE_INT_LITERAL,
    /*19*/ NODE_FLOAT_LITERAL,
    /*20*/ NODE_BOOL_LITERAL,
    /*21*/ NODE_STRING_LITERAL,
    /*22*/ NODE_CALL,
    /*23*/ NODE_ENUM_DECL,
    /*24*/ NODE_ENUM_VARIANT,
    /*25*/ NODE_STRUCT_DECL,
    /*26*/ NODE_STRUCT_LITERAL,
    /*27*/ NODE_STRUCT_FIELD,
    /*28*/ NODE_LIST_LITERAL,
    /*29*/ NODE_LIST_PATTERN,
    /*30*/ NODE_PIPELINE,
    /*31*/ NODE_PLACEHOLDER, // '_'
    /*32*/ NODE_MEMBER_ACCESS, // '.'
    /*33*/ NODE_RANGE,
    /*34*/ NODE_SCOPE,
    /*35*/ NODE_FOREACH,
} NodeType;

// note: I may use a union in the future to save some memory
// typedef struct ASTNode {
//     NodeType type;
//     int line;
//     int col;
//     const char* lexeme;
//     int int_val;
//     double float_val;
//     int bool_val;
//     struct ASTNode* left;
//     struct ASTNode* right;
//     struct ASTNode* next; // for lists/sequences
//     struct ASTNode* cond;
//     struct ASTNode* body;
//     struct ASTNode* else_branch;
//     struct ASTNode* generic_args; // for generig paramters
//     struct ASTNode* args; // for calls
//     // Populated in the semantic analysis phase
//     // That is, if this node evaluates to a type, then after calling `analyze_node`,
//     // this value must've been populated
//     struct ASTNode* evaluates_to_type;
//     // For functions
//     struct ASTNode* return_type;
// } ASTNode;


typedef struct {
    int line;
    int col;
    int lastcol;
} SourceLoc;

typedef struct ASTNode ASTNode;
struct ASTNode {
    NodeType type;
    SourceLoc loc;

    ASTNode* next;
    // Populated in the semantic analysis phase
    // That is, if this node evaluates to a type, then after calling `analyze_node`,
    // this value must've been populated
    ASTNode* evaluates_to_type;

    union {
        /* NODE_PROGRAM */
        struct { char* name; ASTNode* body; } program;

        /* NODE_FUNCTION */
        struct {
            char* name;              /* "<lambda>" for anonymous functions */
            bool is_lambda;          /* true for anonymous functions */
            ASTNode* params;         /* call_args */
            ASTNode* generic_args;   /* type params */
            ASTNode* return_type;
            ASTNode* body;
        } function;

        /* NODE_SCOPE */
        struct { ASTNode* body; } scope;

        /* NODE_LET */
        struct { char* name; ASTNode* declared_type;  ASTNode* value; } let;

        /* NODE_ASSIGN */
        struct {
            char* op;            /* = += -= etc */
            ASTNode* target;     /* identifier */
            ASTNode* value;
        } assign;

        /* NODE_IF */
        struct {
            ASTNode* cond;
            ASTNode* then_body;
            ASTNode* else_body;  /* optional */
        } if_expr;

        /* NODE_FOR */
        struct {
            ASTNode* init;       /* c-style init expr, optional */
            ASTNode* cond;       /* condition, optional */
            ASTNode* step;       /* c-style increment, optional */
            ASTNode* body;
            ASTNode* else_body;  /* optional */
        } for_expr;

        /* NODE_FOREACH */
        struct {
            char* binded_term;   /* identifier for the loop variable */
            ASTNode* iterator;   /* iterable expression */
            ASTNode* body;
            ASTNode* else_body;  /* optional */
        } foreach_expr;

        /* NODE_LOOP */
        struct {
            ASTNode* body;
            ASTNode* else_body;
        } loop_expr;

        /* NODE_MATCH */
        struct {
            ASTNode* subject;
            ASTNode* arms;
        } match_expr;

        /* NODE_MATCH_ARM */
        struct {
            ASTNode* pattern;
            ASTNode* guard;      /* optional */
            ASTNode* body;
        } match_arm;

        /* NODE_RETURN */
        struct {
            ASTNode* value;      /* optional */
            bool is_explicit;
        } return_stmt;

        /* NODE_BREAK */
        struct {
            ASTNode* value;      /* optional */
        } break_stmt;

        /* NODE_IDENT_LIST */
        struct {
            ASTNode* items;
        } ident_list;

        /* NODE_FUNC_PARAMETER */
        struct {
            char* name;
            ASTNode* type_expr;
        } func_param;

        /* NODE_BINARY_OP */
        struct {
            char* op;
            ASTNode* left;
            ASTNode* right;
        } binop;

        /* NODE_UNARY_OP */
        struct {
            char* op;
            ASTNode* operand;
        } unary;

        /* NODE_IDENTIFIER */
        struct {
            char* name;
        } ident;

        /* NODE_PLAIN_TYPE */
        struct {
            const char* name;
            ASTNode* generic_args;
        } type;

        /* NODE_INT_LITERAL */
        struct {
            int value;
        } int_lit;

        /* NODE_FLOAT_LITERAL */
        struct {
            double value;
        } float_lit;

        /* NODE_BOOL_LITERAL */
        struct {
            int value;
        } bool_lit;

        /* NODE_STRING_LITERAL */
        struct {
            char* value;
        } string_lit;

        /* NODE_CALL */
        struct {
            char* debug_name;    /* identifier or "<dynamic>" */
            ASTNode* callee;
            ASTNode* args;
        } call;

        /* NODE_ENUM_DECL */
        struct {
            char* name;
            ASTNode* generic_args;
            ASTNode* variants;
        } enum_decl;

        /* NODE_ENUM_VARIANT */
        struct {
            char* name;
            ASTNode* payload_types;  /* optional */
        } enum_variant;

        /* NODE_STRUCT_DECL */
        struct {
            char* name;
            ASTNode* generic_args;
            ASTNode* fields;
        } struct_decl;

        /* NODE_STRUCT_LITERAL */
        struct {
            char* name;
            ASTNode* generic_args;
            ASTNode* fields;
        } struct_lit;

        /* NODE_STRUCT_FIELD */
        struct {
            char* name;
            ASTNode* value;      /* expr or type_expr */
        } struct_field;

        /* NODE_LIST_LITERAL */
        struct {
            ASTNode* items;
        } list_lit;

        /* NODE_LIST_PATTERN */
        struct {
            ASTNode* items;      /* tail may contain placeholder(rest) */
        } list_pattern;

        /* NODE_PIPELINE */
        struct {
            ASTNode* left;
            ASTNode* right;
        } pipeline;

        /* NODE_PLACEHOLDER */
        struct {
            char* name;          /* "_" or rest capture name */
        } placeholder;

        /* NODE_MEMBER_ACCESS */
        struct {
            char* op;            /* "." or "::" */
            ASTNode* object;
            ASTNode* member;
        } member;

        /* NODE_RANGE */
        struct {
            int inclusive;
            ASTNode* start;
            ASTNode* end;
        } range;
    } as;
};

extern int yylineno;
extern int yycolumn;
extern int yyleng;

static inline ASTNode* create_node(NodeType type) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = type;
    node->loc.line = yylineno;
    node->loc.col = yycolumn;
    node->loc.lastcol = yycolumn - yyleng;
    return node;
}

static inline ASTNode* create_leaf_id(char* lexeme) {
    ASTNode* node = create_node(NODE_IDENTIFIER);
    node->as.ident.name = ast_strdup(lexeme);
    return node;
}

static inline ASTNode* create_leaf_int(int val) {
    ASTNode* node = create_node(NODE_INT_LITERAL);
    node->as.int_lit.value = val;
    return node;
}

static inline ASTNode* create_leaf_float(double val) {
    ASTNode* node = create_node(NODE_FLOAT_LITERAL);
    node->as.float_lit.value = val;
    return node;
}

static inline ASTNode* create_leaf_bool(int val) {
    ASTNode* node = create_node(NODE_BOOL_LITERAL);
    node->as.bool_lit.value = val;
    return node;
}

static inline ASTNode* create_leaf_str(char* str) {
    ASTNode* node = create_node(NODE_STRING_LITERAL);
    node->as.string_lit.value = ast_strdup(str);
    return node;
}

char* node_repr(ASTNode* node);
void print_ast(ASTNode* node, int indent);

