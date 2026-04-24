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

### Decision 5: Pipelines for Functional Composition

**Why**: Enables readable left-to-right data transformation, making complex operations more intuitive and chainable.

```ennuyeux
fn double(n: Int) { n * 2 }
fn square(n: Int) { n * n }
fn to_string(n: Int) { "" + n }

// Without pipelines: hard to read, right-to-left
let result1 = to_string(square(double(5)));

// With pipelines: clear data flow, left-to-right
let result2 = 5 |> double |> square |> to_string;

// Pipelines with placeholders for partial application
let adjusted = 10 |> add(_, 5);
```

**Benefits**: Makes code more readable for data transformations and enables functional composition patterns.

### Decision 6: Anonymous Functions / Lambdas

**Why**: Enables functional programming patterns, higher-order functions, and inline computations without named function overhead.

```ennuyeux
// Basic lambda assignment
let duplicate = fn (a: Int) { a * 2 };
print(duplicate(10));  // 20

// Lambda in pipeline
let thing = 10 |> fn (a: Int) { a * 2 };

// Lambda used with map-like operations
let nums = [1, 2, 3];
nums |> map(fn(x: Int) { x * 2 })
```

**Benefits**: Supports functional programming paradigms and inline transformations.

### Decision 7: Structs for Composite Data

**Why**: Allows grouping related data with named fields, providing clear semantics and type safety for complex data structures.

```ennuyeux
struct Point {
    x: Int;
    y: Int;
}

struct Rectangle {
    origin: Point;
    width: Int;
    height: Int;
}

// Usage
let p = Point { x: 10, y: 20 };
let r = Rectangle { origin: p, width: 5, height: 3 };
print("p.x = " + p.x);
```

**Benefits**: Distinguishes structs from enums (which represent choices), providing clear intent and enabling field-based access patterns.

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

#### Structs

```ennuyeux
// Basic struct
struct Point {
    x: Int;
    y: Int;
}

// Generic struct
struct NonEmptyList<T> {
    head: T;
    tail: List<T>;
}

// Composite struct
struct Rectangle {
    origin: Point;
    width: Int;
    height: Int;
}
```

#### Function Types

```ennuyeux
// Anonymous function type
let double = fn (a: Int) { a * 2 };

// Function returning a function
fn make_adder(n: Int) {
    fn (x: Int) { x + n }
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
