%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "analyzer.h"

void yyerror(const char *s);
int yylex(void);
extern int yylineno;
extern char* yytext;

ASTNode* root;
%}

%locations
%define parse.error detailed
%expect 1

// To carry semantic values from the lexer to the parser
%union {
    int int_val;
    double float_val;
    int bool_val;
    char* string_val;
    struct ASTNode* node_val;
}

%token <int_val> INT_LIT
%token <float_val> FLOAT_LIT
%token <bool_val> BOOL_LIT
%token <string_val> IDENT STRING_LIT
%token LET IF ELSE MATCH FN RETURN BREAK FOR LOOP ENUM STRUCT
%token IN
%token POW EQ NEQ LE GE AND OR SHL SHR PIPE ARROW DOTDOT COLONCOLON

%type <node_val> program definition_list definition function_def enum_def struct_def
%type <node_val> block_body stmt_seq stmt expr match_arms match_arm pattern ident_list
%type <node_val> expr_list call_args struct_fields struct_field enum_variants enum_variant
%type <node_val> list_literal range_expr control_expr struct_literal struct_literal_fields struct_literal_field

%right '='
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NEQ
%left '<' '>' LE GE
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'
%left PIPE
%right POW
%right '!' '~'
%left '.' COLONCOLON '(' '[' '{'

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

program:
    definition_list { root = $1; }
    | /* empty */ { root = NULL; }
    ;

definition_list:
    definition { $$ = $1; }
    | definition_list definition {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $2;
        $$ = $1;
    }
    ;

definition:
    function_def { $$ = $1; }
    | function_def ';' { $$ = $1; }
    | enum_def { $$ = $1; }
    | struct_def { $$ = $1; }
    | stmt { $$ = $1; }
    ;

function_def:
    FN IDENT '(' call_args ')' '{' block_body '}' {
        $$ = create_node(NODE_FUNCTION);
        $$->lexeme = ast_strdup($2);
        $$->args = $4;
        $$->body = $7;
    }
    ;

enum_def:
    ENUM IDENT '{' enum_variants '}' {
        $$ = create_node(NODE_ENUM_DECL);
        $$->lexeme = ast_strdup($2);
        $$->args = $4;
    }
    ;

enum_variants:
    enum_variant { $$ = $1; }
    | enum_variants enum_variant {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $2;
        $$ = $1;
    }
    ;

ident_list:
    IDENT { $$ = create_leaf_id($1); }
    | ident_list ',' IDENT {
        $$ = create_node(NODE_IDENT_LIST);
        $$->left = $1;
        $$->right = create_leaf_id($3);
    }
    ;

enum_variant:
    IDENT ';' {
        $$ = create_node(NODE_ENUM_VARIANT);
        $$->lexeme = ast_strdup($1);
    }
    | IDENT '(' ident_list ')' ';' { 
        $$ = create_node(NODE_ENUM_VARIANT);
        $$->lexeme = ast_strdup($1);
        $$->args = $3;
    }
    ;

struct_def:
    STRUCT IDENT '{' struct_fields '}' {
        $$ = create_node(NODE_STRUCT_DECL);
        $$->lexeme = ast_strdup($2);
        $$->args = $4;
    }
    | STRUCT IDENT '{' struct_fields '}' ';' {
        $$ = create_node(NODE_STRUCT_DECL);
        $$->lexeme = ast_strdup($2);
        $$->args = $4;
    }
    ;

struct_fields:
    struct_field { $$ = $1; }
    | struct_fields ',' struct_field {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $3;
        $$ = $1;
    }
    ;

struct_field:
    IDENT ':' IDENT {
        $$ = create_node(NODE_STRUCT_FIELD);
        $$->lexeme = ast_strdup($1);
        $$->left = create_leaf_id($3);
    }
    ;

block_body:
    /* empty */ { $$ = NULL; }
    | stmt_seq { $$ = $1; }
    | stmt_seq expr {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $2;
        $$ = $1;
    }
    | expr { $$ = $1; }
    ;

stmt_seq:
    stmt { $$ = $1; }
    | stmt_seq stmt {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $2;
        $$ = $1;
    }
    ;

stmt:
    expr ';' { $$ = $1; }
    | LET IDENT '=' expr ';' {
        $$ = create_node(NODE_LET);
        $$->lexeme = ast_strdup($2);
        $$->right = $4;
    }
    | RETURN expr ';' {
        $$ = create_node(NODE_RETURN);
        $$->right = $2;
    }
    | RETURN ';' {
        $$ = create_node(NODE_RETURN);
    }
    | BREAK expr ';' {
        $$ = create_node(NODE_BREAK);
        $$->right = $2;
    }
    | BREAK ';' {
        $$ = create_node(NODE_BREAK);
    }
    ;

