#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>

#include "raylib.h"

// -----------------------------------------------------------------------------
// TYPES & DATA STRUCTURES
// -----------------------------------------------------------------------------

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_ARRAY,
    VAL_LIST
} ValType;

typedef struct {
    ValType type;
    union {
        double num;
        char* str;
        int heap_id;
    } as;
} Value;

// Instructions OP
typedef enum {
    OP_VAR, OP_ARR, OP_LIST, OP_LIST_ADD, OP_ARR_GET, OP_ARR_SET, OP_ARR_LEN, OP_LIST_GET,
    OP_ASSIGN, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_AND, OP_OR, OP_NOT, OP_XOR, OP_EQ, OP_NEQ, OP_GT, OP_GTE, OP_LT, OP_LTE,
    OP_LABEL, OP_GOTO, OP_IF, OP_IFFALSE, OP_FUNCTION, OP_END_FUNCTION,
    OP_PARAM, OP_PARAM_GET, OP_GOSUB, OP_RETURN,
    OP_PRINT, OP_INPUT, OP_PIXEL, OP_SLEEP, OP_KEY, OP_TIME
} OpCode;

typedef struct {
    OpCode op;
    char** args;
    int arg_count;
    int src_line;
} Instruction;

typedef struct {
    char* name;
    Value val;
} VarEntry;

typedef struct {
    VarEntry* vars;
    int capacity;
    int count;
} VarMap;

typedef struct {
    int return_addr;
    char* return_var;
    VarMap locals;
} CallFrame;

typedef struct {
    Value* elements;
    int capacity;
    int count;
} HeapObject;

// -----------------------------------------------------------------------------
// VM STATE
// -----------------------------------------------------------------------------
#define MAX_INSTRUCTIONS 100000
#define MAX_CALL_STACK 1024
#define MAX_HEAP 100000

Instruction instructions[MAX_INSTRUCTIONS];
int inst_count = 0;

typedef struct {
    char* name;
    int addr;
} Label;

Label labels[10000];
int label_count = 0;

VarMap globals = {0};
CallFrame call_stack[MAX_CALL_STACK];
int call_depth = 0;

HeapObject heap[MAX_HEAP];
int heap_counter = 1;

Value param_stack[1024];
int param_depth = 0;

uint8_t screen_buffer[64 * 64];
bool has_graphics = false;

// -----------------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------------

OpCode parse_op(const char* op_str) {
    if (strcmp(op_str, "VAR") == 0) return OP_VAR;
    if (strcmp(op_str, "ARR") == 0) return OP_ARR;
    if (strcmp(op_str, "LIST") == 0) return OP_LIST;
    if (strcmp(op_str, "LIST_ADD") == 0) return OP_LIST_ADD;
    if (strcmp(op_str, "ARR_GET") == 0) return OP_ARR_GET;
    if (strcmp(op_str, "ARR_SET") == 0) return OP_ARR_SET;
    if (strcmp(op_str, "ARR_LEN") == 0) return OP_ARR_LEN;
    if (strcmp(op_str, "LIST_GET") == 0) return OP_LIST_GET;
    if (strcmp(op_str, "ASSIGN") == 0) return OP_ASSIGN;
    if (strcmp(op_str, "ADD") == 0) return OP_ADD;
    if (strcmp(op_str, "SUB") == 0) return OP_SUB;
    if (strcmp(op_str, "MUL") == 0) return OP_MUL;
    if (strcmp(op_str, "DIV") == 0) return OP_DIV;
    if (strcmp(op_str, "MOD") == 0) return OP_MOD;
    if (strcmp(op_str, "POW") == 0) return OP_POW;
    if (strcmp(op_str, "AND") == 0) return OP_AND;
    if (strcmp(op_str, "OR") == 0) return OP_OR;
    if (strcmp(op_str, "NOT") == 0) return OP_NOT;
    if (strcmp(op_str, "XOR") == 0) return OP_XOR;
    if (strcmp(op_str, "EQ") == 0) return OP_EQ;
    if (strcmp(op_str, "NEQ") == 0) return OP_NEQ;
    if (strcmp(op_str, "GT") == 0) return OP_GT;
    if (strcmp(op_str, "GTE") == 0) return OP_GTE;
    if (strcmp(op_str, "LT") == 0) return OP_LT;
    if (strcmp(op_str, "LTE") == 0) return OP_LTE;
    if (strcmp(op_str, "LABEL") == 0) return OP_LABEL;
    if (strcmp(op_str, "GOTO") == 0) return OP_GOTO;
    if (strcmp(op_str, "IF") == 0) return OP_IF;
    if (strcmp(op_str, "IFFALSE") == 0) return OP_IFFALSE;
    if (strcmp(op_str, "FUNCTION") == 0) return OP_FUNCTION;
    if (strcmp(op_str, "END_FUNCTION") == 0) return OP_END_FUNCTION;
    if (strcmp(op_str, "PARAM") == 0) return OP_PARAM;
    if (strcmp(op_str, "PARAM_GET") == 0) return OP_PARAM_GET;
    if (strcmp(op_str, "GOSUB") == 0) return OP_GOSUB;
    if (strcmp(op_str, "RETURN") == 0) return OP_RETURN;
    if (strcmp(op_str, "PRINT") == 0) return OP_PRINT;
    if (strcmp(op_str, "INPUT") == 0) return OP_INPUT;
    if (strcmp(op_str, "PIXEL") == 0) return OP_PIXEL;
    if (strcmp(op_str, "SLEEP") == 0) return OP_SLEEP;
    if (strcmp(op_str, "KEY") == 0) return OP_KEY;
    if (strcmp(op_str, "TIME") == 0) return OP_TIME;
    return (OpCode)-1;
}

