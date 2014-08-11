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
int getcolor_ch(struct png_data_t *im);

//filters
int sub_filter(void *out, void *in, int line, int color_ch);
int up_filter(void *out, void *in, int line_len, int line_no);
int avg_filter(void *out, void *in, int line);
int paeth_filter(void *out, void *in, int line_len, int line_no);

int paeth_predictor(int a, int b, int c);
//***********************exported functions************************************
int PNG_decode(char* file, void** output, int *height, int* length, int *depth,
int *color_ch)
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

	//reverse png filters

	//first allocate buffer for reconstructed signal:
	void *rec_buff;
	rec_buff = malloc(buff_l);
//
	ret = reconstruct_str(buff, rec_buff, buff_l);

	free(buff);

	//fill output data:
	*height = png_data.im_info.height;
	*length = png_data.im_info.width;
	*depth = png_data.im_info.bdepth; //TODO

	*color_ch = getcolor_ch(&png_data);

	int size = ((*height) * (*length) * (*depth) * (*color_ch))/8;
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
	switch(png_data.im_info.color_t){
		case(0): //greyscal image
		if(*bdepth == 16) //special case for longer bdepth
			line_size = 2*width;
		else
			line_size = (*bdepth * width)/8;
		break;

		case(2): //truecolor image
			//on true color, only bdepth 16 and 8 are suported
			if(*bdepth != 16 && *bdepth != 8){
				printf("Wrong bdepth, %d\n", *bdepth);
				return -1;
			};
			//line size in samples is width in px * channels * B per sample
			line_size = width * ((*bdepth)>>3);
			printf("line_size = %d\n", line_size);
			break;

		case(3): //indexed color
			printf("TODO indexed color\n");
			return -1;
			break;
		case(4):
			printf("TODO greyscale+alpha\n");
			return -1;
			break;
		case(6):
			printf("TODO truecolor with alpha\n");
			return -1;
			break;
		default:
			printf("unsuported color type\n");
			return -1;
			break;
	}

	//move pointer to skip method byte
	in_buff++;
	//move accross each line
	int color_ch = getcolor_ch(&png_data);
	if(color_ch <= 0){
		printf("getcolor_ch fail\n");
		return -1;
	}

	for(i=0; i<*height; i++){
		//first byte shows us what filtering method was used:
		switch(*method){
			case(0)://none
				//data is unmodified, we can copy whole line
				memcpy(out_buff, in_buff, line_size);
				break;

			case(1)://sub
				sub_filter(out_buff, in_buff, line_size, color_ch);
//TODO move				//line_size // to function
				//sub = value + sub-1
				//sub-1 means byte before if b_depth < 8
				//or pixel before, when b_depth >= 8
				break;

			case(2)://up
				up_filter(out_buff, in_buff, line_size, i);
				break;

			case(3)://average
				printf("TODO avg\n");
				break;

			case(4)://paeth
				paeth_filter(out_buff, in_buff, line_size, i);
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

int sub_filter(void *out, void *in, int line_len, int color_ch){
	//cast for our convenience:
	uint8_t *last_b = out;
	uint8_t *out_b = out;
	uint8_t *in_b = in;

	uint8_t step_size = png_data.im_info.bdepth > 16? 2 : 1;
	//step_size *= color_ch;
	uint32_t buffr;

	//first pixel hasn't any diff
	memcpy(out_b, in_b, step_size*color_ch);
	out_b += step_size*color_ch;
	in_b += step_size*color_ch;

	int i;
	//now copy each value, also skip first one
	printf("step size = %d, line_len=%d\n", step_size, line_len);

	uint8_t sub_p;
	for(i=1; i<line_len;){
		if(step_size>1){
			for(sub_p=0; sub_p < color_ch; sub_p++){
				buffr = (*(uint16_t *)in_b+sub_p + *(uint16_t *)last_b + sub_p);
				buffr %= 0xFFFF;
				*((uint16_t *)out_b+sub_p) = buffr; //TEST this
			}
		}else{
			for(sub_p = 0; sub_p < color_ch; sub_p++){
				buffr = (*(in_b+sub_p) + *(last_b+sub_p));
				buffr &= 0xFF;
				*(out_b+sub_p) = buffr;
			}
			data_invert(out_b, out_b, color_ch);
		}
		out_b += step_size * color_ch;
		in_b += step_size * color_ch;
		last_b += step_size * color_ch;
		i += step_size;
	}




	return 0;
};

int up_filter(void *out, void *in, int line_len, int line_no)
{
	//cast for our convenience:
	uint8_t *out_b = out;
	uint8_t *in_b = in;

	uint8_t up = 0;
	uint16_t buffr;


	int i;
	//now copy each value, also skip first one
	for(i=1; i<line_len;i++){
		if(line_no != 0){
			up = *(uint8_t *)(out_b - line_len);
		};
		buffr = (*in_b + up);
		*out_b = buffr & 0xFF;
		out_b ++;
		in_b ++;
	}

	return 0;
};

int avg_filter(void *out, void *in, int line){
	printf("AVG TODO\n");
	return 0;
};

int paeth_filter(void *out, void *in, int line_len, int line_no){
// 	works as:
// 	out(x) = in(x) + predictor( out(x-1), out(y-1), out(y-1,x-1))
	//cast for our convenience:
	uint8_t *last_b = out;
	uint8_t *out_b = out;
	uint8_t *in_b = in;

	uint8_t up, left, upleft; //pixel values

	uint8_t step_size = png_data.im_info.bdepth > 16? 2 : 1;
	uint32_t buffr;

	int i;

	for(i=0; i<line_len;){

		//update our predictor values:

		//we won't have any parameters for predictor if we're
		if(i==0 && line_no ==0){
		//at leftmost pixel:
			left = 0;
			upleft = 0;
			up = 0;
		}else if(i==0){
		//if we're leftmost, but not on the first line
			left =0;
			upleft = 0;
			up = *(uint8_t *)(out - line_len);
		}else if(line_no==0){
		//if we're on the first line, but not leftmost
			left = *last_b;
			upleft=0;
			up=0;
		//normal case
		}else{
			left = *last_b;
			upleft = *(uint8_t *)(out - line_len -step_size);
			up = *(uint8_t *)(out - line_len);
		};

		//get predictor
		buffr = paeth_predictor(left, up, upleft);
		//add current value
		buffr += *in_b;
		//truncate it just in case:
		*out_b = buffr & 0xFF;

		//move our data pointers:
		last_b ++;
		out_b ++;
		in_b ++;
		i++;
	}

	return 0;
};

int paeth_predictor(int a, int b, int c)
{
	int p = a + b - c;
	int pa = abs(p-a);
	int pb = abs(p-b);
	int pc = abs(p-c);

	if(pa <= pb && pa <= pc)
		return a;
	if(pb <= pc)
		return b;
	return c;
}

int getcolor_ch(struct png_data_t *im)
{
		//TODO clean this
	switch(png_data.im_info.color_t){
		case(0)://greyscale
			return 1;
		case(2):
		case(3):
			return 3;
		case(4):
			return 2;
		case(6):
			return 4;
		default:
			printf("Invalid color chanel number\n");
			return -1;
	}
};