expr:
    INT_LIT { $$ = create_leaf_int($1); }
    | FLOAT_LIT { $$ = create_leaf_float($1); }
    | BOOL_LIT { $$ = create_leaf_bool($1); }
    | STRING_LIT { $$ = create_leaf_str($1); }
    | IDENT { $$ = create_leaf_id($1); }
    | '_' { $$ = create_node(NODE_PLACEHOLDER); }
    | IDENT '=' expr {
        $$ = create_node(NODE_ASSIGN);
        $$->lexeme = ast_strdup($1);
        $$->right = $3;
    }
    | expr '+' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("+"); $$->left = $1; $$->right = $3; }
    | expr '-' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("-"); $$->left = $1; $$->right = $3; }
    | expr '*' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("*"); $$->left = $1; $$->right = $3; }
    | expr '/' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("/"); $$->left = $1; $$->right = $3; }
    | expr '%' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("%"); $$->left = $1; $$->right = $3; }
    | expr POW expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("**"); $$->left = $1; $$->right = $3; }
    | expr '&' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("&"); $$->left = $1; $$->right = $3; }
    | expr '|' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("|"); $$->left = $1; $$->right = $3; }
    | expr '^' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("^"); $$->left = $1; $$->right = $3; }
    | expr SHL expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("<<"); $$->left = $1; $$->right = $3; }
    | expr SHR expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup(">>"); $$->left = $1; $$->right = $3; }
    | expr EQ expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("=="); $$->left = $1; $$->right = $3; }
    | expr NEQ expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("!="); $$->left = $1; $$->right = $3; }
    | expr '<' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("<"); $$->left = $1; $$->right = $3; }
    | expr '>' expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup(">"); $$->left = $1; $$->right = $3; }
    | expr LE expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("<="); $$->left = $1; $$->right = $3; }
    | expr GE expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup(">="); $$->left = $1; $$->right = $3; }
    | expr AND expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("&&"); $$->left = $1; $$->right = $3; }
    | expr OR expr { $$ = create_node(NODE_BINARY_OP); $$->lexeme = ast_strdup("||"); $$->left = $1; $$->right = $3; }
    | expr PIPE expr {
        $$ = create_node(NODE_PIPELINE);
        $$->left = $1;
        $$->right = $3;
    }
    | '!' expr { $$ = create_node(NODE_UNARY_OP); $$->lexeme = ast_strdup("!"); $$->right = $2; }
    | '~' expr { $$ = create_node(NODE_UNARY_OP); $$->lexeme = ast_strdup("~"); $$->right = $2; }
    | '-' expr %prec '!' { $$ = create_node(NODE_UNARY_OP); $$->lexeme = ast_strdup("-"); $$->right = $2; }
    | '(' expr ')' { $$ = $2; }
    | '{' block_body '}' {
        $$ = create_node(NODE_STMT_LIST);
        $$->body = $2;
    }
    | expr '(' expr_list ')' {
        $$ = create_node(NODE_CALL);
        if ($1->type == NODE_IDENTIFIER) {
            $$->lexeme = ast_strdup($1->lexeme);
        } else {
            $$->lexeme = ast_strdup("<dynamic>");
            $$->left = $1;
        }
        $$->args = $3;
    }
    | expr COLONCOLON IDENT {
        $$ = create_node(NODE_MEMBER_ACCESS);
        $$->left = $1;
        $$->lexeme = ast_strdup("::");
        $$->right = create_leaf_id($3);
    }
    | expr '.' IDENT {
        $$ = create_node(NODE_MEMBER_ACCESS);
        $$->left = $1;
        $$->lexeme = ast_strdup(".");
        $$->right = create_leaf_id($3);
    }
    | control_expr { $$ = $1; }
    | list_literal { $$ = $1; }
    | struct_literal { $$ = $1; }
    | expr '[' expr_list ']' {
        $$ = create_node(NODE_BINARY_OP);
        $$->lexeme = ast_strdup("[]");
        $$->left = $1;
        $$->right = $3;
    }
    /* Some/None/Ok/Err are regular identifiers, not keywords.
       They are parsed via the generic IDENT '(' expr_list ')' rule. */
    ;

struct_literal:
    IDENT '{' struct_literal_fields '}' {
        $$ = create_node(NODE_STRUCT_DECL); 
        $$->lexeme = ast_strdup($1);
        $$->args = $3;
    }
    ;

struct_literal_fields:
    /* empty */ { $$ = NULL; }
    | struct_literal_field { $$ = $1; }
    | struct_literal_fields ',' struct_literal_field {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $3;
        $$ = $1;
    }
    ;

struct_literal_field:
    IDENT ':' expr {
        $$ = create_node(NODE_STRUCT_FIELD);
        $$->lexeme = ast_strdup($1);
        $$->right = $3;
    }
    ;

