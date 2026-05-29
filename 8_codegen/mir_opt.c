#include "mir_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// ============================================================================
// DATA STRUCTURES FOR ANALYSIS
// ============================================================================

#define MAX_TEMPS 10000

typedef struct {
    bool is_constant;
    MIRValue value;
    bool is_copy;
    MIRValue copy_source;
    int use_count;
} TempInfo;

static TempInfo temps[MAX_TEMPS];

static void reset_analysis() {
    memset(temps, 0, sizeof(temps));
}

// ============================================================================
// PASS 1: USE COUNT ANALYSIS
// ============================================================================
static void count_uses(MIRValue val) {
    if (val.kind == MIR_VALUE_TEMP && val.temp_id < MAX_TEMPS) {
        temps[val.temp_id].use_count++;
    }
}

static void analyze_uses(MIRFunction* fn) {
    MIRBlock* blk = fn->first_block;
    while (blk) {
        MIRInst* inst = blk->first_inst;
        while (inst) {
            switch (inst->type) {
                case MIR_INST_MOVE:
                    count_uses(inst->as.move.src);
                    break;
                case MIR_INST_BINARY:
                    count_uses(inst->as.binary.lhs);
                    count_uses(inst->as.binary.rhs);
                    break;
                case MIR_INST_UNARY:
                    count_uses(inst->as.unary.operand);
                    break;
                case MIR_INST_CALL:
                    for (int i = 0; i < inst->as.call.arg_count; i++) {
                        count_uses(inst->as.call.args[i]);
                    }
                    break;
                case MIR_INST_PRINT:
                    count_uses(inst->as.print.value);
                    break;
                case MIR_INST_PIXEL:
                    count_uses(inst->as.pixel.x);
                    count_uses(inst->as.pixel.y);
                    count_uses(inst->as.pixel.color);
                    break;
                case MIR_INST_ARRAY_NEW:
                    count_uses(inst->as.array_new.size);
                    break;
                case MIR_INST_ARRAY_GET:
                    count_uses(inst->as.array_get.array);
                    count_uses(inst->as.array_get.index);
                    break;
                case MIR_INST_ARRAY_SET:
                    count_uses(inst->as.array_set.array);
                    count_uses(inst->as.array_set.index);
                    count_uses(inst->as.array_set.value);
                    break;
                default:
                    break;
            }
            inst = inst->next;
        }
        
        if (blk->terminator.type == MIR_TERM_BRANCH) {
            count_uses(blk->terminator.as.branch.condition);
        } else if (blk->terminator.type == MIR_TERM_RETURN) {
            if (blk->terminator.as.ret.has_value) {
                count_uses(blk->terminator.as.ret.value);
            }
        }
        
        blk = blk->next;
    }
}

// ============================================================================
// PASS 2: CONSTANT FOLDING & PROPAGATION
// ============================================================================
static void optimize_constants_and_copies(MIRFunction* fn) {
    bool changed = true;
    while (changed) {
        changed = false;
        
        MIRBlock* blk = fn->first_block;
        while (blk) {
            MIRInst* inst = blk->first_inst;
            while (inst) {
                if (inst->type == MIR_INST_CONST && inst->as.constant.dst.kind == MIR_VALUE_TEMP) {
                    uint32_t tid = inst->as.constant.dst.temp_id;
                    if (tid < MAX_TEMPS) {
                        temps[tid].is_constant = true;
                        temps[tid].value = inst->as.constant.value;
                    }
                }
                
                if (inst->type == MIR_INST_MOVE && inst->as.move.dst.kind == MIR_VALUE_TEMP) {
                    uint32_t tid = inst->as.move.dst.temp_id;
                    if (tid < MAX_TEMPS) {
                        if (inst->as.move.src.kind == MIR_VALUE_TEMP) {
                            temps[tid].is_copy = true;
                            temps[tid].copy_source = inst->as.move.src;
                        } else if (inst->as.move.src.kind == MIR_VALUE_INT) {
                            temps[tid].is_constant = true;
                            temps[tid].value.kind = MIR_VALUE_INT;
                            temps[tid].value.int_value = inst->as.move.src.int_value;
                        }
                    }
                }
                
                // Replace uses with constants/copies
                if (inst->type == MIR_INST_BINARY) {
                    if (inst->as.binary.lhs.kind == MIR_VALUE_TEMP && temps[inst->as.binary.lhs.temp_id].is_constant) {
                        if (temps[inst->as.binary.lhs.temp_id].value.kind == MIR_VALUE_INT) {
                            inst->as.binary.lhs.kind = MIR_VALUE_INT;
                            inst->as.binary.lhs.int_value = temps[inst->as.binary.lhs.temp_id].value.int_value;
                            changed = true;
                        }
                    }
                    if (inst->as.binary.rhs.kind == MIR_VALUE_TEMP && temps[inst->as.binary.rhs.temp_id].is_constant) {
                        if (temps[inst->as.binary.rhs.temp_id].value.kind == MIR_VALUE_INT) {
                            inst->as.binary.rhs.kind = MIR_VALUE_INT;
                            inst->as.binary.rhs.int_value = temps[inst->as.binary.rhs.temp_id].value.int_value;
                            changed = true;
                        }
                    }
                    
                    // Constant folding
                    if (inst->as.binary.lhs.kind == MIR_VALUE_INT && inst->as.binary.rhs.kind == MIR_VALUE_INT) {
                        long long l = inst->as.binary.lhs.int_value;
                        long long r = inst->as.binary.rhs.int_value;
                        long long res = 0;
                        bool folded = true;
                        
                        switch (inst->as.binary.op) {
                            case MIR_BIN_ADD: res = l + r; break;
                            case MIR_BIN_SUB: res = l - r; break;
                            case MIR_BIN_MUL: res = l * r; break;
                            case MIR_BIN_DIV: if (r != 0) res = l / r; else folded = false; break;
                            case MIR_BIN_MOD: if (r != 0) res = l % r; else folded = false; break;
                            case MIR_BIN_EQ:  res = (l == r); break;
                            case MIR_BIN_NEQ: res = (l != r); break;
                            case MIR_BIN_GT:  res = (l > r); break;
                            case MIR_BIN_GTE: res = (l >= r); break;
                            case MIR_BIN_LT:  res = (l < r); break;
                            case MIR_BIN_LTE: res = (l <= r); break;
                            default: folded = false; break;
                        }
                        
                        if (folded) {
                            MIRValue dst = inst->as.binary.dst;
                            inst->type = MIR_INST_CONST;
                            inst->as.constant.dst = dst;
                            inst->as.constant.value.kind = MIR_VALUE_INT;
                            inst->as.constant.value.int_value = res;
                            changed = true;
                        }
                    }
                }
                
                inst = inst->next;
            }
            blk = blk->next;
        }
    }
}

