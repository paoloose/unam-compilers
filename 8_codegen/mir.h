#pragma once

#include "ast.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/*
===============================================================================
MIR DEFINITIONS
===============================================================================

Design goals:
- CFG-based
- Explicit control flow
- Explicit temporaries
- No recursive AST semantics
- Easy lowering into VM ISA
- Easy optimization passes
- Easy dataflow analysis

Pipeline:

AST
 -> semantic analysis
 -> desugaring
 -> MIR lowering
 -> MIR optimizations
 -> VM lowering
 -> VM instruction serialization

===============================================================================
*/

/*
===============================================================================
FORWARD DECLS
===============================================================================
*/

typedef struct MIRInst MIRInst;
typedef struct MIRTerminator MIRTerminator;
typedef struct MIRBlock MIRBlock;
typedef struct MIRFunction MIRFunction;
typedef struct MIRModule MIRModule;
typedef struct MIRBuilder MIRBuilder;

/*
===============================================================================
SOURCE LOCATION
===============================================================================
*/


typedef struct {
    int line;
    int col;
    int lastcol;
} MIRSourceLoc;

/*
===============================================================================
VALUE SYSTEM
===============================================================================

Everything in MIR operates on MIRValue.

Values may represent:
- temporaries
- constants
- variables
- labels
- function references

===============================================================================
*/

typedef enum {
    MIR_VALUE_NONE,

    MIR_VALUE_TEMP,

    MIR_VALUE_LOCAL,
    MIR_VALUE_GLOBAL,

    MIR_VALUE_INT,
    MIR_VALUE_FLOAT,
    MIR_VALUE_BOOL,
    MIR_VALUE_STRING,

    MIR_VALUE_FUNCTION,
    MIR_VALUE_LABEL,
} MIRValueKind;

typedef struct {
    MIRValueKind kind;

    union {
        uint32_t temp_id;

        const char* local_name;
        const char* global_name;

        int64_t int_value;
        double float_value;
        bool bool_value;

        const char* string_value;

        const char* function_name;
        const char* label_name;
    };
} MIRValue;

/*
===============================================================================
TEMPORARY HELPERS
===============================================================================
*/

typedef uint32_t MIRTemp;

/*
===============================================================================
BINARY OPS
===============================================================================
*/

typedef enum {
    MIR_BIN_ADD,
    MIR_BIN_SUB,
    MIR_BIN_MUL,
    MIR_BIN_DIV,
    MIR_BIN_MOD,
    MIR_BIN_POW,

    MIR_BIN_EQ,
    MIR_BIN_NEQ,

    MIR_BIN_GT,
    MIR_BIN_GTE,
    MIR_BIN_LT,
    MIR_BIN_LTE,

    MIR_BIN_AND,
    MIR_BIN_OR,
    MIR_BIN_XOR,
} MIRBinaryOp;

/*
===============================================================================
UNARY OPS
===============================================================================
*/

typedef enum {
    MIR_UN_NEG,
    MIR_UN_NOT,
} MIRUnaryOp;

/*
===============================================================================
INSTRUCTION TYPES
===============================================================================

Instructions DO NOT encode control flow.

Control flow belongs ONLY in MIRTerminator.

This is EXTREMELY important.

===============================================================================
*/

typedef enum {
    MIR_INST_NOP,

    /*
    dst = immediate
    */
    MIR_INST_CONST,

    /*
    dst = src
    */
    MIR_INST_MOVE,

    /*
    dst = lhs op rhs
    */
    MIR_INST_BINARY,

    /*
    dst = op operand
    */
    MIR_INST_UNARY,

    /*
    dst = call fn(args...)
    */
    MIR_INST_CALL,

    /*
    param push
    */
    MIR_INST_PARAM,

    /*
    dst = param pop
    */
    MIR_INST_PARAM_GET,

    /*
    local/global alloc
    */
    MIR_INST_DECLARE,

    /*
    arrays
    */
    MIR_INST_ARRAY_NEW,
    MIR_INST_ARRAY_GET,
    MIR_INST_ARRAY_SET,
    MIR_INST_ARRAY_LEN,

    /*
    lists
    */
    MIR_INST_LIST_NEW,
    MIR_INST_LIST_ADD,
    MIR_INST_LIST_GET,

    /*
    structs/enums
    */
    MIR_INST_STRUCT_NEW,
    MIR_INST_STRUCT_GET,
    MIR_INST_STRUCT_SET,

    MIR_INST_ENUM_NEW,
    MIR_INST_ENUM_TAG,

    /*
    IO
    */
    MIR_INST_PRINT,
    MIR_INST_INPUT,

    /*
    graphics
    */
    MIR_INST_PIXEL,

    /*
    keyboard
    */
    MIR_INST_KEY,

    /*
    timing
    */
    MIR_INST_TIME,

} MIRInstType;

