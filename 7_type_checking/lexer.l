%option noyywrap yylineno noinput nounput

%{
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "parser.tab.h"

// Ensure fileno is visible
extern int fileno(FILE *stream);

int yycolumn = 1;
#define YY_USER_ACTION \
    yylloc.first_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_line = yylineno; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
%}

%%
"//".*             { /* skip comments */ }
"/*"([^*]|\*+[^*/])*\*+\/   { /* skip multi-line comments */ }

[ \t\r]            { /* skip whitespace */ }
\n                 { yycolumn = 1; }

"let"              { return LET; }
"if"               { return IF; }
"else"             { return ELSE; }
"match"            { return MATCH; }
"fn"               { return FN; }
"return"           { return RETURN; }
"break"            { return BREAK; }
"for"              { return FOR; }
"loop"             { return LOOP; }
"enum"             { return ENUM; }
"struct"           { return STRUCT; }
"true"             { yylval.bool_val = 1; return BOOL_LIT; }
"false"            { yylval.bool_val = 0; return BOOL_LIT; }

"in"               { return IN; }

"**"               { return POW; }
"=="               { return EQ; }
"!="               { return NEQ; }
"<="               { return LE; }
">="               { return GE; }
"&&"               { return AND; }
"||"               { return OR; }
"<<"               { return SHL; }
">>"               { return SHR; }
"|>"               { return PIPE; }
"=>"               { return ARROW; }
".."               { return DOTDOT; }
"::"               { return COLONCOLON; }
"++"               { return INC; }
"--"               { return DEC; }
"+="               { return ADD_ASSIGN; }
"-="               { return SUB_ASSIGN; }
"*="               { return MUL_ASSIGN; }
"/="               { return DIV_ASSIGN; }
"%="               { return MOD_ASSIGN; }

"+"                { return '+'; }
"-"                { return '-'; }
"*"                { return '*'; }
"/"                { return '/'; }
"%"                { return '%'; }
"&"                { return '&'; }
"|"                { return '|'; }
"^"                { return '^'; }
"~"                { return '~'; }
"!"                { return '!'; }
"="                { return '='; }
"<"                { return '<'; }
">"                { return '>'; }

";"                { return ';'; }
","                { return ','; }
"("                { return '('; }
")"                { return ')'; }
"{"                { return '{'; }
"}"                { return '}'; }
"["                { return '['; }
"]"                { return ']'; }
":"                { return ':'; }
"."                { return '.'; }
"_"                { return '_'; }

[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.string_val = ast_strdup(yytext);
    return IDENT;
}

[0-9]+\.[0-9]+ {
    yylval.float_val = atof(yytext);
    return FLOAT_LIT;
}

[0-9]+ {
    yylval.int_val = atoi(yytext);
    return INT_LIT;
}

0x[0-9a-fA-F]+ {
    yylval.int_val = (int)strtol(yytext, NULL, 16);
    return INT_LIT;
}

\"([^"\\]|\\.)*\" {
    char* s = ast_strdup(yytext + 1);
    s[strlen(s) - 1] = '\0';
    yylval.string_val = s;
    return STRING_LIT;
}

. { printf("Unknown character: %s at line %d\n", yytext, yylineno); }

%%
