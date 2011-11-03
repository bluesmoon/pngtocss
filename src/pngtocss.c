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

typedef enum {
	OK = 0,
	E_NOT_PNG,
	E_NO_MEM,
	E_INTERNAL,
	E_NO_FILE,
	E_NOT_SUPPORTED
} status;

typedef enum {
	CSS3 = 0,
	WEBKIT = 1,
	YUI3 = 2
} modes;

typedef enum {
	left,
	top,
	top_left,
	top_right,
	right,
	bottom,
	bottom_left,
	bottom_right
} point;

typedef struct {
	unsigned char *data;
	png_uint_32 height;
	png_uint_32 width;
	int pxbytes;
	png_bytep *row_pointers;
} bmp;

/* We use int to avoid overflow when averaging */
typedef struct {
	int r;
	int g;
	int b;
	int pos;
} rgb;

typedef struct {
	point start;
	int ncolors;
	rgb *colors;
} gradient;

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
		case E_NOT_SUPPORTED:
			emsg = "Gradient type not supported";
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

static rgb byte2rgb(png_bytep triad)
{
	rgb c;

	c.r = triad[0];
	c.g = triad[1];
	c.b = triad[2];

	c.pos = -1;

	return c;
}

static rgb getpixel(const bmp *image, png_uint_32 x, png_uint_32 y)
{
	return byte2rgb(image->data + (y*image->width*image->pxbytes) + x*image->pxbytes);
}

static rgb rgb_avg(rgb a, rgb b)
{
	rgb c;

	c.r = (a.r+b.r)/2;
	c.g = (a.g+b.g)/2;
	c.b = (a.b+b.b)/2;

	return c;
}

static int rgb_equal(rgb a, rgb b)
{
	/* Match with a tolerance of 2 */
	if(a.r <= b.r+2 && a.r >= b.r-2
	&& a.g <= b.g+2 && a.g >= b.g-2
       	&& a.b <= b.b+2 && a.b >= b.b-2) {
		return 1;
	}
	return 0;
}

static void print_rgb(rgb c)
{
	printf("#%02x%02x%02x", c.r, c.g, c.b);
}

static void print_colors(gradient *g, modes mode)
{
	int i;
	if(mode == WEBKIT) {
		printf("from(#%02x%02x%02x), to(#%02x%02x%02x)",
				g->colors[0].r, g->colors[0].g, g->colors[0].b,
				g->colors[g->ncolors-1].r, g->colors[g->ncolors-1].g, g->colors[g->ncolors-1].b);
		for(i=1; i<g->ncolors-1; i++) {
			printf(", color-stop(%u%%, #%02x%02x%02x)",
				g->colors[i].pos,
				g->colors[i].r, g->colors[i].g, g->colors[i].b);
		}
	}
	else if(mode == YUI3) {
		for(i=0; i<g->ncolors; i++) {
			printf("\t\t\t{ color: \"");
			print_rgb(g->colors[i]);
			printf("\"");
			if(g->colors[i].pos != -1)
				printf(", offset: %0.2f", (float)g->colors[i].pos/100);
			printf(" }");
			if(i<g->ncolors-1)
				printf(",");
			printf("\n");
		}
	}
	else {
		print_rgb(g->colors[0]);
		if(g->colors[0].pos != -1)
			printf(" %u%%", g->colors[0].pos);
		for(i=1; i<g->ncolors; i++) {
			printf(", ");
			print_rgb(g->colors[i]);
			if(g->colors[i].pos != -1)
				printf(" %u%%", g->colors[i].pos);
		}
	}
}

