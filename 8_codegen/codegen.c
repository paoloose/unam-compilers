#include "ast.h"
#include "debug.h"
#include "codegen.h"
#include "mir.h"
#include "mir_opt.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// MIR HELPERS (EXTENSIONS)
// ============================================================================
static inline MIRBlock* create_block(const char* label) {
    MIRBlock* block = (MIRBlock*)calloc(1, sizeof(MIRBlock));
    block->label = label;
    block->terminator.type = MIR_TERM_UNREACHABLE;
    return block;
}

static inline void append_block(MIRFunction* fn, MIRBlock* block) {
    if (!fn->first_block) {
        fn->first_block = block;
        fn->last_block = block;
    } else {
        fn->last_block->next = block;
        fn->last_block = block;
    }
}

static inline MIRValue emit_const_float(MIRBuilder* b, double v) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)calloc(1, sizeof(MIRInst));
    i->type = MIR_INST_CONST;
    i->as.constant.dst = dst;
    i->as.constant.value = mir_float(v);
    emit_inst(b, i);
    return dst;
}

static inline MIRValue emit_const_bool(MIRBuilder* b, bool v) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)calloc(1, sizeof(MIRInst));
    i->type = MIR_INST_CONST;
    i->as.constant.dst = dst;
    i->as.constant.value = mir_bool(v);
    emit_inst(b, i);
    return dst;
}

static inline MIRValue emit_const_string(MIRBuilder* b, const char* v) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)calloc(1, sizeof(MIRInst));
    i->type = MIR_INST_CONST;
    i->as.constant.dst = dst;
    i->as.constant.value = mir_string(v);
    emit_inst(b, i);
    return dst;
}

static inline void emit_print(MIRBuilder* b, MIRValue v) {
    MIRInst* i = (MIRInst*)calloc(1, sizeof(MIRInst));
    i->type = MIR_INST_PRINT;
    i->as.print.value = v;
    emit_inst(b, i);
}

static inline MIRValue emit_input(MIRBuilder* b) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)calloc(1, sizeof(MIRInst));
    i->type = MIR_INST_INPUT;
    i->as.input.dst = dst;
    emit_inst(b, i);
    return dst;
}

static inline void emit_pixel(MIRBuilder* b, MIRValue x, MIRValue y, MIRValue color) {
    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_PIXEL;
    i->as.pixel.x = x;
    i->as.pixel.y = y;
    i->as.pixel.color = color;
    emit_inst(b, i);
}

static inline MIRValue emit_arr(MIRBuilder* b, MIRValue size) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_ARRAY_NEW;
    i->as.array_new.dst = dst;
    i->as.array_new.size = size;
    emit_inst(b, i);
    return dst;
}

static inline MIRValue emit_arr_get(MIRBuilder* b, MIRValue arr, MIRValue idx) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_ARRAY_GET;
    i->as.array_get.dst = dst;
    i->as.array_get.array = arr;
    i->as.array_get.index = idx;
    emit_inst(b, i);
    return dst;
}

static inline void emit_arr_set(MIRBuilder* b, MIRValue arr, MIRValue idx, MIRValue val) {
    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_ARRAY_SET;
    i->as.array_set.array = arr;
    i->as.array_set.index = idx;
    i->as.array_set.value = val;
    emit_inst(b, i);
}

static inline MIRValue emit_arr_len(MIRBuilder* b, MIRValue arr) {
    MIRValue dst = mir_new_temp(b);
    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_ARRAY_LEN;
    i->as.array_len.dst = dst;
    i->as.array_len.array = arr;
    emit_inst(b, i);
    return dst;
}

static inline void emit_declare(MIRBuilder* b, const char* name, bool is_global) {
    MIRInst* i = (MIRInst*)calloc(1, sizeof(MIRInst));
    i->type = MIR_INST_DECLARE;
    i->as.declare.name = name;
    i->as.declare.is_global = is_global;
    emit_inst(b, i);
}

// ============================================================================
// PHASE 1: LAMBDA LIFTING
// ============================================================================
static int lambda_counter = 0;
static ASTNode* lifted_lambdas_head = NULL;
static ASTNode* lifted_lambdas_tail = NULL;

static int get_struct_field_index(ASTNode* struct_decl, const char* field_name) {
    if (!struct_decl || struct_decl->type != NODE_STRUCT_DECL) return -1;
    int idx = 0;
    ASTNode* field = struct_decl->as.struct_decl.fields;
    while (field) {
        if (strcmp(field->as.struct_field.name, field_name) == 0) return idx;
        idx++;
        field = field->next;
    }
    return -1;
}

