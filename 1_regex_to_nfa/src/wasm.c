#include "types.h"
#include "result.h"
#include "regex.h"
#include "nfa.h"
#include "da.h"
#include "debug.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// Prints parsed regex elements
void print_postfix(regex r) {
    for (int i = 0; i < r.size; i++) {
        printf("%c", r.items[i].value);
    }
    printf("\n");
}

void test_strings_stdin(const char *regex_str) {
    UNAM_DEBUG("regex: %s\n", regex_str);
    result(regex) rgx = parse_regex(regex_str);
    must(rgx, "Could not parse expression %s", regex_str);
    UNAM_DEBUG("postfix expression: ");
    print_postfix(rgx.val);

    result(nfa) automata = regex_to_nfa(rgx.val);
    must(automata, "Could not build automata from expression %s", regex_str);

    char expr[2048];
    while (fgets(expr, sizeof(expr), stdin)) {
        expr[strcspn(expr, "\r\n")] = '\0';
        bool result = match_nfa(automata.val, expr, strlen(expr));
        UNAM_DEBUG("Testing for \"%s\": %s\n", expr, result ? "MATCH" : "FAILED");
        printf("%d", result ? 1 : 0);
    }
    printf("\n");
}

int main() {
  test_strings_stdin("a+\naaaaa\n");
  return 0;
}
