#import "template.typ": project

#show: project.with(
  title: "LALR(1) Parser Driver",
  subtitle: "5th laboratory report",
  authors: (
    "Flores Cóngora, Paolo Luis",
  ),
  mentors: (
    "Adrián Martínez Manzo",
  ),
  footer-text: "UNAM - Compilers",
  lang: "en",
  school-logo: image("unam_logo.png", width: 50pt),
  branch: "National Autonomous University of Mexico, Computer Science: Compilers",
)

= Introduction

Building on the previous practices, where I implemented a lexical analyzer and computed FIRST and FOLLOW sets, this assignment focuses on building a bottom-up parser.

The goal of this practice is to implement a complete LALR(1) parser generator and an agnostic component that we call the "execution driver". LALR(1) parsing is a highly efficient bottom-up strategy that handles a much larger class of grammars than top-down LL(1) parsers by keeping track of the parsing context using an automaton and a state stack.

The program receives a grammar definition and a source file, parses the grammar, builds the corresponding LALR(1) ACTION and GOTO tables. Finally, a separate "driver" uses those tables to evaluate the source file through a shift-reduce algorithm.

== Problem Statement

Given an input grammar and a source code file, the program must:

1. Parse the grammar into an internal representation.
2. Build the LALR(1) automaton by first computing the canonical LR(0) collection, propagating lookaheads to form LR(1) items, and merging states with identical kernels.
3. Generate the ACTION and GOTO parsing tables from the automaton.
4. Implement a driver that uses the parsing table to perform SHIFT-REDUCE operations to arbitrary inputs.
5. Report whether the input string is accepted or rejected by the grammar, and why.

== Project structure

```
5_lalr1_parser
 ├─ part_1/
 |    Contains the proposed grammar, and the validation test cases (accept/reject).
 ├─ part_2/
 |    Contains the flow diagram
 ├─ part_3/
 |    Contains the Rust™ code for the parser
 └─ src/
      The original source code of the lexical analyzer, with additional changes
      that we describe in this report
```

= Implementation Description

The LALR(1) generation process converts a human-readable grammar into a deterministic state machine.
The program reads the components and builds the LR(0) automaton by expanding states and subsequently propagating lookahead tokens to form the LR(1) items.

== Phase 1: Grammar and tests

Source: #link("https://github.com/paoloose/unam-compilers/tree/main/5_lalr1_parser/part_1")

As part of this phase, a grammar supporting basic variable assignments and arithmetic expressions was designed and uploaded to my repository.

=== Proposed Grammar

```text
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

=== Test Cases

Below are the test cases proposed to validate the parser's operation. Three contain valid syntax and three contain intentional syntax errors to corroborate rejections.

*Valid Syntax (Accept):*

```c
// input_01_accept.txt
a = 10;
```

```c
// input_02_accept.txt
b = a * 5;
```

```c
// input_03_accept.txt
x = 1;
y = x + 2;
```

*Invalid Syntax (Reject):*

```c
// input_01_reject.txt
let a = 10
// Fails for using 'let' (not part of the final grammar)
// and for missing the semicolon.
```

```c
// input_02_reject.txt
b = a * ;
// Fails for having an operator without an operand.
```

```c
// input_03_reject.txt
= 10;
// Fails for missing the identifier in the assignment.
```

== Phase 2: Syntax analyzer architecture

The overall architecture of our parser consists of two main steps, as illustrated in this flowchart:

#figure(
  image("program_flow.png"),
  caption: "System flowchart detailing parse table generation and source code parsing."
)

The main steps are:

1. *Phase 1: Parse Table Generation:* The process begins by reading the grammar file. The grammar is parsed into internal data structures and augmented. Then, the core LALR(1) algorithm calculates FIRST sets, builds the full LR(1) automaton, groups states sharing the same LR(0) core (kernel) items, and merges them to form the LALR(1) automaton. Finally, this automaton is used to build the `ACTION` and `GOTO` parsing tables.

2. *Phase 2: Source Code Parsing:* The execution driver reads the source code file. The lexical analyzer (implemented via Flex/`yylex`) tokenizes the input character stream.

3. *SHIFT-REDUCE Evaluation Loop:* The parser initializes a state stack (pushing state 0) and retrieves the next token from the stream. It consults the generated parse table using the current state and token to determine the action.
- If *SHIFT*, the target state is pushed to the stack and a new token is consumed.
- If *REDUCE*, $N$ states are popped from the stack, the new top state is peeked, and the GOTO state corresponding to the LHS (left hand side) non-terminal is pushed. This loop continues until it reaches an *Accept* state (input is valid) or an *Error* state (input is rejected).

== Phase 3: Parsing driver

The final driver processes the input strings using a generated table. This project already does simple input processing, however, this can be delegated to a se parate program (driver), that will read the parsing table, actions, and symbols in the exported in JSON format.

Source: #link("https://github.com/paoloose/unam-compilers/tree/main/5_lalr1_parser/part_3")

*Rust Implementation:* The shift-reduce driver parser was written and implemented separately using *Rust* (`part_3/`). This program reads the generated JSON file and simulates the parsing behavior agnostically.

```rust
// JSON parsing and Table deserialization in Rust
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
#[serde(tag = "type", content = "value")]
pub enum Action {
    #[serde(rename = "SHIFT")]
    Shift(usize),
    #[serde(rename = "REDUCE")]
    Reduce(usize),
    #[serde(rename = "ERROR")]
    Error(i32),
    #[serde(rename = "ACCEPT")]
    Accept(usize),
}
```

When analyzing the exported classic ACTION-GOTO table, we noticed that for the `REDUCE` operation to be general in the independent SHIFT-REDUCE in Rust, it needs to know the *production length (right hand side)* and the *non-terminal symbol (left hand side)* of the applied production (to pop from the stack and determine the `GOTO` index).
Therefore, it was necessary to modify the `C` generator (`src/main.c`) to inject a new `"productions"` array into `parse_table.json`.

```json
  "productions": [
    {"lhs_idx": 0, "rhs_len": 2},
    {"lhs_idx": 0, "rhs_len": 1},
    {"lhs_idx": 1, "rhs_len": 4},
    {"lhs_idx": 2, "rhs_len": 3},
    {"lhs_idx": 2, "rhs_len": 1},
    {"lhs_idx": 4, "rhs_len": 1}
  ]
    ...
