#include <stdio.h>
#include <stdlib.h>

#include "png_lib.h"

int main(int argc, char **argv) {

	void *buff;
	int height, width, depth;

	PNG_decode("png.png", &buff, &height, &width, &depth);

    return 0;
}
