#import "template.typ": project

#show: project.with(
  title: "Semantic Analysis and AST Construction",
  subtitle: "6th laboratory report",
  authors: (
    "Flores Cóngora, Paolo Luis",
    "Castillo Camacho, Violeta Ardeni"
  ),
  mentors: (
    "Adrián Martínez Manzo",
  ),
  footer-text: "UNAM - Compilers",
  lang: "en",
  school-logo: image("unam_logo.png", width: 50pt),
  branch: "National Autonomous University of Mexico, Computer Engineering: Compilers",
)

= Metadata

== Video links

- *Castillo Camacho, Violeta Ardeni* (link)
- *Flores Cóngora, Paolo Luis* (link)

== Repo URL

#link("https://github.com/paoloose/unam-compilers/tree/main/6_semantics")

= Implementation

== Language design

The language design for Ennuyeux is covered in the #link("README.md")[README.md].

In short, Ennuyeux is the French word for boring, as the language doesn't have any incredible or funny syntax,
because it is aiming to winning the performance competition.

Ennuyeux is a procedure language with functional support, inspired by Gleam and Rust.

Unless Gleam, Ennuyeux does support loops, conditionals, early returns and mutable data, positioning Ennuyeux
as a hybrid language, more to the Rust side.

Unless Rust, it doesn't have a borrow checker, or strong move semantics, making it easier to learn and use.

== Project Structure

```
6_semantics/
 ├─ Makefile         Build system (flex → bison → gcc)
 ├─ lexer.l         Flex lexical analyzer with location tracking
 ├─ parser.y        GLR Bison grammar with semantic actions
 ├─ ast.h           AST node type definitions
 ├─ analyzer.h/c    Semantic analysis (scope and compound assignment tracking)
 ├─ examples/        Test suite (18 individual .ennuyeux files)
 │    ├─ 01_arithmetic.ennuyeux
 │    ├─ 02_logical_bitwise.ennuyeux
 │    ├─ ...
 │    └─ 18_error_invalid_syntax.ennuyeux
 └─ report/          This report
```

The language being implemented is *Ennuyeux*, a functional, expression-based language with Result/Option error handling, pattern matching with guards, and implicit returns. The full language specification is documented in `README.md`.

== Phase 1: Lexical Phase (Flex)

The lexer is defined in `lexer.l` and is responsible for tokenizing the input stream. A key design decision was tracking source locations via the `YY_USER_ACTION` macro, which updates `yylloc` on every token match, enabling precise error messages with line and column information.

```c
int yycolumn = 1;
#define YY_USER_ACTION \
    yylloc.first_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_line = yylineno; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
```

The lexer recognizes the following token categories:

- *Keywords:* `let`, `if`, `else`, `match`, `fn`, `return`, `break`, `for`, `loop`, `enum`, `struct`, `true`, `false`, `in`.
- *Standard Library Enums:* `Some`, `None`, `Ok`, `Err` (handled as regular identifiers/constructors).
- *Multi-character operators:* `**`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, `<<`, `>>`, `|>`, `=>`, `..`, `::`, `++`, `--`, `+=`, `-=`, `*=`, `/=`, `%=`.
- *Single-character operators and delimiters:* `+ - * / % & | ^ ~ ! = < > ; , ( ) { } [ ] : . _`.
- *Literals:* integers (`[0-9]+`), floats (`[0-9]+\.[0-9]+`), hex integers (`0x[0-9a-fA-F]+`), strings (`\"...\"`), booleans (`true`/`false`).
- *Identifiers:* `[a-zA-Z_][a-zA-Z0-9_]*`.

The `_` character is treated as its own token to support Gleam-style placeholder syntax for partial application when using #link("https://tour.gleam.run/functions/pipelines")[pipelines].

== Phase 2: Syntactic and Semantic Phase (Bison)

The parser is defined in `parser.y` and implements the full grammar of the Ennuyeux language using Bison's GLR parsing capabilities. Operator precedence is explicitly declared to resolve shift-reduce conflicts:

```c
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
```

The dangling-else ambiguity is resolved using precedence markers:

```c
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
```

=== Top-Level Definitions

The grammar supports top-level definitions for functions, enums, structs, and standalone statements:

```c
definition:
    function_def
    | function_def ';'   // optional trailing semicolon
    | enum_def
    | struct_def
    | stmt
    ;
```

=== Expression-Based Design

A key design decision is that control flow constructs are expressions, not statements. This means `if`, `match`, `for`, and `loop` all produce values and can appear anywhere an expression is expected:

