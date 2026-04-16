#include "tree.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Creates a new parse tree node.
 * @param symbol The symbol for the node (e.g., "Expr", "id", "+").
 * @return A pointer to the newly allocated TreeNode, or NULL on failure.
 */
TreeNode *create_tree_node(const char *symbol)
{
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    if (node == NULL)
    {
        return NULL;
    }
    // Use strdup to own the symbol string.
    node->symbol = (char *)malloc(strlen(symbol) + 1);
    if(node->symbol == NULL)
    {
        free(node);
        return NULL;
    }
    strcpy(node->symbol, symbol);

    node->num_children = 0;
    node->children = NULL;
    return node;
}


/**
 * @brief Frees a parse tree recursively.
 * @param node The root node of the tree to free.
 */
void free_tree(TreeNode *node)
{
    if (node == NULL)
    {
        return;
    }
    for (int i = 0; i < node->num_children; i++)
    {
        free_tree(node->children[i]);
    }
    free(node->children);
    free(node->symbol);
    free(node);
}

/**
 * @brief Initializes a node stack.
 * @param stack Pointer to the node_stack to initialize.
 * @return true on success, false on allocation failure.
 */
bool init_node_stack(node_stack *stack)
{
    if (stack == NULL)
    {
        return false;
    }
    stack->capacity = 64;
    stack->size = 0;
    stack->nodes = (TreeNode **)malloc((size_t)stack->capacity * sizeof(TreeNode *));
    if (stack->nodes == NULL)
    {
        stack->capacity = 0;
        return false;
    }
    return true;
}

/**
 * @brief Frees the memory used by a node stack. Does not free the nodes themselves.
 * @param stack The node stack to free.
 */
void free_node_stack(node_stack *stack)
{
    if (stack == NULL)
    {
        return;
    }
    free(stack->nodes);
    stack->nodes = NULL;
    stack->size = 0;
    stack->capacity = 0;
}

/**
 * @brief Pushes a node onto the node stack.
 * @param stack The node stack.
 * @param node The TreeNode to push.
 * @return true on success, false on reallocation failure.
 */
bool push_node(node_stack *stack, TreeNode *node)
{
    if (stack == NULL)
    {
        return false;
    }
    if (stack->size >= stack->capacity)
    {
        int new_capacity = stack->capacity * 2;
        TreeNode **resized = (TreeNode **)realloc(stack->nodes, (size_t)new_capacity * sizeof(TreeNode *));
        if (resized == NULL)
        {
            return false;
        }
        stack->nodes = resized;
        stack->capacity = new_capacity;
    }
    stack->nodes[stack->size++] = node;
    return true;
}

/**
 * @brief Pops a specified number of nodes from the stack.
 * @param stack The node stack.
 * @param count The number of nodes to pop.
 * @param children_array An array to store the popped nodes (in reverse order).
 * @return true on success, false if the pop is invalid.
 */
bool pop_nodes(node_stack *stack, int count, TreeNode **children_array)
{
    if (stack == NULL || count < 0 || stack->size < count)
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        // Pop in reverse order to maintain correct child order
        children_array[i] = stack->nodes[stack->size - count + i];
    }
    stack->size -= count;
    return true;
}
