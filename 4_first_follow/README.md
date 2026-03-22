# FIRST and FOLLOW sets computation

Compile with

```bash
cmake . && make
```

Test the algorithm with the following grammar:

```bash
printf "Non-terminals: E T F\nTerminals: + * ( ) id\nE -> E + T\nE -> T\nT -> T * F\nT -> F\nF -> ( E )\nF -> id\n" | ./build/first_and_follow
```

```
FIRST(E): {(, id}
FOLLOW(E): {+, ), $}
FIRST(T): {(, id}
FOLLOW(T): {+, *, ), $}
FIRST(F): {(, id}
FOLLOW(F): {+, *, ), $}
```

## Running the tests

```
docker compose up --build
```
