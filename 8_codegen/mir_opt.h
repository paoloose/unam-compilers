#ifndef MIR_OPT_H
#define MIR_OPT_H

#include "ast.h"
#include "mir.h"

/**
 * Applies a suite of advanced optimizations on the MIR Module.
 * Passes include:
 * - Constant Folding & Propagation
 * - Copy Propagation
 * - Dead Code Elimination (DCE)
 * - Control Flow Graph (CFG) Simplification
 */
void optimize_mir(MIRModule* mod);

#endif // MIR_OPT_H
