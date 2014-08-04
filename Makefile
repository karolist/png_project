CC=gcc
CFLAGS="-Wall"

debug:clean
	$(CC) $(CFLAGS) -g -o png_project main.c
stable:clean
	$(CC) $(CFLAGS) -o png_project main.c
clean:
	rm -vfr *~ png_project
