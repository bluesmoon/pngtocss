all: pngtocss

clean:
	rm -f *.o pngtocss

pngtocss: pngtocss.c
	gcc -o pngtocss -g -I/opt/local/include -L/opt/local/lib -I/usr/X11/include -L/usr/X11/lib -I/usr/include -L/usr/lib pngtocss.c -lpng -lz

.PHONY: all clean
