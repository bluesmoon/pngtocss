all: pngtocss

clean:
	rm -f *.o pngtocss

pngtocss: pngtocss.c
	gcc -o pngtocss -g -I/opt/local/include -L/opt/local/lib -lpng -lz pngtocss.c

.PHONY: all clean
