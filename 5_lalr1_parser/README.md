# Student notes

Run part 1:

```bash
./part_1/run_tests.sh part_1/test_inputs
```



---
---
---
---

Original README:

# LALR(1) Parser + Flex Scanner

This project builds a LALR(1) parsing table from a grammar file and parses a token stream produced by Flex (`scanner.l`).

## Prerequisites

- CMake >= 3.16
- A C compiler (MSVC, MinGW, or Clang)
- Flex available in PATH (required by `find_package(FLEX REQUIRED)`)

## Build

From `lalr_1`:

### Windows

```powershell
cmake -S . -B build
cmake --build build
```
If your default generator does not work on Windows, specify one explicitly, for example:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

### Debian/Ubuntu

Install dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake flex bison
```

Build:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

### Rocky Linux

Install dependencies:

```bash
sudo dnf install -y gcc gcc-c++ make cmake flex bison
```

Build:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Run

Program usage:

```text
first_and_follow <grammar_file> [source_file] [table_output.(csv|json)]
```

- `grammar_file`: grammar definition used to build automaton and table.
- `source_file`: optional input source scanned by Flex (`yyin`).
- `table_output.(csv|json)`: optional output file for the generated parse table.
	If omitted, the program writes `parse_table.csv` in the working directory.

### Example

```powershell
./build/first_and_follow ./examples/grammar_decl.txt ./examples/input_decl.c
```

Example writing JSON table:

```powershell
./build/first_and_follow ./examples/grammar_decl.txt ./examples/input_decl.c ./build/parse_table.json
```

Expected output should end with:

```text
Input accepted.
```

## Grammar File Format

The parser expects:

1. Line 1: non-terminals list with `Non-terminals:` prefix.
2. Line 2: terminals list with `Terminals:` prefix.
3. Following lines: productions in form `A -> alpha`.

Example:

```text
Non-terminals: S Decl Type
Terminals: int float ID ;
S -> Decl
Decl -> Type ID ;
Type -> int
Type -> float
```

## Notes on Token Mapping

`main.c` maps lexer tokens to grammar terminals by:

- Direct lexeme match (for symbols like `int`, `;`, `(`, `)` and so on).
- Aliases for common token classes (`IDENTIFIER`, `ID`, `id`, numeric/string literal aliases).
- EOF is mapped to parser end symbol `$` internally.
