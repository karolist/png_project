CC=gcc
CFLAGS="-Wall"

debug:clean
	$(CC) $(CFLAGS) -g -o png_project -lz main.c png_lib.c
stable:clean
	$(CC) $(CFLAGS) -o png_project main.c png_lib.c
clean:
	rm -vfr *~ png_project
