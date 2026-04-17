# Part 1

Proposed grammar:

```txt
Non-terminals: Program Stmt Expr Term Factor
Terminals: id = ; + * num

Program  -> Program Stmt
Program  -> Stmt
Stmt     -> id = Expr ;
Expr     -> Expr + Term
Expr     -> Term
Term     -> Term * Factor
Term     -> Factor
Factor   -> num
Factor   -> id

```

Examples: see `test_inputs/`
