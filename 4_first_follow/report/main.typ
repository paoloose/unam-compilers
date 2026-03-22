#import "template.typ": project

#show: project.with(
  title: "FIRST and FOLLOW sets",
  subtitle: "4th laboratory report",
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
  branch: "National Autonomous University of Mexico, Computer Science: Compilers",
)

= Video links

*Castillo Camacho, Violeta Ardeni* (link)
\

*Flores Cóngora, Paolo Luis* (vino presencialmente)

#image("pass_card_paolo.png", width: 200pt)

= Introduction

Building on the previous practice, where we implemented a lexical analyzer capable
of producing a token stream from source code, this assignment focuses on the next
logical step toward constructing a compiler: understanding the structure of the
language through its grammar.

The goal of this practice is to implement the *FIRST* and *FOLLOW* set algorithms for
a given context-free grammar (CFG). These sets are the cornerstone of *LL(1) predictive
parsing*, the top-down parsing strategy where, at each step, the parser decides which
production to apply by looking at only the next input token.

The program receives a grammar definition through `stdin`, parses it into an in-memory
representation, and prints the FIRST and FOLLOW sets for every non-terminal.

== Problem Statement

Given an input grammar in a simple textual format, the program must:

1. Parse the grammar into a structured in-memory representation with non-terminals,
   terminals, and productions.
2. Compute the *FIRST* set for every non-terminal, correctly propagating $epsilon$ through
   chains of nullable symbols.
3. Compute the *FOLLOW* set for every non-terminal, applying all three standard closure
   rules and placing `$` in the FOLLOW set of the start symbol.
4. Print results in the format `FIRST(X): {a, b, ...}` and `FOLLOW(X): {c, $, ...}`
   for each non-terminal, in declaration order.

= Project structure

```
4_first_follow
 ├─ grammar.h / grammar.c
 |    Data structures and grammar parser: converts the raw textual input
 |    into the internal grammar representation (symbol arrays, production
 |    tables, and open-addressed hash tables for O(1) symbol lookup)
 ├─ analyzer.h / analyzer.c
 |    The FIRST/FOLLOW computation engine: implements fixed-point iteration
 |    for FIRST sets and rule-based propagation for FOLLOW sets
 ├─ main.c
 |    Entry point: reads the grammar from stdin, drives the computation,
 |    and prints all FIRST/FOLLOW sets
 ├─ CMakeLists.txt
 ├─ Dockerfile
 ├─ docker-compose.yml
 └─ validator
```

= Implementation

== Grammar representation

The grammar is defined in a simple line-based format:

```
Non-terminals: E T F
Terminals: + * ( ) id
E -> E + T
E -> T
...
```

The first two lines declare the symbol sets, and subsequent lines define list productions.
The `create_grammar` function in `grammar.c` parses this text and fills a `grammar` struct:

```c
typedef struct grammar {
    symbol*      non_terminals;
    symbol*      terminals;
    production*  productions;
    symbol_hash_table non_terminal_index;
    symbol_hash_table terminal_index;
    int num_non_terminals, num_terminals, num_productions;
} grammar;
```

Productions are stored in an encoded form: each RHS (right hand side) symbol is
an integer where indices `[0, T)` denote terminals and indices `[T, T+N)`
denote non-terminals (offset by the terminal count). This encoding makes it
trivial to distinguish symbol kinds during analysis without extra pointer lookups.

Symbol lookup uses a custom open-address hash table (djb2, linear probing). During
production parsing, each token is resolved to its integer id through this table:

```c
int symbol_id = get_symbol_id_from_hash(token, &g->terminal_index);
if (symbol_id != -1) {
    production_symbol_ids[production_length++] = symbol_id;
} else {
    symbol_id = get_symbol_id_from_hash(token, &g->non_terminal_index);
    if (symbol_id != -1)
        production_symbol_ids[production_length++] = g->num_terminals + symbol_id;
}
```

Terminals map directly to their index; non-terminals are offset by `num_terminals`.
This is the encoding that `analyzer.c` relies on when decoding production bodies.

== FIRST set algorithm

Our implementation computes the FIRST set using an iterative approach:

1. Initialize the FIRST sets and nullable flags to empty and false, respectively.
2. Iterate through all productions in the grammar.
3. For each production $A arrow.r X_1 X_2 dots X_k$, propagate the FIRST set of $X_i$ to the FIRST set of $A$.
4. Stop propagating if $X_i$ is not nullable. If all symbols $X_i$ are nullable, mark $A$ as nullable.
5. Repeat steps 2 through 4 until no sets or flags change during a complete pass.

Note: to "propagate" means to add the elements of one set to another set.

=== Table allocation

`compute_first_tables` allocates two structures: a
flattened `N × T` boolean matrix for the FIRST sets, and a `N`-length array for
nullable flags. Using `calloc` guarantees everything starts as `false`:

```c
bool *ft = calloc((N * T), sizeof(bool));
bool *nl = calloc(N, sizeof(bool));
```

The epsilon terminal id is resolved once up-front, since we need to skip it when
propagating FIRST entries:

```c
int eps = find_terminal_id(g, "epsilon");
*epsilon_id = eps;
```

=== Fixed-point loop

