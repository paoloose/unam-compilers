%option noyywrap yylineno

%{
#include <errno.h>
#include "scanner.h"

int tok_line = 1;
int tok_col = 1;
int yycolumn = 1; /* running column counter (1-based) */

SymTableEntry* symbols_table;

/*
 * Flex runs the YY_USER_ACTION snippet before every rule's action
*/
#define YY_USER_ACTION \
	tok_line = yylineno; \
	tok_col = yycolumn; \
	yycolumn = (yytext[0] == '\n') ? 1 : yycolumn + yyleng;
%}

%x PP_INCLUDE
%x PP_DEFINE
%x COMMENT

ID     [a-zA-Z_][a-zA-Z_0-9]*
NUM    [0-9]+
INT    0|([1-9]+[0-9]*)
HEX    0[xX][A-Fa-f0-9]+
OCT    0[0-7]+
FLOAT  ({NUM}\.{NUM})|({NUM}(\.{NUM})?[eE][+-]?{NUM})
WS     [ \t\r\n]+
CHAR   '([^'\\]|\\.)'
STRING \"([^"\\]|\\.)*\"

%%

"/*"              BEGIN(COMMENT);
<COMMENT>"*/"     BEGIN(INITIAL);
<COMMENT>.        ;
<COMMENT>\n       ;
<COMMENT><<EOF>>  { return TOK_ERROR; }
"//"[^\n]*        ;

{WS}              ;

"int"             { return TOK_KW_INT; }
"float"           { return TOK_KW_FLOAT; }
"double"          { return TOK_KW_DOUBLE; }
"char"            { return TOK_KW_CHAR; }
"void"            { return TOK_KW_VOID; }
"if"              { return TOK_KW_IF; }
"else"            { return TOK_KW_ELSE; }
"while"           { return TOK_KW_WHILE; }
"for"             { return TOK_KW_FOR; }
"return"          { return TOK_KW_RETURN; }
"break"           { return TOK_KW_BREAK; }
"continue"        { return TOK_KW_CONTINUE; }

"++"              { return TOK_INC; }
"--"              { return TOK_DEC; }
"+="              { return TOK_PLUS_ASSIGN; }
"-="              { return TOK_MINUS_ASSIGN; }
"*="              { return TOK_MUL_ASSIGN; }
"/="              { return TOK_DIV_ASSIGN; }
"%="              { return TOK_MOD_ASSIGN; }
"=="              { return TOK_EQ; }
"!="              { return TOK_NEQ; }
"<="              { return TOK_LE; }
">="              { return TOK_GE; }
"&&"              { return TOK_AND; }
"||"              { return TOK_OR; }

"="               { return TOK_ASSIGN; }
"<"               { return TOK_LT; }
">"               { return TOK_GT; }
"!"               { return TOK_NOT; }
"+"               { return TOK_PLUS; }
"-"               { return TOK_MINUS; }
"*"               { return TOK_MUL; }
"/"               { return TOK_DIV; }
"%"               { return TOK_MOD; }

"("               { return TOK_LPAREN; }
")"               { return TOK_RPAREN; }
"{"               { return TOK_LBRACE; }
"}"               { return TOK_RBRACE; }
"["               { return TOK_LBRACKET; }
"]"               { return TOK_RBRACKET; }
","               { return TOK_COMMA; }
";"               { return TOK_SEMICOLON; }

{FLOAT}           { return TOK_FLOAT_LITERAL; }
{CHAR}            { return TOK_CHAR_LITERAL; }
{STRING}          { return TOK_STRING_LITERAL; }
{ID}              {
	sym_insert(&symbols_table, yytext, tok_line, tok_col);
	return TOK_IDENTIFIER;
}

{HEX} {
  errno = 0;
  strtoull(yytext, NULL, 16);
  return errno == ERANGE || errno == EINVAL ? TOK_ERROR : TOK_HEX_LITERAL;
}
{OCT} {
  errno = 0;
  strtoull(yytext, NULL, 8);
  return errno == ERANGE || errno == EINVAL ? TOK_ERROR : TOK_OCT_LITERAL;
}
{INT} {
  errno = 0;
  strtoull(yytext, NULL, 10);
  return errno == ERANGE || errno == EINVAL ? TOK_ERROR : TOK_INT_LITERAL;
}


{FLOAT}[a-zA-Z_][a-zA-Z0-9_]*  { return TOK_ERROR; }
{HEX}[a-zA-Z_][a-zA-Z0-9_]*    { return TOK_ERROR; }
{OCT}[a-zA-Z_][a-zA-Z0-9_]*    { return TOK_ERROR; }
{INT}[a-zA-Z_][a-zA-Z0-9_]*    { return TOK_ERROR; }
0[0-9]+                        { return TOK_ERROR; }

<<EOF>>           { return TOK_EOF; }
.                 { return TOK_ERROR; }

%%