```c
expr:
    ...
    | control_expr { $$ = $1; }
    ;

control_expr:
    IF expr '{' block_body '}' ELSE '{' block_body '}'
    | MATCH expr '{' match_arms '}'
    | FOR IDENT IN expr '{' block_body '}' ELSE '{' block_body '}'
    | LOOP '{' block_body '}' ELSE '{' block_body '}'
    ;
```

=== Ennuyeux Features (i.e. boring features)

The Ennuyeux specification defines the following features:

*Lists:* List literals are supported via `[expr, expr, ..]` syntax. Pattern matching on lists supports the spread operator `[head, ..rest]`:

```rust
fn sum_list(list: List<T>) {
    match (list) {
        [x, ..rest] => x + sum_list(rest);
        _ => 0;
    }
}
```

*Generics:* Structs, Enums, and Functions support generic type parameters via `<T, U>` syntax. Types can also utilize these parameters, such as `List<Int>`.

```rust
// For example, Ennuyeux is able to write the famous twice function

fn twice<T>(x: Fn<T, T>) {
    x(x(x))
}
```

```c
struct_def:
    STRUCT IDENT '{' struct_fields '}'
    | STRUCT IDENT '<' ident_list '>' '{' struct_fields '}'

function_def:
    FN IDENT '(' call_args ')' '{' block_body '}'
    | FN IDENT '<' ident_list '>' '(' call_args ')' '{' block_body '}'

type_expr:
    IDENT
    | IDENT '<' type_params_list '>'
```

```c
list_literal:
    '[' expr_list ']' { $$ = create_node(NODE_LIST_LITERAL); ... }
    ;

// In patterns:
    '[' expr_list ',' DOTDOT IDENT ']'
    // Creates NODE_LIST_LITERAL with NODE_PLACEHOLDER for the rest
```

*Placeholders:* The `_` token enables Gleam-style partial application: `calculate(1, _, 2)` creates a function that fills in the missing argument.

*Pipelines:* The `|>` operator is implemented as a binary operator with its own precedence level, enabling chained calls like `5 |> double() |> add(3)`:

```rust
// Example: Gleam-style Pipelines (|>)
fn double(n: Int) { n * 2 }
fn add(a: Int, b: Int) { a + b }
fn square(n: Int) { n * n }
fn to_string(n: Int) { "" + n }

fn main() {
    // Simple pipeline: value goes as first argument
    let result = 5 |> double |> square |> to_string;

    // Pipeline with placeholder: value goes where _ is
    let adjusted = 10 |> add(_, 5);

    // Chained with placeholder
    let complex = 3
        |> double
        |> add(_, 100)
        |> to_string;
}
```

*Anonymous Functions (Lambdas):* Lambdas are supported as first-class expressions using the `fn (params) { body }` syntax. Parameters can optionally have type annotations.

```rust
fn main() {
    let duplicate = fn (a: Int) { a * 2 };
    let thing = 10 |> fn (a: Int) { a * 2 }; // can be used with pipelines
}
```

```c
| FN '(' lambda_args ')' '{' block_body '}' {
    $$ = create_node(NODE_LAMBDA);
    $$->args = $3;
    $$->body = $6;
}
```

```c
| expr PIPE expr {
    $$ = create_node(NODE_PIPELINE);
    $$->left = "$1"; $$->right = $3;
}
```

*Structs:* Both struct type declarations and struct literal instantiation are supported:

```rust
struct Point {
    x: int;
    y: int;
}

struct NonEmptyList<T> {
    head: T;
    tail: List<T>;
}
```

```c
struct_def:
    STRUCT IDENT '{' struct_fields '}' { ... }
    ;

struct_literal:
    IDENT '{' struct_literal_fields '}' { ... }
    ;
```

A technical challenge in unparenthesized conditions is distinguishing `Point { ... }` as an expression from a block.

=== Semantic Actions

Each grammar reduction constructs a corresponding AST node. For example, function definitions:

```txt
function_def:
    FN IDENT '(' call_args ')' '{' block_body '}' {
        "$$" = create_node(NODE_FUNCTION);
        "$$"->lexeme = ast_strdup("$2");
        "$$"->args = "$4";
        "$$"->body = "$7";
    }
    ;

| IDENT ADD_ASSIGN expr {
    "$$" = create_node(NODE_ASSIGN);
    "$$"->lexeme = ast_strdup("+=");
    "$$"->left = create_leaf_id("$1"); "$$"->right = "$3";
}

| expr INC {
    "$$" = create_node(NODE_UNARY_OP);
    "$$"->lexeme = ast_strdup("++");
    "$$"->right = "$1";
}
```

