/**
 * pngtocss: Read a png file with a gradient in it and print out the CSS to do that in HTML
 *
 * Note that starting this, I know nothing about PNGs nor CSS gradients.  Use at your own risk
 * YMMV
 *
 * Copyright 2011 Philip Tellis -- <philip@bluesmoon.info> -- http://bluesmoon.info
 */

#include <stdio.h>
#include <png.h>

#define VERSION "0.1"

void version_info()
{
	fprintf(stderr, "pngtocss v%s\n", VERSION);
	fprintf(stderr, "   Compiled with libpng %s; using libpng %s.\n",
			PNG_LIBPNG_VER_STRING, png_libpng_ver);
	fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n",
			ZLIB_VERSION, zlib_version);
}

int main(int argc, char **argv)
{
	if(argc == 1) {
		version_info();
		return 0;
	}
	return 0;
}
