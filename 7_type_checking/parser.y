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
%glr-parser
%define parse.error detailed


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
%token INC DEC ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN

%type <node_val> program definition_list definition function_def enum_def struct_def
%type <node_val> block_body stmt_seq stmt expr match_arms match_arm pattern type_expr type_params_list
%type <node_val> expr_list call_args func_param lambda_args struct_fields struct_field enum_variants enum_variant
%type <node_val> list_literal control_expr struct_literal struct_literal_fields struct_literal_field
%type <node_val> return_type_opt expr_opt for_init let_stmt block_expr expr_no_block

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
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
%left DOTDOT
%right '!' '~'
%left '.' COLONCOLON '(' '[' '{' INC DEC

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

program:
    definition_list {
        $$ = create_node(NODE_PROGRAM);
        $$->as.program.name = ast_strdup("__main__");
        $$->as.program.body = $1;
        root = $$;
    }
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
    | enum_def { $$ = $1; }
    | struct_def { $$ = $1; }
    | let_stmt ';' { $$ = $1; }
    ;
    ;

function_def:
    FN IDENT '(' call_args ')' return_type_opt '{' block_body '}' {
        $$ = create_node(NODE_FUNCTION);
        $$->as.function.name = ast_strdup($2);
        $$->as.function.params = $4;
        $$->as.function.return_type = $6;
        $$->as.function.body = $8;
    }
    | FN IDENT '<' type_params_list '>' '(' call_args ')' return_type_opt '{' block_body '}' {
        $$ = create_node(NODE_FUNCTION);
        $$->as.function.name = ast_strdup($2);
        $$->as.function.params = $7;
        $$->as.function.generic_args = $4;
        $$->as.function.return_type = $9;
        $$->as.function.body = $11;
    }

return_type_opt:
    /* empty */ { $$ = NULL; }
    | ARROW type_expr { $$ = $2; }
    ;