static void walk_ast(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_PROGRAM: walk_ast(node->as.program.body); break;
        case NODE_FUNCTION:
            if (node->as.function.is_lambda) {
                char name[64];
                sprintf(name, "__lambda_%d", lambda_counter++);
                node->as.function.name = ast_strdup(name);
                node->as.function.is_lambda = false;
                
                ASTNode* lifted = create_node(NODE_FUNCTION);
                lifted->as = node->as;
                lifted->loc = node->loc;
                lifted->next = NULL;
                
                if (!lifted_lambdas_head) lifted_lambdas_head = lifted;
                else lifted_lambdas_tail->next = lifted;
                lifted_lambdas_tail = lifted;
                
                node->type = NODE_IDENTIFIER;
                node->as.ident.name = ast_strdup(name);
                
                walk_ast(lifted->as.function.body);
            } else {
                walk_ast(node->as.function.body);
            }
            break;
        case NODE_LET: walk_ast(node->as.let.value); break;
        case NODE_ASSIGN: walk_ast(node->as.assign.target); walk_ast(node->as.assign.value); break;
        case NODE_IF: walk_ast(node->as.if_expr.cond); walk_ast(node->as.if_expr.then_body); walk_ast(node->as.if_expr.else_body); break;
        case NODE_FOR: walk_ast(node->as.for_expr.init); walk_ast(node->as.for_expr.cond); walk_ast(node->as.for_expr.step); walk_ast(node->as.for_expr.body); walk_ast(node->as.for_expr.else_body); break;
        case NODE_FOREACH: walk_ast(node->as.foreach_expr.iterator); walk_ast(node->as.foreach_expr.body); walk_ast(node->as.foreach_expr.else_body); break;
        case NODE_LOOP: walk_ast(node->as.loop_expr.body); walk_ast(node->as.loop_expr.else_body); break;
        case NODE_RETURN: walk_ast(node->as.return_stmt.value); break;
        case NODE_BINARY_OP: walk_ast(node->as.binop.left); walk_ast(node->as.binop.right); break;
        case NODE_UNARY_OP: walk_ast(node->as.unary.operand); break;
        case NODE_CALL: walk_ast(node->as.call.callee); walk_ast(node->as.call.args); break;
        case NODE_SCOPE: walk_ast(node->as.scope.body); break;
        case NODE_STRUCT_LITERAL: walk_ast(node->as.struct_lit.fields); break;
        case NODE_MEMBER_ACCESS: walk_ast(node->as.member.object); walk_ast(node->as.member.args); break;
        default: break;
    }
    walk_ast(node->next);
}

static void lift_lambdas(ASTNode* program) {
    if (program->type != NODE_PROGRAM) return;
    lifted_lambdas_head = NULL;
    lifted_lambdas_tail = NULL;
    lambda_counter = 0;
    walk_ast(program->as.program.body);
    
    if (lifted_lambdas_head) {
        ASTNode* p = program->as.program.body;
        while (p && p->next) p = p->next;
        if (p) p->next = lifted_lambdas_head;
        else program->as.program.body = lifted_lambdas_head;
    }
}

// ============================================================================
// PHASE 2: MIR LOWERING
// ============================================================================
static MIRValue lower_expr(ASTNode* expr, MIRBuilder* b);
static void lower_stmt(ASTNode* stmt, MIRBuilder* b);