/*
===============================================================================
INSTRUCTION
===============================================================================
*/

struct MIRInst {
    MIRInstType type;

    MIRSourceLoc loc;

    union {

        /*
        MIR_INST_CONST
        */
        struct {
            MIRValue dst;
            MIRValue value;
        } constant;

        /*
        MIR_INST_MOVE
        */
        struct {
            MIRValue dst;
            MIRValue src;
        } move;

        /*
        MIR_INST_BINARY
        */
        struct {
            MIRBinaryOp op;

            MIRValue dst;
            MIRValue lhs;
            MIRValue rhs;
        } binary;

        /*
        MIR_INST_UNARY
        */
        struct {
            MIRUnaryOp op;

            MIRValue dst;
            MIRValue operand;
        } unary;

        /*
        MIR_INST_CALL
        */
        struct {
            MIRValue dst;
            const char* function_name;
            MIRValue* args;
            uint32_t arg_count;
        } call;

        /*
        MIR_INST_PARAM
        */
        struct {
            MIRValue value;
        } param;

        /*
        MIR_INST_PARAM_GET
        */
        struct {
            MIRValue dst;
        } param_get;

        /*
        MIR_INST_DECLARE
        */
        struct {
            bool is_global;
            const char* name;
        } declare;

        /*
        ARRAY_NEW
        */
        struct {
            MIRValue dst;
            MIRValue size;
        } array_new;

        /*
        ARRAY_GET
        */
        struct {
            MIRValue dst;
            MIRValue array;
            MIRValue index;
        } array_get;

        /*
        ARRAY_SET
        */
        struct {
            MIRValue array;
            MIRValue index;
            MIRValue value;
        } array_set;

        /*
        ARRAY_LEN
        */
        struct {
            MIRValue dst;
            MIRValue array;
        } array_len;

        /*
        LIST_NEW
        */
        struct {
            MIRValue dst;
        } list_new;

        /*
        LIST_ADD
        */
        struct {
            MIRValue list;
            MIRValue value;
        } list_add;

        /*
        LIST_GET
        */
        struct {
            MIRValue dst;
            MIRValue list;
            MIRValue index;
        } list_get;

        /*
        STRUCT_NEW
        */
        struct {
            MIRValue dst;
            const char* struct_name;
        } struct_new;

        /*
        STRUCT_GET
        */
        struct {
            MIRValue dst;
            MIRValue object;
            const char* field_name;
        } struct_get;

        /*
        STRUCT_SET
        */
        struct {
            MIRValue object;
            const char* field_name;
            MIRValue value;
        } struct_set;

        /*
        ENUM_NEW
        */
        struct {
            MIRValue dst;

            const char* enum_name;
            const char* variant_name;

            MIRValue* payloads;
            uint32_t payload_count;
        } enum_new;

        /*
        ENUM_TAG
        */
        struct {
            MIRValue dst;
            MIRValue enum_value;
        } enum_tag;

        /*
        PRINT
        */
        struct {
            MIRValue value;
        } print;

        /*
        INPUT
        */
        struct {
            MIRValue dst;
        } input;

        /*
        PIXEL
        */
        struct {
            MIRValue x;
            MIRValue y;
            MIRValue color;
        } pixel;

        /*
        KEY
        */
        struct {
            MIRValue dst;
            MIRValue keycode;
        } key;

        /*
        TIME
        */
        struct {
            MIRValue dst;
        } time;
    } as;

    MIRInst* next;
};

/*
===============================================================================
TERMINATORS
===============================================================================

EVERY BLOCK MUST END WITH EXACTLY ONE TERMINATOR.

This invariant is critical.

===============================================================================
*/

typedef enum {
    /*
    goto label
    */
    MIR_TERM_GOTO,

    /*
    if cond goto then else goto else
    */
    MIR_TERM_BRANCH,

    /*
    return value
    */
    MIR_TERM_RETURN,

    /*
    unreachable
    */
    MIR_TERM_UNREACHABLE,
} MIRTerminatorType;


struct MIRTerminator {
    MIRTerminatorType type;

    MIRSourceLoc loc;

