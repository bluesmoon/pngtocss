/**
 * pngtocss: Read a png file with a gradient in it and print out the CSS to do that in HTML
 *
 * Note that starting this, I know nothing about PNGs nor CSS gradients.  Use at your own risk
 * YMMV
 *
 * Copyright 2011 Philip Tellis -- <philip@bluesmoon.info> -- http://bluesmoon.info
 */

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

#define VERSION "0.1"

enum error {
	OK = 0,
	E_NOT_PNG,
	E_NO_MEM,
	E_INTERNAL
};

typedef enum point {
	top,
	left,
	top_left,
	top_right,
	bottom,
	right,
	bottom_left,
	bottom_right
} point;

typedef struct rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} rgb;

typedef struct box {
	rgb tl;
	rgb tr;
	rgb bl;
	rgb br;
} box;

void print_error(const char *fname, enum error err)
{
	char *emsg = "";

	switch(err) {
		case E_NOT_PNG:
			emsg = "File is not a png";
			break;
		case E_NO_MEM:
			emsg = "Out of memory";
			break;
		case E_INTERNAL:
			emsg = "Problem inside libpng";
	}

	fprintf(stderr, "Error with ``%s''; %s\n", fname, emsg);
}

void version_info()
{
	fprintf(stderr, "pngtocss v%s\n", VERSION);
	fprintf(stderr, "   Compiled with libpng %s; using libpng %s.\n",
			PNG_LIBPNG_VER_STRING, png_libpng_ver);
	fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n",
			ZLIB_VERSION, zlib_version);
}

enum error check_sig(FILE *f)
{
	unsigned char sig[8];
	fread(sig, 1, 8, f);
	if(!png_check_sig(sig, 8)) {
		return E_NOT_PNG;
	}

	return OK;
}

enum error read_png(FILE *f, box *out)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type, i;
	png_uint_32 rowbytes;
	png_bytep *row_pointers;
	unsigned char *image_data;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if(!png_ptr) {
		return E_NO_MEM;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return E_NO_MEM;
	}

	/* TODO Figure out how to do this without setjmp */
	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return E_INTERNAL;
	}

	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	if( (row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height)) == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return E_NO_MEM;
	}

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	if( (image_data = (unsigned char *)malloc(rowbytes * height)) == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		free(row_pointers);
		return E_NO_MEM;
	}

	for(i=0; i<height; i++) {
		row_pointers[i] = image_data + i*rowbytes;
	}

	png_read_image(png_ptr, row_pointers);

	out->tl.r = image_data[0];
	out->tl.g = image_data[1];
	out->tl.b = image_data[2];

	out->tr.r = image_data[rowbytes-3];
	out->tr.g = image_data[rowbytes-2];
	out->tr.b = image_data[rowbytes-1];

	out->bl.r = row_pointers[height-1][0];
	out->bl.g = row_pointers[height-1][1];
	out->bl.b = row_pointers[height-1][2];

	out->br.r = row_pointers[height-1][rowbytes-3];
	out->br.g = row_pointers[height-1][rowbytes-2];
	out->br.b = row_pointers[height-1][rowbytes-1];

	png_read_end(png_ptr, NULL);

	free(image_data);
	free(row_pointers);

	return OK;
}

int rgb_equal(rgb a, rgb b)
{
	if(a.r == b.r && a.g == b.g && a.b == b.b) {
		return 1;
	}
	return 0;
}

void print_css_gradient(box b)
{
	point start;
	rgb color1, color2;
	char *points[] = { "top", "left", "left top", "right top" };
	char *wk_s_points[] = { "left top", "left top", "left top", "right top" };
	char *wk_e_points[] = { "left bottom", "right top", "right bottom", "left bottom" };

	color1=b.tl;
	color2=b.br;

	if(rgb_equal(b.tl, b.tr)) {
		start = top;
	}
	else if(rgb_equal(b.tl, b.bl)) {
		start = left;
	}
	else if(rgb_equal(b.tr, b.bl) && !rgb_equal(b.tl, b.br)) {
		start = top_left;
	}
	else if(rgb_equal(b.tl, b.br) && !rgb_equal(b.tr, b.bl)) {
		start = top_right;
		color1=b.tr;
		color2=b.bl;
	}

	printf(".gradient {\n");
	/* Gecko */
	printf("\tbackground-image: -moz-linear-gradient(%s, #%02x%02x%02x, #%02x%02x%02x);\n",
			points[start], color1.r, color1.g, color1.b, color2.r, color2.g, color2.b);
	/* Safari 4+, Chrome 1+ */
	printf("\tbackground-image: -webkit-gradient(linear, %s, %s, from(#%02x%02x%02x), to(#%02x%02x%02x));\n",
			wk_s_points[start], wk_e_points[start], color1.r, color1.g, color1.b, color2.r, color2.g, color2.b);
	/* Safari 5.1+, Chrome 10+ */
	printf("\tbackground-image: -webkit-linear-gradient(%s, #%02x%02x%02x, #%02x%02x%02x);\n",
			points[start], color1.r, color1.g, color1.b, color2.r, color2.g, color2.b);
	/* Opera */
	printf("\tbackground-image: -o-linear-gradient(%s, #%02x%02x%02x, #%02x%02x%02x);\n",
			points[start], color1.r, color1.g, color1.b, color2.r, color2.g, color2.b);
	printf("}\n");

}

enum error process_file(const char *fname)
{
	FILE *f;
	enum error status=OK;
	box b;

	f = fopen(fname, "r");
	status = check_sig(f);

	if(status == OK) {
		status = read_png(f, &b);
	}

	fclose(f);

	if(status == OK) {
		print_css_gradient(b);
	}

	return status;
}

int main(int argc, char **argv)
{
	int i=1;
	enum error err=OK;

	if(argc == 1) {
		version_info();
		return OK;
	}

	for(i=1; i<argc; i++) {
		if((err = process_file(argv[i])) != OK) {
			print_error(argv[i], err);
		}
	}

	return err;
}

