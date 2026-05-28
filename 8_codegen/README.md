# Ennuyeux

Ennuyeux now performs semantic analysis and type checking!

## Running the tests suite

Build the compiler

```console
make

./run_tests.sh
```

Or choose any example of your choice, for instance, this cute `twice` function:

```txt
fn twice<T>(x: (T)->T) -> (T)->T {
    fn (val: T) { x(x(val)) }
}

fn main() {
    let add1 = fn (v: int) { v + 1 };
    twice(add1)(0); // 2
}
```

```console
./ennuyeux_parser tests/functions/twice.ennuyeux
```

## Running with docker

You can run our test suite building the provided Dockerfile, or using docker compose.

```console
$ docker compose up --build

Attaching to test-1
test-1  | make: Nothing to be done for 'all'.
test-1  |
test-1  | 🪰 Running Test Suite
test-1  |
test-1  | ✅ [arithmetic/basic.ennuyeux        ] PASS
test-1  | ✅ [bitwise/basic.ennuyeux           ] PASS
test-1  | ✅ [comparison/basic.ennuyeux        ] PASS
test-1  | ✅ [enums/enum_basic.ennuyeux        ] PASS
test-1  | ✅ [enums/result_option.ennuyeux     ] PASS
test-1  | ✅ [for/c_style.ennuyeux             ] PASS
test-1  | ✅ [for/for_break_value.ennuyeux     ] PASS
test-1  | ✅ [for/for_scope.ennuyeux           ] PASS
test-1  | ✅ [for/range_exclusive.ennuyeux     ] PASS
test-1  | ✅ [foreach/list_iteration.ennuyeux  ] PASS
test-1  | ✅ [functions/basic.ennuyeux         ] PASS
test-1  | ✅ [functions/recursive.ennuyeux     ] PASS
test-1  | ✅ [functions/twice.ennuyeux         ] PASS
test-1  | ✅ [if/if_expression.ennuyeux        ] PASS
test-1  | ✅ [if/if_type_mismatch.ennuyeux     ] PASS (Expected Failure)
test-1  | ✅ [invalid/missing_brace.ennuyeux   ] PASS (Expected Failure)
test-1  | ✅ [invalid/placeholder.ennuyeux     ] PASS (Expected Failure)
test-1  | ✅ [invalid/struct_literal.ennuyeux  ] PASS (Expected Failure)
test-1  | ✅ [invalid/unclosed_string.ennuyeux ] PASS (Expected Failure)
test-1  | ✅ [lambdas/basic.ennuyeux           ] PASS
test-1  | ✅ [let/decl_and_assign.ennuyeux     ] PASS
test-1  | ✅ [let/placeholders.ennuyeux        ] PASS
test-1  | ✅ [let/shadowing_same_scope.ennuyeux] PASS
test-1  | ✅ [let/type_mismatch.ennuyeux       ] PASS (Expected Failure)
test-1  | ✅ [lists/basic.ennuyeux             ] PASS
test-1  | ✅ [logic/basic.ennuyeux             ] PASS
test-1  | ✅ [match/guards.ennuyeux            ] PASS
test-1  | ✅ [match/list_patterns.ennuyeux     ] PASS
test-1  | ✅ [match/match_basic.ennuyeux       ] PASS
test-1  | ✅ [member_access/basic.ennuyeux     ] PASS
test-1  | ✅ [pipelines/basic.ennuyeux         ] PASS
test-1  | ✅ [pipelines/pipe_simple.ennuyeux   ] PASS
test-1  | ✅ [scopes/block_return.ennuyeux     ] PASS
test-1  | ✅ [structs/struct_basic.ennuyeux    ] PASS
test-1  |
test-1  | 🪰 Got 34 / 34 passed
test-1  | All tests passed successfully 🪰🪰
test-1 exited with code 0
```