```

Knowing this, the Rust execution loop is designed as follows:

First, we maintain two stacks, the state stack, and the value stack.

```rs
let mut state_stack: Vec<usize> = vec![0]; // state number
let mut value_stack: Vec<ASTNode> = Vec::new(); // the current parsed tree
```

Then, in a loop, we pop the last state and apply the corresponding action

```rs
let state = *state_stack.last().unwrap();
let action = &self.table.action[state][term_idx];
```

Here are the implementations for the actions

- `Action::Shift`: Push the terminal token, and go to next state
- `Action::Reduce`: Pop `rhs_len` states and values, which will create a new non-terminal node. Then we decide the next state by looking at the GOTO table.
- `Action::Accept`: finish, return the top value
- `Action::Error`: we look at the action table to see what tokens were expected in this state, and print them along with the token loc information.

```rust
match action {
  Action::Shift(next_state) => {
    // Push the new state and token onto their respective stacks
    state_stack.push(*next_state);
    value_stack.push(ASTNode::Terminal(token.clone()));
    if token.kind != ScannerToken::Eof {
      next_token = token_iter.next();
    }
  }
  Action::Reduce(prod_idx) => {
    let prod = &self.productions[*prod_idx];

    // Pop the elements corresponding to the right hand side length
    let mut popped_nodes = Vec::new();
    for _ in 0..prod.rhs_len {
      state_stack.pop();
      popped_nodes.push(value_stack.pop().unwrap());
    }
    popped_nodes.reverse();

    // Create the resulting non terminal node wrapping its children
    let lhs_name = self.table.non_terminals[prod.lhs_idx].clone();
    let new_node = ASTNode::NonTerminal(lhs_name.clone(), popped_nodes);

    // Compute the next state using the GOTO table
    let top_state = *state_stack.last().unwrap();
    let next_state = self.table.goto[top_state][prod.lhs_idx];

    if next_state == -1 {
      return Err(format!(
        "GOTO error: from state {} with non terminal {} ({})",
        top_state, lhs_name, prod.lhs_idx
      ));
    }

    // Push the new state and the reduced non terminal onto the stacks
    state_stack.push(next_state as usize);
    value_stack.push(new_node);
  }
  Action::Accept(_) => {
    return Ok(value_stack.pop().unwrap());
  }
  Action::Error(_) => {
    // we retrieve expected terminals for a detailed error message
    let mut expected_terminals = Vec::new();
    for (i, act) in self.table.action[state].iter().enumerate() {
      if !matches!(act, Action::Error(_)) {
        expected_terminals.push(self.table.terminals[i].clone());
      }
    }
    let expected_str = expected_terminals.join(", ");

    return Err(format!(

      "Syntax error at line {}:{}\:'
      U. Expected one of: [{}]",
      token.line, token.column, token.lexeme, expected_str
    ));
  }
}
```

A test suite is available, that runs my proposed accept/reject examples.

The results for each of my test cases are presented in the next section:

= Results Obtained

Below are the driver trace outputs for the test cases designed in Phase 1:

*Valid Case 01:* `a = 10;`
```text
Stack: [0] | Lookahead: 'a' | Action: SHIFT 1
Stack: [0, 1] | Lookahead: '=' | Action: SHIFT 4
Stack: [0, 1, 4] | Lookahead: '10' | Action: SHIFT 7
Stack: [0, 1, 4, 7] | Lookahead: ';' | Action: REDUCE by Factor -> rhs_len: 1
Stack: [0, 1, 4, 10] | Lookahead: ';' | Action: REDUCE by Term -> rhs_len: 1
Stack: [0, 1, 4, 9] | Lookahead: ';' | Action: REDUCE by Expr -> rhs_len: 1
Stack: [0, 1, 4, 8] | Lookahead: ';' | Action: SHIFT 11
Stack: [0, 1, 4, 8, 11] | Lookahead: '$' | Action: REDUCE by Stmt -> rhs_len: 4
Stack: [0, 3] | Lookahead: '$' | Action: REDUCE by Program -> rhs_len: 1
Stack: [0, 2] | Lookahead: '$' | Action: ACCEPT
```

*Valid Case 02:* `b = a * 5;`
```text
Stack: [0] | Lookahead: 'b' | Action: SHIFT 1
Stack: [0, 1] | Lookahead: '=' | Action: SHIFT 4
Stack: [0, 1, 4] | Lookahead: 'a' | Action: SHIFT 6
Stack: [0, 1, 4, 6] | Lookahead: '*' | Action: REDUCE by Factor -> rhs_len: 1
Stack: [0, 1, 4, 10] | Lookahead: '*' | Action: REDUCE by Term -> rhs_len: 1
Stack: [0, 1, 4, 9] | Lookahead: '*' | Action: SHIFT 13
Stack: [0, 1, 4, 9, 13] | Lookahead: '5' | Action: SHIFT 7
Stack: [0, 1, 4, 9, 13, 7] | Lookahead: ';' | Action: REDUCE by Factor -> rhs_len: 1
Stack: [0, 1, 4, 9, 13, 15] | Lookahead: ';' | Action: REDUCE by Term -> rhs_len: 3
Stack: [0, 1, 4, 9] | Lookahead: ';' | Action: REDUCE by Expr -> rhs_len: 1
Stack: [0, 1, 4, 8] | Lookahead: ';' | Action: SHIFT 11
Stack: [0, 1, 4, 8, 11] | Lookahead: '$' | Action: REDUCE by Stmt -> rhs_len: 4
Stack: [0, 3] | Lookahead: '$' | Action: REDUCE by Program -> rhs_len: 1
Stack: [0, 2] | Lookahead: '$' | Action: ACCEPT
```

*Invalid Case 01:* `let a = 10`
```text
Stack: [0] | Lookahead: 'let' | Action: SHIFT 1
Stack: [0, 1] | Lookahead: 'a' | Action: ERROR