== Phase 3: AST Structure

The AST is defined in `ast.h` as a single unified `ASTNode` structure. This design uses a tagged union approach where the `NodeType` enum determines which fields are relevant:

```c
typedef enum {
    NODE_PROGRAM,        NODE_FUNCTION,      NODE_STMT_LIST,
    NODE_LET,            NODE_ASSIGN,        NODE_IF,
    NODE_FOR,            NODE_LOOP,          NODE_RANGE,
    NODE_MATCH,          NODE_MATCH_ARM,     NODE_RETURN,
    NODE_BREAK,          NODE_IDENT_LIST,    NODE_PARAMETER,
    NODE_BINARY_OP,      NODE_UNARY_OP,      NODE_IDENTIFIER,
    NODE_INT_LITERAL,    NODE_FLOAT_LITERAL, NODE_BOOL_LITERAL,
    NODE_STRING_LITERAL, NODE_CALL,          NODE_ENUM_DECL,
    NODE_ENUM_VARIANT,   NODE_STRUCT_DECL,   NODE_STRUCT_FIELD,
    NODE_LIST_LITERAL,   NODE_PIPELINE,      NODE_PLACEHOLDER,
    NODE_MEMBER_ACCESS,  NODE_GENERIC_TYPE,  NODE_LAMBDA
} NodeType;

typedef struct ASTNode {
    NodeType type;
    char* lexeme;
    int int_val;
    double float_val;
    int bool_val;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* next;        // for linked lists of siblings
    struct ASTNode* cond;        // condition (if, while, match)
    struct ASTNode* body;        // body block
    struct ASTNode* else_branch; // else branch
    struct ASTNode* args;        // function/call arguments
} ASTNode;
```

Helper functions provide type-safe constructors for leaf and internal nodes:

```c
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
```

The `print_ast` function recursively traverses the tree, printing each node with proper indentation and labeling child sections (`cond`, `body`, `args`, `else`):

```c
static inline void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("[%s]", type_names[node->type]);
    if (node->lexeme) printf(" lexeme: %s", node->lexeme);
    // ... print children recursively
    if (node->next) print_ast(node->next, indent);
}
```

And this structure is used in the #link("https://www.gnu.org/software/bison/manual/html_node/Union-Decl.html")[`%union`] declaration, which defines the entire
collection of possible data types for semantic values.

```c
// To carry semantic values from the lexer to the parser
%union {
    int int_val;
    double float_val;
    int bool_val;
    char* string_val;
    struct ASTNode* node_val;
}
```

== Phase 4: Validation Engine

The validation engine is integrated into the `main()` function of `parser.y`. It reads source code from a file argument or stdin, and produces one of two outcomes:

- *Success:* The AST is printed in a human-readable tree format, followed by semantic analysis output.
- *Failure:* A precise error message is reported with line and column information.

```c
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
    // ... file handling ...
    if (yyparse() == 0) {
        printf("Parsing successful!\nAST Structure:\n");
        print_ast(root, 0);
        analyze_semantics(root);
    }
    else {
        printf("Parsing failed.\n");
        return 1;
    }
}
```

=== Semantic Analysis Module

The `analyzer.c` module implements a basic semantic analysis pass with scope-based symbol management:

```c
typedef struct Symbol {
    char* name;
    ASTNode* type_node;
    int is_function;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;
```

The analyzer walks the AST, pushing/popping scopes as it enters/exits functions, and registers variables via `let` declarations.

```c
void push_scope() {
    Scope* scope = (Scope*)calloc(1, sizeof(Scope));
    scope->parent = current_scope;
    current_scope = scope;
}

void pop_scope() {
    if (!current_scope) return;
    Scope* parent = current_scope->parent;
    // Note: In a real compiler, we'd free the symbols here but we'll keep them for now
    current_scope = parent;
}
```

The following code is how we analyze a single function node.

