#pragma once

#include "tree.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Generates a .dot file from a parse tree
 * @param root The root of the parse tree
 * @param filename The name of the output .dot file
 * @return true on success, false on failure to open the file
 */
bool generate_dot_file(TreeNode *root, const char *filename);
