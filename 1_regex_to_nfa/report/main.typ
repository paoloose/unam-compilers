#import "template.typ": project

#show: project.with(
  title: "From regex to NFA",
  subtitle: "1st laboratory report",
  authors: (
    "Flores Cóngora, Paolo Luis",
  ),
  mentors: (
    "Adrián Martínez Manzo",
  ),
  footer-text: "UNAM - Compilers",
  lang: "en",
  school-logo: image("../../assets/imgs/unam_logo.png", width: 50pt),
  branch: "National Autonomous University of Mexico, Computer Science: Compilers",
)

= Introduction and Validation

The goal of this assignment is to create a compiler component that converts regular expressions
into their equivalent non-deterministic finite automata (NFA) using Thompson's construction algorithm.

== Video URL

#underline(link("https://drive.google.com/file/d/16dRq8qyAKckhLyOCZk7d4TMYWeVB34Mt/view"))

== Problem Statement

Given a regular expression as input, the program must:

1. Parse the expression and convert it to postfix notation
2. Construct an NFA using Thompson's algorithm
3. Simulate the NFA to determine whether multiple input strings are accepted

== Supported Operators

The implementation supports the following regular expression operators:

- `|` (alternation): matches either left or right operand
- `.` (concatenation): explicit operator for sequential matching
- `*` (Kleene star): zero or more repetitions
- `+` (positive closure): one or more repetitions
- `?` (optional): zero or one occurrence

== Passing Validation

The implementation successfully passes all validation tests.

#image("assets/validator_output.png", alt: "Validator test execution showing successful compilation and test results", width: 90%)

= Implementation Description

== Data Structures

=== NFA Representation

The NFA is represented using a state-transition model:

```c
typedef struct {
    int id;
    transition trans[MAX_TRANSITIONS];
    int trans_count;
    bool is_accept;
} nfa_node;

typedef struct {
    char on;
    nfa_node* target;
} transition;
```

Each state maintains a fixed-size array of transitions (`MAX_TRANSITIONS = 2`), which is sufficient because:
- Non-epsilon transitions occur only for specific input characters
- Epsilon transitions are limited to structural composition points in Thompson's algorithm
- This design avoids dynamic allocation overhead for common cases

The epsilon transition is represented using a special sentinel value: `#define TRANSITION_EPSILON '\0'`

=== NFA Fragment Abstraction

Thompson's algorithm operates on fragments during construction:

```c
typedef struct {
    nfa_node* start;
    nfa_node* accept;
} nfa_fragment;
```

So each fragment represents a partial NFA with a designated entry and exit point (accepted state), enabling modular composition of larger automata.

=== Dynamic Arrays

A generic macro-based dynamic array implementation provides type-safe collections without code duplication:

```c
#define create_da(TYPE, NAME) \
    typedef struct { TYPE* data; int length; int capacity; } da_##NAME; \
    static inline void da_##NAME##_init(da_##NAME* arr, int init_cap) { \
        arr->data = malloc(sizeof(TYPE) * init_cap); \
        arr->length = 0; \
        arr->capacity = init_cap; \
    } \
    // ... append, pop, free functions
```

This approach declares all necessary functions (`da_T_init`, `da_T_append`, `da_T_pop`, `da_T_free`) for each type without manual repetition.

== Regex Parser

=== Explicit Concatenation Insertion

Our syntax allows the user to omit the concatenation operator, writing `ab` instead of `a.b`.
The parser's first pass makes these implicit operators explicit:

```c
int prev_ends = is_operand(prev) || prev == ')' || prev == '*' ||
                prev == '+' || prev == '?';
int curr_starts = is_operand(c) || c == '(';

if (prev_ends && curr_starts) {
    da_char_append(&infix, '.');
}
```

The insertion occurs between:
- Any "closing" element (operand, '`)`' or postfix operator)
- Any "opening" element (operand or '`(`')

For example: `ab+c?d` becomes `a.b+.c?.d`

=== Shunting-Yard Algorithm

The second pass converts infix notation to postfix using the standard shunting-yard algorithm with left-associative operators:

```c
if (top_prec >= curr_prec) {
    da_char_append(&output, *da_char_pop(&op_stack));
} else {
    break;
}
```

Operator precedence is defined as:
- `|` (alternation): precedence 1
- `.` (concatenation): precedence 2
- `*`, `+`, `?` (postfix): precedence 4

The use of `>=` (rather than `>`) ensures left-associative evaluation, meaning operators of equal precedence are processed in the order they appear.

== Thompson's Algorithm

Thompson's algorithm constructs NFAs through composition of primitive fragments. The implementation provides four fundamental operations:

=== Atom Handler
Creates a single-state NFA for a character:

```c
nfa_fragment create_atom(char c) {
    nfa_fragment frag = {0};
    frag.start = create_nfa_node();
    frag.accept = create_nfa_node();
    add_transition(frag.start, frag.accept, c);
    return frag;
}
```

=== Concatenation
Connects two fragments sequentially via epsilon transition:

```c
nfa_fragment concat(nfa_fragment a, nfa_fragment b) {
    a.accept->is_accept = false;
    add_transition(a.accept, b.start, TRANSITION_EPSILON);
    return (nfa_fragment){ .start = a.start, .accept = b.accept };
}
```

=== Alternation
Creates parallel paths:

```c
nfa_fragment union_nfa(nfa_fragment a, nfa_fragment b) {
    nfa_fragment result = {
        .start = create_nfa_node(),
        .accept = create_nfa_node()
    };
    add_transition(result.start, a.start, TRANSITION_EPSILON);
    add_transition(result.start, b.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);
    add_transition(b.accept, result.accept, TRANSITION_EPSILON);
    return result;
}
```

