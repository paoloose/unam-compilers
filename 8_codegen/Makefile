.PHONY: all clean build run debug help

# Tools
FLEX = flex
BISON = bison
CC = gcc

# Flags
CFLAGS = -Wall -Wextra -std=c11
DEBUGFLAGS = -g -O0

# Allow passing CLI args:
# make run ARGS="file.en arg2"
# make debug ARGS="file.en"
ARGS ?=

# Source files
LEX_FILE = lexer.l
YACC_FILE = parser.y
ANALYZER_SRC = analyzer.c
AST_SRC = ast.c
HEADERS = ast.h analyzer.h debug.h darray.h
BUILTINS = builtins.c

# Generated files
LEX_OUT = lex.yy.c
YACC_OUT = parser.tab.c
YACC_HEADER = parser.tab.h
OBJS = lex.yy.o parser.tab.o analyzer.o builtins.o ast.o

EXECUTABLE = ennuyeux_parser

# Default target
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJS) -lm

# Dependencies
builtins.o: $(BUILTINS) $(HEADERS)
	$(CC) $(CFLAGS) -c $(BUILTINS)

analyzer.o: analyzer.c $(HEADERS)
	$(CC) $(CFLAGS) -c analyzer.c

ast.o: ast.c $(HEADERS)
	$(CC) $(CFLAGS) -c $(AST_SRC)

parser.tab.o: $(YACC_OUT) $(HEADERS)
	$(CC) $(CFLAGS) -c $(YACC_OUT)

lex.yy.o: $(LEX_OUT) $(YACC_HEADER) $(HEADERS)
	$(CC) $(CFLAGS) -c $(LEX_OUT)

$(YACC_OUT) $(YACC_HEADER): $(YACC_FILE) $(HEADERS)
	$(BISON) -d $(YACC_FILE)

$(LEX_OUT): $(LEX_FILE) $(YACC_HEADER)
	$(FLEX) $(LEX_FILE)

# Utility targets
build: all

run: all
	./$(EXECUTABLE) $(ARGS)

debug: CFLAGS += $(DEBUGFLAGS)
debug: clean all
	gdb --args ./$(EXECUTABLE) $(ARGS)

clean:
	rm -f $(LEX_OUT) $(YACC_OUT) $(YACC_HEADER) $(OBJS) $(EXECUTABLE)
	rm -f *.o

help:
	@echo "🪰 Ennuyeux Language Compiler"
	@echo "------------------------------"
	@echo
	@echo "Targets:"
	@echo "  make all                 - Build the parser (default)"
	@echo "  make run                 - Build and run"
	@echo "  make run ARGS=\"file\""
	@echo "  make debug               - Build with -g and open gdb"
	@echo "  make debug ARGS=\"file\""
	@echo "  make clean               - Remove generated files"
	@echo "  make help                - Show this message"
