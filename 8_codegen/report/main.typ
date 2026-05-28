#import "template.typ": project, note, important

#show: project.with(
  title: "Semantic Analysis and Type checking",
  subtitle: "7th laboratory report",
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

- *Castillo Camacho, Violeta Ardeni*
  \ #link("redacted.com")

- *Flores Cóngora, Paolo Luis*
  \ #link("redacted.com")

== Repo URL

GitHub: #link("https://github.com/paoloose/unam-compilers/tree/main/7_type_checking")

= Design Questionnaire

== Symbol Management in C/C++

- *Function Interface: What would be the difference in the signature (arguments and return value) between your function to define a variable and your function to search/modify an existing one?*

  To define/modify a variable, we will need the symbol table, scope, variable name, and the actual value to set/modify.

  For defining however, we may receive the type of the variable, as the compiler is able to infer it. Furthermore, defining *mutates* the symbol table,
  while modifying only reads it.

  And as for modifying, the type is already known in the symbols table, so no need to specify it.

  #note[
    Now that the implementation is complete, these are the final types I ended up chosing:

    ```c
    SymbolTableEntry* find_symbol(
      const Scope* current_scope,
      const char* name
    );

    void add_symbol_unchecked(
      Scope* current_scope,
      const char* name,
      const ASTNode* type_node
    );
    ```
  ]

- *Memory Management: If you use structs or classes for the symbols, at what exact moment would you free the dynamic memory (e.g., calls to free or delete) of a local symbol table when exiting a block?*

  When existing a block, we will perform a `pop_scope()` operation, which will also free its symbol table.

== Scope Analysis

- *Shadowing Logic: If the user declares int x globally and float x locally, how would you algorithmically ensure that when referencing x inside the C/C++ block, the local version is obtained without deleting the global one from the table?*

  The scopes have a notion of "parent scope", in a stack-like structure, and because each scope will maintain its own symbols table,
  this problem is easily solved by:

  - always modifying only the nearest scope on declarations (shadowing);
  - start searching from the nearest scope to its parents when referencing variables

- *Redeclaration Protection: In the code { int a; float a; }, how would you detect that it is a semantic error? Describe what table operation you would perform before insertion.*

  Before inserting any new symbol in the symbols table, we will perform
  a search to see if it already exists in the current scope. If so,
  then we return a redeclaration error.

== Type Checking and AST Decoration

- *Node Structure: Describe what additional fields you would add to your AST structs/classes to store the "evaluated data type" once the node is processed.*

  The current AST node type is the following

  ```c
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
  ```

  And as expected, it doesn't have the concept of "evaluating to a type" yet.
  We may add an additional `SymbolTableEntry* evaluates_to_type` field, which may hold a reference
  to a type (builtin or user-defined) available in the symbols table.

  This implies that by design, user-defined types are also symbols.

  #note[I ended up changing this big struct to be a tagged union, so that we reduce
  the memory usage, and unsafety around accesing fields not mean to be
  accesed by the node type]

- *Operational Validation: For an operation like \*, how would you iterate over the children of the AST node in C/C++ to validate that both operands are numeric before assigning the resulting type to the parent node?*

  This is an straightforward task, that will consist of checking its `left/right` fields,
  verify the types they evaluate to (recursively), and ensure that both are numeric types
  before evaluating its parent.

== Explicit/Implicit Promotion Logic

- *Cast Nodes: When a valid promotion occurs (e.g., integer to float), would you inject a new "conversion node" (cast node) transparent in the AST or just update the type of the original node? Justify the computational advantages of your choice.*

  Ennuyeux does not have type promotion, though if we were to implement it, we would cast the
  node in-place.

  That is, if a node that evaluates to `float` is expected to be an `int` by its parent,
  then we would first ask "is this type=`float` able to be casted to this other type=`int`?",
  and if the compiler provides a mechanism to do so, either rouding or flooring/ceiling its value,
  we will execute it, and mofify its type if succesful.

  The main advantage is that this approach is simple, however, adding a conversion node would be
  better if our language already (or wants to) support type casting, making this a formalized
  process compiler operation supported by the language.

- *Active Loss Control: How would you detect and restrict in C/C++ the attempt to assign a float to an integer (int a = 3.14) if your language rules prohibit it?*

  If we were to maintain a set of permitted implicit casts, then `float->int` will not be part of
  it, and trying to do so will trigger a type mismatch error.