```c
void analyze_node(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_FUNCTION: {
            add_symbol(node->lexeme, NULL, 1);
            push_scope();
            printf("Function: %s\n", node->lexeme);
            // Process parameters
            ASTNode* arg = node->args;
            while (arg) {
                if (arg->type == NODE_PARAMETER) {
                    char type_buf[128];
                    get_type_name(arg->left, type_buf, sizeof(type_buf));
                    printf("  Parameter: %s : %s\n", arg->lexeme, type_buf);
                    add_symbol(arg->lexeme, arg->left, 0);
                }
                else {
                    add_symbol(arg->lexeme, NULL, 0);
                }
                arg = arg->next;
            }
            analyze_node(node->body);
            pop_scope();
            break;
        }
        // ...
        // ...
    }

    analyze_node(node->next);
}
```

= Results Obtained

The test suite consists of 18 individual example files in the #link("https://github.com/paoloose/unam-compilers/tree/main/6_semantics/examples")[`examples/`] directory, each exercising a specific language feature. All 17 valid examples parse successfully, and the 1 intentionally invalid example correctly produces a diagnostic error.

#figure(
  table(
    columns: (auto, 1fr, auto),
    align: (left, left, left),
    table.header(
      [*File*], [*Feature Tested*], [*Result*],
    ),
    [`01_arithmetic`], [Arithmetic operators (`+ - * / % **`)], [Pass],
    [`02_logical_bitwise`], [Logical (`&& || !`) and bitwise (`& | ^ << >> ~`)], [Pass],
    [`03_conditionals`], [If / else if / else, if-as-expression], [Pass],
    [`04_loops`], [C-style `for`, `loop` + `break` with value and `else`], [Pass],
    [`05_for_range`], [Range-based `for i in 0..5` + `else` + compound ops], [Pass],
    [`06_pattern_matching`], [Match with literal patterns and guards], [Pass],
    [`07_range_patterns`], [Range patterns inside match arms], [Pass],
    [`08_functions`], [Functions, early return, expression as last value], [Pass],
    [`09_result_option`], [Standard Library Enums (`Ok`/`Err`/`Some`/`None`)], [Pass],
    [`10_enums`], [Enum declarations + `::` variant access], [Pass],
    [`11_structs`], [Struct declaration, literal instantiation, `.` access], [Pass],
    [`12_lists`], [List literals, indexing, `[head, ..rest]` patterns], [Pass],
    [`13_pipelines`], [`|>` pipeline operator with chaining], [Pass],
    [`14_placeholders`], [`_` for partial application], [Pass],
    [`15_fibonacci`], [Recursive Fibonacci with `Result`], [Pass],
    [`16_member_access`], [`.` and `::` member access], [Pass],
    [`17_block_expressions`], [Block `{...}` as expression], [Pass],
    [`18_error_invalid_syntax`], [Intentionally invalid syntax], [*Fail* (expected)],
    [`19_lambdas`], [Anonymous functions (lambdas) as first-class citizens], [Pass],
  ),
  caption: "Test suite results. All examples parse as expected."
)

== Structs and Member Access (`11_structs.ennuyeux`)

This example tests struct type declarations, nested struct literals, and the `.` member access operator:

```rust
struct Point { x: Int, y: Int }
struct LargeRectangle<T> { origin: Point, width: T, height: T }

fn area(rect: Rectangle) { rect.width * rect.height }

fn main() {
    let p: Point = Point { x: 10, y: 20 };
    let r: Rectangle = Rectangle {
        origin: Point { x: 0, y: 0 },
        width: 5,
        height: 3
    };
    print("area = " + area(r));
}
```

*AST output:*

```text
[NODE_STRUCT_DECL] lexeme: Point
  (args)
    [NODE_STRUCT_FIELD] lexeme: x
      [NODE_IDENTIFIER] lexeme: int
    [NODE_STRUCT_FIELD] lexeme: y
      [NODE_IDENTIFIER] lexeme: int
[NODE_STRUCT_DECL] lexeme: Rectangle
  (args)
    [NODE_STRUCT_FIELD] lexeme: origin
      [NODE_IDENTIFIER] lexeme: Point
    [NODE_STRUCT_FIELD] lexeme: width
      [NODE_IDENTIFIER] lexeme: int
    [NODE_STRUCT_FIELD] lexeme: height
      [NODE_IDENTIFIER] lexeme: int
[NODE_FUNCTION] lexeme: area
  (args)
    [NODE_PARAMETER] lexeme: rect
      [NODE_IDENTIFIER] lexeme: Rectangle
  (body)
    [NODE_BINARY_OP] lexeme: *
      [NODE_MEMBER_ACCESS] lexeme: .
        [NODE_IDENTIFIER] lexeme: rect
        [NODE_IDENTIFIER] lexeme: width
      [NODE_MEMBER_ACCESS] lexeme: .
        [NODE_IDENTIFIER] lexeme: rect
        [NODE_IDENTIFIER] lexeme: height
```

