# Part 3

Driver parser written in Rust™

Because the teacher allowed us to choose a language, with the condition of including the
evaluation of inputs in our report. This is embedded as evaluation traces in the `report.pdf`

## Run the parser

First, we build the LALR(1) parse table generator:

```bash
cd 5_lalr1_parser

cmake -S . -B build
cmake --build build -j"$(nproc)"
```

Then we run it using our grammar

```bash
./build/first_and_follow ./part_1/grammar.txt table.json
```

And finally, we use the generated `table.json` to test inputs:

```rs
run ../part_1/test_inputs/input_01_reject.txt  ../table.json
```
