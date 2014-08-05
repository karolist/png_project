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

struct ihdr_t im_info;

//exported private functions
int PNG_header(void *data, int len);
int PNG_IHDR(void *data, int len);
int PNG_IDAT(void *data, int len);
int PNG_IEND(void *data, int len);

//private helper functions
void data_invert(void *in, void *out, int len);
int chunk_parser(uint32_t type, void *data, int len);
int deflate(void *data, int i_len, void **output, int *o_len);

//***********************exported functions************************************
int PNG_decode(char* file, void** output, int *height, int* length, int *depth)
{
	int ret;
	void *buff = malloc(PNG_CHUNK_HEADER_LEN);
	if(!buff)
		return -1; //TODO

	FILE *fd = fopen(file, "rb"); //only reading a binary file
	if(!fd)
		return -1; //TODO

	//try to read header from file, which should be 8B int length
	ret = fread(buff, 1, PNG_CHUNK_HEADER_LEN, fd);
	if(ret < PNG_CHUNK_HEADER_LEN){
		free(buff);
		return -1;
	};

	//change data endianness
	data_invert(buff, buff, PNG_CHUNK_HEADER_LEN);

	ret = PNG_header(buff, PNG_CHUNK_HEADER_LEN);
	if(ret < 0){
		free(buff);
		return ret;
	};

	//if we succeeded in header, scan whole file:
	uint32_t ch_len, ch_typ, ch_crc;
	while(1){
		//read chunk length and type:
		ret = fread(buff, 1, PNG_CHUNK_HEADER_LEN, fd);
		if(ret < PNG_CHUNK_HEADER_LEN){
			ret = EOF;
			break;
		};

		//flip endianness of red data
		data_invert(buff, &ch_len, 4);
		data_invert(buff+4, &ch_typ, 4);

		//sanity check these values:
		printf("len = %x, type = %x\n", ch_len, ch_typ);

		//try to allocate chunk sized memory + ch_len +ch_typ for CRC
		//calculation
		buff = realloc(buff, ch_len + PNG_CHUNK_HEADER_LEN);
		if(!buff)
			break;

		fread(buff+8, 1, ch_len, fd); //skip header bytes

		//after data has been read, check its crc:

		//read crc of current chunk
		fread(&ch_crc, 1, 4, fd);
		data_invert(&ch_crc, &ch_crc, 4);
		printf("crc = %x\n", ch_crc);

		//calculate crc of read data:
		//TODO, for now fake it
		uint32_t mycrc = ch_crc;

		//compare calculated and read crc:
		if(ch_crc != mycrc)
			break;

		//now differentiate data by type
		//and pass it to corresponding function
		ret = chunk_parser(ch_typ, buff+8, ch_len);
		if(ret<0)
			break;
	}

	//after we're done, free buff:
	if(buff)
		free(buff);

	//check what result we ended with:

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
	if(ret != 0){
		printf("comparying %lx %lx\n", magic, *(long unsigned int *)data);
		return -1; //TODO
	}

	return ret;
};

//inverts endianness of stream of length len
void data_invert(void *in, void *out, int len){

	void *tmp_buff;
	//if points to itself, create local buffer
	if(in == out){
		tmp_buff = malloc(len);
		memcpy(tmp_buff, in, len);
	}else
		tmp_buff = in;

	if(!tmp_buff)
		return;

	int i;
	tmp_buff += len-1;

	for(i=0; i<len; i++){
		*(uint8_t *)out = *(uint8_t *)tmp_buff;
		tmp_buff--;
		out++;
	}
}

int chunk_parser(uint32_t type, void* data, int len)
{
	//for now only critical chunks are defined

	//because each chunk type consists of 4byte strings,
	//we can safely cast it

	int ret;

	switch(type){
		case(IHDR):
			ret = PNG_IHDR(data, len);
			break;
		case(IDAT):
			printf("iDAT\n");
			break;
		case(IEND):
			printf("iEND\n");
			break;
		default:
			printf("unknown type = %x\n", type);
			break;
	}

	return ret;
}

int PNG_IHDR(void* data, int len)
{
	//check if we have correct header file:
	if(sizeof(ihdr_t) != len){
		printf("wrong size %lu vs %d\n", sizeof(ihdr_t), len);
		return -1;
	}

	//if size fits, cast data into it, print some stuff:
	struct ihdr_t *hdr = data;

	//we must change endianness of multibyte variables:
	data_invert(&hdr->width, &hdr->width, 4);
	data_invert(&hdr->height, &hdr->height, 4);

	//check for 0 sized edges
	if(!hdr->width || !hdr->height)
		return -1; //TODO

	printf("w=%x, h=%x, bdepth=%x, col=%x, compr=%x, fil=%x, interl=%x\n",
		hdr->width,
		hdr->height,
		hdr->bdepth,
		hdr->color_t,
		hdr->compression_t,
		hdr->filter_m,
		hdr->interlace_m);

	//TODO add more checks as defined in png specs

	//if we accepted header, copy it to local buffer
	memcpy(&im_info, hdr, sizeof(ihdr_t));

	//we're done
	return 0;
}

int PNG_IDAT(void* data, int len)
{
	//actual data decoder
	if(!data || len==0)
		return -1; //TODO

	//output of uncompressed data
	void *output;
	int o_len;

	//firstly, check what compression method was used in image
	//and deflate accordingly
	switch(im_info.compression_t){
		case(0): //now only 0, "DEFLATE" compression is used
			return deflate(data, len, &output, &o_len);
			break;
		default:
			return -1;
	}
	return 0;
}

int PNG_IEND(void* data, int len)
{
	//we don't have to do anything at end
	return 0;
}

int deflate(void* data, int i_len, void** output, int* o_len)
{

	return 0;
}
