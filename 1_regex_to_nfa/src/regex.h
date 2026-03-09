#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "result.h"
#include "da.h"

static result(input_op) get_operator_from_symbol(char symbol) {
    input_op op = { 1, 1 };
    op.symbol = symbol;

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
    case ')':
    case '(': {
        op.precedence = 0;
        op.arguments = 0;
        return_ok_with(input_op, op);
    }
    default: {
        return_err(input_op, "Got unknown character for regex: '%c'", symbol);
    }
    }
}

static bool is_operator(char symbol) {
    return is_ok(get_operator_from_symbol(symbol));
}

static bool is_operand(char symbol) {
    return is_err(get_operator_from_symbol(symbol));
}

static bool is_unary(char symbol) {
    result(input_op) op = get_operator_from_symbol(symbol);
    if (is_err(op)) return false;
    return op.val.arguments == 1;
}

static result(regex) parse_regex(const char* input) {
    result(regex) res = {0};
    size_t input_len = strlen(input);

    if (input_len == 0) {
        res.err = create_error("Empty regex string");
        return res;
    }

    // Step 1: we add explicit concatenation

    da_char new_str = {0};
    da_char_init(&new_str, input_len * 2);

    for (int i = 0; i < input_len; i++) {
        char curr = input[i];
        if (i == 0) {
            da_char_append(&new_str, curr);
            continue;
        }
        char before = input[i-1];

        bool before_matches = before == ')' || is_operand(before) || is_unary(before);
        bool current_matches = curr == '(' || is_operand(curr);

        if (before_matches && current_matches) {
            da_char_append(&new_str, '.');
        }
        da_char_append(&new_str, curr);
    }

    char* new_input = calloc(sizeof(char), new_str.length + 1);
    int new_input_len = new_str.length;
    for (int i = 0; i < new_str.length; i++) {
        new_input[i] = new_str.data[i];
    }
    UNAM_DEBUG("inserted concatenations: %s\n", new_input);

    da_input_op holding_stack = {0};
    da_char output_stack = {0};

    da_input_op_init(&holding_stack, 16);
    da_char_init(&output_stack, 16);

    for (int i = 0; i < new_input_len; i++) {
        char curr = new_input[i];
        // Operands go directly to the output stack
        if (is_operand(curr)) {
            da_char_append(&output_stack, curr);
            continue;
        }
        // If it's not an operand, then it's an operator
        result(input_op) maybe_op = get_operator_from_symbol(curr);
        must(maybe_op);
        input_op op = maybe_op.val;

        if (op.symbol == '(') {
            da_input_op_append(&holding_stack, op);
            continue;
        }
        if (op.symbol == ')') {
            char popped = '\0';
            // We pop all the operators until we reach the open parenthesis
            while (true) {
                popped = da_input_op_pop(&holding_stack)->symbol;
                if (popped == '(') {
                    break;
                }
                da_char_append(&output_stack, popped);
            }
            continue;
        }

        while (!da_empty(&holding_stack)) {
            input_op* last_op = da_input_op_peek_last(&holding_stack);
            must(be_not_null(last_op), "Peek failed but array was supposed to be not empty");

            if (last_op->symbol == '(') {
                // we do not go beyond the current "context"
                break;
            }

            if (last_op->precedence >= op.precedence) {
                da_char_append(&output_stack, da_input_op_pop(&holding_stack)->symbol);
            } else {
                break;
            }
        }
        da_input_op_append(&holding_stack, op);
    }

    while (!da_empty(&holding_stack)) {
        da_char_append(&output_stack, da_input_op_pop(&holding_stack)->symbol);
    }

    // And then we copy the output to our new regex
    res.val.items = calloc(sizeof(regex_item), output_stack.length);
    res.val.size = output_stack.length;

    for (int i = 0; i < res.val.size; i++) {
        regex_item item = { .value = output_stack.data[i] };
        res.val.items[i] = item;
    }

    da_free(&new_str);
    da_free(&holding_stack);
    da_free(&output_stack);

    return res;
}