The main loop runs until a full pass over all productions changes nothing.
For each production it walks the RHS (right hand side) from left to right:

```c
bool changed = true;
while (changed) {
    changed = false;
    for each production (A -> X1 X2 ... Xk) {
        bool all_nullable = true;
        for each Xi in RHS {
            if Xi is a terminal {
                add Xi to FIRST(A);
                all_nullable = false;
                break;
            } else {
                // non terminal, propagate FIRST(Xi) to FIRST(A)
                merge FIRST(Xi) - {ε} into FIRST(A);
                if Xi is not nullable {
                    all_nullable = false;
                    break;
                }
            }
        }
        if (all_nullable) {
            nullable[A] = true;
        }
    }
}
```

The key correctness property is the nullable propagation: when iterating over
the RHS of `A -> B C D`, if B is nullable we continue onto C, if C is also
nullable we continue onto D, and if all three are nullable then A itself
becomes nullable. This handles chains of any depth.

The `epsilon` terminal (when present in the grammar) is treated as a regular
terminal in the input but *excluded* from FIRST rows and instead reflected
exclusively through the `nullable` flag, keeping the table structure uniform.

== FOLLOW set algorithm

The FOLLOW algorithm computes the set of terminals that can appear immediately to the right of a non-terminal. It uses a similar iterative strategy:

1. Initialize the FOLLOW sets to empty.
2. Add the end-of-input marker `$` (EOF) to the FOLLOW set of the start symbol.
3. Iterate through all productions. For each production $A arrow.r alpha B beta$, add the FIRST set of $beta$ (excluding $epsilon$) to the FOLLOW set of $B$.
4. If $beta$ can derive $epsilon$ (or if $beta$ is empty), add the FOLLOW set of $A$ to the FOLLOW set of $B$.
5. Repeat steps 3 and 4 until no FOLLOW sets change during a complete pass.

=== Table allocation and Rule 1

The FOLLOW table extends the FIRST table by one column: `follow_table[N × (T+1)]`,
where column index `T` represents the `$` EOF marker.

Three closure rules are applied iteratively:

- *Rule 1*: `$` is placed in FOLLOW of the start symbol (index 0) unconditionally before the first iteration.

- *Rule 2*: For every production $A arrow.r alpha B beta$: all terminals in FIRST($beta$) - {$epsilon$} are added to FOLLOW(B).

- *Rule 3*: For every production $A arrow.r alpha B$ (or when $beta arrow.r^* epsilon$): FOLLOW(A) is added into FOLLOW(B).

```c
// Rule 1
follow_table[0][dollar_col] = true;

// Rules 2 & 3
for each production (A -> ... B ...) {
    bool beta_nullable = true;
    for each symbol after B (the "beta" suffix) {
        if beta symbol is terminal t { add t to FOLLOW(B); break; }
        else {
            merge FIRST(beta_nt) \ {ε} into FOLLOW(B);
            if beta_nt is not nullable { beta_nullable = false; break; }
        }
    }
    if (beta_nullable) merge FOLLOW(A) into FOLLOW(B);
}
```

The fixed-point loop repeats until no FOLLOW entry changes across a full pass
over all productions, guaranteeing convergence for any acyclic or left-recursive
grammar.

== Collecting results

Once the tables are stable, `collect_first_for_non_terminal` and
`collect_follow_for_non_terminal` read the relevant row from each table and
build a dynamically allocated `symbol[]` array for the caller. The helper
`add_symbol_to_array` handles `realloc` growth and `strdup` copies so the
caller owns the result independently of the internal tables:

```c
static bool add_symbol_to_array(symbol **arr, int *count,
                                const char *text, bool is_terminal) {
    symbol *resized = realloc(*arr, (*count + 1) * sizeof(symbol));
    resized[*count].symbol = strdup(text);
    resized[*count].is_terminal = is_terminal;
    *arr = resized;
    (*count)++;
    return true;
}
```

== Output

Running with the classic arithmetic expression grammar:

```bash
printf "Non-terminals: E T F\nTerminals: + * ( ) id\nE -> E + T\nE -> T\nT -> T * F\nT -> F\nF -> ( E )\nF -> id\n" | ./build/first_and_follow
```

```
FIRST(E): {(, id}
FOLLOW(E): {+, ), $}
FIRST(T): {(, id}
FOLLOW(T): {+, *, ), $}
FIRST(F): {(, id}
FOLLOW(F): {+, *, ), $}
```

These results match the standard textbook derivations for this grammar.

To validate the 5 tests written for this assignment, run the Docker compose file:

```bash
docker compose up --build
```

#image("./tests.png")

= Difficulties

The most subtle case was the interaction between the `epsilon` terminal and the
`nullable` flag. Initially, treating `epsilon` as a plain terminal inside FIRST
rows caused it to appear in FOLLOW sets indirectly. The fix was to exclude
`epsilon` from FIRST row population entirely and instead surface it only
through the `nullable` flag during collection, so the two concerns remain cleanly
separated.

A secondary issue was the encoding of symbols as plain integers: because
non-terminals are stored offset by `num_terminals`, any change to the terminal
list invalidates all production id arrays. This is safe as long as the grammar
is immutable after construction, but it means the encoding assumption must be
respected in every function that inspects production bodies.

--

And that's it :)

Thank you!
