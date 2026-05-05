#import "template.typ": project

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

- *Castillo Camacho, Violeta Ardeni* (link)
- *Flores Cóngora, Paolo Luis* (link)

== Repo URL

#link("https://github.com/paoloose/unam-compilers/tree/main/7_type_checking")

= Design Questionnaire

== Symbol Management in C/C++

- *Function Interface: What would be the difference in the signature (arguments and return value) between your function to define a variable and your function to search/modify an existing one?*

  To define/modify a variable, we will need the symbol table, scope, variable name, and the actual value to set/modify.

  For defining however, we may receive the type of the variable, as the compiler is able to infer it. Furthermore, defining *mutates* the symbol table,
  while modifying only reads it.

  And as for modifying, the type is already known in the symbols table, so no need to specify it.

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

== Symbols table

The symbol table is implemented using a linked list structure `SymbolTableEntry` that stores the symbol name and a reference to its AST type information via the `type_node` field. This allows the compiler to handle both variables and user-defined types (like structs) uniformly.

```c
typedef struct SymbolTableEntry {
    char* name;
    ASTNode* type_node; // type information
    struct SymbolTableEntry* next;
} SymbolTableEntry;
```

It supports lookup operations like `find_symbol`, which recursively traverses the scope chain, and `add_symbol_unshadowed`, which prevents duplicate declarations within the same scope.

== Scope analysis

Scopes are managed via a stack-like structure of `Scope` objects. The `push_scope` and `pop_scope` functions manipulate a pointer to the current active scope.

```c
typedef struct Scope {
    SymbolTableEntry* symbols;
    struct Scope* parent;
} Scope;
```

When a function, block, or struct declaration is entered, `push_scope` is invoked, creating a new environment. When exiting, `pop_scope` correctly restores the parent scope. Variable lookups start from the innermost scope and traverse upward to the global scope, naturally supporting shadowing.

== Type checking

Type checking is performed by traversing the AST via the `analyze_node` function. As expressions are evaluated, their resulting types are assigned to the `evaluates_to_type` property of their AST nodes.

For operations like function calls, the arguments passed are verified against the expected parameters defined in the symbol table using the `types_are_equal` helper:

```c
bool types_are_equal(ASTNode* type1, ASTNode* type2) {
    if (!type1 && !type2) return true;
    if (!type1 || !type2) return false;

    // First check lexemes
    if (type1->lexeme && type2->lexeme && strcmp(type1->lexeme, type2->lexeme) != 0) {
        return false;
    }

    // Then recursively check generic args
    // ...
}
```

This logic accurately checks base types as well as structured/generic types.

== Casting logic

The current implementation of Ennuyeux prioritizes strict type safety, so implicit type promotion (such as casting `int` to `float` automatically) is not natively executed during AST evaluation. The compiler enforces exact type matches across assignments and operations.

If casting logic were to be incorporated, it would be implemented via a type hierarchy evaluation during node analysis, injecting explicit cast operations into the AST when a safe promotion (e.g., `int` -> `float`) is detected, or throwing a semantic error otherwise.

== Semantic validation

The validation engine collects multiple errors without crashing immediately. This is achieved using a dynamic string array, `da_cstr semantic_errors`, to accumulate faults.

Errors are reported via `report_semantic_error`, which formats the error message with precise line and column information from the AST:

```c
void report_semantic_error(ASTNode* node, const char* format, ...) {
    // ... formatting logic ...
    char* final_message;
    if (node) {
        int len = snprintf(NULL, 0, "[%d:%d] %s", node->line, node->col, message);
        final_message = malloc(len + 1);
        snprintf(final_message, len + 1, "[%d:%d] %s", node->line, node->col, message);
    }
    da_cstr_append(&semantic_errors, final_message);
}
```

At the end of the analysis, `analyze_semantics` determines if the validation was successful by checking the length of the error array, and prints all gathered errors neatly for the developer.

= Results

The semantic analyzer successfully traverses the AST and correctly enforces scoping and typing rules across several Ennuyeux language constructs. The analyzer correctly reports scoping issues (like redeclarations and undefined symbols) and identifies type mismatches in function calls, return statements, and struct field definitions.

Valid programs complete the semantic pass returning a populated and checked AST, whereas invalid programs correctly accumulate precise error messages pointing directly to the faulty lines and columns.

= Extras

== Dockerfile (+1)

A Dockerfile is provided in the repository root to containerize the compiler environment and quickly build and test the parser and semantic validation engine. The Dockerfile leverages a minimal environment with GCC, Flex, and Bison to execute the project independently of local setups.

```sh
docker build -t ennuyeux-tests .
docker run --rm ennuyeux-tests
```

== Type inference (+1.5)

Type inference is partially supported during variable declarations (`let`). If a variable is declared without an explicit type, the compiler attempts to infer the type from the right-hand side expression that is being assigned.

```c
        case NODE_LET: {
            // First we evaluate what we got at the right side
            analyze_node(current_scope, node->right);

            if (node->left) {
                // Explicitly typed
                add_symbol_unshadowed(current_scope, node->lexeme, node->left);
            } else {
                // Inferred from right side
                add_symbol_unshadowed(current_scope, node->lexeme, node->right->evaluates_to_type);
            }
            break;
        }
```

== Warnings (+1)

Detection for unused variables is currently not implemented in the final version of the code, but the architecture allows for it cleanly. It would merely require adding an `is_used` boolean flag to the `SymbolTableEntry` struct, setting it to `true` on lookup, and emitting a diagnostic warning for any symbol with `is_used == false` right before a scope is popped and destroyed.
