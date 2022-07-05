all: pngtocss

clean:
	rm -f *.o pngtocss

pngtocss: pngtocss.c
	gcc -o pngtocss -g $(shell libpng-config --I_opts) $(shell libpng-config --L_opts) pngtocss.c -lpng -lz

.PHONY: all clean
