#include "types.h"
#include "result.h"
#include "regex.h"
#include "nfa.h"
#include "da.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test counters
static int g_tests_passed = 0;
static int g_tests_failed = 0;

// Helper macro to assert test conditions
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  ✗ FAILED: %s\n", msg); \
        g_tests_failed++; \
    } else { \
        fprintf(stderr, "  ✓ %s\n", msg); \
        g_tests_passed++; \
    } \
} while (0)

// -> TEST: Regex Parsing (Shunting Yard Algorithm)

void test_regex_parsing(void) {
    fprintf(stderr, "\n=== Testing Regex Parsing (Shunting Yard) ===\n");

    // Test 1: Simple concatenation
    {
        result(regex) rgx = parse_regex("ab");
        ASSERT(!rgx.err, "Parse 'ab'");
        if (!rgx.err) {
            ASSERT(rgx.val.size == 3, "Parse 'ab' produces 3 tokens"); // ab.
            ASSERT(rgx.val.items[0].value == 'a', "First token is 'a'");
            ASSERT(rgx.val.items[1].value == 'b', "Second token is 'b'");
            ASSERT(rgx.val.items[2].value == '.', "Third token is '.'");
        }
    }

    // Test 2: Alternation
    {
        result(regex) rgx = parse_regex("a|b");
        ASSERT(!rgx.err, "Parse 'a|b'");
        if (!rgx.err) {
            ASSERT(rgx.val.size == 3, "Parse 'a|b' produces 3 tokens");
            ASSERT(rgx.val.items[2].value == '|', "Last token is '|'");
        }
    }

    // Test 3: Kleene star
    {
        result(regex) rgx = parse_regex("a*");
        ASSERT(!rgx.err, "Parse 'a*'");
        if (!rgx.err) {
            ASSERT(rgx.val.size == 2, "Parse 'a*' produces 2 tokens");
            ASSERT(rgx.val.items[1].value == '*', "Second token is '*'");
        }
    }

    // Test 4: Complex expression with parentheses
    {
        result(regex) rgx = parse_regex("(ab)*|(a*b)");
        ASSERT(!rgx.err, "Parse '(ab)*|(a*b)'");
        if (!rgx.err) {
            // Expected postfix: a b . * a * b . |
            ASSERT(rgx.val.size == 9, "Parse '(ab)*|(a*b)' produces 9 tokens");
            ASSERT(rgx.val.items[rgx.val.size - 1].value == '|', "Last token is '|'");
        }
    }

    // Test 5: One or more
    {
        result(regex) rgx = parse_regex("a+");
        ASSERT(!rgx.err, "Parse 'a+'");
        if (!rgx.err) {
            ASSERT(rgx.val.size == 2, "Parse 'a+' produces 2 tokens");
            ASSERT(rgx.val.items[1].value == '+', "Second token is '+'");
        }
    }

    // Test 6: Zero or one
    {
        result(regex) rgx = parse_regex("a?");
        ASSERT(!rgx.err, "Parse 'a?'");
        if (!rgx.err) {
            ASSERT(rgx.val.size == 2, "Parse 'a?' produces 2 tokens");
            ASSERT(rgx.val.items[1].value == '?', "Second token is '?'");
        }
    }
}

// -> TEST: NFA Construction (Thompson's Algorithm)

