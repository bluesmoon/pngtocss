/**
 * pngtocss: Read a png file with a gradient in it and print out the CSS to do that in HTML
 *
 * Note that starting this, I know nothing about PNGs nor CSS gradients.  Use at your own risk
 * YMMV
 *
 * Copyright 2011 Philip Tellis -- <philip@bluesmoon.info> -- http://bluesmoon.info
 *
 * License: BSD
 */

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

#define VERSION "0.1"

typedef enum status {
	OK = 0,
	E_NOT_PNG,
	E_NO_MEM,
	E_INTERNAL,
	E_NO_FILE
} status;

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

static void print_error(const char *fname, status err)
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
			break;
		case E_NO_FILE:
			emsg = "Could not open file";
			break;
	}

	fprintf(stderr, "Error with ``%s''; %s\n", fname, emsg);
}

static void version_info()
{
	fprintf(stderr, "pngtocss v%s\n", VERSION);
	fprintf(stderr, "   Copyright 2011 Philip Tellis\n");
	fprintf(stderr, "   https://github.com/bluesmoon/pngtocss\n\n");
	fprintf(stderr, "   Distributed under the terms of the BSD license\n\n");
	fprintf(stderr, "   Compiled with libpng %s; using libpng %s.\n",
			PNG_LIBPNG_VER_STRING, png_libpng_ver);
	fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n\n",
			ZLIB_VERSION, zlib_version);
}

static void usage_info()
{
	fprintf(stderr, "Usage: pngtocss <image1.png> <image2.png> ...\n");
}

/*
 * Thanks to this site for information on reading a png:
 * http://www.libpng.org/pub/png/book/chapter13.html
 */
static status check_sig(FILE *f)
{
	unsigned char sig[8];
	int n;
	n = fread(sig, 1, 8, f);
	if(n < 8 || !png_check_sig(sig, 8)) {
		return E_NOT_PNG;
	}

	return OK;
}

static status read_png(FILE *f, box *out)
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

static int rgb_equal(rgb a, rgb b)
{
	if(a.r == b.r && a.g == b.g && a.b == b.b) {
		return 1;
	}
	return 0;
}

/*
 * Thanks to the following pages for info about this function:
 * http://css-tricks.com/css3-gradients/
 * http://webdesignerwall.com/tutorials/cross-browser-css-gradient
 * http://hacks.mozilla.org/2009/11/css-gradients-firefox-36/
 * http://www.tankedup-imaging.com/css_dev/css-gradient.html
 */
static void print_css_gradient(const char *fname, box b)
{
	point start;
	rgb color1, color2;
	char *points[] = { "top", "left", "left top", "right top" };
	char *wk_s_points[] = { "left top", "left top", "left top", "right top" };
	char *wk_e_points[] = { "left bottom", "right top", "right bottom", "left bottom" };
	char *classname, *c;

	classname = (char *)malloc(strlen(fname));
	c=strrchr(fname, '/');
	if(c)
		c++;
	else
		c=(char *)fname;

	strcpy(classname, c);
	c=strchr(classname, '.');
	if(c) {
		*c='\0';
	}

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

	printf(".%s {\n", classname);
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

static status process_file(const char *fname)
{
	FILE *f;
	status stat=OK;
	box b;

	f = fopen(fname, "rb");
	if(!f) {
		return E_NO_FILE;
	}
	stat = check_sig(f);

	if(stat == OK) {
		stat = read_png(f, &b);
	}

	fclose(f);

	if(stat == OK) {
		print_css_gradient(fname, b);
	}

	return stat;
}

int main(int argc, char **argv)
{
	int i=1;
	status err=OK;

	if(argc == 1) {
		version_info();
		usage_info();
		return OK;
	}

	for(i=1; i<argc; i++) {
		if((err = process_file(argv[i])) != OK) {
			print_error(argv[i], err);
		}
	}

	return err;
}

