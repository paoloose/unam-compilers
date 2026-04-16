#pragma once

#include <stdbool.h>

// Represents a node in the parse tree.
typedef struct TreeNode
{
    char *symbol;
    int num_children;
    struct TreeNode **children;
} TreeNode;

// A stack to hold nodes of the parse tree during parsing.
typedef struct node_stack
{
    TreeNode **nodes;
    int size;
    int capacity;
} node_stack;

// Function declarations
TreeNode *create_tree_node(const char *symbol);
void free_tree(TreeNode *node);
bool init_node_stack(node_stack *stack);
void free_node_stack(node_stack *stack);
bool push_node(node_stack *stack, TreeNode *node);
bool pop_nodes(node_stack *stack, int count, TreeNode **children_array);