static MIRValue lower_expr(ASTNode* expr, MIRBuilder* b) {
    if (!expr) return mir_temp(0);

    switch (expr->type) {
        case NODE_INT_LITERAL: return emit_const_int(b, expr->as.int_lit.value);
        case NODE_FLOAT_LITERAL: return emit_const_float(b, expr->as.float_lit.value);
        case NODE_BOOL_LITERAL: return emit_const_bool(b, expr->as.bool_lit.value);
        case NODE_STRING_LITERAL: return emit_const_string(b, expr->as.string_lit.value);
        case NODE_IDENTIFIER: {
            return mir_local(expr->as.ident.name);
        }
        case NODE_BINARY_OP: {
            MIRValue lhs = lower_expr(expr->as.binop.left, b);
            MIRValue rhs = lower_expr(expr->as.binop.right, b);
            MIRBinaryOp op = MIR_BIN_ADD;
            if (strcmp(expr->as.binop.op, "+") == 0) op = MIR_BIN_ADD;
            else if (strcmp(expr->as.binop.op, "-") == 0) op = MIR_BIN_SUB;
            else if (strcmp(expr->as.binop.op, "*") == 0) op = MIR_BIN_MUL;
            else if (strcmp(expr->as.binop.op, "/") == 0) op = MIR_BIN_DIV;
            else if (strcmp(expr->as.binop.op, "%") == 0) op = MIR_BIN_MOD;
            else if (strcmp(expr->as.binop.op, "**") == 0) op = MIR_BIN_POW;
            else if (strcmp(expr->as.binop.op, "==") == 0) op = MIR_BIN_EQ;
            else if (strcmp(expr->as.binop.op, "!=") == 0) op = MIR_BIN_NEQ;
            else if (strcmp(expr->as.binop.op, ">") == 0) op = MIR_BIN_GT;
            else if (strcmp(expr->as.binop.op, ">=") == 0) op = MIR_BIN_GTE;
            else if (strcmp(expr->as.binop.op, "<") == 0) op = MIR_BIN_LT;
            else if (strcmp(expr->as.binop.op, "<=") == 0) op = MIR_BIN_LTE;
            else if (strcmp(expr->as.binop.op, "&&") == 0) op = MIR_BIN_AND;
            else if (strcmp(expr->as.binop.op, "||") == 0) op = MIR_BIN_OR;
            else if (strcmp(expr->as.binop.op, "^") == 0) op = MIR_BIN_XOR;
            
            return emit_binary(b, op, lhs, rhs);
        }
        case NODE_UNARY_OP: {
            MIRValue op = lower_expr(expr->as.unary.operand, b);
            if (strcmp(expr->as.unary.op, "-") == 0) {
                MIRValue zero = emit_const_int(b, 0);
                return emit_binary(b, MIR_BIN_SUB, zero, op);
            } else if (strcmp(expr->as.unary.op, "!") == 0) {
                return emit_unary(b, MIR_UN_NOT, op);
            }
            return op;
        }
        case NODE_STRUCT_LITERAL: {
            ASTNode* struct_decl = expr->evaluates_to_type;
            int num_fields = 0;
            ASTNode* f = struct_decl->as.struct_decl.fields;
            while (f) { num_fields++; f = f->next; }
            MIRValue size_val = emit_const_int(b, num_fields);
            MIRValue arr_val = emit_arr(b, size_val);
            ASTNode* lit_field = expr->as.struct_lit.fields;
            while (lit_field) {
                int idx = get_struct_field_index(struct_decl, lit_field->as.struct_field.name);
                MIRValue val = lower_expr(lit_field->as.struct_field.value, b);
                MIRValue idx_val = emit_const_int(b, idx);
                emit_arr_set(b, arr_val, idx_val, val);
                lit_field = lit_field->next;
            }
            return arr_val;
        }
        case NODE_MEMBER_ACCESS: {
            MIRValue object_val = lower_expr(expr->as.member.object, b);
            ASTNode* struct_decl = expr->as.member.object->evaluates_to_type;
            int idx = get_struct_field_index(struct_decl, expr->as.member.member->as.ident.name);
            MIRValue idx_val = emit_const_int(b, idx);
            return emit_arr_get(b, object_val, idx_val);
        }
        case NODE_ARRAY_ACCESS: {
            MIRValue array_val = lower_expr(expr->as.array_access.array, b);
            MIRValue index_val = lower_expr(expr->as.array_access.index, b);
            return emit_arr_get(b, array_val, index_val);
        }
        case NODE_CALL: {
            const char* fname = expr->as.call.debug_name;
            ASTNode* arg = expr->as.call.args;
            int argc = 0;
            while (arg) { argc++; arg = arg->next; }
            MIRValue* args = argc > 0 ? (MIRValue*)malloc(sizeof(MIRValue) * argc) : NULL;
            arg = expr->as.call.args;
            for (int i = 0; i < argc; i++) {
                args[i] = lower_expr(arg, b);
                arg = arg->next;
            }
            
            if (strcmp(fname, "print") == 0) {
                if (argc > 0) emit_print(b, args[0]);
                if (args) free(args);
                return mir_int(0); 
            } else if (strcmp(fname, "input") == 0) {
                MIRValue ret = emit_input(b);
                if (args) free(args);
                return ret;
            } else if (strcmp(fname, "pixel") == 0) {
                if (argc >= 3) emit_pixel(b, args[0], args[1], args[2]);
                if (args) free(args);
                return mir_int(0);
            } else if (strcmp(fname, "arr") == 0) {
                MIRValue ret = emit_arr(b, args[0]);
                if (args) free(args);
                return ret;
            } else if (strcmp(fname, "arr_get") == 0) {
                MIRValue ret = emit_arr_get(b, args[0], args[1]);
                if (args) free(args);
                return ret;
            } else if (strcmp(fname, "arr_set") == 0) {
                emit_arr_set(b, args[0], args[1], args[2]);
                if (args) free(args);
                return mir_int(0);
            } else {
                return emit_call(b, fname, args, argc);
            }
        }
        case NODE_IF: {
            static int if_counter_expr = 0;
            char then_name[32], else_name[32], end_name[32];
            sprintf(then_name, "if_then_e_%d", if_counter_expr);
            sprintf(else_name, "if_else_e_%d", if_counter_expr);
            sprintf(end_name, "if_end_e_%d", if_counter_expr++);
            
            MIRValue cond = lower_expr(expr->as.if_expr.cond, b);
            MIRBlock* then_blk = create_block(ast_strdup(then_name));
            MIRBlock* else_blk = create_block(ast_strdup(else_name));
            MIRBlock* end_blk = create_block(ast_strdup(end_name));
            
            emit_branch(b, cond, then_blk, else_blk);
            append_block(b->current_function, then_blk);
            append_block(b->current_function, else_blk);
            append_block(b->current_function, end_blk);
            
            MIRValue res = mir_new_temp(b);
            
            set_block(b, then_blk);
            MIRValue then_val = lower_expr(expr->as.if_expr.then_body, b);
            emit_move(b, res, then_val);
            if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_goto(b, end_blk);
            }
            
            set_block(b, else_blk);
            if (expr->as.if_expr.else_body) {
                MIRValue else_val = lower_expr(expr->as.if_expr.else_body, b);
                emit_move(b, res, else_val);
            } else {
                MIRValue zero = emit_const_int(b, 0);
                emit_move(b, res, zero);
            }
            if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_goto(b, end_blk);
            }
            
            set_block(b, end_blk);
            return res;
        }
        case NODE_SCOPE: {
            ASTNode* child = expr->as.scope.body;
            MIRValue last_val = mir_int(0);
            while (child) {
                if (!child->next && child->type == NODE_RETURN && !child->as.return_stmt.is_explicit) {
                    if (child->as.return_stmt.value) {
                        last_val = lower_expr(child->as.return_stmt.value, b);
                    }
                } else {
                    lower_stmt(child, b);
                }
                child = child->next;
            }
            return last_val;
        }
        default:
            return mir_int(0);
    }
}