== Semantic Validation Engine

- *Traversal Strategy: Will you use standard nested recursion in C or opt to implement the Visitor pattern with classes in C++? Argue in terms of code scalability.*

  I'm not yet aware of the limitations regarding the user of recursion for this specifc use-case.

  However, because Ennuyeux programs can contains an indefinite set of nested operations,
  using a non-recursive Visitor approach would be better, to avoid potential stack overflows.

  In terms of our own code scalability, replacing recursive calls with an explicit stack/queue
  is a simple, one-time change that is not hard at all to maintain.

- *Error Containment: Instead of throwing an exit(1) at the first compilation error, what structure would you use in C/C++ (e.g., a global list of strings, or an array of logs) to accumulate and report multiple failures at the end of processing?*

  Interesting questions! I would like to explain this with a set of rules:

  - The compiler will report an error and their details using the DiagnosticError structure,
    containing severity, location in source code, and human readable message.

    ```c
    typedef struct {
        int severity;
        CodeLocation loc;
        char message[256];
    } Diagnostic;
    ```
  - Instead of rejecting the entire program, the compiler will skip checking the current
    instruction to the next one.
  - For each error the compiler reports, a global array of `DiagosticError`'s will be updated.
  - At the end of the program, all diagnostic errors will be reported to the user.

= Implementation

== AST Traversing

I opted to use a non recursive two-phase BFS traversing!

The traversing starts in the root node

```c
typedef enum {
    PHASE_ENTER,
    PHASE_MID,
    PHASE_EXIT
} VisitPhase;

typedef struct {
    ASTNode* node;
    VisitPhase phase;
} AnalyzeFrame;

void semantic_analyze(Scope* initial_scope, ASTNode* root) {
     Scope* current_scope = initial_scope;

    da_analyze_frames stack;
    da_analyze_frames_init(&stack, 128);
    // First node to iterate
    da_analyze_frames_append(&stack, (AnalyzeFrame){ root, PHASE_ENTER });

    AnalyzeFrame frame;

    while (da_analyze_frames_pop(&stack, &frame)) {
        ASTNode* node = frame.node;
        VisitPhase phase = frame.phase;
    }
}
```

The first phase, `PHASE_ENTER` is responsible for the traversing, that is, it piles more nodes
to traverse on top of the stack, that the current node will need.

In recursion, this is trivial:

```py
# Some pseudocode
def eval(node):
    if node is Add:
        left = eval(node.left)
        right = eval(node.right)
        # Left and right are evaluated at this point
        return left + right
```

But in the way we implemented this, it works by first piling the nodes in order, and then
evaluating them in the later `PHASE_EXIT`.

```c
while (da_analyze_frames_pop(&stack, &frame)) {
    ASTNode* node = frame.node;
    VisitPhase phase = frame.phase;

    switch (node->type) {
      case NODE_BINARY_OP: {
          if (phase == PHASE_ENTER) {
              // First schedule the child nodes
              da_analyze_frames_append(&stack, { node, PHASE_EXIT });
              da_analyze_frames_append(&stack, { node->as.binop.right, PHASE_ENTER });
              da_analyze_frames_append(&stack, { node->as.binop.left, PHASE_ENTER });
          } else if (phase == PHASE_EXIT) {
              // Evaluate the thing
          }
    }
}
```

Later we realized some instructions require an additional `PHASE_MID`! For example, look at an
Ennuyeux foreach:

```rust
let nums = [1, 2, 3, 4, 5];

// Name convention: "x" = binded_term, "nums" = iterator
for x in nums {
  print(x);
}
```

We need the three phases for:

- `PHASE_ENTER`: creating the scope, and scheduling the iterator
- `PHASE_MID`: only once the iterator evalutes, we bind the "x" to be the type of the iterator,
  and now we analyze the foreach body (`then`), and its potential `else` branch
- `PHASE_EXIT`: and finally, we type check that the `then` and `else` type matches, and set
  the type that the `for` evaluates to.

== Scope management

To handle scopes, we maintain a linked list of `Scope`'s instances. Each scope consist
of a set of symbols defined in that scope. Is up to the program logic to design when
is valid to define a symbol.