Parse error: Syntax error at line 1:5
Unexpected token 'a'. Expected one of: [=]
```

*Invalid Case 02:* `b = a * ;`
```text
Stack: [0] | Lookahead: 'b' | Action: SHIFT 1
Stack: [0, 1] | Lookahead: '=' | Action: SHIFT 4
Stack: [0, 1, 4] | Lookahead: 'a' | Action: SHIFT 6
Stack: [0, 1, 4, 6] | Lookahead: '*' | Action: REDUCE by Factor -> rhs_len: 1
Stack: [0, 1, 4, 10] | Lookahead: '*' | Action: REDUCE by Term -> rhs_len: 1
Stack: [0, 1, 4, 9] | Lookahead: '*' | Action: SHIFT 13
Stack: [0, 1, 4, 9, 13] | Lookahead: ';' | Action: ERROR

Parse error: Syntax error at line 1:9
Unexpected token ';'. Expected one of: [id, num]
```

*Invalid Case 03:* `= 10;`
```text
Stack: [0] | Lookahead: 'id' | Action: SHIFT 1
Stack: [0, 1] | Lookahead: '10' | Action: ERROR

Parse error: Syntax error at line 1:3
Unexpected token '10'. Expected one of: [=]
```

All tested cases produce the expected shift-reduce transitions and cleanly identify syntax errors, aborting the process correctly with helpful messages indicating the expected terminals.

= Extra Points

== DOT Visualization (+1.5 pts)

For the extra points section, I implemented automatic derivation tree visualization generation for accepted strings (`src/dot_generator.c`).

Once the parser declares the input valid and reaches the `ACCEPT` state, it traverses the tree structure in memory recursively to print the graphs and parent-child relationships to the `derivation_tree.dot` file in Graphviz format. This allows for immediate visual inspection.

#figure(
  image("derivation_tree.png", width: 200pt),
  caption: "Derivation tree generated with the DOT utility for the string 'b = a * 5;'."
)
