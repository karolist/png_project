/*
 * Sample output device for png_project
 *
 * You are free to copy, use and modify
 *
 * Karolis Tarasauskas, karolis.tarasauskas@gmail.com
 *
 * 2014-08-10
 */

#include "display.h"

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);

int display_raw(void* data, int heigth, int width, int bsize)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0){
		printf("SDL fail\n");
		return -1;
	};

	SDL_Surface *disp;

	disp = SDL_SetVideoMode(width,
							heigth,
						 32,
						 (SDL_HWSURFACE|SDL_DOUBLEBUF));

	if(!disp){
		printf("set video fail\n");
// 		return -1;
	};


	SDL_Surface *im;

	im = SDL_CreateRGBSurface(0,
							  width,
							  heigth,
						      32,
							  0,0,0,0);
	if(!im)
		printf("create fail\n");

	int i,j;
	uint32_t px;
	for(i=0; i<width; i++){
		for(j=0; j<heigth; j++){
			memset(&px, *(uint8_t *)data, 3);
			putpixel(im, width - i, j, px);
			data++;
		}
	};

	SDL_BlitSurface(im, NULL, disp, NULL);

	SDL_Flip(disp);

// 	if(!im){
// 		printf("load im fail\n");
// 	}

	SDL_Event event;
	while(1){
		if(SDL_PollEvent(&event)){
			if(event.type == SDL_QUIT)
				break;
		}
	};
	SDL_Quit();
	return 0;
}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}
