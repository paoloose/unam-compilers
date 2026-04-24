.PHONY: all clean build run help

# Tools
FLEX = flex
BISON = bison
CC = gcc
CFLAGS = -Wall -Wextra -std=c11

# Source files
LEX_FILE = lexer.l
YACC_FILE = parser.y
ANALYZER_SRC = analyzer.c
HEADERS = ast.h analyzer.h

# Generated files
LEX_OUT = lex.yy.c
YACC_OUT = parser.tab.c
YACC_HEADER = parser.tab.h
OBJS = lex.yy.o parser.tab.o analyzer.o

EXECUTABLE = ennuyeux_parser

# Default target
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJS) -lm

# Dependencies
analyzer.o: analyzer.c $(HEADERS)
	$(CC) $(CFLAGS) -c analyzer.c

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
	./$(EXECUTABLE)

clean:
	rm -f $(LEX_OUT) $(YACC_OUT) $(YACC_HEADER) $(OBJS) $(EXECUTABLE)
	rm -f *.o

help:
	@echo "🪰 Ennuyeux Language Compiler"
	@echo "------------------------------"
	@echo
	@echo "Targets:"
	@echo "  make all       - Build the parser (default)"
	@echo "  make run       - Build and run"
	@echo "  make clean     - Remove generated files"
	@echo "  make help      - Show this message"