=== Kleene Star, Positive Closure, and Optional

The Kleene star creates a loop with a bypass path:

```c
nfa_fragment kleene(nfa_fragment a) {
    nfa_fragment result = {
        .start = create_nfa_node(),
        .accept = create_nfa_node()
    };
    add_transition(result.start, a.start, TRANSITION_EPSILON);
    add_transition(result.start, result.accept, TRANSITION_EPSILON);
    add_transition(a.accept, a.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);
    return result;
}
```

The `+` operator is equivalent to `a.a*`.

```c
nfa_fragment plus(nfa_fragment a) {
    nfa_fragment result = { .start = a.start, .accept = create_nfa_node() };
    add_transition(a.accept, a.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);
    return result;
}
```

The optional operator is equivalent to `a | epsilon`, requiring two paths to the accepted state:

```c
nfa_fragment question(nfa_fragment a) {
    nfa_fragment result = {
        .start = create_nfa_node(),
        .accept = create_nfa_node()
    };
    add_transition(result.start, a.start, TRANSITION_EPSILON);
    add_transition(a.accept, result.accept, TRANSITION_EPSILON);
    add_transition(result.start, result.accept, TRANSITION_EPSILON);
    return result;
}
```

Construction from postfix notation uses a stack of fragments, applying the appropriate operation for each token:

```c
for (int i = 0; i < r.size; i++) {
    char token = r.items[i].value;

    if (is_operand(token)) {
        da_nfa_fragment_append(&stack, create_atom(token));
    } else if (token == '|') {
        nfa_fragment b = *da_nfa_fragment_pop(&stack);
        nfa_fragment a = *da_nfa_fragment_pop(&stack);
        da_nfa_fragment_append(&stack, union_nfa(a, b));
    } // ... handle '.', '*', '+', '?'
}
```

The following image shows a visual representation of the fragments and how they can be composed.

#image("assets/thompson.png")

== NFA Simulator

The simulator determines whether a given string is accepted by the constructed NFA. It operates by tracking all simultaneously active states, accounting for epsilon transitions through epsilon closure computation.

=== Epsilon Closure Calculation

A recursive helper function computes the epsilon closure of a state:

```c
void add_state_with_closure(nfa_node* node, nfa_node** states, int* count) {
    if (*count >= MAX_STATES) return;

    // Check if already in states
    for (int i = 0; i < *count; i++) {
        if (states[i] == node) return;
    }

    states[(*count)++] = node;

    // Follow epsilon transitions
    for (int i = 0; i < node->trans_count; i++) {
        if (node->trans[i].on == TRANSITION_EPSILON) {
            add_state_with_closure(node->trans[i].target, states, count);
        }
    }
}
```

The algorithm prevents infinite loops by maintaining a set of visited states and limits the total number of active states with a fixed array `nfa_node* current_states[MAX_STATES]`.

=== Character Processing

For each input character, the simulator:
1. Computes which states can be reached from the current set via that character
2. For each subsequent state, computes its epsilon closure
3. Updates the current state set to these new states

```c
for (int i = 0; i < buflen; i++) {
    char c = buf[i];
    next_count = 0;

    for (int j = 0; j < current_count; j++) {
        nfa_node* state = current_states[j];
        for (int k = 0; k < state->trans_count; k++) {
            if (state->trans[k].on == c) {
                add_state_with_closure(state->trans[k].target, next_states, &next_count);
            }
        }
    }
    for (int j = 0; j < next_count; j++) {
        current_states[j] = next_states[j];
    }
    current_count = next_count;
}
```

Acceptance is determined by checking whether any final state in the current set is marked as accepting.

== Design Decisions

=== Error Handling Framework

The implementation uses a result monad pattern with optional format messages:

```c
static inline result(regex) parse_regex(const char* str) {
  result(regex) res = {0};

  if (!str || *str == '\0') {
      res.err = create_error("Empty regex string");
      return res;
  }

  // continue...
}

// The Result syntax

result(regex) rgx = parse_regex(regex_str);
must(rgx, "Could not parse expression '%s'", regex_str);
```

#image("assets/error_format.png")

And thanks to this pattern, we can easily write asserts with pretty formatted debugging information
and type-safe error checking, altought I had to use tricky/advanced macro features that may be difficult to extend.

=== Memory Management

Manual management was avoided as much as possible to make the implementation easier to read.
Multiple parts of the code prefer to allocate fixed-size arrays in the stack, and making
assumptions that the input will not exceed predefined limits.

= Results

== Validation Output

As shown before, the implementation passes all the `validator` tests.

#image("assets/validator_output.png", alt: "Terminal screenshot showing successful validation test execution", width: 100%)

== Unit Test Coverage

The implementation includes 73+ test conditions available in the `tests.c` compilation unit. The
`-x` flag can be used to run the test suite.

After uploading this report, I'm planning to make the implementation more robust, so more complex tests are on the way.

== Visualization

I didn't had enough time to achieve this.

I am planning on porting this code to WASM and make the entire compiler available in my personal
web page.

For the visualization engine, instead of relying in complex canvas integration, I will
simply render the graphs using SVG format.

I have already tested this approach for a logic expressions evaluator that I wrote in Rust and then ported to WASM: #underline()[#link("https://paoloose.site/discmaths/project/1/")]

I have not decided yet if I want to take upon this work, but if I do so, I'll be linking the URL in the private comments section :)
