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
