CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iraylib/src -g -O2
LDFLAGS = -Lraylib/src -lraylib -lm -ldl -lpthread

# Assuming X11 libraries are required on Linux (Raylib dependencies)
LDFLAGS += -lX11 -lXrandr -lXinerama -lXi -lXcursor -lGL

EXEC = vm

all: raylib/src/libraylib.a $(EXEC)

raylib/src/libraylib.a:
	$(MAKE) -C raylib/src

$(EXEC): main.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC) main.o
	$(MAKE) -C raylib/src clean

.PHONY: all clean
