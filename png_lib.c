/*
 * A simple standalone png file decoder
 *
 * You are free to copy, use and modify
 *
 * Karolis Tarasauskas, karolis.tarasauskas@gmail.com
 *
 * 2014-08-04
 */

#include "png_lib.h"

#define PNG_MAGIC_NUM 0x89504e470d0a1a0a
#define PNG_CHUNK_HEADER_LEN 8

//exported private functions
int PNG_header(void *data, int len);

//private helper functions
void data_invert(void *in, void *out, int len);

//***********************exported functions************************************
int PNG_decode(char* file, void** output, int *height, int* length, int *depth)
{
	int ret;
	uint8_t buff[8];

	FILE *fd = fopen(file, "rb"); //only reading a binary file
	if(!fd)
		return -1; //TODO

	//try to read header from file, which should be 8B int length
	ret = fread(buff, 1, PNG_CHUNK_HEADER_LEN, fd);
	if(ret < PNG_CHUNK_HEADER_LEN)
		return -1;

	ret = PNG_header(buff, PNG_CHUNK_HEADER_LEN);
	if(!ret)
		return ret;

	//if we succeeded in header, scan whole file:
	uint32_t ch_len, ch_typ, ch_crc;
	while(1){
		//read chunk length and type:
		ret = fread(buff, 1, PNG_CHUNK_HEADER_LEN, fd);
		if(ret < PNG_CHUNK_HEADER_LEN)
			return -1;

		int i;
		for(i=0;i<8;i++)
			printf("%x ", buff[i]);
		printf("\n");

		//flip endianness of red data
		data_invert(buff, &ch_len, 4);
		data_invert(buff+4, &ch_typ, 4);

		//sanity check these values:
		printf("len = %x, type = %x\n", ch_len, ch_typ);

		//forward that number of bytes:
		fseek(fd, ch_len, SEEK_CUR);

		//read crc of current chunk
		fread(buff, 1, 4, fd);
		data_invert(buff, &ch_crc, 4);
		printf("crc = %x\n", ch_crc);
	}



	printf("decode succesfull\n");
	return 0;
}

int PNG_info(char* file)
{
	return 0;
}

//***********************private functions*************************************

// as png files are divided into small chunks,
// we differentiate these chunks to call corresponding function.

/* We use the same format for each of these functions: *data points to
 * location in file, len - length of this chunk.
 */

//header decoder.
int PNG_header(void *data, int len){
	//firstly, length of header should be 8B:
	if(len != PNG_CHUNK_HEADER_LEN)
		return -1; //TODO proper error codes

	//then compare header beggining to a magic num:
	long unsigned int magic = PNG_MAGIC_NUM;
	int ret = memcmp(data, &magic, len);

	if(ret != 0)
		return -1; //TODO

	return ret;
};

//inverts endianness of stream of length len
void data_invert(void *in, void *out, int len){

	int i;
	in += len-1;

	for(i=0; i<len; i++){
		*(uint8_t *)out = *(uint8_t *)in;
		in--;
		out++;
	}
}