The AST correctly represents `rect.width` as a `NODE_MEMBER_ACCESS` with the struct instance on the left and the field name on the right. Nested struct literals (`Point { x: 0, y: 0 }` inside `Rectangle`) are represented as nested `NODE_STRUCT_DECL` nodes.

== Lists and Pattern Matching (`12_lists.ennuyeux`)

This example tests list literals, the spread operator `...`, and recursive list processing via pattern matching:

```
fn head(list: List<T>) {
    match list {
        [first, ..rest] => Some(first);
        _ => None;
    }
}

fn sum_list(list: List<T>) {
    match (list) {
        [x, ..rest] => x + sum_list(rest);
        _ => 0;
    }
}
```

*AST output (excerpt):*

```text
[NODE_FUNCTION] lexeme: head
  (body)
    [NODE_MATCH]
      (cond)
        [NODE_IDENTIFIER] lexeme: list
      (body)
        [NODE_MATCH_ARM]
          [NODE_LIST_LITERAL]
            (args)
              [NODE_IDENTIFIER] lexeme: first
              [NODE_PLACEHOLDER] lexeme: rest
          [NODE_CALL] lexeme: Some
            (args)
              [NODE_IDENTIFIER] lexeme: first
        [NODE_MATCH_ARM]
          [NODE_PLACEHOLDER]
          [NODE_CALL] lexeme: None
```

The spread pattern `[first, ..rest]` is correctly represented as a `NODE_LIST_LITERAL` containing a regular identifier for `first` and a `NODE_PLACEHOLDER` with `lexeme: rest` for the rest binding. The wildcard `_` is a separate `NODE_PLACEHOLDER` without a lexeme.

== Pipelines and Placeholders (`13_pipelines.ennuyeux`)

This example demonstrates the Gleam-style `|>` pipeline operator with both positional and placeholder-based piping:

```rust
fn double(n: Int) { n * 2 };
fn add(a: Int, b: Int) { a + b };

fn main() {
    let result = 5 |> double() |> square() |> to_string();
    let adjusted = 10 |> add("_", 5);
    let complex = 3 |> double() |> add("_", 100) |> to_string();
}
```

*AST output (main function):*

```text
[NODE_FUNCTION] lexeme: main
  (body)
    [NODE_LET] lexeme: result
      [NODE_PIPELINE]
        [NODE_PIPELINE]
          [NODE_PIPELINE]
            [NODE_INT_LITERAL] val: 5
            [NODE_CALL] lexeme: double
          [NODE_CALL] lexeme: square
        [NODE_CALL] lexeme: to_string
    [NODE_LET] lexeme: adjusted
      [NODE_PIPELINE]
        [NODE_INT_LITERAL] val: 10
        [NODE_CALL] lexeme: add
          (args)
            [NODE_PLACEHOLDER]
            [NODE_INT_LITERAL] val: 5
```

The chained pipeline `5 |> double() |> square()` is represented as nested left-associative `NODE_PIPELINE` nodes. The placeholder in `add("_", 5)` produces a `NODE_PLACEHOLDER` argument, allowing the downstream semantic phase to determine where the piped value should be inserted.

== Recursive Fibonacci (`15_fibonacci.ennuyeux`)

This example tests the interplay of recursion, `Result` error handling, and nested pattern matching:

```rust
fn fibonacci(n: Int) {
    if n < 0 { return Err("Negative number not allowed"); };
    if n == 0 || n == 1 { return Ok(n); };

    match fibonacci(n - 1) {
        Ok(fib_n_1) => {
            match fibonacci(n - 2) {
                Ok(fib_n_2) => Ok(fib_n_1 + fib_n_2);
                err => err;
            }
        };
        err => err;
    }
};
```

*AST output (excerpt):*

