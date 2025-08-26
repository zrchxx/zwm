# ZWM version
VERSION = 0.4

CC = gcc

zwm: zwm.c
	$(CC) -Wall -o zwm zwm.c -lX11

clean: 
	rm -rf zwm
