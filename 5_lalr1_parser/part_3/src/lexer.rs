use crate::tokens::ScannerToken;

// Represents a recognized lexeme along with its kind and position in the source
#[derive(Debug, Clone)]
pub struct Token {
    pub kind: ScannerToken,
    pub lexeme: String,
    pub line: usize,
    pub column: usize,
}

// A simple lexical analyzer that reads characters and produces tokens
pub struct Lexer<'a> {
    input: std::str::Chars<'a>,
    current_char: Option<char>,
    pub line: usize,
    pub column: usize,
}

impl<'a> Lexer<'a> {
    // Creates a new Lexer instance from a source string
    pub fn new(input: &'a str) -> Self {
        let mut lexer = Lexer {
            input: input.chars(),
            current_char: None,
            line: 1,
            column: 0,
        };
        lexer.advance();
        lexer
    }

    // Moves the character iterator forward while updating line and column counters
    fn advance(&mut self) {
        if let Some(c) = self.current_char {
            if c == '\n' {
                self.line += 1;
                self.column = 0;
            } else {
                self.column += 1;
            }
        }
        self.current_char = self.input.next();
    }

    // Consumes all whitespace characters until a non whitespace character is found
    fn skip_whitespace(&mut self) {
        while let Some(c) = self.current_char {
            if c.is_whitespace() {
                self.advance();
            } else {
                break;
            }
        }
    }
}

impl<'a> Iterator for Lexer<'a> {
    type Item = Token;

    // Returns the next recognized token from the input stream
    fn next(&mut self) -> Option<Self::Item> {
        self.skip_whitespace();

        let c = self.current_char?;
        let start_line = self.line;
        let start_column = if self.column == 0 { 1 } else { self.column };

        // Parse identifiers
        if c.is_alphabetic() {
            let mut id = String::new();
            while let Some(c) = self.current_char {
                if c.is_alphanumeric() || c == '_' {
                    id.push(c);
                    self.advance();
                } else {
                    break;
                }
            }
            return Some(Token {
                kind: ScannerToken::Identifier,
                lexeme: id,
                line: start_line,
                column: start_column,
            });
        }

        // Parse numeric literals
        if c.is_numeric() {
            let mut num = String::new();
            while let Some(c) = self.current_char {
                if c.is_numeric() {
                    num.push(c);
                    self.advance();
                } else {
                    break;
                }
            }
            return Some(Token {
                kind: ScannerToken::IntLiteral,
                lexeme: num,
                line: start_line,
                column: start_column,
            });
        }

        // Parse operators and symbols
        let kind = match c {
            '=' => ScannerToken::Assign,
            ';' => ScannerToken::Semicolon,
            '+' => ScannerToken::Plus,
            '*' => ScannerToken::Mul,
            _ => ScannerToken::Error,
        };
        let lexeme = c.to_string();
        self.advance();

        Some(Token {
            kind,
            lexeme,
            line: start_line,
            column: start_column,
        })
    }
}