    union {

        /*
        goto block
        */
        struct {
            MIRBlock* target;
        } goto_term;

        /*
        conditional branch
        */
        struct {
            MIRValue condition;

            MIRBlock* then_block;
            MIRBlock* else_block;
        } branch;

        /*
        return
        */
        struct {
            bool has_value;
            MIRValue value;
        } ret;
    } as;
};

/*
===============================================================================
BASIC BLOCK
===============================================================================

A block:
- owns instructions
- ends with terminator
- has explicit CFG edges

===============================================================================
*/


struct MIRBlock {
    const char* label;

    MIRInst* first_inst;
    MIRInst* last_inst;

    MIRTerminator terminator;

    /*
    linked list in function
    */
    MIRBlock* next;
};

/*
===============================================================================
FUNCTION
===============================================================================
*/

typedef struct MIRParam {
    const char* name;
    MIRValue value;

    struct MIRParam* next;
} MIRParam;

struct MIRFunction {
    const char* name;

    bool is_extern;

    /*
    parameters
    */
    MIRParam* params;
    uint32_t param_count;

    /*
    entry block
    */
    MIRBlock* entry;

    /*
    linked list of blocks
    */
    MIRBlock* first_block;
    MIRBlock* last_block;

    /*
    temp allocator
    */
    uint32_t next_temp_id;

    /*
    linked list in module
    */
    MIRFunction* next;
};

/*
===============================================================================
MODULE
===============================================================================
*/


struct MIRModule {
    const char* name;

    MIRFunction* first_function;
    MIRFunction* last_function;
};

/*
===============================================================================
LOWERING CONTEXT
===============================================================================
*/


struct MIRBuilder {
    MIRModule* module;

    MIRFunction* current_function;
    MIRBlock* current_block;

    /*
    loop lowering support
    */
    MIRBlock* break_target;
    MIRBlock* continue_target;

    /*
    optional break value temp
    */
    MIRValue break_value;
};

/*
===============================================================================
HELPERS
===============================================================================
*/

static inline MIRValue mir_temp(uint32_t id) {
    MIRValue v;
    v.kind = MIR_VALUE_TEMP;
    v.temp_id = id;
    return v;
}

static inline MIRValue mir_int(int64_t x) {
    MIRValue v;
    v.kind = MIR_VALUE_INT;
    v.int_value = x;
    return v;
}

static inline MIRValue mir_bool(bool x) {
    MIRValue v;
    v.kind = MIR_VALUE_BOOL;
    v.bool_value = x;
    return v;
}

static inline MIRValue mir_float(double x) {
    MIRValue v;
    v.kind = MIR_VALUE_FLOAT;
    v.float_value = x;
    return v;
}

static inline MIRValue mir_string(const char* s) {
    MIRValue v;
    v.kind = MIR_VALUE_STRING;
    v.string_value = s;
    return v;
}

static inline MIRValue mir_local(const char* s) {
    MIRValue v;
    v.kind = MIR_VALUE_LOCAL;
    v.local_name = s;
    return v;
}

static inline MIRValue mir_global(const char* s) {
    MIRValue v;
    v.kind = MIR_VALUE_GLOBAL;
    v.global_name = s;
    return v;
}

/*
===============================================================================
TEMP CREATION
===============================================================================
*/

static inline MIRValue mir_new_temp(MIRBuilder* b) {
    return mir_temp(b->current_function->next_temp_id++);
}

/* ============================================================================
   INTERNAL: APPEND INSTRUCTION
============================================================================ */

static inline void emit_inst(MIRBuilder* b, MIRInst* inst) {
    inst->next = NULL;

    if (!b->current_block->first_inst) {
        b->current_block->first_inst = inst;
        b->current_block->last_inst = inst;
    } else {
        b->current_block->last_inst->next = inst;
        b->current_block->last_inst = inst;
    }
}

/* ============================================================================
   EMIT API (CORE)
============================================================================ */

/* CONST */
static inline MIRValue emit_const_int(MIRBuilder* b, int64_t v) {
    MIRValue dst = mir_new_temp(b);

    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_CONST;
    i->as.constant.dst = dst;
    i->as.constant.value = mir_int(v);

    emit_inst(b, i);
    return dst;
}

/* BINARY */
static inline MIRValue emit_binary(MIRBuilder* b, MIRBinaryOp op, MIRValue a, MIRValue c) {
    MIRValue dst = mir_new_temp(b);

    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_BINARY;
    i->as.binary.op = op;
    i->as.binary.dst = dst;
    i->as.binary.lhs = a;
    i->as.binary.rhs = c;

    emit_inst(b, i);
    return dst;
}

