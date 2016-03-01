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
#include <zlib.h>
#include <string.h>

#define VERSION "0.1"

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

/* The super-class of a png_image, contains the decoded image plus additional
 * data. The height and width of the image are in the png_image structure.
 */
typedef struct
{
	png_image image;
	const char *file_name;
	png_bytep buffer;
	ptrdiff_t stride;
	png_uint_16 colormap[256 * 4];
	png_uint_16 pixel_bytes;
} image;

/* We use int to avoid overflow when averaging */
typedef struct {
	int r;
	int g;
	int b;
	int a;
	int pos;
} rgba;

typedef struct {
	point start;
	int ncolors;
	rgba *colors;
} gradient;

static void version_info()
{
	fprintf(stderr, "pngtocss v%s\n", VERSION);
	fprintf(stderr, "   Copyright 2011-2015 Philip Tellis\n");
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

static rgba byte2rgba(png_bytep quartet)
{
	rgba c;

	/* Pixels with an alpha value of 0 have bogus color values in the buffer, so
	 * we are normalizing them to 0.
	 */
	if (quartet[3] == 0) {
		c.r = 0;
		c.g = 0;
		c.b = 0;
	}
	else {
		c.r = quartet[0];
		c.g = quartet[1];
		c.b = quartet[2];
	}

	c.a = quartet[3];

	c.pos = -1;

	return c;
}

static rgba getpixel(const image *image, png_uint_32 x, png_uint_32 y)
{
	return byte2rgba(image->buffer +
		(y*image->image.width*image->pixel_bytes) +
		x*image->pixel_bytes);
}

static rgba rgba_avg(rgba a, rgba b)
{
	rgba c;

	/* Pixels with an alpha value of 0 will be black, which messes up the
	 * averaging in most cases. To circumvent this, the black pixel will take on
	 * the color values of the pixel it is being averaged with.
	 */

	if(a.a == 0) {
		a.r = b.r;
		a.g = b.g;
		a.b = b.b;
	}

	if(b.a == 0) {
		b.r = a.r;
		b.g = a.g;
		b.b = a.b;
	}

	c.r = (a.r+b.r)/2;
	c.g = (a.g+b.g)/2;
	c.b = (a.b+b.b)/2;
	c.a = (a.a+b.a)/2;

	return c;
}

static int rgba_equal(const rgba a, const rgba b)
{
	const int tolerance = 2;
	if(a.a == 0 && b.a == 0)
		return 1;
	/* Match with a tolerance */
	else if(a.r <= b.r+tolerance && a.r >= b.r-tolerance &&
			a.g <= b.g+tolerance && a.g >= b.g-tolerance &&
			a.b <= b.b+tolerance && a.b >= b.b-tolerance &&
			a.a <= b.a+tolerance && a.a >= b.a-tolerance)
		return 1;
	
	return 0;
}

static double decimal2ratio(int a)
{
	char buffer[256];
	snprintf(buffer, 256, "%.2f", a/255.0);
	return atof(buffer);
}

static void print_rgba(rgba c)
{
	printf("rgba(%i,%i,%i,%.3g)", c.r, c.g, c.b, decimal2ratio(c.a));
}

static void print_rgb(rgba c)
{
	printf("#%02x%02x%02x", c.r, c.g, c.b);
}

static void print_colors(gradient *g, modes mode)
{
	int i;
	if(mode == WEBKIT) {
		if(g->colors[0].a == 255)
			printf("from(#%02x%02x%02x), to(#%02x%02x%02x)",
				g->colors[0].r, g->colors[0].g, g->colors[0].b,
				g->colors[g->ncolors-1].r, g->colors[g->ncolors-1].g, g->colors[g->ncolors-1].b);
		else
			printf("from(rgba(%i,%i,%i,%.3g)), to(rgba(%i,%i,%i,%.3g))",
				g->colors[0].r, g->colors[0].g, g->colors[0].b, decimal2ratio(g->colors[0].a),
				g->colors[g->ncolors-1].r, g->colors[g->ncolors-1].g, g->colors[g->ncolors-1].b,
				decimal2ratio(g->colors[g->ncolors-1].a));
		for(i=1; i<g->ncolors-1; i++) {
			if(g->colors[i].a == 255)
				printf(", color-stop(%u%%, #%02x%02x%02x)",
					g->colors[i].pos,
					g->colors[i].r, g->colors[i].g, g->colors[i].b);
			else
				printf(", color-stop(%u%%, rgba(%i,%i,%i,%.3g))",
					g->colors[i].pos,
					g->colors[i].r, g->colors[i].g, g->colors[i].b,
					decimal2ratio(g->colors[i].a));
		}
	}
	else if(mode == YUI3) {
		for(i=0; i<g->ncolors; i++) {
			printf("\t\t\t{ color: \"");
			if(g->colors[i].a == 255)
				print_rgb(g->colors[i]);
			else
				print_rgba(g->colors[i]);
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
		if(g->colors[0].a == 255)
			print_rgb(g->colors[0]);
		else
			print_rgba(g->colors[0]);
		if(g->colors[0].pos != -1)
			printf(" %u%%", g->colors[0].pos);
		for(i=1; i<g->ncolors; i++) {
			printf(", ");
			if(g->colors[i].a == 255)
				print_rgb(g->colors[i]);
			else
				print_rgba(g->colors[i]);
			if(g->colors[i].pos != -1)
				printf(" %u%%", g->colors[i].pos);
		}
	}
}

static int calculate_gradient(const image *image, gradient *g)
{
	rgba tl, tr, bl, br, mid, avg;
	png_uint_32 i, l, base, min, max;
	png_uint_32 xy[2][2] = {{0, 0},{0, 0}};

	tl = getpixel(image, 0, 0);
	tr = getpixel(image, image->image.width-1, 0);
	bl = getpixel(image, 0, image->image.height-1);
	br = getpixel(image, image->image.width-1, image->image.height-1);

	if(rgba_equal(tl, tr)) {
		g->start = top;
		l=image->image.height;
		if(l % 2 == 1) {
			mid = getpixel(image, 0, l/2);
		}
		else {
			mid = rgba_avg(getpixel(image, 0, l/2), getpixel(image, 0, l/2-1));
		}
	}
	else if(rgba_equal(tl, bl)) {
		g->start = left;
		l=image->image.width;
		if(l % 2 == 1) {
			mid = getpixel(image, l/2, 0);
		}
		else {
			mid = rgba_avg(getpixel(image, l/2, 0), getpixel(image, l/2-1, 0));
		}
	}
	else if(rgba_equal(tr, bl) && !rgba_equal(tl, br)) {
		g->start = top_left;
		l=image->image.height;
	}
	else if(rgba_equal(tl, br) && !rgba_equal(tr, bl)) {
		g->start = top_right;
		l=image->image.height;
		tl = tr;
		br = bl;
	}
	else { /* Defaulting to vertical gradient */
		g->start = top;
		l=image->image.height;
		if(l % 2 == 1) {
			mid = getpixel(image, 0, l/2);
		}
		else {
			mid = rgba_avg(getpixel(image, 0, l/2), getpixel(image, 0, l/2-1));
		}
	}

	g->colors = (rgba *)calloc(2, sizeof(rgba));
	g->colors[0] = tl;
	g->colors[1] = br;
	g->ncolors = 2;

	/* If it's a diagonal, we only support 2 colours */
	/* If it's horizontal or vertical and the middle colour is the avg of the ends, then
	 * we only need two colours */
	/* Also, if the image is less than 3 pixels in the direction of the gradient, then you
	 * really cannot have more than 2 colours */
	if(g->start == top_left || g->start == top_right || l < 3
		|| rgba_equal(mid, rgba_avg(tl, br))) {

		return 0;
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
		avg = rgba_avg(getpixel(image, xy[0][0], xy[0][1]), getpixel(image, xy[1][0], xy[1][1]));
		if((i+base) % 2 == 0) {
			mid = getpixel(image, (xy[1][0]+xy[0][0])/2, (xy[1][1]+xy[0][1])/2);
		}
		else {
			mid = rgba_avg(
					getpixel(image, (xy[1][0]+xy[0][0])/2, (xy[1][1]+xy[0][1])/2),
					getpixel(image, (xy[1][0]+xy[0][0])/2+1, (xy[1][1]+xy[0][1])/2+1)
				     );
		}


		if(!rgba_equal(avg, mid)) {
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
				/* If current and previous positions are not the same */
				if (g->colors[g->ncolors - 2].pos != (i + base) * 100 / l &&
					(i + base) * 100 / l != 0) {
					g->ncolors++;
					g->colors = (rgba *)realloc(g->colors, sizeof(rgba)*g->ncolors);
					g->colors[g->ncolors-2] = getpixel(image, xy[1][0], xy[1][1]);
					g->colors[g->ncolors-2].pos = (i+base)*100/l;
				}

				if (i == 0) /* Needed to avoid infinite loop */
					base += i + 1;
				else
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

	if(g->colors == 0)
		return 0;
	else
		return 1;
}

static int read_png(const char *fname, image *image, gradient *g)
{
	int result = 0;
	memset(&image->image, 0, sizeof image->image);
	image->image.version = PNG_IMAGE_VERSION;

	if (png_image_begin_read_from_file(&image->image, image->file_name))
	{
		image->image.format = PNG_FORMAT_RGBA;
		image->stride = PNG_IMAGE_ROW_STRIDE(image->image);
		image->buffer = malloc(PNG_IMAGE_SIZE(image->image));
		image->pixel_bytes = PNG_IMAGE_PIXEL_SIZE(image->image.format);

		if (image->buffer != NULL) {
			if(png_image_finish_read(&image->image, NULL /*background*/,
									 image->buffer, (png_int_32)image->stride,
									 image->colormap)) {

				if(calculate_gradient(image, g))
					result = 1;
				else
					printf("pngtocss: Gradient type not supported\n");
			}
			else {
				fprintf(stderr, "pngtocss: read %s: %s\n", fname,
						image->image.message);

				png_image_free(&image->image);
			}
		}
		else
			fprintf(stderr, "pngtocss: out of memory: %lu bytes\n",
					(unsigned long)PNG_IMAGE_SIZE(image->image));
	}
	else
		/* Failed to read the argument: */
		fprintf(stderr, "pngtocss: %s: %s\n", fname, image->image.message);

	return result;
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
	char *w3points[] = { "to right", "to bottom", "to right bottom", "to left bottom" };
	char *wk_s_points[] = { "left top", "left top", "left top", "right top" };
	char *wk_e_points[] = { "right top", "left bottom", "right bottom", "left bottom" };
	int  rotations[]   = { 0, 90, 45, 135 };
	char *classname, *c;

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
	printf("\tbackground-image: linear-gradient(%s, ", w3points[g.start]);
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

static int process_file(const char *fname)
{
	image image;
	image.file_name = fname;
	int result;
	gradient g;

	result = read_png(fname, &image, &g);

	if(result)
		print_css_gradient(image.file_name, g);

	return result;
}

int main(int argc, char **argv)
{
	int i=1;
	int result = 1;

	if(argc == 1) {
		version_info();
		usage_info();
		return result;
	}

	for(i=1; i<argc; i++)
		if(result)
			result = process_file(argv[i]);

	return result;
}