Value make_num(double n) { Value v; v.type = VAL_NUMBER; v.as.num = n; return v; }
Value make_str(char* s) { Value v; v.type = VAL_STRING; v.as.str = s; return v; }

bool get_var(VarMap* map, const char* name, Value* out) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->vars[i].name, name) == 0) {
            *out = map->vars[i].val;
            return true;
        }
    }
    return false;
}

void set_var(VarMap* map, const char* name, Value val) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->vars[i].name, name) == 0) {
            map->vars[i].val = val;
            return;
        }
    }
    if (map->count == map->capacity) {
        map->capacity = map->capacity == 0 ? 8 : map->capacity * 2;
        map->vars = realloc(map->vars, sizeof(VarEntry) * map->capacity);
    }
    map->vars[map->count].name = strdup(name);
    map->vars[map->count].val = val;
    map->count++;
}

Value get_value(const char* arg) {
    char* endptr;
    double d = strtod(arg, &endptr);
    if (*endptr == '\0' && arg[0] != '\0') return make_num(d);
    
    if (strcmp(arg, "true") == 0) return make_num(1);
    if (strcmp(arg, "false") == 0) return make_num(0);
    if (arg[0] == '"') {
        char* str = strdup(arg + 1);
        str[strlen(str) - 1] = '\0';
        return make_str(str);
    }
    
    Value out;
    if (call_depth > 0) {
        if (get_var(&call_stack[call_depth - 1].locals, arg, &out)) return out;
    }
    if (get_var(&globals, arg, &out)) return out;
    
    fprintf(stderr, "Error: undefined variable: \"%s\"\n", arg);
    exit(1);
}

double get_num(const char* arg) {
    Value v = get_value(arg);
    if (v.type != VAL_NUMBER) {
        fprintf(stderr, "Error: expected number for arg %s\n", arg);
        exit(1);
    }
    return v.as.num;
}

void set_value(const char* dest, Value val) {
    if (call_depth > 0) {
        VarMap* locals = &call_stack[call_depth - 1].locals;
        for (int i = 0; i < locals->count; i++) {
            if (strcmp(locals->vars[i].name, dest) == 0) {
                locals->vars[i].val = val;
                return;
            }
        }
    }
    
    for (int i = 0; i < globals.count; i++) {
        if (strcmp(globals.vars[i].name, dest) == 0) {
            globals.vars[i].val = val;
            return;
        }
    }
    
    if (call_depth > 0) {
        set_var(&call_stack[call_depth - 1].locals, dest, val);
    } else {
        set_var(&globals, dest, val);
    }
}

int get_label(const char* name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) return labels[i].addr;
    }
    fprintf(stderr, "Error: unknown label: %s\n", name);
    exit(1);
}

// -----------------------------------------------------------------------------
// PARSER
// -----------------------------------------------------------------------------
void parse_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Error opening file %s\n", path); exit(1); }
    
    char line[1024];
    int line_no = 0;
    while (fgets(line, sizeof(line), f)) {
        line_no++;
        char* comment = strstr(line, "//");
        if (comment) *comment = '\0';
        
        char* token = strtok(line, " \t\r\n");
        if (!token) continue;
        
        OpCode op = parse_op(token);
        if (op == (OpCode)-1) continue;
        
        Instruction inst;
        inst.op = op;
        inst.src_line = line_no;
        inst.arg_count = 0;
        inst.args = malloc(sizeof(char*) * 8);
        
        while ((token = strtok(NULL, " \t\r\n"))) {
            if (token[0] == '"') {
                char buffer[1024];
                strcpy(buffer, token);
                while (buffer[strlen(buffer)-1] != '"') {
                    token = strtok(NULL, " \t\r\n");
                    if (!token) break;
                    strcat(buffer, " ");
                    strcat(buffer, token);
                }
                inst.args[inst.arg_count++] = strdup(buffer);
            } else {
                inst.args[inst.arg_count++] = strdup(token);
            }
        }
        
        if (op == OP_LABEL) {
            labels[label_count].name = strdup(inst.args[0]);
            labels[label_count].addr = inst_count;
            label_count++;
        } else {
            if (op == OP_PIXEL) has_graphics = true;
            instructions[inst_count++] = inst;
        }
    }
    fclose(f);
}

