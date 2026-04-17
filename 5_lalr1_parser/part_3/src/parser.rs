use crate::lexer::Token;
use crate::tokens::ScannerToken;
use crate::Action;
use crate::Table;

use serde::{Deserialize, Serialize};

// Represents a node in the AST
#[derive(Debug, Clone)]
pub enum ASTNode {
    Terminal(Token),
    NonTerminal(String, Vec<ASTNode>),
}

// Defines a grammar production rule with minimal info (lhs and rhs length)
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Production {
    pub lhs_idx: usize,
    pub rhs_len: usize,
}

/// A shift reduce parser that relies on precomputed action and goto tables
pub struct Parser {
    table: Table,
    productions: Vec<Production>,
}

impl Parser {
    /// Instantiates a new parser with the given table and productions list
    pub fn new(table: Table, productions: Vec<Production>) -> Self {
        Self { table, productions }
    }

    /// Executes the main shift reduce parsing loop over an iterator of tokens
    pub fn parse<I>(&self, mut token_iter: I) -> Result<ASTNode, String>
    where
        I: Iterator<Item = Token>,
    {
        let mut state_stack: Vec<usize> = vec![0];
        let mut value_stack: Vec<ASTNode> = Vec::new();

        let mut next_token = token_iter.next();

        loop {
            let state = *state_stack.last().unwrap();

            // Extract the next token, or EOF token if exhausted
            let token = match &next_token {
                Some(t) => t.clone(),
                None => Token {
                    kind: ScannerToken::Eof,
                    lexeme: "$".to_string(),
                    line: 0,
                    column: 0,
                },
            };

            let term_idx = match self.get_terminal_index(&token.kind) {
                Some(idx) => idx,
                None => {
                    return Err(format!(
                        "Unexpected token '{}' at line {}, col {} not found in parse table terminals",
                        token.lexeme, token.line, token.column
                    ));
                }
            };

            let action = &self.table.action[state][term_idx];

            // Print the trace of current state stack and lookahead token
            let action_str = match action {
                Action::Shift(s) => format!("SHIFT {}", s),
                Action::Reduce(p) => format!(
                    "REDUCE by {} -> rhs_len: {}",
                    self.table.non_terminals[self.productions[*p].lhs_idx],
                    self.productions[*p].rhs_len
                ),
                Action::Accept(_) => "ACCEPT".to_string(),
                Action::Error(_) => "ERROR".to_string(),
            };

            println!(
                "Stack: {:?} | Lookahead: '{}' | Action: {}",
                state_stack, token.lexeme, action_str
            );

            // Execute the action defined by the parsing table
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
                        "Syntax error at line {}:{}\nUnexpected token '{}'. Expected one of: [{}]",
                        token.line, token.column, token.lexeme, expected_str
                    ));
                }
            }
        }
    }

        // Resolves the terminal index in the parse table matching the token symbol
    fn get_terminal_index(&self, token: &ScannerToken) -> Option<usize> {
        self.table
            .terminals
            .iter()
            .position(|t| t == &token.get_symbol())
    }
}