static void lower_stmt(ASTNode* stmt, MIRBuilder* b) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case NODE_LET: {
            MIRValue val = lower_expr(stmt->as.let.value, b);
            emit_declare(b, stmt->as.let.name, false);
            emit_move(b, mir_local(stmt->as.let.name), val);
            break;
        }
        case NODE_ASSIGN: {
            MIRValue val = lower_expr(stmt->as.assign.value, b);
            const char* op = stmt->as.assign.op;
            
            if (stmt->as.assign.target->type == NODE_ARRAY_ACCESS) {
                MIRValue array_val = lower_expr(stmt->as.assign.target->as.array_access.array, b);
                MIRValue index_val = lower_expr(stmt->as.assign.target->as.array_access.index, b);
                
                if (strcmp(op, "=") == 0) {
                    emit_arr_set(b, array_val, index_val, val);
                } else {
                    MIRValue curr_val = emit_arr_get(b, array_val, index_val);
                    MIRBinaryOp binop = MIR_BIN_ADD;
                    if (strcmp(op, "+=") == 0) binop = MIR_BIN_ADD;
                    else if (strcmp(op, "-=") == 0) binop = MIR_BIN_SUB;
                    else if (strcmp(op, "*=") == 0) binop = MIR_BIN_MUL;
                    else if (strcmp(op, "/=") == 0) binop = MIR_BIN_DIV;
                    else if (strcmp(op, "%=") == 0) binop = MIR_BIN_MOD;
                    
                    MIRValue res = emit_binary(b, binop, curr_val, val);
                    emit_arr_set(b, array_val, index_val, res);
                }
                break;
            } else if (stmt->as.assign.target->type == NODE_MEMBER_ACCESS) {
                MIRValue object_val = lower_expr(stmt->as.assign.target->as.member.object, b);
                ASTNode* struct_decl = stmt->as.assign.target->as.member.object->evaluates_to_type;
                int idx = get_struct_field_index(struct_decl, stmt->as.assign.target->as.member.member->as.ident.name);
                MIRValue idx_val = emit_const_int(b, idx);
                
                if (strcmp(op, "=") == 0) {
                    emit_arr_set(b, object_val, idx_val, val);
                } else {
                    MIRValue curr_val = emit_arr_get(b, object_val, idx_val);
                    MIRBinaryOp binop = MIR_BIN_ADD;
                    if (strcmp(op, "+=") == 0) binop = MIR_BIN_ADD;
                    else if (strcmp(op, "-=") == 0) binop = MIR_BIN_SUB;
                    else if (strcmp(op, "*=") == 0) binop = MIR_BIN_MUL;
                    else if (strcmp(op, "/=") == 0) binop = MIR_BIN_DIV;
                    else if (strcmp(op, "%=") == 0) binop = MIR_BIN_MOD;
                    
                    MIRValue res = emit_binary(b, binop, curr_val, val);
                    emit_arr_set(b, object_val, idx_val, res);
                }
                break;
            }
            
            const char* target = stmt->as.assign.target->as.ident.name;
            
            if (strcmp(op, "=") == 0) {
                emit_move(b, mir_local(target), val);
            } else {
                MIRBinaryOp binop = MIR_BIN_ADD;
                if (strcmp(op, "+=") == 0) binop = MIR_BIN_ADD;
                else if (strcmp(op, "-=") == 0) binop = MIR_BIN_SUB;
                else if (strcmp(op, "*=") == 0) binop = MIR_BIN_MUL;
                else if (strcmp(op, "/=") == 0) binop = MIR_BIN_DIV;
                else if (strcmp(op, "%=") == 0) binop = MIR_BIN_MOD;
                
                MIRValue res = emit_binary(b, binop, mir_local(target), val);
                emit_move(b, mir_local(target), res);
            }
            break;
        }
        case NODE_IF: {
            static int if_counter_stmt = 0;
            char then_name[32], else_name[32], end_name[32];
            sprintf(then_name, "if_then_s_%d", if_counter_stmt);
            sprintf(else_name, "if_else_s_%d", if_counter_stmt);
            sprintf(end_name, "if_end_s_%d", if_counter_stmt++);
            
            MIRValue cond = lower_expr(stmt->as.if_expr.cond, b);
            MIRBlock* then_blk = create_block(ast_strdup(then_name));
            MIRBlock* else_blk = create_block(ast_strdup(else_name));
            MIRBlock* end_blk = create_block(ast_strdup(end_name));
            
            emit_branch(b, cond, then_blk, stmt->as.if_expr.else_body ? else_blk : end_blk);
            append_block(b->current_function, then_blk);
            if (stmt->as.if_expr.else_body) append_block(b->current_function, else_blk);
            append_block(b->current_function, end_blk);
            
            set_block(b, then_blk);
            lower_stmt(stmt->as.if_expr.then_body, b);
            if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_goto(b, end_blk);
            }
            
            if (stmt->as.if_expr.else_body) {
                set_block(b, else_blk);
                lower_stmt(stmt->as.if_expr.else_body, b);
                if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                    emit_goto(b, end_blk);
                }
            }
            
            set_block(b, end_blk);
            break;
        }
        case NODE_FOR: {
            if (stmt->as.for_expr.init) lower_stmt(stmt->as.for_expr.init, b);
            
            static int loop_counter = 0;
            char loop_cond_name[32], loop_body_name[32], loop_end_name[32];
            sprintf(loop_cond_name, "loop_cond_%d", loop_counter);
            sprintf(loop_body_name, "loop_body_%d", loop_counter);
            sprintf(loop_end_name, "loop_end_%d", loop_counter++);
            
            MIRBlock* cond_blk = create_block(ast_strdup(loop_cond_name));
            MIRBlock* body_blk = create_block(ast_strdup(loop_body_name));
            MIRBlock* end_blk = create_block(ast_strdup(loop_end_name));
            
            append_block(b->current_function, cond_blk);
            append_block(b->current_function, body_blk);
            append_block(b->current_function, end_blk);
            
            emit_goto(b, cond_blk);
            set_block(b, cond_blk);
            
            MIRBlock* saved_break = b->break_target;
            MIRBlock* saved_continue = b->continue_target;
            b->break_target = end_blk;
            b->continue_target = cond_blk;
            
            if (stmt->as.for_expr.cond) {
                MIRValue cond = lower_expr(stmt->as.for_expr.cond, b);
                emit_branch(b, cond, body_blk, end_blk);
            } else {
                emit_goto(b, body_blk);
            }
            
            set_block(b, body_blk);
            lower_stmt(stmt->as.for_expr.body, b);
            if (stmt->as.for_expr.step) lower_stmt(stmt->as.for_expr.step, b);
            if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_goto(b, cond_blk);
            }
            
            b->break_target = saved_break;
            b->continue_target = saved_continue;
            
            set_block(b, end_blk);
            break;
        }
        case NODE_LOOP: {
            static int loop_counter = 0;
            char loop_body_name[32], loop_end_name[32];
            sprintf(loop_body_name, "loop_body_%d", loop_counter);
            sprintf(loop_end_name, "loop_end_%d", loop_counter++);
            
            MIRBlock* body_blk = create_block(ast_strdup(loop_body_name));
            MIRBlock* end_blk = create_block(ast_strdup(loop_end_name));
            
            append_block(b->current_function, body_blk);
            append_block(b->current_function, end_blk);
            
            emit_goto(b, body_blk);
            set_block(b, body_blk);
            
            MIRBlock* saved_break = b->break_target;
            b->break_target = end_blk;
            
            lower_stmt(stmt->as.loop_expr.body, b);
            if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_goto(b, body_blk);
            }
            
            b->break_target = saved_break;
            
            set_block(b, end_blk);
            break;
        }
        case NODE_BREAK: {
            emit_goto(b, b->break_target);
            break;
        }
        case NODE_FOREACH: {
            // Desugar: for ident in expr { body }
            // Into:    let __arr = expr; let __len = arr_len(__arr); let __idx = 0;
            //          loop { if __idx >= __len break; let ident = __arr[__idx]; body; __idx++ }
            static int foreach_counter = 0;
            char cond_name[48], body_name[48], end_name[48];
            sprintf(cond_name, "foreach_cond_%d", foreach_counter);
            sprintf(body_name, "foreach_body_%d", foreach_counter);
            sprintf(end_name, "foreach_end_%d", foreach_counter++);
            
            // Evaluate the iterable expression
            MIRValue arr_val = lower_expr(stmt->as.foreach_expr.iterator, b);
            // Get the length of the array
            MIRValue len_val = emit_arr_len(b, arr_val);
            // Initialize the index counter to 0
            MIRValue idx_val = mir_new_temp(b);
            MIRValue idx_init = emit_const_int(b, 0);
            emit_move(b, idx_val, idx_init);
            
            MIRBlock* cond_blk = create_block(ast_strdup(cond_name));
            MIRBlock* body_blk = create_block(ast_strdup(body_name));
            MIRBlock* end_blk  = create_block(ast_strdup(end_name));
            
            append_block(b->current_function, cond_blk);
            append_block(b->current_function, body_blk);
            append_block(b->current_function, end_blk);
            
            emit_goto(b, cond_blk);
            set_block(b, cond_blk);
            
            MIRBlock* saved_break = b->break_target;
            MIRBlock* saved_continue = b->continue_target;
            b->break_target = end_blk;
            b->continue_target = cond_blk;
            
            // Condition: __idx < __len
            MIRValue cmp = emit_binary(b, MIR_BIN_LT, idx_val, len_val);
            emit_branch(b, cmp, body_blk, end_blk);
            
            set_block(b, body_blk);
            
            // Bind the loop variable: let ident = arr[idx]
            const char* ident_name = stmt->as.foreach_expr.binded_term->as.ident.name;
            MIRValue elem = emit_arr_get(b, arr_val, idx_val);
            emit_declare(b, ident_name, false);
            MIRValue named = mir_local(ident_name);
            emit_move(b, named, elem);
            
            // Lower the body
            lower_stmt(stmt->as.foreach_expr.body, b);
            
            // Increment __idx
            MIRValue one = mir_int(1);
            MIRValue next_idx = emit_binary(b, MIR_BIN_ADD, idx_val, one);
            emit_move(b, idx_val, next_idx);
            
            if (b->current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_goto(b, cond_blk);
            }
            
            b->break_target = saved_break;
            b->continue_target = saved_continue;
            
            set_block(b, end_blk);
            break;
        }
        case NODE_RETURN: {
            if (!stmt->as.return_stmt.is_explicit) {
                if (stmt->as.return_stmt.value) {
                    lower_expr(stmt->as.return_stmt.value, b);
                }
                break;
            }
            MIRValue val = mir_int(0);
            if (stmt->as.return_stmt.value) {
                val = lower_expr(stmt->as.return_stmt.value, b);
            }
            emit_return(b, val, stmt->as.return_stmt.value != NULL);
            break;
        }
        case NODE_SCOPE: {
            ASTNode* child = stmt->as.scope.body;
            while (child) {
                lower_stmt(child, b);
                child = child->next;
            }
            break;
        }
        default:
            lower_expr(stmt, b);
            break;
    }
}