// -----------------------------------------------------------------------------
// EXECUTION ENGINE
// -----------------------------------------------------------------------------
void execute() {
    int ip = 0;
    
    if (has_graphics) {
        InitWindow(512, 512, "Ennuyeux VM");
        SetTargetFPS(60);
    }
    
    long inst_executed_count = 0;
    double last_draw_time = 0.0;
    
    while (ip < inst_count) {
        Instruction* inst = &instructions[ip];
        OpCode op = inst->op;
        
        inst_executed_count++;
        if (has_graphics && (inst_executed_count % 5000 == 0)) {
            if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE)) break;
            
            double current_time = GetTime();
            if (current_time - last_draw_time >= 1.0 / 60.0) {
                BeginDrawing();
                ClearBackground(BLACK);
                for (int py = 0; py < 64; py++) {
                    for (int px = 0; px < 64; px++) {
                        if (screen_buffer[py * 64 + px]) {
                            DrawRectangle(px * 8, py * 8, 8, 8, WHITE);
                        }
                    }
                }
                EndDrawing();
                last_draw_time = current_time;
            }
        }
        
        switch(op) {
            case OP_VAR:
                set_value(inst->args[0], make_num(0));
                ip++;
                break;
            case OP_ARR: {
                int size = (int)get_num(inst->args[0]);
                int hid = heap_counter++;
                heap[hid].capacity = size;
                heap[hid].count = size;
                heap[hid].elements = calloc(size, sizeof(Value));
                for(int i = 0; i < size; i++) heap[hid].elements[i] = make_num(0);
                Value v; v.type = VAL_ARRAY; v.as.heap_id = hid;
                set_value(inst->args[1], v);
                ip++;
                break;
            }
            case OP_ARR_GET: {
                Value arr = get_value(inst->args[0]);
                int idx = (int)get_num(inst->args[1]);
                Value res = make_num(0);
                if (arr.type == VAL_ARRAY && idx >= 0 && idx < heap[arr.as.heap_id].count) {
                    res = heap[arr.as.heap_id].elements[idx];
                }
                set_value(inst->args[2], res);
                ip++;
                break;
            }
            case OP_ARR_SET: {
                Value arr = get_value(inst->args[0]);
                int idx = (int)get_num(inst->args[1]);
                Value val = get_value(inst->args[2]);
                if (arr.type == VAL_ARRAY && idx >= 0 && idx < heap[arr.as.heap_id].count) {
                    heap[arr.as.heap_id].elements[idx] = val;
                }
                ip++;
                break;
            }
            case OP_ARR_LEN: {
                Value arr = get_value(inst->args[0]);
                int len = 0;
                if (arr.type == VAL_ARRAY) {
                    len = heap[arr.as.heap_id].count;
                }
                set_value(inst->args[1], make_num(len));
                ip++;
                break;
            }
            case OP_ASSIGN:
                set_value(inst->args[1], get_value(inst->args[0]));
                ip++;
                break;
            case OP_ADD: {
                Value v1 = get_value(inst->args[0]);
                Value v2 = get_value(inst->args[1]);
                if (v1.type == VAL_STRING || v2.type == VAL_STRING) {
                    char buf1[256];
                    char buf2[256];
                    const char* s1 = (v1.type == VAL_STRING) ? v1.as.str : (sprintf(buf1, "%g", v1.as.num), buf1);
                    const char* s2 = (v2.type == VAL_STRING) ? v2.as.str : (sprintf(buf2, "%g", v2.as.num), buf2);
                    char* res = malloc(strlen(s1) + strlen(s2) + 1);
                    strcpy(res, s1);
                    strcat(res, s2);
                    set_value(inst->args[2], make_str(res));
                } else {
                    set_value(inst->args[2], make_num(v1.as.num + v2.as.num));
                }
                ip++; break;
            }
            case OP_SUB:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) - get_num(inst->args[1])));
                ip++; break;
            case OP_MUL:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) * get_num(inst->args[1])));
                ip++; break;
            case OP_DIV:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) / get_num(inst->args[1])));
                ip++; break;
            case OP_MOD:
                set_value(inst->args[2], make_num(fmod(get_num(inst->args[0]), get_num(inst->args[1]))));
                ip++; break;
            case OP_POW:
                set_value(inst->args[2], make_num(pow(get_num(inst->args[0]), get_num(inst->args[1]))));
                ip++; break;
            case OP_EQ:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) == get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_NEQ:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) != get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_GT:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) > get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_GTE:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) >= get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_LT:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) < get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_LTE:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) <= get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_AND:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) && get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_OR:
                set_value(inst->args[2], make_num(get_num(inst->args[0]) || get_num(inst->args[1]) ? 1 : 0));
                ip++; break;
            case OP_NOT:
                set_value(inst->args[1], make_num(!get_num(inst->args[0]) ? 1 : 0));
                ip++; break;
            case OP_GOTO:
                ip = get_label(inst->args[0]);
                break;
            case OP_IF:
                if (get_num(inst->args[0])) ip = get_label(inst->args[2]);
                else ip++;
                break;
            case OP_IFFALSE:
                if (!get_num(inst->args[0])) ip = get_label(inst->args[2]);
                else ip++;
                break;
            case OP_FUNCTION: {
                int depth = 1;
                while (depth > 0) {
                    ip++;
                    if (ip >= inst_count) break;
                    if (instructions[ip].op == OP_FUNCTION) depth++;
                    if (instructions[ip].op == OP_END_FUNCTION) depth--;
                }
                ip++;
                break;
            }
            case OP_PARAM:
                param_stack[param_depth++] = get_value(inst->args[0]);
                ip++;
                break;
            case OP_PARAM_GET:
                set_value(inst->args[0], param_stack[--param_depth]);
                ip++;
                break;
            case OP_GOSUB: {
                int addr = get_label(inst->args[0]);
                CallFrame* frame = &call_stack[call_depth++];
                frame->return_addr = ip + 1;
                frame->return_var = inst->arg_count > 1 ? strdup(inst->args[1]) : NULL;
                frame->locals.count = 0;
                ip = addr;
                break;
            }
            case OP_RETURN: {
                Value ret_val = inst->arg_count > 0 ? get_value(inst->args[0]) : make_num(0);
                if (call_depth == 0) {
                    ip = inst_count; // End
                    break;
                }
                CallFrame* frame = &call_stack[--call_depth];
                ip = frame->return_addr;
                if (frame->return_var) {
                    set_value(frame->return_var, ret_val);
                    free(frame->return_var);
                }
                // Free locals
                for (int i = 0; i < frame->locals.count; i++) {
                    free(frame->locals.vars[i].name);
                }
                free(frame->locals.vars);
                frame->locals.vars = NULL;
                frame->locals.capacity = 0;
                break;
            }
            case OP_END_FUNCTION: {
                if (call_depth == 0) { ip = inst_count; break; }
                CallFrame* frame = &call_stack[--call_depth];
                ip = frame->return_addr;
                if (frame->return_var) {
                    set_value(frame->return_var, make_num(0));
                    free(frame->return_var);
                }
                break;
            }
            case OP_PRINT: {
                Value v = get_value(inst->args[0]);
                if (v.type == VAL_STRING) printf("%s\n", v.as.str);
                else printf("%g\n", v.as.num);
                ip++;
                break;
            }
            case OP_INPUT: {
                double val = 0.0;
                if (scanf("%lf", &val) != 1) {
                    val = 0.0; // default if non-numeric
                }
                set_value(inst->args[0], make_num(val));
                ip++;
                break;
            }
            case OP_PIXEL: {
                int x = get_num(inst->args[0]);
                int y = get_num(inst->args[1]);
                bool c = get_num(inst->args[2]) != 0;
                if (x >= 0 && x < 64 && y >= 0 && y < 64) {
                    screen_buffer[y * 64 + x] = c ? 255 : 0;
                }
                ip++;
                break;
            }
            default:
                fprintf(stderr, "Unimplemented opcode: %d at line %d\n", op, inst->src_line);
                exit(1);
        }
    }
    
    // Final draw loop if graphics were initialized
    if (has_graphics) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            for (int py = 0; py < 64; py++) {
                for (int px = 0; px < 64; px++) {
                    if (screen_buffer[py * 64 + px]) {
                        DrawRectangle(px * 8, py * 8, 8, 8, WHITE);
                    }
                }
            }
            EndDrawing();
        }
        CloseWindow();
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: vm <program.fis>\n");
        return 1;
    }
    
    parse_file(argv[1]);
    execute();
    return 0;
}
