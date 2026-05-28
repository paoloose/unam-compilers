# Virtual Machine ISA Reference (Final Structured Model)

## Overview

This VM is a **label-based procedural virtual machine with structured subroutines**.

It combines three layers:

- **Execution layer:** `GOSUB`, `RETURN`, `callStack`
- **Jump layer:** `LABEL`, `GOTO`, `IF`, `IFFALSE`
- **Structure layer:** `FUNCTION`, `END_FUNCTION` (non-executable metadata)

The key idea:

> The VM is NOT function-based — but it still supports structured function-like regions for readability and tooling.

---

# Execution Model

## Program Flow

Execution begins at the first instruction unless redirected:

```text
GOTO Main
```

There is no implicit entry point.

---

# Data Types

## Numbers
```text
42
3.14
-7
```

## Booleans
```text
true
false
```

Internally:
```text
true = 1
false = 0
```

## Strings
```text
"Hello"
```

Used primarily by `PRINT`.

---

# Variables

## VAR
Declares a variable:

```text
VAR x
```

Initial value: `0`

---

## ASSIGN
```text
ASSIGN source destination
```

Examples:
```text
ASSIGN 10 x
ASSIGN x y
```

---

# Scope System

The VM has only two scopes:

## 1. Global Scope
- Exists always
- Stored in `globals`

## 2. Local Scope
- Created only by `GOSUB`
- Stored in call frame

```js
{ returnAddr, returnVar, locals: Map }
```

---

## Scope Rules

| Context | Behavior |
|--------|----------|
| Inside `GOSUB` | Variables are local by default |
| Outside `GOSUB` | Variables are global |
| Local name conflicts | Local shadows global |
| No nested lexical scope | Correct |

---

# Arithmetic

```text
ADD a b dest
SUB a b dest
MUL a b dest
DIV a b dest
MOD a b dest
POW a b dest
```

---

# Logic

```text
AND a b dest
OR  a b dest
NOT a dest
XOR a b dest
```

Returns `1` or `0`.

---

# Comparisons

```text
EQ  a b dest
NEQ a b dest
GT  a b dest
GTE a b dest
LT  a b dest
LTE a b dest
```

---

# Control Flow

## LABEL
Defines a jump target:

```text
LABEL Loop
```

---

## GOTO
```text
GOTO Loop
```

---

## IF / IFFALSE

```text
IF condition GOTO label
IFFALSE condition GOTO label
```

---

# Subroutines (CORE EXECUTION MODEL)

Subroutines are implemented using:

```text
LABEL + GOSUB + RETURN
```

---

## GOSUB

```text
GOSUB label returnVariable
```

Behavior:

- Push call frame
- Create local scope
- Store return address
- Jump to label

---

## RETURN

```text
RETURN value
```

- Restores caller state
- Optionally stores return value

---

## Mental Model

```text
GOSUB Foo result
```

means:

> call Foo and store return value in `result`

---

# FUNCTION BLOCKS (STRUCTURE LAYER)

## FUNCTION / END_FUNCTION

```text
FUNCTION Name
    ...
END_FUNCTION
```

### IMPORTANT PROPERTY

These are **NOT executable constructs**.

They do NOT:

- define callable functions
- create labels
- affect callStack
- influence execution flow directly

---

### What they DO:

- Group code visually
- Mark semantic boundaries
- Enable nested block skipping
- Prevent accidental execution inside structured regions

---

## Execution Behavior

When the VM encounters `FUNCTION`:

- It skips all instructions until matching `END_FUNCTION`
- Nested FUNCTION blocks are supported
- No runtime effect is produced

---

## Example

```text
GOTO Main

FUNCTION Helper
    PRINT "This will be skipped"
END_FUNCTION

LABEL Main
PRINT "Running program"
```

Output:
```text
Running program
```

---

# Parameters (Stack-Based)

## PARAM
Push argument:

```text
PARAM value
```

## PARAM_GET
Pop argument:

```text
PARAM_GET x
```

LIFO order applies.

---

# Arrays

```text
ARR size dest
ARR_GET arr index dest
ARR_SET arr index value
```

---

# Lists

```text
LIST dest
LIST_ADD list value
LIST_GET list index dest
```

---

# Input / Output

## PRINT
```text
PRINT "Hello"
PRINT x
```

## INPUT
```text
INPUT x
```

Pauses execution until user input is provided.

---

# Graphics

## PIXEL
```text
PIXEL x y color
```

- Screen size: 64×64
- Monochrome buffer

---

# Keyboard

## KEY
```text
KEY code dest
```

Key map:

| Code | Key |
|------|-----|
| 0 | Up |
| 1 | Down |
| 2 | Left |
| 3 | Right |
| 4 | W |
| 5 | S |
| 6 | A |
| 7 | D |
| 8 | Space |

---

# Timing

## TIME
Returns either:

- Time since execution start (VM1)
- Unix timestamp (VM2)

```text
TIME dest
```

---

# SLEEP

No-op (no delay implemented).

---

# FULL EXAMPLES

---

## 1. Subroutine with Return Value

```text
GOTO Main

LABEL Add
PARAM_GET b
PARAM_GET a

ADD a b result
RETURN result

LABEL Main

PARAM 10
PARAM 20
GOSUB Add sum

PRINT "Sum:"
PRINT sum
```

Output:
```text
Sum:
30
```

---

## 2. Local vs Global Scope

```text
VAR x
ASSIGN 100 x

GOSUB Test

PRINT x

LABEL Test
VAR x
ASSIGN 999 x
RETURN
```

Output:
```text
100
```

---

## 3. Loop with GOTO

```text
VAR i
ASSIGN 0 i

LABEL Loop
PRINT i
ADD i 1 i

LT i 5 cond
IF cond GOTO Loop
```

Output:
```text
0
1
2
3
4
```

---

## 4. Function Block (STRUCTURE ONLY)

```text
GOTO Main

FUNCTION DrawRoutine
    PIXEL 10 10 1
    PIXEL 11 10 1
END_FUNCTION

LABEL Main
PRINT "Start"
```

Output:
```text
Start
```

---

## 5. Nested FUNCTION Blocks

```text
GOTO Main

FUNCTION Outer
    FUNCTION Inner
        PRINT "Skipped completely"
    END_FUNCTION
END_FUNCTION

LABEL Main
PRINT "Running"
```

Output:
```text
Running
```

---

## 6. Recursive Factorial

```text
LABEL Fact

PARAM_GET n

LTE n 1 base
IFFALSE base GOTO recurse

RETURN 1

LABEL recurse

SUB n 1 n2
PARAM n2
GOSUB Fact sub

MUL n sub result
RETURN result


LABEL Main

PARAM 5
GOSUB Fact res

PRINT res
```

Output:
```text
120
```

---

# Final Mental Model

```text
LABEL / GOTO / IF     = control flow
GOSUB / RETURN        = real functions (call stack)
FUNCTION / END_FUNCTION = structure only (ignored at runtime)
```

> The VM is a structured assembly-like language with optional function-like organization for readability.