control_expr:
    IF '(' expr ')' '{' block_body '}' %prec LOWER_THAN_ELSE {
        $$ = create_node(NODE_IF);
        $$->cond = $3;
        $$->body = $6;
    }
    | IF '(' expr ')' '{' block_body '}' ELSE '{' block_body '}' {
        $$ = create_node(NODE_IF);
        $$->cond = $3;
        $$->body = $6;
        $$->else_branch = $10;
    }
    | IF '(' expr ')' '{' block_body '}' ELSE control_expr {
        $$ = create_node(NODE_IF);
        $$->cond = $3;
        $$->body = $6;
        $$->else_branch = $9;
    }
    | MATCH '(' expr ')' '{' match_arms '}' {
        $$ = create_node(NODE_MATCH);
        $$->cond = $3;
        $$->body = $6;
    }
    | FOR '(' IDENT '=' expr ';' expr ';' expr ')' '{' block_body '}' {
        $$ = create_node(NODE_FOR);
        $$->lexeme = ast_strdup($3);
        $$->left = $5;
        $$->cond = $7;
        $$->right = $9;
        $$->body = $12;
    }
    | FOR '(' LET IDENT '=' expr ';' expr ';' expr ')' '{' block_body '}' {
        $$ = create_node(NODE_FOR);
        $$->lexeme = ast_strdup($4);
        $$->left = $6; 
        $$->cond = $8; 
        $$->right = $10; 
        $$->body = $13;
    }
    | FOR IDENT IN range_expr '{' block_body '}' {
        $$ = create_node(NODE_FOR);
        $$->lexeme = ast_strdup($2);
        $$->cond = $4; 
        $$->body = $6;
    }
    | LOOP '{' block_body '}' {
        $$ = create_node(NODE_LOOP);
        $$->body = $3;
    }
    ;

range_expr:
    expr DOTDOT expr {
        $$ = create_node(NODE_RANGE);
        $$->left = $1;
        $$->right = $3;
    }
    | expr DOTDOT '=' expr {
        $$ = create_node(NODE_RANGE);
        $$->lexeme = ast_strdup("inclusive");
        $$->left = $1;
        $$->right = $4;
    }
    ;

list_literal:
    '[' expr_list ']' {
        $$ = create_node(NODE_LIST_LITERAL);
        $$->args = $2;
    }
    ;

expr_list:
    /* empty */ { $$ = NULL; }
    | expr { $$ = $1; }
    | expr_list ',' expr {
        ASTNode* n = $1;
        if (!n) { $$ = $3; }
        else {
            while(n->next) n = n->next;
            n->next = $3;
            $$ = $1;
        }
    }
    ;

call_args:
    /* empty */ { $$ = NULL; }
    | IDENT { $$ = create_leaf_id($1); }
    | call_args ',' IDENT {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = create_leaf_id($3);
        $$ = $1;
    }
    ;

match_arms:
    match_arm { $$ = $1; }
    | match_arms match_arm {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $2;
        $$ = $1;
    }
    ;

match_arm:
    pattern ARROW stmt {
        $$ = create_node(NODE_MATCH_ARM);
        $$->left = $1;
        $$->right = $3;
    }
    | pattern IF expr ARROW stmt {
        $$ = create_node(NODE_MATCH_ARM);
        $$->left = $1;
        $$->cond = $3;
        $$->right = $5;
    }
    ;

pattern:
    expr { $$ = $1; }
    | range_expr { $$ = $1; }
    /* '[' expr_list ']' removed, already covered by expr list_literal */
    | '[' expr_list ',' DOTDOT IDENT ']' {
        $$ = create_node(NODE_LIST_LITERAL);
        ASTNode* n = $2;
        if (n) {
            while(n->next) n = n->next;
            n->next = create_node(NODE_PLACEHOLDER);
            n->next->lexeme = ast_strdup($5);
            $$->args = $2;
        } else {
            $$->args = create_node(NODE_PLACEHOLDER);
            $$->args->lexeme = ast_strdup($5);
        }
    }
    ;

%%

void yyerror(const char *s) {
    // Bison detailed errors look like:
    //   "syntax error, unexpected ';', expecting IDENT or INT_LIT"
    //   "syntax error, unexpected ';'"
    // We reformat to: "Syntax error: expected '<value>' but got ';' at line N."
    const char *unexpected = strstr(s, "unexpected ");
    const char *expecting  = strstr(s, "expecting ");

    if (unexpected) {
        const char *got_start = unexpected + 11; // skip "unexpected "

        if (expecting) {
            const char *got_end = expecting - 2; // skip ", "
            int got_len = (int)(got_end - got_start);
            const char *exp_start = expecting + 10; // skip "expecting "
            fprintf(stderr, "Syntax error: expected %s but got %.*s at line %d.\n",
                    exp_start, got_len, got_start, yylloc.first_line);
        } else {
            // No expecting clause — too many valid tokens
            fprintf(stderr, "Syntax error: expected <expression> but got %s at line %d.\n",
                    got_start, yylloc.first_line);
        }
    } else {
        fprintf(stderr, "Syntax error: %s at line %d.\n", s, yylloc.first_line);
    }
}

int main(int argc, char** argv) {
    if (argc > 1) {
        FILE* f = fopen(argv[1], "r");
        if (!f) {
            perror("fopen");
            return 1;
        }
        extern FILE* yyin;
        yyin = f;
    }
    if (yyparse() == 0) {
        printf("Parsing successful!\nAST Structure:\n");
        print_ast(root, 0);
        analyze_semantics(root);
    } else {
        printf("Parsing failed.\n");
        return 1;
    }
    return 0;
}
