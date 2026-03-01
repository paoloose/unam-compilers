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

// test runner from tests.c
int run_tests(void);

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
    // print_postfix(rgx.val);
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

int main(int argc, char *argv[]) {
    int opt;
    char regex_str[2048];

    // NOTE: This must() thing is part of my approach to support types in c
    //       must() and similars help us to write result assertions in a single line
    //       See result.h for more details
    must(fgets(regex_str, sizeof(regex_str), stdin), "Could not read stdin");

    while ((opt = getopt(argc, argv, "rtx")) != -1) {
        switch (opt) {
            // run unit tests
            case 'x':
                return run_tests();
            // convert from regex to postfix
            case 'r':
                regex_str[strcspn(regex_str, "\r\n")] = '\0';
                result(regex) rgx = parse_regex(regex_str);
                must(rgx, "Could not parse expression '%s'", regex_str);
                print_postfix(rgx.val);
                return 0;
            // validate input strings
            case 't':
                regex_str[strcspn(regex_str, "\r\n")] = '\0';
                test_strings_stdin(regex_str);
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-r | -t | -x]\n", argv[0]);
                return 1;
        }
    }

    fprintf(stderr, "Usage: %s [-r | -t | -x]\n", argv[0]);
    return 1;
}

