#include "ast.h"

static char repr_buf[512];
static int repr_len = 0;

static void append_repr(const char* str) {
    if (!str) return;
    int len = strlen(str);
    if (repr_len + len < (int)sizeof(repr_buf) - 1) {
        strcpy(repr_buf + repr_len, str);
        repr_len += len;
    }
}

static void build_repr(ASTNode* node) {
    if (!node) return;
    char temp[128];
    switch (node->type) {
        case NODE_PROGRAM:
            append_repr(node->as.program.name);
            break;
        case NODE_FUNCTION:
            append_repr(node->as.function.name);
            break;
        case NODE_LET:
            append_repr(node->as.let.name);
            break;
        case NODE_ASSIGN:
            append_repr(node->as.assign.op);
            break;
        case NODE_FOR:
            append_repr(node->as.for_expr.name);
            break;
        case NODE_FUNC_PARAMETER:
            append_repr(node->as.func_param.name);
            break;
        case NODE_BINARY_OP:
            append_repr(node->as.binop.op);
            break;
        case NODE_UNARY_OP:
            append_repr(node->as.unary.op);
            break;
        case NODE_IDENTIFIER:
            append_repr(node->as.ident.name);
            break;
        case NODE_CONCRETE_TYPE:
            append_repr(node->as.type.name);
            if (node->as.type.generic_args) {
                append_repr("<");
                ASTNode* arg = node->as.type.generic_args;
                while (arg) {
                    build_repr(arg);
                    if (arg->next) append_repr(", ");
                    arg = arg->next;
                }
                append_repr(">");
            }
            break;
        case NODE_GENERIC_TYPE:
            append_repr(node->as.type.name);
            break;
        case NODE_INT_LITERAL:
            snprintf(temp, sizeof(temp), "%d", node->as.int_lit.value);
            append_repr(temp);
            break;
        case NODE_FLOAT_LITERAL:
            snprintf(temp, sizeof(temp), "%f", node->as.float_lit.value);
            append_repr(temp);
            break;
        case NODE_BOOL_LITERAL:
            append_repr(node->as.bool_lit.value ? "true" : "false");
            break;
        case NODE_STRING_LITERAL:
            append_repr("\"");
            append_repr(node->as.string_lit.value);
            append_repr("\"");
            break;
        case NODE_CALL:
            append_repr(node->as.call.debug_name);
            break;
        case NODE_ENUM_DECL:
            append_repr(node->as.enum_decl.name);
            break;
        case NODE_ENUM_VARIANT:
            append_repr(node->as.enum_variant.name);
            break;
        case NODE_STRUCT_DECL:
            append_repr(node->as.struct_decl.name);
            break;
        case NODE_STRUCT_LITERAL:
            append_repr(node->as.struct_lit.name);
            break;
        case NODE_STRUCT_FIELD:
            append_repr(node->as.struct_field.name);
            break;
        case NODE_PLACEHOLDER:
            append_repr(node->as.placeholder.name);
            break;
        case NODE_MEMBER_ACCESS:
            append_repr(node->as.member.op);
            break;
        default:
            break;
    }
}

char* node_repr(ASTNode* node) {
    repr_len = 0;
    repr_buf[0] = '\0';
    build_repr(node);
    return repr_buf;
}

#define PRINT_INDEN(indent) for (int i = 0; i <= (indent); i++) printf("  ");