void test_nfa_construction(void) {
    fprintf(stderr, "\n=== Testing NFA Construction (Thompson's Algorithm) ===\n");

    // Test 1: Single character
    {
        result(regex) rgx = parse_regex("a");
        ASSERT(!rgx.err, "Parse 'a'");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            ASSERT(!automata.err, "Convert 'a' to NFA");
            ASSERT(automata.val.origin != NULL, "NFA has origin node");
            ASSERT(automata.val.node_count > 0, "NFA has nodes");
        }
    }

    // Test 2: Concatenation
    {
        result(regex) rgx = parse_regex("ab");
        ASSERT(!rgx.err, "Parse 'ab'");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            ASSERT(!automata.err, "Convert 'ab' to NFA");
            ASSERT(automata.val.node_count >= 4, "Concatenation creates at least 4 nodes");
        }
    }

    // Test 3: Alternation
    {
        result(regex) rgx = parse_regex("a|b");
        ASSERT(!rgx.err, "Parse 'a|b'");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            ASSERT(!automata.err, "Convert 'a|b' to NFA");
            ASSERT(automata.val.node_count >= 6, "Alternation creates at least 6 nodes");
        }
    }

    // Test 4: Kleene star
    {
        result(regex) rgx = parse_regex("a*");
        ASSERT(!rgx.err, "Parse 'a*'");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            ASSERT(!automata.err, "Convert 'a*' to NFA");
            ASSERT(automata.val.node_count >= 4, "Kleene star creates at least 4 nodes");
        }
    }

    // Test 5: One or more
    {
        result(regex) rgx = parse_regex("a+");
        ASSERT(!rgx.err, "Parse 'a+'");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            ASSERT(!automata.err, "Convert 'a+' to NFA");
            ASSERT(automata.val.node_count >= 3, "One or more creates at least 3 nodes");
        }
    }

    // Test 6: Zero or one
    {
        result(regex) rgx = parse_regex("a?");
        ASSERT(!rgx.err, "Parse 'a?'");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            ASSERT(!automata.err, "Convert 'a?' to NFA");
            ASSERT(automata.val.node_count >= 4, "Zero or one creates at least 4 nodes");
        }
    }
}

// -> TEST: NFA Matching

void test_nfa_matching(void) {
    fprintf(stderr, "\n=== Testing NFA Matching ===\n");

    // Test 1: Simple character match
    {
        result(regex) rgx = parse_regex("a");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "a", 1);
                bool m2 = match_nfa(automata.val, "b", 1);
                ASSERT(m1, "Match 'a' against 'a'");
                ASSERT(!m2, "Don't match 'b' against 'a'");
            }
        }
    }

    // Test 2: Concatenation match
    {
        result(regex) rgx = parse_regex("ab");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "ab", 2);
                bool m2 = match_nfa(automata.val, "a", 1);
                bool m3 = match_nfa(automata.val, "ba", 2);
                ASSERT(m1, "Match 'ab' against 'ab'");
                ASSERT(!m2, "Don't match 'a' against 'ab'");
                ASSERT(!m3, "Don't match 'ba' against 'ab'");
            }
        }
    }

    // Test 3: Alternation match
    {
        result(regex) rgx = parse_regex("a|b");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "a", 1);
                bool m2 = match_nfa(automata.val, "b", 1);
                bool m3 = match_nfa(automata.val, "c", 1);
                ASSERT(m1, "Match 'a' against 'a|b'");
                ASSERT(m2, "Match 'b' against 'a|b'");
                ASSERT(!m3, "Don't match 'c' against 'a|b'");
            }
        }
    }

    // Test 4: Kleene star match
    {
        result(regex) rgx = parse_regex("a*");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "", 0);
                bool m2 = match_nfa(automata.val, "a", 1);
                bool m3 = match_nfa(automata.val, "aa", 2);
                bool m4 = match_nfa(automata.val, "aaa", 3);
                bool m5 = match_nfa(automata.val, "b", 1);
                ASSERT(m1, "Match empty string against 'a*'");
                ASSERT(m2, "Match 'a' against 'a*'");
                ASSERT(m3, "Match 'aa' against 'a*'");
                ASSERT(m4, "Match 'aaa' against 'a*'");
                ASSERT(!m5, "Don't match 'b' against 'a*'");
            }
        }
    }

    // Test 5: Complex expression
    {
        result(regex) rgx = parse_regex("(ab)*|(a*b)");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "ab", 2);
                bool m2 = match_nfa(automata.val, "abab", 4);
                bool m3 = match_nfa(automata.val, "b", 1);
                bool m4 = match_nfa(automata.val, "aab", 3);
                ASSERT(m1, "Match 'ab' against '(ab)*|(a*b)'");
                ASSERT(m2, "Match 'abab' against '(ab)*|(a*b)'");
                ASSERT(m3, "Match 'b' against '(ab)*|(a*b)'");
                ASSERT(m4, "Match 'aab' against '(ab)*|(a*b)'");
            }
        }
    }

    // Test 6: One or more match
    {
        result(regex) rgx = parse_regex("a+");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "", 0);
                bool m2 = match_nfa(automata.val, "a", 1);
                bool m3 = match_nfa(automata.val, "aa", 2);
                bool m4 = match_nfa(automata.val, "b", 1);
                ASSERT(!m1, "Don't match empty string against 'a+'");
                ASSERT(m2, "Match 'a' against 'a+'");
                ASSERT(m3, "Match 'aa' against 'a+'");
                ASSERT(!m4, "Don't match 'b' against 'a+'");
            }
        }
    }

    // Test 7: Zero or one match
    {
        result(regex) rgx = parse_regex("a?");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "", 0);
                bool m2 = match_nfa(automata.val, "a", 1);
                bool m3 = match_nfa(automata.val, "aa", 2);
                bool m4 = match_nfa(automata.val, "b", 1);
                ASSERT(m1, "Match empty string against 'a?'");
                ASSERT(m2, "Match 'a' against 'a?'");
                ASSERT(!m3, "Don't match 'aa' against 'a?'");
                ASSERT(!m4, "Don't match 'b' against 'a?'");
            }
        }
    }

    // Test 8: Mixed operators
    {
        result(regex) rgx = parse_regex("ab+c?d");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m1 = match_nfa(automata.val, "abd", 3);
                bool m2 = match_nfa(automata.val, "abcd", 4);
                bool m3 = match_nfa(automata.val, "abbcd", 5);
                bool m4 = match_nfa(automata.val, "ad", 2);
                ASSERT(m1, "Match 'abd' against 'ab+c?d'");
                ASSERT(m2, "Match 'abcd' against 'ab+c?d'");
                ASSERT(m3, "Match 'abbcd' against 'ab+c?d'");
                ASSERT(!m4, "Don't match 'ad' against 'ab+c?d' (need at least one b)");
            }
        }
    }
}