static void calculate_gradient(const bmp *image, gradient *g)
{
	rgb tl, tr, bl, br, mid, avg;
	png_uint_32 i, l, base, min, max;
	png_uint_32 xy[2][2] = {{0, 0},{0, 0}};

	tl = getpixel(image, 0, 0);
	tr = getpixel(image, image->width-1, 0);
	bl = getpixel(image, 0, image->height-1);
	br = getpixel(image, image->width-1, image->height-1);

	if(rgb_equal(tl, tr)) {
		g->start = top;
		l=image->height;
		if(l % 2 == 1) {
			mid = getpixel(image, 0, l/2);
		}
		else {
			mid = rgb_avg(getpixel(image, 0, l/2), getpixel(image, 0, l/2-1));
		}
	}
	else if(rgb_equal(tl, bl)) {
		g->start = left;
		l=image->width;
		if(l % 2 == 1) {
			mid = getpixel(image, l/2, 0);
		}
		else {
			mid = rgb_avg(getpixel(image, l/2, 0), getpixel(image, l/2-1, 0));
		}
	}
	else if(rgb_equal(tr, bl) && !rgb_equal(tl, br)) {
		g->start = top_left;
		l=image->height;
	}
	else if(rgb_equal(tl, br) && !rgb_equal(tr, bl)) {
		g->start = top_right;
		l=image->height;
		tl = tr;
		br = bl;
	}

	g->colors = (rgb *)calloc(2, sizeof(rgb));
	g->colors[0] = tl;
	g->colors[1] = br;
	g->ncolors = 2;

	/* If it's a diagonal, we only support 2 colours */
	/* If it's horizontal or vertical and the middle colour is the avg of the ends, then
	 * we only need two colours */
	/* Also, if the image is less than 3 pixels in the direction of the gradient, then you
	 * really cannot have more than 2 colours */
	if(g->start == top_left || g->start == top_right || l < 3
		|| rgb_equal(mid, rgb_avg(tl, br))) {

		return;
	}

	/* Now we come to the complicated part where there are more than 2 colours 
	 * The good thing though is that it's either horizontal or vertical at this point
	 * and that it is at least 3 pixels long in the direction of the gradient
	 *
	 * So this is what we'll do.
	 * - take a slice of the image from the top (or left) and see if the mid pixel matches
	 *   the average of the two ends.  we start at 3 pixels.
	 * - if it does, then double the size of the slice and retry (until you reach the end of the image)
	 * - if it does not match, then reduce until it does match this is the first stop
	 */

	min = base = 0;
	xy[0][g->start] = base;
	max = i = 2;
	while(i+base<l) {
		xy[1][g->start] = i+base;
		avg = rgb_avg(getpixel(image, xy[0][0], xy[0][1]), getpixel(image, xy[1][0], xy[1][1]));
		if((i+base) % 2 == 0) {
			mid = getpixel(image, (xy[1][0]+xy[0][0])/2, (xy[1][1]+xy[0][1])/2);
		}
		else {
			mid = rgb_avg(
					getpixel(image, (xy[1][0]+xy[0][0])/2, (xy[1][1]+xy[0][1])/2),
					getpixel(image, (xy[1][0]+xy[0][0])/2+1, (xy[1][1]+xy[0][1])/2+1)
				     );
		}


		if(!rgb_equal(avg, mid)) {
			if(min == max) {
				min++;
				max=i=min+2;
			}
			else {
				max = i;
				i = (i+min)/2;
			}
		}
		else if(max-i<=1 && i-min<=1) {
			/* We've converged */
			if(base+i >= l-1) {
				/* This is the same as the end point, so skip */
				i++;
			}
			else {
				g->ncolors++;
				g->colors = (rgb *)realloc(g->colors, sizeof(rgb)*g->ncolors);
				g->colors[g->ncolors-2] = getpixel(image, xy[1][0], xy[1][1]);
				g->colors[g->ncolors-2].pos = (i+base)*100/l;

				base += i;
				min = 0;
				max = i = l-base-1;
				xy[0][g->start] = base;
			}
		}
		else {
			min = i;
			if(i == max) {
				i*=2;
				if(i+base >= l)
					i = l-base-1;
				max = i;
			}
			else {
				i = (i+max)/2;
			}
		}
	}
	g->colors[g->ncolors-1] = br;
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

static status read_png(FILE *f, gradient *g)
{
	png_structp png_ptr;
	png_infop info_ptr;
	int bit_depth, color_type, i;
	png_uint_32 rowbytes;
	bmp image;

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
#if (PNG_LIBPNG_VER < 10500)
	if (setjmp(png_ptr->jmpbuf)) {
#else
	if (setjmp(png_jmpbuf(png_ptr))) {
#endif
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return E_INTERNAL;
	}

	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &image.width, &image.height, &bit_depth, &color_type, NULL, NULL, NULL);

	if( (image.row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * image.height)) == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return E_NO_MEM;
	}

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	image.pxbytes = rowbytes/image.width;

	if( (image.data = (unsigned char *)malloc(rowbytes * image.height)) == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		free(image.row_pointers);
		return E_NO_MEM;
	}

	for(i=0; i<image.height; i++) {
		image.row_pointers[i] = image.data + i*rowbytes;
	}

	png_read_image(png_ptr, image.row_pointers);
	png_read_end(png_ptr, NULL);

	calculate_gradient(&image, g);

	free(image.data);
	free(image.row_pointers);

	if(g->ncolors == 0) {
		return E_NOT_SUPPORTED;
	}

	return OK;
}

