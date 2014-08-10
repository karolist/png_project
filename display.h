/*
 * Sample output device for png_project
 *
 * You are free to copy, use and modify
 *
 * Karolis Tarasauskas, karolis.tarasauskas@gmail.com
 *
 * 2014-08-10
 */

#include <stdio.h>

//uses sdl, TODO use something more sane
#include <SDL/SDL.h>

typedef enum color_t{
	GREYSCALE,
	RGB,
	GREYSCALE_ALPHA,
	RGB_ALPHA,
}color_t;

int display_raw(void *data, int heigth, int width, int bsize, color_t color);