```c
// Symbol table entry
typedef struct Symbol {
    char* name;
    ASTNode* type_node; // type information
    struct Symbol* next;
} Symbol;

// Scope that we can push and pop from
typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;
```

We push a new scope for every new context we get into, including functions, if/loop/for statements,
or normal, empty scopes `{}`.

```c
Scope* current_scope = NULL;

push_scope(&current_scope);
// Analyze the body
pop_scope(&current_scope);
```

#note[
  Theoretically, `pop_scope` is responsible for freeing up the memory allocated by the nodes,
  but this is currently limited to symbols allocated in this scope.

  ```c
  SymbolTableEntry* sym = scope->symbols;
  while (sym) {
      SymbolTableEntry* next = sym->next;
      if (sym->name) free((char*)sym->name);
      free(sym);
      sym = next;
  }
  ```

  I suspect there is leaking data because of this limitation, as valgrind reports.
]

== Symbol Management

The compiler manages identifiers through a set of specialized functions that handle scoping,
shadowing, and type checking.

=== Hierarchical Lookup (`find_symbol`)

This function implements the core scoping rule: "look in the current scope, then move to the parent." It also handles built-in types (int, float, bool, string, List) by checking them first.

```c
SymbolTableEntry* find_symbol(Scope* current_scope, const char* name) {
    // "harcoded" builtins
    if (strcmp(name, "int")    == 0) return get_int_symbol();
    if (strcmp(name, "float")  == 0) return get_float_symbol();
    // ...

    // And user-defined types
    Scope* s = current_scope;
    while (s) {
        SymbolTableEntry* sym = s->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        s = s->parent; // Traverse up
    }
    return NULL;
}
```

=== Symbol Insertion Variants

We provide three levels of symbol insertion to handle different semantic requirements:

- *`add_symbol_unchecked`*: It allocates a `SymbolTableEntry` without checking th scope. Used
  when we are absolutely sure that it won't cause a conflict and want to save some instructions.
- *`add_symbol_unshadowed`*: Used for top-level declarations (functions, structs) where name
  collision is forbidden.
- *`add_symbol_shadowed`*: Used for local variables. It prevents redeclaration in the *same* scope but allows overriding names from outer scopes.

```c
void add_symbol_unchecked(Scope* current_scope, const char* name, ASTNode* type_node) {
    SymbolTableEntry* sym = calloc(1, sizeof(SymbolTableEntry));
    sym->name = ast_strdup(name);
    sym->node = type_node;
    sym->depth = current_scope->depth;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;
}
```

== Contextual Analysis (`find_nearest`)

Because our analyzer is non-recursive, we maintain a `contexts_stack` (a stack of `ASTNode*`) that tracks the path from current node to the root:

- `find_nearest_function`: Finds the current function context, used to validate `return` types.
- `find_nearest_breakable_context`: Locates the nearest `for`, `foreach`, or `loop` to validate `break` statements.

```c
ASTNode* find_nearest_function(const da_astnodes* contexts_stack) {
    size_t i = contexts_stack->length;
    while (i-- > 0) {
        ASTNode* entry = contexts_stack->data[i];
        if (entry->type == NODE_FUNCTION) return entry;
    }
    return NULL;
}
```

#note[
  Remember that in Ennuyeux, `break` serves as a return for loops! Classic example:

  ```rust
  let nums = [1, 2, 3, 4, 5];

  let three_exists = for x in nums {
      if x == 3 { break true; }
  }
  else { break false; };
  ```
]

== Type System & Inference

Ennuyeux and strongly and statically typed language, with support for generics.

Our type system is conceptually divided in two kind of types:

- *Plain types*: `int`, `float`, `bool`, `string`. Their node type is se to `NODE_PLAIN_TYPE`.
- *User-defined types*: which types declared using `struct`'s or `enum`'s declarations.

What about generic types?

== Generics

Generics are initially set to `NODE_PLAIN_TYPE` which is a wrong assumption, but the only choice,
as it's not feasible for static analysis to decide which types are generics and which are not:

```rs
fn example<T>(p: List<T>) {
    let: T = p[0]; // how to know that `T` is generic in static analysis?
}
```

So later in the semantic evaluation, this nodes have their `is_generic` set to true.

We can proudly say that this example compiles:

```rs
fn head<T>(list: List<T>) -> Option<T> {
    match (list) {
        [first, ..rest] -> Option::Some(first);
        _ -> Option::None;
    }
}

fn main() {
    let arr = [[1], [2], [3]]; // type List<List<int>>
    let first: List<int> = head(arr);
}
```

=== Generic Compatibility (`types_match`)

The compiler implements a sophisticated type matching algorithm that handles nested generic types and polymorphic specialization.

```c
bool types_match(ASTNode* expected, ASTNode* actual, ASTNode* decl_gen, ASTNode* lit_gen) {
    // 1. Resolve Generic Specialization
    if (expected->type == NODE_PLAIN_TYPE) {
        // ... Match placeholder T to concrete type ...
    }
    // 2. Recursive Comparison for Nested Types
    // e.g., Comparing List<int> against List<float>
    while (eg && ag) {
        if (!types_match(eg, ag, decl_gen, lit_gen)) return false;
        eg = eg->next; ag = ag->next;
    }
}
```

For now, this is exclusively used to analyze calls for generic (and non generic) functions,
so that we now if the passed argument matches the parameter type.

// TODO: add reference to code

=== Type Inference Logic

Type inference is implemented during the `PHASE_EXIT` of a `NODE_LET`. The right-hand expression is analyzed first, and its evaluated type is then bound to the identifier.

```c
// analyzer.c: NODE_LET Exit Phase
ASTNode* inferred_type = let_value->evaluates_to_type;
if (declared_type) {
    if (!types_are_equal(declared_type, inferred_type)) {
        report_error(node, "Type mismatch");
    }
}
// The variable is bound to the inferred type
add_symbol_unshadowed(current_scope, let_name, inferred_type);
```

== Promoting types

Ennuyeux was designed as an strongly typed language from the start, so I was not planning
of adding promotions, though for the sake of this laboratory, we have promotion for numeric operations.

- `int` + `float` -> `float`
- `int` \* `float` -> `float`

And the logic is as easy as just mutate the node returnint type.

```c
bool left_is_int    = strcmp(type_left_name, "int") == 0;
bool left_is_float  = strcmp(type_left_name, "float") == 0;
bool right_is_int   = strcmp(type_right_name, "int") == 0;
bool right_is_float = strcmp(type_right_name, "float") == 0;

bool is_int_sum = left_is_int && right_is_int;
bool is_float_sum =    (left_is_float && right_is_float)
                    || (left_is_int   && right_is_float)
                    || (left_is_float && right_is_int);

if (is_int_sum) {
    node->evaluates_to_type = find_symbol(current_scope, "int")->node;
} else if (is_float_sum) {
    // And we promote this binary operator to a float
    node->evaluates_to_type = find_symbol(current_scope, "float")->node;
} else {
    report_error(
      node, "Trying to add non-numeric types: '%s' and '%s'",
      type_left_name, type_right_name
    );
}
```

== Validation Engine

The validation mechanisms consist of reporting errors in the order they happen. When reporting
an error, we usually keep parsing the rest of the block.

However, if we consider that the error may affect future instructions, then we abort the block.

These are the structs used for diagnostics:

```c
typedef enum {
    SEVERITY_ERROR,
    SEVERITY_WARNING
} Severity;

typedef struct {
    SourceLoc loc;
    char* message;
    Severity severity;
} SemanticDiagnostic;
```

And the funcitons to populate the diagnostics, which is currently a global array:

```c
da_diagnostics diagnostics;

static void report_diagnostic(
    ASTNode* node,
    Severity severity,
    const char* format,
    va_list args
) { ... }

void report_error(ASTNode* node, const char* format, ...) {
    va_list args;
    va_start(args, format);
    report_diagnostic(node, SEVERITY_ERROR, format, args);
    va_end(args);
}

void report_warning(ASTNode* node, const char* format, ...) {
    va_list args;
    va_start(args, format);
    report_diagnostic(node, SEVERITY_WARNING, format, args);
    va_end(args);
}
```

And both warnings and errors are reported by the compiler:

```rs
fn main() {
  let foo = "unused";
  let bar: int = 10.0;
}
```

```
Warnings:
  [5:1] Symbol 'foo' is defined, but never used
  [4:2] Symbol 'work' is defined, but never used

Errors:
  [3:23] Cannot bind variable 'bar', expected type 'int', got 'float'
```

