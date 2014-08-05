/*
 * A simple standalone png file decoder
 *
 * You are free to copy, use and modify
 *
 * Karolis Tarasauskas, karolis.tarasauskas@gmail.com
 *
 * 2014-08-04
 */

#ifndef PNG_LIB_H_
#define PNG_LIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//define chunks:
#define IHDR 0x49484452
#define IDAT 0x49444154
#define IEND 0x49454E44

//header consists of the following data structure:
struct __attribute__((packed, aligned(1))) ihdr_t{
	uint32_t width;
	uint32_t height;
	uint8_t bdepth;
	uint8_t color_t;
	uint8_t compression_t;
	uint8_t filter_m;
	uint8_t interlace_m;
}ihdr_t;

/*
 * Main decoder function.
 * Arguments:
 * *file - pointer to a file to be opened,
 * *output - uncompressed picture memory location, which is
 * heigth * length * depth size
 */
int PNG_decode(char *file, void **output, int *height, int *length, int *depth);

/*
 * prints info about file
 * used mainly for debug
 */
int PNG_info(char *file);

#endif //PNG_LIB_H_
