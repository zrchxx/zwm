zwm: zwm.c
	$(CC) -Wall -o zwm zwm.c -lX11
clean: 
	rm -rf zwm