```text
[NODE_FUNCTION] lexeme: fibonacci
  (body)
    [NODE_IF]
      (cond)
        [NODE_BINARY_OP] lexeme: <
          [NODE_IDENTIFIER] lexeme: n
          [NODE_INT_LITERAL] val: 0
      (body)
        [NODE_RETURN]
          [NODE_CALL] lexeme: Err
            (args)
              [NODE_STRING_LITERAL] lexeme: Negative number not allowed
    [NODE_IF]
      (cond)
        [NODE_BINARY_OP] lexeme: ||
          [NODE_BINARY_OP] lexeme: ==
            [NODE_IDENTIFIER] lexeme: n
            [NODE_INT_LITERAL] val: 0
          [NODE_BINARY_OP] lexeme: ==
            [NODE_IDENTIFIER] lexeme: n
            [NODE_INT_LITERAL] val: 1
      (body)
        [NODE_RETURN]
          [NODE_CALL] lexeme: Ok
            (args)
              [NODE_IDENTIFIER] lexeme: n
    [NODE_MATCH]
      (cond)
        [NODE_CALL] lexeme: fibonacci
          (args)
            [NODE_BINARY_OP] lexeme: -
              [NODE_IDENTIFIER] lexeme: n
              [NODE_INT_LITERAL] val: 1
```

This demonstrates how the AST faithfully preserves the full structure: early returns produce `NODE_RETURN` nodes, `Ok`/`Err` are represented as `NODE_CALL` nodes with their tag names (as they are regular identifiers representing enum variants from the standard library), and the nested match expressions maintain their hierarchical block structure.

== Semantic Analysis

After parsing, the semantic analyzer performs scope-based symbol resolution. Using `13_pipelines.ennuyeux` as an example:

```text
🪰 Starting Semantic Analysis...
Function: double
  Parameter: n : Int
Function: add
  Parameter: a : Int
  Parameter: b : Int
Function: square
  Parameter: n : Int
Function: to_string
  Parameter: n : Int
Function: main
Lambda expression:
  Parameter: a : Int
Defining variable: duplicate (inferred)
Lambda expression:
  Parameter: x : unknown
Defining variable: simple (inferred)
Defining variable: nums (inferred)
Lambda expression:
  Parameter: x : unknown
Defining variable: result (inferred)
Defining variable: adjusted (inferred)
Defining variable: complex (inferred)
```

Since all four helper functions are defined before `main`, the analyzer correctly resolves every call without warnings. This contrasts with earlier test runs where forward references produced warnings, highlighting the single-pass limitation.

== Error Reporting (`18_error_invalid_syntax.ennuyeux`)

The intentionally invalid file tests the diagnostic engine:

```rust
fn main() {
    let x = ;
}
```

*Output:*

```text
Syntax error: expected <expression> but got ';' at 4:13.
Parsing failed.
```

The parser identifies the unexpected `;` where an expression was expected after `=`, producing a clear diagnostic in the format `expected X but got Y at line N`. Additional error cases:

```text
$ echo 'fn foo() { let = 5; }' | ./ennuyeux_parser
Syntax error: expected IDENT but got '=' at 1:16.
Parsing failed.

$ echo 'fn 42() {}' | ./ennuyeux_parser
Syntax error: expected IDENT or '(' but got INT_LIT at 1:4.
Parsing failed.
```

When the set of expected tokens is small enough, Bison enumerates them. When there are too many valid alternatives (e.g. after `=`, any expression is valid), the parser falls back to a generic `<expression>` label. Both cases fulfill the requirement for precise, actionable diagnostic output.

== Anonymous Functions and Lambdas (`19_lambdas.ennuyeux`)

This example demonstrates the declaration and usage of anonymous functions:

```rust
let duplicate = fn (a: Int) { a * 2 };
let thing = 10 |> fn (a: Int) { a * 2 };
let simple = fn (x) { x + 1 };
```

*AST output (excerpt):*

```text
[NODE_LET] lexeme: duplicate
  [NODE_LAMBDA]
    (args)
      [NODE_PARAMETER] lexeme: a
        [NODE_IDENTIFIER] lexeme: Int
    (body)
      [NODE_BINARY_OP] lexeme: *
        [NODE_IDENTIFIER] lexeme: a
        [NODE_INT_LITERAL] val: 2
[NODE_LET] lexeme: thing
  [NODE_PIPELINE]
    [NODE_INT_LITERAL] val: 10
    [NODE_LAMBDA]
      (args)
        [NODE_PARAMETER] lexeme: a
          [NODE_IDENTIFIER] lexeme: Int
      (body)
        [NODE_BINARY_OP] lexeme: *
          [NODE_IDENTIFIER] lexeme: a
          [NODE_INT_LITERAL] val: 2
```

Lambdas are represented via `NODE_LAMBDA` nodes, which contain an optional list of parameters (`args`) and a block body (`body`). The flexibility of the grammar allows lambdas to be used anywhere an expression is valid, including variable assignments and pipeline stages.

