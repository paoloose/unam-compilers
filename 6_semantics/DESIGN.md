# Ennuyeux Language Design Document

## Design Decisions

### Decision 1: Expression-Based Evaluation

**Why**: Consistent value flow makes code more predictable and allows treating control structures as expressions.

```ennuyeux
let max = if (a > b) { a } else { b };
let category = match (x) { 0 => "Zero"; _ => "Other"; };
```

### Decision 2: Loop with Break Returns Value

**Why**: Enables loops to contribute meaningful values to the computation.

```ennuyeux
let result = loop {
    for (let i = 0; i < 100; i = i + 1) {
        if (condition(i)) {
            break i;
        }
    }
}
```

**Without this**: Would need to create a mutable variable outside the loop.

### Decision 3: Result/Option Instead of Exceptions

**Why**: Error handling is explicit and type-safe; no unexpected exceptions.

```ennuyeux
// Before
try {
    let x = risky_operation();
} catch {
    // Handle error
}

// After (Ennuyeux)
match (risky_operation()) {
    Ok(x) => { /* use x */ };
    Err(e) => { /* handle error */ };
}
```

### Decision 4: Implicit Function Returns

**Why**: Cleaner code; the last expression is the return value.

```ennuyeux
fn add(a: Int, b: Int) {
    a + b  // Implicitly returned, no semicolon
}

fn divide(a: Int, b: Int) {
    if (b == 0) {
        return Err("Division by zero");  // Explicit early return
    }
    Ok(a / b)  // Implicitly returned
}
```

---

## 3. Type System

### Primitive Types

```ennuyeux
let int_val = 42;           // Integer
let float_val = 3.14;       // Floating-point
let bool_val = true;        // Boolean
let str_val = "hello";      // String
```

### Composite Types

#### Enums

```ennuyeux
// Simple enum
enum Color { Red; Green; Blue; };

// Enum with data
enum Option<T> {
    Some(T);
    None;
}

// Enum with multiple fields
enum Message {
    Text(String);
    Pair(Int, Int);
}
```

### Type Inference

The language supports basic type inference:

- Literals are immediately typed
- Function return types are inferred from the last expression
- Operations maintain type consistency (can't add string + int without conversion)

---

## 4. Lexical Structure

### Tokens

**Keywords**:

```ennuyeux
let, if, else, match, fn, return, break, for, enum, true, false
```

**Operators**:

```ennuyeux
Arithmetic: +, -, *, /, %, **
Bitwise: &, |, ^, ~, <<, >>
Logical: &&, ||, !
Comparison: ==, !=, <, >, <=, >=
Assignment: =
```

**Punctuation**:

```ennuyeux
; , ( ) { } [ ] => .. : . _
```

### Identifiers

```ennuyeux
Pattern: [a-zA-Z_][a-zA-Z0-9_]*
Examples: x, count, my_variable, _unused
```

### Literals

```ennuyeux
Numbers: 42, 3.14, 0xFF, 0b1010
Strings: "hello", "line1\nline2"
Booleans: true, false
```
