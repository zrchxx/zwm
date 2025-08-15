zwm: zwm.c
	$(CC) -Wall -Wextra -o zwm zwm.c -lX11
clean: 
	rm -rf zwm