// ============================================================================
// PHASE 3: FIS25 EMISSION
// ============================================================================
static void emit_fis25(MIRModule* mod, const char* filename) {
    FILE* out = fopen(filename, "w");
    if (!out) return;

    MIRFunction* fn = mod->first_function;
    while (fn) {
        if (strcmp(fn->name, "print") == 0 || strcmp(fn->name, "pixel") == 0) {
            fn = fn->next;
            continue;
        }
        
        if (strcmp(fn->name, "__init__") != 0) {
            fprintf(out, "LABEL %s\n", fn->name);
        }
        
        MIRBlock* blk = fn->first_block;
        while (blk) {
            if (strcmp(blk->label, "entry") != 0) {
                fprintf(out, "LABEL %s_%s\n", fn->name, blk->label);
            }
            
            MIRInst* inst = blk->first_inst;
            while (inst) {
                switch (inst->type) {
                    case MIR_INST_CONST: {
                        if (inst->as.constant.value.kind == MIR_VALUE_INT) {
                            fprintf(out, "ASSIGN %ld __t%u\n", inst->as.constant.value.int_value, inst->as.constant.dst.temp_id);
                        } else if (inst->as.constant.value.kind == MIR_VALUE_FLOAT) {
                            fprintf(out, "ASSIGN %f __t%u\n", inst->as.constant.value.float_value, inst->as.constant.dst.temp_id);
                        } else if (inst->as.constant.value.kind == MIR_VALUE_BOOL) {
                            fprintf(out, "ASSIGN %s __t%u\n", inst->as.constant.value.bool_value ? "true" : "false", inst->as.constant.dst.temp_id);
                        } else if (inst->as.constant.value.kind == MIR_VALUE_STRING) {
                            fprintf(out, "ASSIGN \"%s\" __t%u\n", inst->as.constant.value.string_value, inst->as.constant.dst.temp_id);
                        }
                        break;
                    }
                    case MIR_INST_MOVE: {
                        char dst[64]; char src[64];
                        if (inst->as.move.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.move.dst.temp_id);
                        else sprintf(dst, "%s", inst->as.move.dst.local_name);
                        
                        if (inst->as.move.src.kind == MIR_VALUE_TEMP) sprintf(src, "__t%u", inst->as.move.src.temp_id);
                        else if (inst->as.move.src.kind == MIR_VALUE_INT) sprintf(src, "%ld", inst->as.move.src.int_value);
                        else sprintf(src, "%s", inst->as.move.src.local_name);
                        
                        fprintf(out, "ASSIGN %s %s\n", src, dst);
                        break;
                    }
                    case MIR_INST_BINARY: {
                        char lhs[64]; char rhs[64]; char dst[64];
                        if (inst->as.binary.lhs.kind == MIR_VALUE_TEMP) sprintf(lhs, "__t%u", inst->as.binary.lhs.temp_id);
                        else if (inst->as.binary.lhs.kind == MIR_VALUE_INT) sprintf(lhs, "%ld", inst->as.binary.lhs.int_value);
                        else sprintf(lhs, "%s", inst->as.binary.lhs.local_name);
                        
                        if (inst->as.binary.rhs.kind == MIR_VALUE_TEMP) sprintf(rhs, "__t%u", inst->as.binary.rhs.temp_id);
                        else if (inst->as.binary.rhs.kind == MIR_VALUE_INT) sprintf(rhs, "%ld", inst->as.binary.rhs.int_value);
                        else sprintf(rhs, "%s", inst->as.binary.rhs.local_name);
                        
                        if (inst->as.binary.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.binary.dst.temp_id);
                        else sprintf(dst, "%s", inst->as.binary.dst.local_name);
                        
                        const char* op_str = "ADD";
                        switch (inst->as.binary.op) {
                            case MIR_BIN_ADD: op_str = "ADD"; break;
                            case MIR_BIN_SUB: op_str = "SUB"; break;
                            case MIR_BIN_MUL: op_str = "MUL"; break;
                            case MIR_BIN_DIV: op_str = "DIV"; break;
                            case MIR_BIN_MOD: op_str = "MOD"; break;
                            case MIR_BIN_POW: op_str = "POW"; break;
                            case MIR_BIN_EQ: op_str = "EQ"; break;
                            case MIR_BIN_NEQ: op_str = "NEQ"; break;
                            case MIR_BIN_GT: op_str = "GT"; break;
                            case MIR_BIN_GTE: op_str = "GTE"; break;
                            case MIR_BIN_LT: op_str = "LT"; break;
                            case MIR_BIN_LTE: op_str = "LTE"; break;
                            case MIR_BIN_AND: op_str = "AND"; break;
                            case MIR_BIN_OR: op_str = "OR"; break;
                            case MIR_BIN_XOR: op_str = "XOR"; break;
                        }
                        fprintf(out, "%s %s %s %s\n", op_str, lhs, rhs, dst);
                        break;
                    }
                    case MIR_INST_UNARY: {
                        char opnd[64]; char dst[64];
                        if (inst->as.unary.operand.kind == MIR_VALUE_TEMP) sprintf(opnd, "__t%u", inst->as.unary.operand.temp_id);
                        else sprintf(opnd, "%s", inst->as.unary.operand.local_name);
                        
                        if (inst->as.unary.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.unary.dst.temp_id);
                        else sprintf(dst, "%s", inst->as.unary.dst.local_name);
                        
                        if (inst->as.unary.op == MIR_UN_NOT) {
                            fprintf(out, "NOT %s %s\n", opnd, dst);
                        } else {
                            fprintf(out, "SUB 0 %s %s\n", opnd, dst);
                        }
                        break;
                    }
                    case MIR_INST_CALL: {
                        char dst[64];
                        if (inst->as.call.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.call.dst.temp_id);
                        else sprintf(dst, "%s", inst->as.call.dst.local_name);
                        
                        for (int i = 0; i < inst->as.call.arg_count; i++) {
                            char arg[64];
                            if (inst->as.call.args[i].kind == MIR_VALUE_TEMP) sprintf(arg, "__t%u", inst->as.call.args[i].temp_id);
                            else if (inst->as.call.args[i].kind == MIR_VALUE_INT) sprintf(arg, "%ld", inst->as.call.args[i].int_value);
                            else sprintf(arg, "%s", inst->as.call.args[i].local_name);
                            fprintf(out, "PARAM %s\n", arg);
                        }
                        
                        fprintf(out, "GOSUB %s %s\n", inst->as.call.function_name, dst);
                        break;
                    }
                    case MIR_INST_PARAM_GET: {
                        char dst[64];
                        if (inst->as.param_get.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.param_get.dst.temp_id);
                        else sprintf(dst, "%s", inst->as.param_get.dst.local_name);
                        fprintf(out, "PARAM_GET %s\n", dst);
                        break;
                    }
                    case MIR_INST_DECLARE: {
                        fprintf(out, "VAR %s\n", inst->as.declare.name);
                        break;
                    }
                    case MIR_INST_PRINT: {
                        char val[64];
                        if (inst->as.print.value.kind == MIR_VALUE_TEMP) sprintf(val, "__t%u", inst->as.print.value.temp_id);
                        else if (inst->as.print.value.kind == MIR_VALUE_STRING) sprintf(val, "\"%s\"", inst->as.print.value.string_value);
                        else if (inst->as.print.value.kind == MIR_VALUE_INT) sprintf(val, "%ld", inst->as.print.value.int_value);
                        else sprintf(val, "%s", inst->as.print.value.local_name);
                        fprintf(out, "PRINT %s\n", val);
                        break;
                    }
                    case MIR_INST_INPUT: {
                        char dst[64];
                        if (inst->as.input.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.input.dst.temp_id);
                        else sprintf(dst, "%s", inst->as.input.dst.local_name);
                        fprintf(out, "INPUT %s\n", dst);
                        break;
                    }
                    case MIR_INST_PIXEL: {
                        char x[64], y[64], c[64];
                        if (inst->as.pixel.x.kind == MIR_VALUE_TEMP) sprintf(x, "__t%u", inst->as.pixel.x.temp_id); else sprintf(x, "%s", inst->as.pixel.x.local_name);
                        if (inst->as.pixel.y.kind == MIR_VALUE_TEMP) sprintf(y, "__t%u", inst->as.pixel.y.temp_id); else sprintf(y, "%s", inst->as.pixel.y.local_name);
                        if (inst->as.pixel.color.kind == MIR_VALUE_TEMP) sprintf(c, "__t%u", inst->as.pixel.color.temp_id); else sprintf(c, "%s", inst->as.pixel.color.local_name);
                        fprintf(out, "PIXEL %s %s %s\n", x, y, c);
                        break;
                    }
                    case MIR_INST_ARRAY_NEW: {
                        char dst[64], sz[64];
                        if (inst->as.array_new.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.array_new.dst.temp_id); else sprintf(dst, "%s", inst->as.array_new.dst.local_name);
                        if (inst->as.array_new.size.kind == MIR_VALUE_TEMP) sprintf(sz, "__t%u", inst->as.array_new.size.temp_id); else if (inst->as.array_new.size.kind == MIR_VALUE_INT) sprintf(sz, "%ld", inst->as.array_new.size.int_value); else sprintf(sz, "%s", inst->as.array_new.size.local_name);
                        fprintf(out, "ARR %s %s\n", sz, dst);
                        break;
                    }
                    case MIR_INST_ARRAY_GET: {
                        char dst[64], arr[64], idx[64];
                        if (inst->as.array_get.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.array_get.dst.temp_id); else sprintf(dst, "%s", inst->as.array_get.dst.local_name);
                        if (inst->as.array_get.array.kind == MIR_VALUE_TEMP) sprintf(arr, "__t%u", inst->as.array_get.array.temp_id); else sprintf(arr, "%s", inst->as.array_get.array.local_name);
                        if (inst->as.array_get.index.kind == MIR_VALUE_TEMP) sprintf(idx, "__t%u", inst->as.array_get.index.temp_id); else if (inst->as.array_get.index.kind == MIR_VALUE_INT) sprintf(idx, "%ld", inst->as.array_get.index.int_value); else sprintf(idx, "%s", inst->as.array_get.index.local_name);
                        fprintf(out, "ARR_GET %s %s %s\n", arr, idx, dst);
                        break;
                    }
                    case MIR_INST_ARRAY_SET: {
                        char arr[64], idx[64], val[64];
                        if (inst->as.array_set.array.kind == MIR_VALUE_TEMP) sprintf(arr, "__t%u", inst->as.array_set.array.temp_id); else sprintf(arr, "%s", inst->as.array_set.array.local_name);
                        if (inst->as.array_set.index.kind == MIR_VALUE_TEMP) sprintf(idx, "__t%u", inst->as.array_set.index.temp_id); else if (inst->as.array_set.index.kind == MIR_VALUE_INT) sprintf(idx, "%ld", inst->as.array_set.index.int_value); else sprintf(idx, "%s", inst->as.array_set.index.local_name);
                        if (inst->as.array_set.value.kind == MIR_VALUE_TEMP) sprintf(val, "__t%u", inst->as.array_set.value.temp_id); else if (inst->as.array_set.value.kind == MIR_VALUE_INT) sprintf(val, "%ld", inst->as.array_set.value.int_value); else sprintf(val, "%s", inst->as.array_set.value.local_name);
                        fprintf(out, "ARR_SET %s %s %s\n", arr, idx, val);
                        break;
                    }
                    case MIR_INST_ARRAY_LEN: {
                        char dst[64], arr[64];
                        if (inst->as.array_len.dst.kind == MIR_VALUE_TEMP) sprintf(dst, "__t%u", inst->as.array_len.dst.temp_id); else sprintf(dst, "%s", inst->as.array_len.dst.local_name);
                        if (inst->as.array_len.array.kind == MIR_VALUE_TEMP) sprintf(arr, "__t%u", inst->as.array_len.array.temp_id); else sprintf(arr, "%s", inst->as.array_len.array.local_name);
                        fprintf(out, "ARR_LEN %s %s\n", arr, dst);
                        break;
                    }
                    default: break;
                }
                inst = inst->next;
            }
            
            if (blk->terminator.type == MIR_TERM_GOTO) {
                fprintf(out, "GOTO %s_%s\n", fn->name, blk->terminator.as.goto_term.target->label);
            } else if (blk->terminator.type == MIR_TERM_BRANCH) {
                char cond[64];
                if (blk->terminator.as.branch.condition.kind == MIR_VALUE_TEMP) sprintf(cond, "__t%u", blk->terminator.as.branch.condition.temp_id);
                else sprintf(cond, "%s", blk->terminator.as.branch.condition.local_name);
                fprintf(out, "IF %s GOTO %s_%s\n", cond, fn->name, blk->terminator.as.branch.then_block->label);
                fprintf(out, "GOTO %s_%s\n", fn->name, blk->terminator.as.branch.else_block->label);
            } else if (blk->terminator.type == MIR_TERM_RETURN) {
                char val[64];
                if (blk->terminator.as.ret.has_value) {
                    if (blk->terminator.as.ret.value.kind == MIR_VALUE_TEMP) sprintf(val, "__t%u", blk->terminator.as.ret.value.temp_id);
                    else if (blk->terminator.as.ret.value.kind == MIR_VALUE_INT) sprintf(val, "%ld", blk->terminator.as.ret.value.int_value);
                    else sprintf(val, "%s", blk->terminator.as.ret.value.local_name);
                    
                    if (strcmp(fn->name, "__init__") == 0) fprintf(out, "GOTO main\n");
                    else fprintf(out, "RETURN %s\n", val);
                } else {
                    if (strcmp(fn->name, "__init__") == 0) fprintf(out, "GOTO main\n");
                    else fprintf(out, "RETURN 0\n");
                }
            } else if (blk->terminator.type == MIR_TERM_UNREACHABLE) {
                if (strcmp(fn->name, "__init__") == 0) fprintf(out, "GOTO main\n");
                else fprintf(out, "RETURN 0\n");
            }
            
            blk = blk->next;
        }
        
        fn = fn->next;
        fprintf(out, "\n");
    }
    
    fclose(out);
}