// ============================================================================
// PASS 3: DEAD CODE ELIMINATION
// ============================================================================
static void eliminate_dead_code(MIRFunction* fn) {
    MIRBlock* blk = fn->first_block;
    while (blk) {
        MIRInst* inst = blk->first_inst;
        MIRInst* prev = NULL;
        while (inst) {
            bool dead = false;
            
            if (inst->type == MIR_INST_CONST && inst->as.constant.dst.kind == MIR_VALUE_TEMP) {
                if (temps[inst->as.constant.dst.temp_id].use_count == 0) dead = true;
            } else if (inst->type == MIR_INST_MOVE && inst->as.move.dst.kind == MIR_VALUE_TEMP) {
                if (temps[inst->as.move.dst.temp_id].use_count == 0) dead = true;
            } else if (inst->type == MIR_INST_BINARY && inst->as.binary.dst.kind == MIR_VALUE_TEMP) {
                if (temps[inst->as.binary.dst.temp_id].use_count == 0) dead = true;
            } else if (inst->type == MIR_INST_UNARY && inst->as.unary.dst.kind == MIR_VALUE_TEMP) {
                if (temps[inst->as.unary.dst.temp_id].use_count == 0) dead = true;
            } else if (inst->type == MIR_INST_ARRAY_NEW && inst->as.array_new.dst.kind == MIR_VALUE_TEMP) {
                if (temps[inst->as.array_new.dst.temp_id].use_count == 0) dead = true;
            } else if (inst->type == MIR_INST_ARRAY_GET && inst->as.array_get.dst.kind == MIR_VALUE_TEMP) {
                if (temps[inst->as.array_get.dst.temp_id].use_count == 0) dead = true;
            }
            
            if (dead) {
                if (prev) {
                    prev->next = inst->next;
                } else {
                    blk->first_inst = inst->next;
                }
                // Memory is intentionally leaked here for simplicity in this pass,
                // but normally we would free(inst);
            } else {
                prev = inst;
            }
            
            inst = inst->next;
        }
        blk = blk->next;
    }
}

// ============================================================================
// PASS 4: CFG SIMPLIFICATION (UNREACHABLE BLOCKS)
// ============================================================================
#define MAX_BLOCKS 2048

static void mark_reachable(MIRFunction* fn) {
    bool reachable[MAX_BLOCKS] = {false};
    MIRBlock* blocks[MAX_BLOCKS];
    int block_count = 0;
    
    MIRBlock* blk = fn->first_block;
    while (blk && block_count < MAX_BLOCKS) {
        blocks[block_count++] = blk;
        blk = blk->next;
    }
    
    if (block_count == 0) return;
    
    reachable[0] = true; 
    bool changed = true;
    
    while (changed) {
        changed = false;
        for (int i = 0; i < block_count; i++) {
            if (!reachable[i]) continue;
            
            MIRBlock* curr = blocks[i];
            if (curr->terminator.type == MIR_TERM_GOTO) {
                for (int j = 0; j < block_count; j++) {
                    if (blocks[j] == curr->terminator.as.goto_term.target && !reachable[j]) {
                        reachable[j] = true;
                        changed = true;
                    }
                }
            } else if (curr->terminator.type == MIR_TERM_BRANCH) {
                for (int j = 0; j < block_count; j++) {
                    if ((blocks[j] == curr->terminator.as.branch.then_block || 
                         blocks[j] == curr->terminator.as.branch.else_block) && !reachable[j]) {
                        reachable[j] = true;
                        changed = true;
                    }
                }
            }
        }
    }
    
    // Eliminate unreachable blocks
    MIRBlock* prev = NULL;
    blk = fn->first_block;
    int idx = 0;
    while (blk && idx < block_count) {
        if (!reachable[idx]) {
            if (prev) {
                prev->next = blk->next;
            } else {
                fn->first_block = blk->next;
            }
        } else {
            prev = blk;
        }
        blk = blk->next;
        idx++;
    }
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
void optimize_mir(MIRModule* mod) {
    if (!mod) return;
    
    MIRFunction* fn = mod->first_function;
    while (fn) {
        // Repeatedly apply passes until steady state
        for (int i = 0; i < 3; i++) {
            reset_analysis();
            optimize_constants_and_copies(fn);
            analyze_uses(fn);
            eliminate_dead_code(fn);
        }
        
        mark_reachable(fn);
        fn = fn->next;
    }
}
