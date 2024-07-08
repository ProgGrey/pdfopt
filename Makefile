CC=clang
C_FLAGS=-O0 -march=native -std=gnu11 -g -fsanitize=address
LIBS=-lzopfli -lz

test:pdfopt
	./pdfopt && sha256sum *.pdf

pdfopt:main.c
	$(CC) ${C_FLAGS} ${LIBS} main.c -o pdfopt

clean:
	rm pdfopt