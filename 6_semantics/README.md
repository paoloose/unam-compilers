# Ennuyeux

Ennuyeux is a functional, expression-based language with strong semantics support. It features:

- Expression-based evaluation with mandatory semicolons
- Loop constructs that return values on break
- Basic pattern matching with guards (no recursion, no struct patterns, no aliases)
- Result/Option types for error handling (no exceptions)
- Implicit returns from functions
- Explicit early returns with `return` keyword

---

## 1. Variables & Assignments

```ennuyeux
// Declaration
let x = 42;
let name = "Ennuyeux";
let pi = 3.14;

// Assignment
x = 69;
name = "Ennuyeux mais rapide!";

// Reassignment
let x = 100;
```

---

## 2. Arithmetic Operations

```ennuyeux
let a = 10;
let b = 3;

print(a + b);     // 13
print(a - b);     // 7
print(a * b);     // 30
print(a / b);     // 3 (integer division)
print(a % b);     // 1 (modulo)
print(a ** b);    // 1000 (exponentiation)
```

---

## 3. Binary & Bitwise Operations

```ennuyeux
let x = 12;
let y = 5;

print(x & y);     // 4 (bitwise AND)
print(x | y);     // 13 (bitwise OR)
print(x ^ y);     // 9 (bitwise XOR)
print(~x);        // ~12 (bitwise NOT)
print(x << 2);    // 48 (left shift)
print(x >> 1);    // 6 (right shift)
```

---

## 4. Logical Operations

```ennuyeux
let a = true;
let b = false;

print(a && b);    // false
print(a || b);    // true
print(!a);        // false
print(a && true); // true
print(b || true); // true
```

---

## 5. Comparison Operations

```ennuyeux
let x = 10;
let y = 20;

print(x == y);    // false
print(x != y);    // true
print(x < y);     // true
print(x > y);     // false
print(x <= y);    // true
print(x >= y);    // false
```

---

## 6. Conditionals

```ennuyeux
// Basic if-else
let age = 18;

if age >= 18 {
    print("Adult");
}
else {
    print("Minor");
}

// If-else if-else chain
if x < 10 {
    print("Small");
}
else if x < 20 {
    print("Medium");
}
else {
    print("Large");
}

// And conditionals also evaluate to an expression
// Both branches are required to have the same type

let category = if (age >= 18) { "Adult" } else { "Minor" };
print(category);
```

---

## 7. Loops

### Simple Loop (repeats until break)

```ennuyeux
// Loops return a value on break

let result = loop {
    print("Iteration");
    break 42;
}

print(result); // 42

// Infinite loop that terminates with break
let final_value = loop {
    let x = read_line();
    if (x == "exit") {
        break "Done";
    }
}
```

### For Loop

For loops have special syntax sugar, `0..10` for non inclusive end, and `0..=10` for inclusive end.

```ennuyeux
// For loop syntax
for i in 0..10 {
    print(i);
}

// For loop with break
let sum = loop {
    for i in 0..=100 {
        if (i > 50) {
            break i;
        }
    }
}
```

---

## 8. Pattern Matching

Basic pattern matching with guards (no recursion, no struct patterns, no aliases for now):

```ennuyeux
// Simple value matching
match (x) {
    0 => print("Zero");
    1 => print("One");
    n => print("Other: {n}");
}

// Pattern matching with guards
match (age) {
    18..30 => print("Young adult");
    31..60 => print("Adult");
    n if n < 18 => print("Minor");
    _ => print("Senior");
}

// Matching with compound guards
match (value) {
    n if n > 0 && n < 10 => print("Small positive");
    n if n >= 10 => print("Large");
    n if n < 0 => print("Negative");
    _ => print("Zero");
}
```

---

## 9. Error Handling with Result & Option

Ennuyeux have no exceptions. Instead, use `Result` and `Option` types:

```ennuyeux
// Option type
enum Option<T> {
    Some(T);
    None;
}

// Result type
enum Result<T, E> {
    Ok(T);
    Err(E);
}

// Using Option
fn get_first(arr) {
    if (arr.len() > 0) {
        return Some(arr[0]);
    }
    None
}

// Using Result
fn divide(a, b) {
    if (b == 0) {
        return Err("Division by zero");
    }
    Ok(a / b)
}

// Checking Result
let result = divide(10, 2);
match (result) {
    Ok(value) => print(value);
    Err(msg) => print("Error: " + msg);
}
```

---

## 10. Functions

Functions evaluate to their last expression. Use explicit `return` for early returns.

```ennuyeux
// Simple function
fn add(a, b) {
    a + b
}

// Function with explicit return
fn absolute(x) {
    if (x < 0) {
        return -x;
    }
    x
}

// Function with multiple statements
fn greet(name) {
    print("Hello, " + name);
    "Greeting complete"
}

// Recursive function
fn factorial(n) {
    if (n <= 1) {
        return 1;
    }
    n * factorial(n - 1)
}

// Calling functions
print(add(5, 3));           // 8
print(absolute(-42));       // 42
print(greet("World"));      // "Greeting complete"
print(factorial(5));        // 120
```

---

## 11. Structs (Enums)

```ennuyeux
// Simple enum
enum Color {
    Red;
    Green;
    Blue;
}

// Enum with associated data
enum Message {
    Text(String);
    Number(Int);
    Pair(Int, Int);
}

// Using enums
let color = Color::Red;
let msg = Message::Text("Hello");
let pair_msg = Message::Pair(10, 20);
```

---

## 12. Print & I/O

```ennuyeux
// Basic print
print("Hello, world");
print(42);
print(3.14);
print(true);

// Print with multiple values (concatenation)
print("Value: " + 42);
print("Count: " + count);

// Read line from input
let user_input = read_line();
print("You entered: " + user_input);

// Print multiple values
print("X=" + x + " Y=" + y);
```

---

## 13. Comments

```ennuyeux
// Single-line comment

/*
  Multi-line comment
  can span multiple lines
*/
```

---

## 14. Examples

### Example 1: Factorial with Early Return

```ennuyeux
fn factorial(n) {
    if (n < 0) {
        return Err("Negative number");
    }

    if (n <= 1) {
        return Ok(1);
    }

    match (factorial(n - 1)) {
        Ok(sub_fact) => Ok(n * sub_fact);
        err => err;
    }
}

print(factorial(5));
```

### Example 2: Find Number with Loop and Break

```ennuyeux
fn find_number(target) {
    let result = loop {
        for (let i = 1; i <= 100; i = i + 1) {
            if (i == target) {
                break Some(i);
            }
        }
        None
    }
    result
}

print(find_number(42));
```

### Example 3: Interactive Calculator

```ennuyeux
fn calculator() {
    loop {
        print("Enter command (add/sub/exit):");
        let cmd = read_line();

        match (cmd) {
            "exit" => break None;
            "add" => {
                print("Enter two numbers:");
                let a = read_line();
                let b = read_line();
                print(a + b);
            }
            "sub" => {
                print("Enter two numbers:");
                let a = read_line();
                let b = read_line();
                print(a - b);
            }
            _ => print("Unknown command");
        }
    }
}

calculator();
```

### Example 4: Pattern Matching with Guards

```ennuyeux
fn classify_age(age) {
    match (age) {
        n if n < 0 => "Invalid";
        0 => "Newborn";
        n if n < 13 => "Child";
        n if n < 20 => "Teenager";
        n if n < 65 => "Adult";
        _ => "Senior";
    }
}

print(classify_age(25));   // "Adult"
print(classify_age(12));   // "Child"
print(classify_age(70));   // "Senior"
```
