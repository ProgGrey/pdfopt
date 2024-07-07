/*
Copyrigh 2024 Sergey Astafiev
This code licensed under BSD3-clause license
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
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


struct walker
{
    u8 *in;
    u8 *out;
    unsigned long in_pos;
    unsigned long out_pos;
    unsigned long size;
} typedef walker;

walker walk_init(u8* orig, long in_size)
{
    walker desc;
    desc.in_pos = 0;
    desc.out_pos = 0;
    desc.in = orig;
    desc.out = malloc(in_size);
    desc.size = in_size;
    return desc;
}

void walk_finish(walker *desc)
{
    memcpy(desc->out + desc->out_pos, desc->in + desc->in_pos, desc->size - desc->in_pos);
    desc->out_pos += desc->size - desc->in_pos;
}


struct object
{
    u8* params;
    unsigned long par_len;
    u8* body;
    unsigned long body_len;
}typedef object;


void delete_object(object* obj)
{
    free(obj->params);
    free(obj->body);
}

bool walk_obj(walker *desc, object *obj)
{
    delete_object(obj);

}

u8* through_pdf(u8* orig, long *in_size)
{
    walker desc = walk_init(orig, *in_size);
    walk_finish(&desc);
    *in_size = desc.out_pos;
    return desc.out;
}


int main()
{
    printf("pdfopt version 0.1\n");
    printf("pdfopt is a lossless pdf compressor.\n");
    long pdf_size;
    u8* pdf_orig =  load_pdf("in.pdf", &pdf_size);
    u8* pdf_comp = through_pdf(pdf_orig, &pdf_size);
    save_pdf("out.pdf", pdf_comp, pdf_size);
    free(pdf_comp);
    free(pdf_orig);
    return 0;
}