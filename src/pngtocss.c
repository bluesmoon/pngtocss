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

enum error {
	OK = 0,
	E_NOT_PNG = -1
};

void print_error(const char *fname, enum error err)
{
	char *emsg = "";

	switch(err) {
		case E_NOT_PNG:
			emsg = "File is not a png";
			break;
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

enum error process_file(const char *fname)
{
	FILE *f;
	unsigned char sig[8];

	f = fopen(fname, "r");
	fread(sig, 1, 8, f);

	if(!png_check_sig(sig, 8)) {
		return E_NOT_PNG;
	}

	return OK;
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

