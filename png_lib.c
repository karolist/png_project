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
#define PNG_CRC_POLY 0x04C11DB7
#define PNG_CHUNK_HEADER_LEN 8

struct png_data_t{
	struct ihdr_t im_info; //header info
	void *i_data; //concatenated data stream pointer
	size_t i_data_l; //length of i_data;

	void *o_data; //output data stream pointer
	size_t o_data_l; //length of o_data
}png_data_t;

struct png_data_t png_data;

//exported private functions
int PNG_header(void *data, int len);
int PNG_IHDR(void *data, int len);
int PNG_IDAT(void *data, int len);
int PNG_IEND(void *data, int len);

//private helper functions
void data_invert(void *in, void *out, int len);
int chunk_parser(uint32_t type, void *data, int len);
int inflate_str(void *data, size_t i_len, void **output, size_t *o_len);
int data_writer(void *output, size_t o_len);
int reconstruct_str(void *in_buff, void *out_buff, int len);

//***********************exported functions************************************
int PNG_decode(char* file, void** output, int *height, int* length, int *depth)
{
	//clear memory just in case
	memset(&png_data, 0, sizeof(png_data_t));

    int ret;
    void *buff = malloc(PNG_CHUNK_HEADER_LEN);
    if(!buff)
        return -1; //TODO

    FILE *fd = fopen(file, "rb"); //only reading a binary file
    if(!fd)
        return -1; //TODO

    //try to read header from file, which should be 8B int length
    ret = fread(buff, 1, PNG_CHUNK_HEADER_LEN, fd);
    if(ret < PNG_CHUNK_HEADER_LEN) {
        free(buff);
        return -1;
    };

    //change data endianness
    data_invert(buff, buff, PNG_CHUNK_HEADER_LEN);

    ret = PNG_header(buff, PNG_CHUNK_HEADER_LEN);
    if(ret < 0) {
        free(buff);
        return ret;
    };

    //if we succeeded in header, scan whole file:
    uint32_t ch_len, ch_typ, ch_crc;
    while(1) {
        //read chunk length and type:
        ret = fread(buff, 1, PNG_CHUNK_HEADER_LEN, fd);
        if(ret < PNG_CHUNK_HEADER_LEN) {
            ret = EOF;
            break;
        };

        //flip endianness of red data
        data_invert(buff, &ch_len, 4);
        data_invert(buff+4, &ch_typ, 4);

        //sanity check these values:
//         printf("len = %x, type = %x\n", ch_len, ch_typ);

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
//         printf("crc = %x\n", ch_crc);

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
    printf("read succ, i_data_l = %ld\n", png_data.i_data_l);

	//TODO remove this
	printf("red data = \n");
	int i;
	for(i=0; i<png_data.i_data_l; i++)
		printf("%x ", *(uint8_t *)(png_data.i_data+i) );
	printf("\n");

	//now try to uncompress our stream:

	//allocate memory for data
	size_t buff_l = 300*png_data.i_data_l;
	buff = malloc(buff_l);

	ret = inflate_str(png_data.i_data,
					  png_data.i_data_l,
					  &buff,
					  &buff_l);

	printf("uncompressed with %d, len=%ld\n", ret, buff_l);
	for(i=0; i<buff_l; i++)
		printf("%x ", *(uint8_t *)(buff+i) );
	printf("\n");

	//reverse png filters

	//first allocate buffer for reconstructed signal:
	void *rec_buff;
	rec_buff = malloc(buff_l);

	ret = reconstruct_str(buff, rec_buff, buff_l);

	printf("reconstructed with %d\n", ret);
	for(i=0; i<buff_l; i++)
		printf("%x ", *(uint8_t *)(rec_buff+i) );
	printf("\n");

	free(buff);

	//fill output data:
	*height = png_data.im_info.height;
	*length = png_data.im_info.width;
	*depth = png_data.im_info.bdepth; //TODO

	int size = ((*height) * (*length) * (*depth))/8;
// 	size = size > buff_l ? buff_l : size;

	printf("good, size = %d, =%lu, =%d\n", size, buff_l, *depth);
	*output = realloc(*output, size );
	memcpy(*output, rec_buff, size);

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
int PNG_header(void *data, int len) {
    //firstly, length of header should be 8B:
    if(len != PNG_CHUNK_HEADER_LEN)
        return -1; //TODO proper error codes

    //then compare header beggining to a magic num:
    long unsigned int magic = PNG_MAGIC_NUM;
    int ret = memcmp(data, &magic, len);
    if(ret != 0) {
        printf("comparying %lx %lx\n", magic, *(long unsigned int *)data);
        return -1; //TODO
    }

    return ret;
};

//inverts endianness of stream of length len
void data_invert(void *in, void *out, int len) {

    void *tmp_buff;
    //if points to itself, create local buffer
    if(in == out) {
        tmp_buff = malloc(len);
        memcpy(tmp_buff, in, len);
    } else
        tmp_buff = in;

    if(!tmp_buff)
        return;

    int i;
    tmp_buff += len-1;

    for(i=0; i<len; i++) {
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
	char type_str[5];

    switch(type) {
    case(IHDR):
        ret = PNG_IHDR(data, len);
        break;
    case(IDAT):
		ret = PNG_IDAT(data, len);
        break;
    case(IEND):
		ret = PNG_IEND(data, len);
        break;
    default:
		//convert to string:
		memcpy(type_str, (void *)&type, 4);
		data_invert(type_str, type_str, 4);
		type_str[4] = 0;
        printf("unknown type '%s'\n", type_str);
        break;
    }

    return ret;
}

int PNG_IHDR(void* data, int len)
{
    //check if we have correct header file:
    if(sizeof(ihdr_t) != len) {
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

    printf("w=%d, h=%d, bdepth=%x, col=%x, compr=%x, fil=%x, interl=%x\n",
           hdr->width,
           hdr->height,
           hdr->bdepth,
           hdr->color_t,
           hdr->compression_t,
           hdr->filter_m,
           hdr->interlace_m);

    //TODO add more checks as defined in png specs

    //if we accepted header, copy it to local buffer
    memcpy(&png_data.im_info,  hdr, sizeof(ihdr_t));

    //we're done
    return 0;
}

int PNG_IDAT(void* data, int len)
{
	//here we just concatenate IDAT chunk to previous IDAT chunkds

	//check arguments
	if(!data || len==0)
        return -1; //TODO
	printf("IDAT len = %d\n", len);
	//enlarge our input data pool
	png_data.i_data = realloc(png_data.i_data, png_data.i_data_l + len);
	if(!png_data.i_data)
		return -1;

	memcpy(png_data.i_data+png_data.i_data_l, data, len);
	png_data.i_data_l += len;

    return 0;
}

int PNG_IEND(void* data, int len)
{
    //we don't have to do anything at end
    return 0;
}

int inflate_str(void* data, size_t i_len, void** output, size_t* o_len)
{
    int ret;

    //zlib's stream
    ret = uncompress((Bytef *)*output, o_len, (Bytef *)data, i_len);

    return ret;
}

int data_writer(void *output, size_t o_len)
{
	//check if we have set our output
	//TODO now just fake it
	FILE *f;
	f = fopen("png_o.bmp", "wb+");

	printf("%x %x %x %x\n",
			*(uint8_t *)output,
			*(uint8_t *)(output+1),
			*(uint8_t *)(output+2),
			*(uint8_t *)(output+3));

	printf("writing output of size %ld\n", o_len);
	fwrite(output, 1, o_len, f);

	fclose(f);
	return 0;
}

int reconstruct_str(void *in_buff, void *out_buff, int len)
{
	//our data is structured in data lines.
	int i;
	uint32_t width = png_data.im_info.width;
	uint32_t *height = &png_data.im_info.height;
	uint8_t *bdepth = &png_data.im_info.bdepth; //how many bits are in a pixel;

	uint8_t *method = in_buff;
	uint32_t line_size;
	//clear output buffer
	memset(out_buff, 0, len);

	//now how many bits are in a sample:
	if(png_data.im_info.color_t == 0){
		if(*bdepth == 16) //special case for longer bdepth
			line_size = 2*width;
		else
			line_size = (*bdepth * width)/8;
	}else{
		printf("TODO color types\n");
		return -1;
	}
	printf("dep= %d\nlen=%d", *bdepth, line_size);
// 	if(png_data.im_info.bdepth == 1 && png_data.im_info.color_t == 0)
// 		width >>= 3;

	//move pointer to skip method byte
	in_buff++;
	printf("meth=%d\n", *method);
	//move accross each line
	for(i=0; i<*height; i++){

		//first byte shows us what filtering method was used:
		switch(*method){
			case(0)://none
				//data is unmodified, we can copy whole line
				memcpy(out_buff, in_buff, line_size);
				break;

			case(1)://sub
				printf("TODO sub\n");
				break;

			case(2)://up
				printf("TODO up\n");
				break;

			case(3)://average
				printf("TODO avg\n");
				break;

			case(4)://paeth
				printf("TODO pae\n");
				break;

			default:
				printf("Invalid data, %d\n", *method);
				return 0;
		};
		//move our buffer pointers accordingly
		in_buff +=line_size+1;
		out_buff +=line_size;
		method += line_size+1;
	}

	return 0;
}
