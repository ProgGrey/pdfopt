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
    desc->in_pos = desc->size;
}

void walk_steps(walker *desc, unsigned long steps)
{
    memcpy(desc->out + desc->out_pos, desc->in + desc->in_pos, steps);
    desc->in_pos += steps;
    desc->out_pos += steps;
}


struct object
{
    u8* header;
    unsigned long long h_len;
    u8* body;
    unsigned long long b_len;
}typedef object;


void delete_object(object* obj)
{
    free(obj->body);
    free(obj->header);
    obj->body = NULL;
    obj->header = NULL;
    obj->b_len = 0;
    obj->h_len = 0;
}



u8* obj_read_header(u8* str, size_t str_len, unsigned long long *size)
{
    int num = 1;
    u8* p = memmem(str, str_len, "<<", 2);
    if(p != NULL){
        p += 2;
    } else{
        return NULL;
    }
    u8* start = p;
    //printf("%.*s\n",10,start);
    do
    {
        u8* pl = memmem(p, str_len - (p - str), "<<", 2);
        u8* pr = memmem(p, str_len - (p - str), ">>", 2);
        if((pl !=  NULL ) && (pl < pr)){
            p = pl+2;
            num++;
        }else{
            p = pr+2;
            num--;
        }
    } while (num>0);
    //printf("%.*s\n",10,p);
    *size = (u8*)p - start - 2;
    u8* buf = malloc(*size + 1);
    memcpy(buf, start, *size);
    buf[*size] = 0;
    return buf;
}



u8* obj_read_stream(u8* str, size_t str_len, u8 **end, unsigned long long size)
{
    u8 stream_begin_id[] = {0x0A, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x0A};
    u8 stream_end_id[] = {0x0A, 0x65, 0x6E, 0x64, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x0A};
    u8* p = memmem(str, str_len, stream_begin_id, 8);
    if(p != NULL){
        u8* buf = malloc(size);
        memcpy(buf, p+8, size);
        *end = memmem(p+8+size, str_len - (p+8+size - str), stream_end_id, 11)+10;
        return buf;
    } else{
        
        return NULL;
    }
    //*/
   return NULL;
}
//*/

bool walk_obj(walker *desc, object *obj)
{
    const char obj_begin_id[] = {0x20, 0x6F, 0x62, 0x6A, 0x0A, 0x3C, 0x3C};//" obj\n<<""
    const char obj_end_id[] = {0x0A, 0x65, 0x6E, 0x64, 0x6F, 0x62, 0x6A, 0x0A};//"\nendobj\n"
    delete_object(obj);
    //Find begining of the object
    u8* po1 = (u8*) memmem((desc->in + desc->in_pos), desc->size - desc->in_pos, obj_begin_id, 7);
    if(po1 == NULL){
        return false;
    }
    // Copy all bytes before object
    walk_steps(desc, po1 - (desc->in + desc->in_pos));
    // Read object header:
    obj->header = obj_read_header(po1 + 4, desc->size - (po1 + 4 - desc->in) , &obj->h_len);
    u8* po2 = po1 + 7 + obj->h_len + 2;//End of the object. Now it point to the end of the header
    // If header contains Length value then this object contains stream
    u8* plen = memmem(obj->header, obj->h_len, "/Length ", 8);
    if(plen != NULL){
        obj->b_len = atoll((const char*)plen+8);
        obj->body = obj_read_stream(po2, desc->size - (po2 - desc->in), &po2, obj->b_len);
    }else{
        obj->body = NULL;
        obj->b_len = 0;
    }
    //find end of the object.
    po2 = memmem(po2, desc->size - (po2 - desc->in) , obj_end_id, 8);
    walk_steps(desc, po2 - po1);
    //*/
    return true;
}

u8* through_pdf(u8* orig, long *in_size)
{
    walker desc = walk_init(orig, *in_size);
    object obj;
    obj.body = NULL;
    obj.header = NULL;
    while(walk_obj(&desc, &obj)){
    //walk_obj(&desc, &obj);
        if(obj.h_len != 0){
            printf("%s\n", obj.header);
        }
        if(obj.b_len != 0){
            printf("Found stream: length %llu\n", obj.b_len);
        }
    };
    //delete_object(&obj);
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