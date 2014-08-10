#include <stdio.h>
#include <stdlib.h>

#include "png_lib.h"
#include "display.h"

struct img_params{
	int height;
	int width;
	int bdepth;
	int color_ch;
};

struct img_params img;


int extract_pixel(void *data, struct img_params, int x_coord, int y_coord);


int main(int argc, char **argv) {

	void *buff = NULL;
	int height, width, depth, colors;
	int ret;

	ret = PNG_decode("basn2c08.png", &buff, &height, &width, &depth, &colors);

	if(ret != 0){
		printf("Decode error = %d\n", ret);
		return ret;
	}

	printf("decode complete\n");
	printf("height = %d\nwidth = %d\ndepth = %d\n",
			height,
			width,
			depth);

	//print our image:
	img.height = height;
	img.bdepth = depth;
	img.width = width;
	img.color_ch = colors;


	int x_iter, y_iter=0;
// 		for(y_iter = 0; y_iter<height; y_iter++){
 			for(x_iter = 0; x_iter<width; x_iter++){
 				printf("%x ", extract_pixel(buff, img, x_iter, y_iter));
 			}
 			printf("\n");
// 		}
// 		printf("\n");

	display_raw(buff, height, width, depth, colors);


    return 0;
}

int extract_pixel(void* data, struct img_params img, int x_coord, int y_coord)
{
	int ret_val;
	int off_per_line;
	int off;
	uint8_t *start_b;
	uint8_t bit_p;
	//by png specs, we're allowed 1,2,4,8,16 bdepths.
	off_per_line = img.color_ch* img.width * img.bdepth / 8;

	off = x_coord * img.color_ch * img.bdepth/8 + y_coord * off_per_line;
	start_b = ((uint8_t *)data)+off;

	switch(img.bdepth){
		case(1):
			bit_p  = 8 - (x_coord % 8);
			ret_val = *start_b & (1<<bit_p);
			ret_val = ret_val >> bit_p;
			break;
		case(2):
			bit_p = 6 - 2*(x_coord % 4);
			ret_val = *start_b & (3<<bit_p);
			ret_val = ret_val >> bit_p;
			//apply two bit bitmask
			ret_val &= 3;
			break;
		case(4):
			bit_p = 4 - 4*(x_coord % 2);
			ret_val = *start_b & (15<<bit_p);
			ret_val = ret_val >> bit_p;
			ret_val &= 15;
			break;
		case(8):
			//just return every value
			ret_val = *start_b;
			break;
		case(16):
			//ret_val will be 2B long.
			ret_val = *start_b;
			start_b ++;
			ret_val = (ret_val<<8) + *start_b;
			break;
		default:
			printf("Invalid\n");
			return -1;
	};

	return ret_val;
// 	return x_coord*img.bdepth/8 + y_coord * off_per_line;
};
