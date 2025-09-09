# ZWM version
VERSION = 0.4

# Compiler
CC = gcc

# Flags
CFLAGS = -g -Wall -Wextra -Wpedantic -c -s
LDFLAGS = -lX11 -o

zwm: zwm.c
		${CC} ${CFLAGS} zwm.c
		${CC} ${LDFLAGS} zwm zwm.o

clean: 
	rm -rf zwm