// -> TEST: Edge Cases

void test_edge_cases(void) {
    fprintf(stderr, "\n=== Testing Edge Cases ===\n");

    // Test 1: Empty regex should fail
    {
        result(regex) rgx = parse_regex("");
        ASSERT(rgx.err != NULL, "Empty regex produces error");
    }

    // Test 2: Invalid parentheses
    {
        result(regex) rgx = parse_regex("(a");
        ASSERT(rgx.err != NULL, "Unmatched opening paren produces error");
    }

    // Test 3: NFA with null origin
    {
        nfa n = {0};
        bool m = match_nfa(n, "a", 1);
        ASSERT(!m, "Null NFA doesn't match anything");
    }

    // Test 4: Empty string matching
    {
        result(regex) rgx = parse_regex("a*");
        if (!rgx.err) {
            result(nfa) automata = regex_to_nfa(rgx.val);
            if (!automata.err) {
                bool m = match_nfa(automata.val, "", 0);
                ASSERT(m, "Empty string matches 'a*'");
            }
        }
    }

    // Test 5: Single character escaping special chars (if supported)
    {
        result(regex) rgx = parse_regex("(");
        ASSERT(rgx.err != NULL, "Opening paren alone is invalid");
    }
}

// Main test runner

int run_tests(void) {
    fprintf(stderr, "REGEX_TO_NFA UNIT TESTS\n");

    test_regex_parsing();
    test_nfa_construction();
    test_nfa_matching();
    test_edge_cases();

    fprintf(stderr, "\n═══════════════════════════════════════════════════════════\n");
    fprintf(stderr, "RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    fprintf(stderr, "═══════════════════════════════════════════════════════════\n");

    return g_tests_failed == 0 ? 0 : 1;
}