enum_def:
    ENUM IDENT '{' enum_variants '}' {
        $$ = create_node(NODE_ENUM_DECL);
        $$->as.enum_decl.name = ast_strdup($2);
        $$->as.enum_decl.variants = $4;
    }
    | ENUM IDENT '<' type_params_list '>' '{' enum_variants '}' {
        $$ = create_node(NODE_ENUM_DECL);
        $$->as.enum_decl.name = ast_strdup($2);
        $$->as.enum_decl.generic_args = $4;
        $$->as.enum_decl.variants = $7;
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

enum_variant:
    IDENT ';' {
        $$ = create_node(NODE_ENUM_VARIANT);
        $$->as.enum_variant.name = ast_strdup($1);
    }
    | IDENT '(' type_params_list ')' ';' {
        $$ = create_node(NODE_ENUM_VARIANT);
        $$->as.enum_variant.name = ast_strdup($1);
        $$->as.enum_variant.payload_types = $3;
    }
    ;

struct_def:
    STRUCT IDENT '{' struct_fields '}' {
        $$ = create_node(NODE_STRUCT_DECL);
        $$->as.enum_decl.name = ast_strdup($2);
        $$->as.enum_decl.variants = $4;
    }
    | STRUCT IDENT '<' type_params_list '>' '{' struct_fields '}' {
        $$ = create_node(NODE_STRUCT_DECL);
        $$->as.enum_decl.name = ast_strdup($2);
        $$->as.enum_decl.generic_args = $4;
        $$->as.enum_decl.variants = $7;
    }
    ;

struct_fields:
    struct_field { $$ = $1; }
    | struct_fields struct_field {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $2;
        $$ = $1;
    }
    ;

struct_field:
    IDENT ':' type_expr ';' {
        $$ = create_node(NODE_STRUCT_FIELD);
        $$->as.struct_field.name = ast_strdup($1);
        $$->as.struct_field.value = $3;
    }
    ;

block_body:
    /* empty */ {
        $$ = create_node(NODE_SCOPE);
        $$->as.scope.body = NULL;
    }
    | stmt_seq %dprec 2 {
        $$ = create_node(NODE_SCOPE);
        $$->as.scope.body = $1;
    }
    | stmt_seq expr %dprec 1 {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        ASTNode* ret_node = create_node(NODE_RETURN);
        ret_node->as.return_stmt.is_explicit = false;
        ret_node->as.return_stmt.value = $2;
        n->next = ret_node;

        $$ = create_node(NODE_SCOPE);
        $$->as.scope.body = $1;
    }
    | expr %dprec 1 {
        ASTNode* ret_node = create_node(NODE_RETURN);
        ret_node->as.return_stmt.is_explicit = false;
        ret_node->as.return_stmt.value = $1;

        $$ = create_node(NODE_SCOPE);
        $$->as.scope.body = ret_node;
    }
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

let_stmt:
    LET IDENT '=' expr {
        $$ = create_node(NODE_LET);
        $$->as.let.name = ast_strdup($2);
        $$->as.let.value = $4;
    }
    | LET IDENT ':' type_expr '=' expr {
        $$ = create_node(NODE_LET);
        $$->as.let.name = ast_strdup($2);
        $$->as.let.declared_type = $4;
        $$->as.let.value = $6;
    }
    ;

stmt:
    ';' { $$ = NULL; }
    | block_expr { $$ = $1; }
    | expr_no_block ';' { $$ = $1; }
    | let_stmt ';' { $$ = $1; }
    | RETURN expr ';' {
        $$ = create_node(NODE_RETURN);
        $$->as.return_stmt.is_explicit = true;
        $$->as.return_stmt.value = $2;
    }
    | RETURN ';' {
        $$ = create_node(NODE_RETURN);
        $$->as.return_stmt.is_explicit = true;
    }
    | BREAK expr ';' {
        $$ = create_node(NODE_BREAK);
        $$->as.break_stmt.value = $2;
    }
    | BREAK ';' {
        $$ = create_node(NODE_BREAK);
    }
    ;

for_init:
    /* empty */ { $$ = NULL; }
    | let_stmt { $$ = $1; }
    | expr { $$ = $1; }
    ;

expr_opt:
    /* empty */ { $$ = NULL; }
    | expr { $$ = $1; }
    ;

expr:
    expr_no_block { $$ = $1; }
    | block_expr { $$ = $1; }
    ;

block_expr:
    control_expr { $$ = $1; }
    | '{' block_body '}' {
        $$ = $2;
    }
    | FN '(' lambda_args ')' return_type_opt '{' block_body '}' {
        $$ = create_node(NODE_FUNCTION);
        $$->as.function.name = ast_strdup("<lambda>");
        $$->as.function.is_lambda = true;
        $$->as.function.params = $3;
        $$->as.function.return_type = $5;
        $$->as.function.body = $7;
    }
    | FN '<' type_params_list '>' '(' lambda_args ')' return_type_opt '{' block_body '}' {
        $$ = create_node(NODE_FUNCTION);
        $$->as.function.name = ast_strdup("<lambda>");
        $$->as.function.is_lambda = true;
        $$->as.function.params = $6;
        $$->as.function.generic_args = $3;
        $$->as.function.return_type = $8;
        $$->as.function.body = $10;
    }
    ;

expr_no_block:
    INT_LIT { $$ = create_leaf_int($1); }
    | FLOAT_LIT { $$ = create_leaf_float($1); }
    | BOOL_LIT { $$ = create_leaf_bool($1); }
    | STRING_LIT { $$ = create_leaf_str($1); }
    | IDENT { $$ = create_leaf_id($1); }
    | '_' { $$ = create_node(NODE_PLACEHOLDER); }
    | IDENT '=' expr {
        $$ = create_node(NODE_ASSIGN);
        $$->as.assign.op = ast_strdup("=");
        $$->as.assign.target = create_leaf_id($1);
        $$->as.assign.value = $3;
    }
    | IDENT ADD_ASSIGN expr {
        $$ = create_node(NODE_ASSIGN);
        $$->as.assign.op = ast_strdup("+=");
        $$->as.assign.target = create_leaf_id($1);
        $$->as.assign.value = $3;
    }
    | IDENT SUB_ASSIGN expr {
        $$ = create_node(NODE_ASSIGN);
        $$->as.assign.op = ast_strdup("-=");
        $$->as.assign.target = create_leaf_id($1);
        $$->as.assign.value = $3;
    }
    | IDENT MUL_ASSIGN expr {
        $$ = create_node(NODE_ASSIGN);
        $$->as.assign.op = ast_strdup("*=");
        $$->as.assign.target = create_leaf_id($1);
        $$->as.assign.value = $3;
    }
    | IDENT DIV_ASSIGN expr {
        $$ = create_node(NODE_ASSIGN);
        $$->as.assign.op = ast_strdup("/=");
        $$->as.assign.target = create_leaf_id($1);
        $$->as.assign.value = $3;
    }
    | IDENT MOD_ASSIGN expr {
        $$ = create_node(NODE_ASSIGN);
        $$->as.assign.op = ast_strdup("%=");
        $$->as.assign.target = create_leaf_id($1);
        $$->as.assign.value = $3;
    }
    | expr INC {
        $$ = create_node(NODE_UNARY_OP);
        $$->as.unary.op = ast_strdup("++");
        $$->as.unary.operand = $1;
    }
    | expr DEC {
        $$ = create_node(NODE_UNARY_OP);
        $$->as.unary.op = ast_strdup("--");
        $$->as.unary.operand = $1;
    }
    | expr '+' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("+"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '-' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("-"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '*' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("*"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '/' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("/"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '%' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("%"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr POW expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("**"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '&' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("&"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '|' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("|"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '^' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("^"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr SHL expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("<<"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '>' '>' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup(">>"); $$->as.binop.left = $1; $$->as.binop.right = $4; }
    | expr EQ expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("=="); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr NEQ expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("!="); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '<' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("<"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr '>' expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup(">"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr LE expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("<="); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr GE expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup(">="); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr AND expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("&&"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr OR expr { $$ = create_node(NODE_BINARY_OP); $$->as.binop.op = ast_strdup("||"); $$->as.binop.left = $1; $$->as.binop.right = $3; }
    | expr PIPE expr {
        $$ = create_node(NODE_PIPELINE);
        $$->as.pipeline.left = $1;
        $$->as.pipeline.right = $3;
    }
    | '!' expr { $$ = create_node(NODE_UNARY_OP); $$->as.unary.op = ast_strdup("!"); $$->as.unary.operand = $2; }
    | '~' expr { $$ = create_node(NODE_UNARY_OP); $$->as.unary.op = ast_strdup("~"); $$->as.unary.operand = $2; }
    | '-' expr %prec '!' { $$ = create_node(NODE_UNARY_OP); $$->as.unary.op = ast_strdup("-"); $$->as.unary.operand = $2; }
    | '(' expr ')' { $$ = $2; }
    | expr '(' expr_list ')' {
        $$ = create_node(NODE_CALL);
        if ($1->type == NODE_IDENTIFIER) {
            $$->as.call.debug_name = ast_strdup($1->as.ident.name);
        }
        else {
            // Must evaluate before calling!
            $$->as.call.debug_name = ast_strdup("<dynamic>");
        }
        $$->as.call.callee = $1;
        $$->as.call.args = $3;
    }
    | expr COLONCOLON IDENT {
        $$ = create_node(NODE_MEMBER_ACCESS);
        $$->as.member.object = $1;
        $$->as.member.op = ast_strdup("::");
        $$->as.member.member = create_leaf_id($3);
    }
    | expr '.' IDENT {
        $$ = create_node(NODE_MEMBER_ACCESS);
        $$->as.member.object = $1;
        $$->as.member.op = ast_strdup(".");
        $$->as.member.member = create_leaf_id($3);
    }
    | list_literal { $$ = $1; }
    | struct_literal { $$ = $1; }
    | expr DOTDOT expr {
        $$ = create_node(NODE_RANGE);
        $$->as.range.inclusive = 0;
        $$->as.range.start = $1;
        $$->as.range.end = $3;
    }
    | expr DOTDOT '=' expr {
        $$ = create_node(NODE_RANGE);
        $$->as.range.inclusive = 1;
        $$->as.range.start = $1;
        $$->as.range.end = $4;
    }
    | expr '[' expr_list ']' {
        $$ = create_node(NODE_BINARY_OP);
        $$->as.binop.op = ast_strdup("[]");
        $$->as.binop.left = $1;
        $$->as.binop.right = $3;
    }
    ;
    ;
    /* Some/None/Ok/Err are regular identifiers, not keywords.
       They are parsed via the generic IDENT '(' expr_list ')' rule. */
    ;

struct_literal:
    IDENT '<' type_params_list '>' '{' struct_literal_fields '}' {
        $$ = create_node(NODE_STRUCT_LITERAL);
        $$->as.struct_lit.name = ast_strdup($1);
        $$->as.struct_lit.generic_args = $3;
        $$->as.struct_lit.fields = $6;
    }
    | IDENT '{' struct_literal_fields '}' {
        $$ = create_node(NODE_STRUCT_LITERAL);
        $$->as.struct_lit.name = ast_strdup($1);
        $$->as.struct_lit.fields = $3;
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
        $$->as.struct_field.name = ast_strdup($1);
        $$->as.struct_field.value = $3;
    }
    ;

control_expr:
    IF expr '{' block_body '}' %prec LOWER_THAN_ELSE %dprec 2 {
        $$ = create_node(NODE_IF);
        $$->as.if_expr.cond = $2;
        $$->as.if_expr.then_body = $4;
    }
    | IF expr '{' block_body '}' ELSE '{' block_body '}' %dprec 2 {
        $$ = create_node(NODE_IF);
        $$->as.if_expr.cond = $2;
        $$->as.if_expr.then_body = $4;
        $$->as.if_expr.else_body = $8;
    }
    | IF expr '{' block_body '}' ELSE control_expr %dprec 2 {
        $$ = create_node(NODE_IF);
        $$->as.if_expr.cond = $2;
        $$->as.if_expr.then_body = $4;
        $$->as.if_expr.else_body = $7;
    }
    | IF '(' expr ')' '{' block_body '}' %prec LOWER_THAN_ELSE %dprec 1 {
        $$ = create_node(NODE_IF);
        $$->as.if_expr.cond = $3;
        $$->as.if_expr.then_body = $6;
    }
    | IF '(' expr ')' '{' block_body '}' ELSE '{' block_body '}' %dprec 1 {
        $$ = create_node(NODE_IF);
        $$->as.if_expr.cond = $3;
        $$->as.if_expr.then_body = $6;
        $$->as.if_expr.else_body = $10;
    }
    | IF '(' expr ')' '{' block_body '}' ELSE control_expr %dprec 1 {
        $$ = create_node(NODE_IF);
        $$->as.if_expr.cond = $3;
        $$->as.if_expr.then_body = $6;
        $$->as.if_expr.else_body = $9;
    }
    | MATCH '(' expr ')' '{' match_arms '}' {
        $$ = create_node(NODE_MATCH);
        $$->as.match_expr.subject = $3;
        $$->as.match_expr.arms = $6;
    }
    | FOR '(' for_init ';' expr_opt ';' expr_opt ')' '{' block_body '}' {
        $$ = create_node(NODE_FOR);
        $$->as.for_expr.init = $3;
        $$->as.for_expr.cond = $5;
        $$->as.for_expr.step = $7;
        $$->as.for_expr.body = $10;
    }
    | FOR '(' for_init ';' expr_opt ';' expr_opt ')' '{' block_body '}' ELSE '{' block_body '}' {
        $$ = create_node(NODE_FOR);
        $$->as.for_expr.init = $3;
        $$->as.for_expr.cond = $5;
        $$->as.for_expr.step = $7;
        $$->as.for_expr.body = $10;
        $$->as.for_expr.else_body = $14;
    }
    | FOR IDENT IN expr '{' block_body '}' {
        $$ = create_node(NODE_FOREACH);
        $$->as.foreach_expr.binded_term = ast_strdup($2);
        $$->as.foreach_expr.iterator = $4;
        $$->as.foreach_expr.body = $6;
    }
    | FOR IDENT IN expr '{' block_body '}' ELSE '{' block_body '}' {
        $$ = create_node(NODE_FOREACH);
        $$->as.foreach_expr.binded_term = ast_strdup($2);
        $$->as.foreach_expr.iterator = $4;
        $$->as.foreach_expr.body = $6;
        $$->as.foreach_expr.else_body = $10;
    }
    | FOR IDENT IN '(' expr ')' '{' block_body '}' {
        $$ = create_node(NODE_FOREACH);
        $$->as.foreach_expr.binded_term = ast_strdup($2);
        $$->as.foreach_expr.iterator = $5;
        $$->as.foreach_expr.body = $8;
    }
    | FOR IDENT IN '(' expr ')' '{' block_body '}' ELSE '{' block_body '}' {
        $$ = create_node(NODE_FOREACH);
        $$->as.foreach_expr.binded_term = ast_strdup($2);
        $$->as.foreach_expr.iterator = $5;
        $$->as.foreach_expr.body = $8;
        $$->as.foreach_expr.else_body = $12;
    }
    | LOOP '{' block_body '}' {
        $$ = create_node(NODE_LOOP);
        $$->as.loop_expr.body = $3;
    }
    | LOOP '{' block_body '}' ELSE '{' block_body '}' {
        $$ = create_node(NODE_LOOP);
        $$->as.loop_expr.body = $3;
        $$->as.loop_expr.else_body = $7;
    }
    ;


list_literal:
    '[' expr_list ']' {
        $$ = create_node(NODE_LIST_LITERAL);
        $$->as.list_lit.items = $2;
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
    | func_param { $$ = $1; }
    | call_args ',' func_param {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $3;
        $$ = $1;
    }
    ;

func_param:
    IDENT ':' type_expr {
        $$ = create_node(NODE_FUNC_PARAMETER);
        $$->as.func_param.name = ast_strdup($1);
        $$->as.func_param.type_expr = $3;
    }
    ;

lambda_args:
    /* empty */ { $$ = NULL; }
    | func_param { $$ = $1; }
    | lambda_args ',' func_param {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $3;
        $$ = $1;
    }
    ;

type_expr:
    IDENT {
        $$ = create_node(NODE_CONCRETE_TYPE);
        $$->as.type.name = ast_strdup($1);
    }
    | IDENT '<' type_params_list '>' {
        $$ = create_node(NODE_CONCRETE_TYPE);
        $$->as.type.name = ast_strdup($1);
        $$->as.type.generic_args = $3;
    }
    ;

type_params_list:
    type_expr { $$ = $1; }
    | type_params_list ',' type_expr {
        ASTNode* n = $1;
        while(n->next) n = n->next;
        n->next = $3;
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
        $$->as.match_arm.pattern = $1;
        $$->as.match_arm.body = $3;
    }
    | pattern IF expr ARROW stmt {
        $$ = create_node(NODE_MATCH_ARM);
        $$->as.match_arm.pattern = $1;
        $$->as.match_arm.guard = $3;
        $$->as.match_arm.body = $5;
    }
    ;

pattern:
    expr { $$ = $1; }
    /* '[' expr_list ']' removed, already covered by expr list_literal */
    | '[' expr_list ',' DOTDOT IDENT ']' {
        $$ = create_node(NODE_LIST_PATTERN);
        ASTNode* n = $2;
        if (n) {
            while(n->next) n = n->next;
            n->next = create_node(NODE_PLACEHOLDER);
            n->next->as.placeholder.name = ast_strdup($5);
            $$->as.list_pattern.items = $2;
        }
        else {
            $$->as.list_pattern.items = create_node(NODE_PLACEHOLDER);
            $$->as.list_pattern.items->as.placeholder.name = ast_strdup($5);
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
            fprintf(stderr, "Syntax error: expected %s but got %.*s at %d:%d.\n",
                    exp_start, got_len, got_start, yylloc.first_line, yylloc.first_column);
        }
        else {
            // No expecting clause — too many valid tokens
            fprintf(stderr, "Syntax error: expected <expression> but got %s at %d:%d.\n",
                    got_start, yylloc.first_line, yylloc.first_column);
        }
    }
    else {
        fprintf(stderr, "Syntax error: %s at %d:%d.\n", s, yylloc.first_line, yylloc.first_column);
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
        print_ast(root, 0);
        if (!analyze_semantics(root)) {
            return 1;
        }
    }
    else {
        printf("Parsing failed.\n");
        return 1;
    }

    return 0;
}
