CC=clang
C_FLAGS=-O2 -march=native -std=c11
LIBS=-lzopfli -lz

pdfopt:main.c
	$(CC) ${C_FLAGS} ${LIBS} main.c -o pdfopt

clean:
	rm pdfopt