void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    
    PRINT_INDEN(indent - 1);
    
    const char* type_names[] = {
        "NODE_PROGRAM", "NODE_FUNCTION", "NODE_STMT_LIST", "NODE_LET", "NODE_ASSIGN",
        "NODE_IF", "NODE_FOR", "NODE_LOOP", "NODE_MATCH",
        "NODE_MATCH_ARM", "NODE_RETURN", "NODE_BREAK", "NODE_IDENT_LIST", "NODE_FUNC_PARAMETER",
        "NODE_BINARY_OP", "NODE_UNARY_OP", "NODE_IDENTIFIER", "NODE_CONCRETE_TYPE", "NODE_GENERIC_TYPE",
        "NODE_INT_LITERAL", "NODE_FLOAT_LITERAL", "NODE_BOOL_LITERAL", "NODE_STRING_LITERAL", "NODE_CALL",
        "NODE_ENUM_DECL", "NODE_ENUM_VARIANT", "NODE_STRUCT_DECL", "NODE_STRUCT_LITERAL", "NODE_STRUCT_FIELD",
        "NODE_LIST_LITERAL", "NODE_LIST_PATTERN", "NODE_PIPELINE", "NODE_PLACEHOLDER", "NODE_MEMBER_ACCESS",
        "NODE_RANGE", "NODE_LAMBDA"
    };

    printf("[%s]", type_names[node->type]);
    
    // We print the repr, but first we must copy it or just print it directly.
    // It's safe to use directly since printf processes it before we call node_repr again.
    char* repr = node_repr(node);
    if (repr && repr[0] != '\0') {
        printf(" lexeme: %s", repr);
    }
    
    if (node->evaluates_to_type) {
        printf(" type: %s", node_repr(node->evaluates_to_type));
    }
    
    printf("\n");

    // Print children recursively depending on node type
    switch (node->type) {
        case NODE_PROGRAM:
            print_ast(node->as.program.body, indent + 1);
            break;
        case NODE_FUNCTION:
            print_ast(node->as.function.generic_args, indent + 1);
            print_ast(node->as.function.params, indent + 1);
            print_ast(node->as.function.return_type, indent + 1);
            print_ast(node->as.function.body, indent + 1);
            break;
        case NODE_STMT_LIST:
            print_ast(node->as.stmt_list.body, indent + 1);
            break;
        case NODE_LET:
            print_ast(node->as.let.declared_type, indent + 1);
            print_ast(node->as.let.value, indent + 1);
            break;
        case NODE_ASSIGN:
            print_ast(node->as.assign.target, indent + 1);
            print_ast(node->as.assign.value, indent + 1);
            break;
        case NODE_IF:
            print_ast(node->as.if_expr.cond, indent + 1);
            print_ast(node->as.if_expr.then_body, indent + 1);
            print_ast(node->as.if_expr.else_body, indent + 1);
            break;
        case NODE_FOR:
            print_ast(node->as.for_expr.init, indent + 1);
            print_ast(node->as.for_expr.cond, indent + 1);
            print_ast(node->as.for_expr.step, indent + 1);
            print_ast(node->as.for_expr.body, indent + 1);
            print_ast(node->as.for_expr.else_body, indent + 1);
            break;
        case NODE_LOOP:
            print_ast(node->as.loop_expr.body, indent + 1);
            print_ast(node->as.loop_expr.else_body, indent + 1);
            break;
        case NODE_MATCH:
            print_ast(node->as.match_expr.subject, indent + 1);
            print_ast(node->as.match_expr.arms, indent + 1);
            break;
        case NODE_MATCH_ARM:
            print_ast(node->as.match_arm.pattern, indent + 1);
            print_ast(node->as.match_arm.guard, indent + 1);
            print_ast(node->as.match_arm.body, indent + 1);
            break;
        case NODE_RETURN:
            print_ast(node->as.return_stmt.value, indent + 1);
            break;
        case NODE_BREAK:
            print_ast(node->as.break_stmt.value, indent + 1);
            break;
        case NODE_IDENT_LIST:
            print_ast(node->as.ident_list.items, indent + 1);
            break;
        case NODE_FUNC_PARAMETER:
            print_ast(node->as.func_param.type_expr, indent + 1);
            break;
        case NODE_BINARY_OP:
            print_ast(node->as.binop.left, indent + 1);
            print_ast(node->as.binop.right, indent + 1);
            break;
        case NODE_UNARY_OP:
            print_ast(node->as.unary.operand, indent + 1);
            break;
        case NODE_CALL:
            print_ast(node->as.call.callee, indent + 1);
            print_ast(node->as.call.args, indent + 1);
            break;
        case NODE_ENUM_DECL:
            print_ast(node->as.enum_decl.generic_args, indent + 1);
            print_ast(node->as.enum_decl.variants, indent + 1);
            break;
        case NODE_ENUM_VARIANT:
            print_ast(node->as.enum_variant.payload_types, indent + 1);
            break;
        case NODE_STRUCT_DECL:
            print_ast(node->as.struct_decl.generic_args, indent + 1);
            print_ast(node->as.struct_decl.fields, indent + 1);
            break;
        case NODE_STRUCT_LITERAL:
            print_ast(node->as.struct_lit.generic_args, indent + 1);
            print_ast(node->as.struct_lit.fields, indent + 1);
            break;
        case NODE_STRUCT_FIELD:
            print_ast(node->as.struct_field.value, indent + 1);
            break;
        case NODE_LIST_LITERAL:
            print_ast(node->as.list_lit.items, indent + 1);
            break;
        case NODE_LIST_PATTERN:
            print_ast(node->as.list_pattern.items, indent + 1);
            break;
        case NODE_PIPELINE:
            print_ast(node->as.pipeline.left, indent + 1);
            print_ast(node->as.pipeline.right, indent + 1);
            break;
        case NODE_MEMBER_ACCESS:
            print_ast(node->as.member.object, indent + 1);
            print_ast(node->as.member.member, indent + 1);
            break;
        case NODE_RANGE:
            print_ast(node->as.range.start, indent + 1);
            print_ast(node->as.range.end, indent + 1);
            break;
        case NODE_LAMBDA:
            print_ast(node->as.lambda.params, indent + 1);
            print_ast(node->as.lambda.return_type, indent + 1);
            print_ast(node->as.lambda.body, indent + 1);
            break;
        default:
            break;
    }
    
    if (node->next) print_ast(node->next, indent);
}