/* UNARY */
static inline MIRValue emit_unary(MIRBuilder* b, MIRUnaryOp op, MIRValue v) {
    MIRValue dst = mir_new_temp(b);

    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_UNARY;
    i->as.unary.op = op;
    i->as.unary.dst = dst;
    i->as.unary.operand = v;

    emit_inst(b, i);
    return dst;
}

/* MOVE */
static inline void emit_move(MIRBuilder* b, MIRValue dst, MIRValue src) {
    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_MOVE;
    i->as.move.dst = dst;
    i->as.move.src = src;

    emit_inst(b, i);
}

/* CALL */
static inline MIRValue emit_call(
    MIRBuilder* b,
    const char* fn,
    MIRValue* args,
    uint32_t argc
) {
    MIRValue dst = mir_new_temp(b);

    MIRInst* i = (MIRInst*)malloc(sizeof(MIRInst));
    i->type = MIR_INST_CALL;
    i->as.call.dst = dst;
    i->as.call.function_name = fn;
    i->as.call.args = args;
    i->as.call.arg_count = argc;

    emit_inst(b, i);
    return dst;
}

/* ============================================================================
   CONTROL FLOW EMIT (TERMINATORS)
============================================================================ */

static inline void emit_goto(MIRBuilder* b, MIRBlock* target) {
    b->current_block->terminator.type = MIR_TERM_GOTO;
    b->current_block->terminator.as.goto_term.target = target;
}

static inline void emit_return(MIRBuilder* b, MIRValue v, bool has_value) {
    b->current_block->terminator.type = MIR_TERM_RETURN;
    b->current_block->terminator.as.ret.value = v;
    b->current_block->terminator.as.ret.has_value = has_value;
}

static inline void emit_branch(
    MIRBuilder* b,
    MIRValue cond,
    MIRBlock* t,
    MIRBlock* f
) {
    b->current_block->terminator.type = MIR_TERM_BRANCH;
    b->current_block->terminator.as.branch.condition = cond;
    b->current_block->terminator.as.branch.then_block = t;
    b->current_block->terminator.as.branch.else_block = f;
}

static inline MIRFunction* emit_function(
    MIRBuilder* b,
    const char* name
) {
    MIRFunction* fn = (MIRFunction*)calloc(1, sizeof(MIRFunction));

    fn->name = name;

    /* create entry block */

    MIRBlock* block = (MIRBlock*)calloc(1, sizeof(MIRBlock));

    block->label = "entry";

    fn->entry = block;
    fn->first_block = block;
    fn->last_block = block;
    block->terminator.type = MIR_TERM_UNREACHABLE;

    /* switch builder state */

    b->current_function = fn;
    b->current_block = block;

    return fn;
}

static inline MIRParam* mir_param(const char* name, MIRValue val) {
    MIRParam* p = (MIRParam*)calloc(1, sizeof(MIRParam));
    p->name = name;
    p->value = val;
    p->next = NULL;
    return p;
}

static inline MIRValue emit_param(
    MIRBuilder* b,
    const char* name
) {
    MIRValue temp = mir_new_temp(b);

    MIRParam* p = mir_param(name, temp);

    if (!b->current_function->params) {
        b->current_function->params = p;
    } else {
        MIRParam* cur = b->current_function->params;

        while (cur->next)
            cur = cur->next;

        cur->next = p;
    }

    return temp;
}

static inline void emit_params(
    MIRBuilder* b,
    ASTNode* params
) {
    ASTNode* cur = params;

    while (cur) {
        emit_param(
            b,
            cur->as.func_param.name
        );

        cur = cur->next;
    }
}

/* ============================================================================
   BLOCK SWITCHING
============================================================================ */

static inline void set_block(MIRBuilder* b, MIRBlock* blk) {
    b->current_block = blk;
}

/*
===============================================================================
CORE INVARIANTS
===============================================================================

1. No nested control flow in MIR.
2. CFG is explicit.
3. Every block has exactly one terminator.
4. Expressions are flattened into temporaries.
5. Pattern matching must already be lowered.
6. foreach must already be lowered.
7. pipelines must already be lowered.
8. structured loops become CFG.
9. break/continue become jumps.
10. MIR is target-independent.
11. VM ISA lowering should be mostly mechanical.

===============================================================================
*/
