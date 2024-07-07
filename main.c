/*
Copyrigh 2024 Sergey Astafiev
This code licensed under BSD3-clause license
*/
#include <stdio.h>
#include <stdint.h>
#include <zopfli/zopfli.h>

typedef unsigned char u8;


// Load pdf to memory
u8* load_pdf(const char* filename, long *size)
{
    FILE *desc = fopen(filename, "rb");
    fseek(desc, 0, SEEK_END);
    *size = ftell(desc);
    u8* buf = malloc(*size);
    fseek(desc, 0, SEEK_SET);
    fread(buf, 1, *size, desc);
    fclose(desc);
    return buf;
}

// Save pdf to disc
void save_pdf(const char* filename, u8* buff, size_t size)
{
    FILE *desc = fopen(filename, "wb");
    fwrite(buff,1,size,desc);
    fclose(desc);
}




int main()
{
    printf("pdfopt version 0.1\n");
    printf("pdfopt is a lossless pdf compressor.\n");
    long pdf_size;
    u8* pdf_orig =  load_pdf("in.pdf", &pdf_size);
    save_pdf("out.pdf", pdf_orig, pdf_size);
    return 0;
}