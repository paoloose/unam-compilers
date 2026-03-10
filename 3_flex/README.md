# Practice 3: Flex

The following practice uses Flex to define a lexer for a C-like grammar.

The program supports:

- Number literals in decimal, octal and hexadecimal format
- Floating point and scientific notation
- Characters and strings
- Reserved names
- Tracking the line and column for tokens
- Symbols table for identifiers

## Running the tests

```bash
docker compose up --build
```

## Running the REPL

```bash
flex scanner.l

gcc lex.yy.c scanner.c symtable.c -o scanner -lfl

./scanner
```

Example output:

```bash
$ ./scanner

int age = 10;
[KW_INT:int]
[IDENTIFIER:age]
[ASSIGN:=]
[INT_LITERAL:10]
[SEMICOLON:;]

age = age + 1;
[IDENTIFIER:age]
[ASSIGN:=]
[IDENTIFIER:age]
[PLUS:+]
[INT_LITERAL:1]
[SEMICOLON:;]

age *= 2;
[IDENTIFIER:age]
[MUL_ASSIGN:*=]
[INT_LITERAL:2]
[SEMICOLON:;]

char* name = "jonny dou";
[IDENTIFIER:char]
[MUL:*]
[IDENTIFIER:name]
[ASSIGN:=]
[STRING_LITERAL:"jonny dou"]
[SEMICOLON:;]

print(name);
[IDENTIFIER:print]
[LPAREN:(]
[IDENTIFIER:name]
[RPAREN:)]
[SEMICOLON:;]

char favorite_letter;
[KW_CHAR:char]
[IDENTIFIER:favorite_letter]
[SEMICOLON:;]

favorite_letter = 'y';
[IDENTIFIER:favorite_letter]
[ASSIGN:=]
[CHAR_LITERAL:'y']
[SEMICOLON:;]

if (age > 15e) { /* nothing */ }
[KW_IF:if]
[LPAREN:(]
[IDENTIFIER:age]
[GT:>]
[ERROR:15e]
Error at line 15, col 11: unexpected character '15e'
[RPAREN:)]
[LBRACE:{]
[RBRACE:}]

--------------
Symbols table
--------------

age: 4 occurrences
 - line: 1, col: 5
 - line: 3, col: 1
 - line: 3, col: 7
 - line: 5, col: 1
 - line: 14, col: 1

name: 2 occurrences
 - line: 7, col: 8
 - line: 9, col: 7

print: 1 occurrences
 - line: 9, col: 1

favorite_letter: 2 occurrences
 - line: 11, col: 6
 - line: 13, col: 1
```