// ============================================================================
// MAIN ENTRY
// ============================================================================
void codegen(const ASTNode* program, const char* filename) {
    if (program->type != NODE_PROGRAM) return;

    lambda_counter = 0;
    lifted_lambdas_head = NULL;
    lifted_lambdas_tail = NULL;

    ASTNode* prog_copy = (ASTNode*)program; 
    lift_lambdas(prog_copy);

    MIRModule mod = { .name = "main", .first_function = NULL, .last_function = NULL };
    
    MIRBuilder init_builder = {0};
    init_builder.module = &mod;
    MIRFunction* init_fn = emit_function(&init_builder, "__init__");
    mod.first_function = init_fn;
    mod.last_function = init_fn;
    
    ASTNode* node = prog_copy->as.program.body;
    while (node) {
        if (node->type == NODE_FUNCTION) {
            MIRBuilder builder = {0};
            builder.module = &mod;
            
            MIRFunction* fn = emit_function(&builder, node->as.function.name);
            
            if (!mod.first_function) {
                mod.first_function = fn;
                mod.last_function = fn;
            } else {
                mod.last_function->next = fn;
                mod.last_function = fn;
            }
            
            ASTNode* p = node->as.function.params;
            int param_count = 0;
            while (p) {
                param_count++;
                p = p->next;
            }
            
            if (param_count > 0) {
                ASTNode** params_arr = malloc(sizeof(ASTNode*) * param_count);
                p = node->as.function.params;
                for (int i = 0; i < param_count; i++) {
                    params_arr[i] = p;
                    p = p->next;
                }
                
                for (int i = param_count - 1; i >= 0; i--) {
                    MIRInst* inst = (MIRInst*)calloc(1, sizeof(MIRInst));
                    inst->type = MIR_INST_PARAM_GET;
                    inst->as.param_get.dst = mir_local(params_arr[i]->as.func_param.name);
                    emit_inst(&builder, inst);
                }
                free(params_arr);
            }
            
            MIRValue ret_val = lower_expr(node->as.function.body, &builder);
            
            if (builder.current_block->terminator.type == MIR_TERM_UNREACHABLE) {
                emit_return(&builder, ret_val, false);
            }
        } else {
            lower_stmt(node, &init_builder);
        }
        node = node->next;
    }

    if (init_builder.current_block->terminator.type == MIR_TERM_UNREACHABLE) {
        emit_return(&init_builder, (MIRValue){0}, false);
    }

    optimize_mir(&mod);
    emit_fis25(&mod, filename);
}

void lowering(const ASTNode* program) {
    (void)program;
}