= Extras

== Dockerization (+1.0)

The following command builds and runs the Ennuyeux compiler against the entire test suite.

```sh
docker compose up --build
```

== Type Inference (+1.5)

The compiler supports type inference for let statements:

```rs
let a = 10; // trivial

let arr = [[[3]]]; // List<List<List<int>>>

let first: List<List<int>> = arr[0]; // OK
let second: List<int> = first[0];    // OK
let third: int = second[0];          // OK
```

As well as functions! Their return value can be both explicit or inferred:

```rs
// Both are the same function
fn double(n: int) { n * 2 }
fn double(n: int) -> int { n * 2 }
```

And they can even infer nested expressions. For example, in order to infer the return value for
`generate_adder`, it must first infer the lambda's return value.

```rs
fn generate_adder(to_add: int) {
    fn (n: int) { n + to_add }
}

// Equivalent to
fn generate_add(to_add: int) -> (int) -> int {
    fn (n: int) -> int { n + to_add }
}
```

The logic behind this feature is surprisingly easy. For `let`'s, we first evaluate the right-hand-side
of the statement, and once finished (may need to evaluate recursive structures), the resulting type
is set to the right-hand-side.

Here is this logic handled by our two-phase BFS traversing:

```c
case NODE_LET: {
    ASTNode* left_side = node->as.let.declared_type; // explicit typing (optional)
    ASTNode* right_side = node->as.let.value;        // the value

    if (phase == PHASE_ENTER) {
        da_analyze_frames_append(&stack, (AnalyzeFrame){ node, PHASE_EXIT });
        da_analyze_frames_append(&stack, (AnalyzeFrame){ right_side, PHASE_ENTER });
    } else if (phase == PHASE_EXIT) {
        ASTNode* evaluated_type_node_right = right_side->evaluates_to_type;

        // If explicitely type, we check if matches the right-hand-side
        if (left_side) {
            if (!types_are_equal(left_side, evaluated_type_node_right)) {
                // report error
                break;
            }
        }

        new_node->evaluates_to_type = evaluated_type_node_right;
    }
    break;
};

```

== Warnings (+1.0)

Warnings are emitted for logically questionable but syntactically valid code.

One of the most notable examples is unused bindings

```rust
fn work() {
  let f = fn () { 10 };
}
```

We got the warnings:

```text
Warnings:
  [4:1] Symbol 'f' is defined, but never used
  [3:2] Symbol 'work' is defined, but never used
```

This is implemented by maintaing a `SymbolTableEntry.referenced_count` integer field, that ins
incremented everytime that symbol appears in the tree when traversing. If we pop the scope
and this count is 0, we know it hasn't been used in the code.

```c
static void pop_scope(Scope** current_scope) {
    // ...
    SymbolTableEntry* symbol = current_scope->symbols;

    while (symbol) {
        // Before exiting the scope, let's see what symbols were never referenced
        if (symbol->referenced_count == 0) {
            report_warning(symbol->node,
              "Symbol '%s' is defined, but never used", node_repr(symbol->node)
            );
        }
        symbol = symbol->next;
    }
    // ...
}
```

= Results

The parser now successfully analyzes and enforces semantic rules, strong typing, and static typing.
It can handle complex nested evaluations without recursion, by using a two-phase BFS traversing.

Ennuyeux currently supports beautiful and complex expressions, and is able to parse and verify them
semantically:

```rust
fn twice<T>(x: (T)->T) -> (T)->T {
    fn (val: T) { x(x(val)) }
}

fn main() {
    let add1 = fn (v: int) { v + 1 };
    twice(add1)(0); // 2
}
```

