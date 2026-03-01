#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "result.h"
#include "da.h"

static inline result(input_op) get_operator_from_symbol(char symbol) {
    input_op op = { 1, 1 };

    switch (symbol) {
    case '*': {
        op.precedence = 4;
        op.arguments = 1;
        return_ok_with(input_op, op);
    }
    case '+': {
        op.precedence = 4;
        op.arguments = 1;
        return_ok_with(input_op, op);
    }
    case '?': {
        op.precedence = 4;
        op.arguments = 1;
        return_ok_with(input_op, op);
    }
    case '.': {
        op.precedence = 2;
        op.arguments = 2;
        return_ok_with(input_op, op);
    }
    case '|': {
        op.precedence = 1;
        op.arguments = 2;
        return_ok_with(input_op, op);
    }
    default:
        return_err(input_op, "Got unknown character for regex: '%c'", symbol);
    }
}

static inline int is_operator(char c) {
    // TODO: too much repetition, maybe we can globally define these operators?
    return c == '*' || c == '+' || c == '?' || c == '|' || c == '.';
}

static inline int is_operand(char c) {
    return c != '(' && c != ')' && c != '|' && c != '*' && c != '+' && c != '?' && c != '.' && c != '\0';
}

static inline result(regex) parse_regex(const char* str) {
    result(regex) res = {0};

    if (!str || *str == '\0') {
        res.err = create_error("Empty regex string");
        return res;
    }

    // First pass: insert explicit concatenation operator '.'
    da_char infix = {0};
    da_char_init(&infix, 128);

    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];

        if (infix.length > 0) {
            char prev = infix.data[infix.length - 1];

            // Need to insert '.' if:
            // prev is: operand, ')', '*', '+', or '?'
            // and c is: operand or '('
            int prev_ends = is_operand(prev) || prev == ')' || prev == '*' || prev == '+' || prev == '?';
            int curr_starts = is_operand(c) || c == '(';

            if (prev_ends && curr_starts) {
                da_char_append(&infix, '.');
            }
        }

        da_char_append(&infix, c);
    }

    // DEBUG: print infix
    // printf("Infix: ");
    // for (int i = 0; i < infix.length; i++) printf("%c", infix.data[i]);
    // printf("\n");

    // Second pass: Shunting yard algorithm
    da_char output = {0};
    da_char op_stack = {0};
    da_char_init(&output, 128);
    da_char_init(&op_stack, 64);

    for (int i = 0; i < infix.length; i++) {
        char token = infix.data[i];

        if (is_operand(token)) {
            // Operand goes to output
            da_char_append(&output, token);
        }
        else if (token == '(') {
            // Push ( to stack
            da_char_append(&op_stack, token);
        }
        else if (token == ')') {
            // Pop until we find (
            int found = 0;
            while (op_stack.length > 0) {
                char top = *da_char_pop(&op_stack);
                if (top == '(') {
                    found = 1;
                    break;
                }
                da_char_append(&output, top);
            }

            if (!found) {
                res.err = create_error("Mismatched parentheses");
                da_char_free(&infix);
                da_char_free(&output);
                da_char_free(&op_stack);
                return res;
            }
        }
        else if (is_operator(token)) {
            // Get current operator precedence
            result(input_op) curr_res = get_operator_from_symbol(token);
            must(curr_res);
            int curr_prec = curr_res.val.precedence;

            // Pop operators with higher or equal precedence (left-associative)
            while (op_stack.length > 0) {
                char top = op_stack.data[op_stack.length - 1];

                if (top == '(') break;

                result(input_op) top_res = get_operator_from_symbol(top);
                if (top_res.err != nil) break;  // Not an operator

                int top_prec = top_res.val.precedence;
                if (top_prec >= curr_prec) {
                    da_char_append(&output, *da_char_pop(&op_stack));
                } else {
                    break;
                }
            }

            // Push current operator
            da_char_append(&op_stack, token);
        }
    }

    // Pop all remaining operators
    while (op_stack.length > 0) {
        char top = *da_char_pop(&op_stack);
        if (top == '(' || top == ')') {
            res.err = create_error("Mismatched parentheses");
            da_char_free(&infix);
            da_char_free(&output);
            da_char_free(&op_stack);
            return res;
        }
        da_char_append(&output, top);
    }

    // Build result
    res.val.size = output.length;
    res.val.items = (regex_item*)malloc(sizeof(regex_item) * output.length);
    must(res.val.items, "Failed to allocate regex items");

    for (int i = 0; i < output.length; i++) {
        res.val.items[i].value = output.data[i];
    }

    da_char_free(&infix);
    da_char_free(&output);
    da_char_free(&op_stack);

    return res;
}