/*
 * Thanks to the following pages for info about this function:
 * http://css-tricks.com/css3-gradients/
 * http://webdesignerwall.com/tutorials/cross-browser-css-gradient
 * http://hacks.mozilla.org/2009/11/css-gradients-firefox-36/
 * http://www.tankedup-imaging.com/css_dev/css-gradient.html
 */
static void print_css_gradient(const char *fname, gradient g)
{
	char *points[] = { "left", "top", "left top", "right top" };
	char *wk_s_points[] = { "left top", "left top", "left top", "right top" };
	char *wk_e_points[] = { "right top", "left bottom", "right bottom", "left bottom" };
	int  rotations[]   = { 0, 90, 45, 135 };
	char *classname, *c;
	int i;

	if(g.ncolors == 0) {
		return;
	}

	classname = (char *)calloc(strlen(fname)+1, 1);
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

	printf(".%s {\n", classname);

	/* Gecko */
	printf("\tbackground-image: -moz-linear-gradient(%s, ", points[g.start]);
	print_colors(&g, CSS3);
	printf(");\n");
	/* Safari 4+, Chrome 1+ */
	printf("\tbackground-image: -webkit-gradient(linear, %s, %s, ", wk_s_points[g.start], wk_e_points[g.start]);
	print_colors(&g, WEBKIT);
	printf(");\n");
	/* Safari 5.1+, Chrome 10+ */
	printf("\tbackground-image: -webkit-linear-gradient(%s, ", points[g.start]);
	print_colors(&g, CSS3);
	printf(");\n");
	/* Opera */
	printf("\tbackground-image: -o-linear-gradient(%s, ", points[g.start]);
	print_colors(&g, CSS3);
	printf(");\n");
	/* Unprefixed */
	printf("\tbackground-image: linear-gradient(%s, ", points[g.start]);
	print_colors(&g, CSS3);
	printf(");\n");

	printf("}\n");

	printf("graphics = graphics || {};\n");
	printf("graphics[\"%s\"] = new Y.Graphic({ render: '#%s' });\n", classname, classname);
	printf("graphics[\"%s\"].addShape({\n", classname);
	printf("\ttype: \"rect\",\n");
	printf("\theight: 200, width: 200,\n");
	printf("\tfill: {\n");
	printf("\t\ttype: \"linear\",\n");
	printf("\t\tstops: [\n");
	print_colors(&g, YUI3);
	printf("\t\t],\n");
	printf("\t\trotation: %d\n", rotations[g.start]);
	printf("\t}\n");
	printf("\n});\n");

	free(classname);
	free(g.colors);
	g.colors=NULL;
}

static status process_file(const char *fname)
{
	FILE *f;
	status stat=OK;
	gradient g;

	f = fopen(fname, "rb");
	if(!f) {
		return E_NO_FILE;
	}
	stat = check_sig(f);

	if(stat == OK) {
		stat = read_png(f, &g);
	}

	fclose(f);

	if(stat == OK) {
		print_css_gradient(fname, g);
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