```text
[NODE_PROGRAM] lexeme: __main__
  [NODE_FUNCTION] lexeme: twice returns: (T)->T
    [NODE_FUNC_PARAMETER] lexeme: x
      [NODE_SIGNATURE_TYPE] lexeme: (T)->T
    [NODE_PLAIN_TYPE] lexeme: T
    [NODE_SCOPE] lexeme: { scope }
      [NODE_RETURN] lexeme: <node 9>
        [NODE_FUNCTION] lexeme: <lambda>
          [NODE_FUNC_PARAMETER] lexeme: val
            [NODE_PLAIN_TYPE] lexeme: T
          [NODE_SCOPE] lexeme: { scope }
            [NODE_RETURN] lexeme: <node 9>
              [NODE_CALL] lexeme: x
                [NODE_IDENTIFIER] lexeme: x
                [NODE_CALL] lexeme: x
                  [NODE_IDENTIFIER] lexeme: x
                  [NODE_IDENTIFIER] lexeme: val
  [NODE_FUNCTION] lexeme: main
    [NODE_SCOPE] lexeme: { scope }
      [NODE_LET] lexeme: add1
        [NODE_FUNCTION] lexeme: <lambda>
          [NODE_FUNC_PARAMETER] lexeme: v
            [NODE_PLAIN_TYPE] lexeme: int
          [NODE_SCOPE] lexeme: { scope }
            [NODE_RETURN] lexeme: <node 9>
              [NODE_BINARY_OP] lexeme: +
                [NODE_IDENTIFIER] lexeme: v
                [NODE_INT_LITERAL] lexeme: 1
      [NODE_CALL] lexeme: <dynamic>
        [NODE_CALL] lexeme: twice
          [NODE_IDENTIFIER] lexeme: twice
          [NODE_IDENTIFIER] lexeme: add1
        [NODE_INT_LITERAL] lexeme: 0
```

The project includes an automated test runner, `run_tests.sh`, that validates 34 distinct scenarios, including edge cases for shadowing and generic recursion.

```sh
# Run all tests
./run_tests.sh
```

```text
🪰 Running Test Suite

✅ [arithmetic/basic.ennuyeux        ] PASS
✅ [bitwise/basic.ennuyeux           ] PASS
✅ [comparison/basic.ennuyeux        ] PASS
✅ [enums/enum_basic.ennuyeux        ] PASS
✅ [enums/result_option.ennuyeux     ] PASS
✅ [for/c_style.ennuyeux             ] PASS
✅ [foreach/list_iteration.ennuyeux  ] PASS
✅ [for/for_break_value.ennuyeux     ] PASS
✅ [for/for_scope.ennuyeux           ] PASS
✅ [for/range_exclusive.ennuyeux     ] PASS
✅ [functions/basic.ennuyeux         ] PASS
✅ [functions/recursive.ennuyeux     ] PASS
✅ [functions/twice.ennuyeux         ] PASS
✅ [if/if_expression.ennuyeux        ] PASS
✅ [if/if_type_mismatch.ennuyeux     ] PASS (Expected Failure)
✅ [invalid/missing_brace.ennuyeux   ] PASS (Expected Failure)
✅ [invalid/placeholder.ennuyeux     ] PASS (Expected Failure)
✅ [invalid/struct_literal.ennuyeux  ] PASS (Expected Failure)
✅ [invalid/unclosed_string.ennuyeux ] PASS (Expected Failure)
✅ [lambdas/basic.ennuyeux           ] PASS
✅ [lambdas/dynamic_call.ennuyeux    ] PASS
✅ [let/decl_and_assign.ennuyeux     ] PASS
✅ [let/placeholders.ennuyeux        ] PASS
✅ [let/shadowing_same_scope.ennuyeux] PASS
✅ [let/type_mismatch.ennuyeux       ] PASS (Expected Failure)
✅ [lists/basic.ennuyeux             ] PASS
✅ [logic/basic.ennuyeux             ] PASS
✅ [match/guards.ennuyeux            ] PASS
✅ [match/list_patterns.ennuyeux     ] PASS
✅ [match/match_basic.ennuyeux       ] PASS
✅ [member_access/basic.ennuyeux     ] PASS
✅ [pipelines/basic.ennuyeux         ] PASS
✅ [pipelines/pipe_simple.ennuyeux   ] PASS
✅ [scopes/block_return.ennuyeux     ] PASS
✅ [structs/error_redeclared.ennuyeux] PASS (Expected Failure)
✅ [structs/struct_basic.ennuyeux    ] PASS

🪰 Got 36 / 36 passed
All tests passed successfully 🪰🪰
```

We currently maintain #link("https://github.com/paoloose/unam-compilers/tree/main/7_type_checking/tests")[36 examples] showcasing different features of Ennuyeux. You can find them, and the complete code of
the analyzer in our #link("https://github.com/paoloose/unam-compilers/tree/main/7_type_checking")[repository].

---

-- Thanks!
