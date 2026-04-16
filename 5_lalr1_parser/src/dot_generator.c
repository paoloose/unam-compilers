#include "dot_generator.h"
#include <errno.h>
#include <string.h>

/**
 * @brief Recursively traverses the parse tree and prints nodes and edges in DOT format
 * @param f The file to write to
 * @param node The current node to print
 * @param id_counter pointer to a global counter for generating unique node IDs
 */
static void dot_print_node_recursive(FILE *f, TreeNode *node, int *id_counter)
{
    int current_id = (*id_counter)++;
    fprintf(f, "  node%d [label=\"%s\"];\n", current_id, node->symbol);

    for (int i = 0; i < node->num_children; i++)
    {
        int child_id = *id_counter;
        dot_print_node_recursive(f, node->children[i], id_counter);
        fprintf(f, "  node%d -> node%d;\n", current_id, child_id);
    }
}

bool generate_dot_file(TreeNode *root, const char *filename)
{
    if (root == NULL) {
        return false;
    }
    FILE *f = fopen(filename, "w");
    if (f == NULL)
    {
        fprintf(stderr, "Failed to open '%s' for writing: %s\n", filename, strerror(errno));
        return false;
    }

    fprintf(f, "digraph DerivationTree {\n");
    int id_counter = 0;
    dot_print_node_recursive(f, root, &id_counter);
    fprintf(f, "}\n");

    fclose(f);
    printf("Derivation tree written to %s\n", filename);
    return true;
}
