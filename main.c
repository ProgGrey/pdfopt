/*
Copyrigh 2024 Sergey Astafiev
This code licensed under BSD3-clause license
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <zlib.h>
#include <zopfli/zopfli.h>

typedef unsigned char u8;


u8 obj_begin_id[] = {0x20, 0x6F, 0x62, 0x6A, 0x0A, 0x3C, 0x3C};//" obj\n<<""
u8 obj_end_id[] = {0x0A, 0x65, 0x6E, 0x64, 0x6F, 0x62, 0x6A, 0x0A};//"\nendobj\n"
u8 stream_begin_id[] = {0x0A, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x0A};//\nstream\n
u8 stream_end_id[] = {0x0A, 0x65, 0x6E, 0x64, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x0A};//\nendstream\n



// Load pdf into memory
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
    u8* p = memmem(str, str_len, stream_begin_id, sizeof(stream_begin_id));
    if(p != NULL){
        u8* buf = malloc(size);
        memcpy(buf, p+8, size);
        *end = memmem(p+8+size, str_len - (p+8+size - str), stream_end_id, sizeof(stream_end_id));
        *end += 10;
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
    delete_object(obj);
    //Find begining of the object
    u8* po1 = (u8*) memmem((desc->in + desc->in_pos), desc->size - desc->in_pos, obj_begin_id, sizeof(obj_begin_id));
    if(po1 == NULL){
        return false;
    }
    // Copy all bytes before object
    walk_steps(desc, po1 - (desc->in + desc->in_pos));
    // Read object header:
    obj->header = obj_read_header(po1 + 4, desc->size - (po1 + 4 - desc->in) , &obj->h_len);
    u8* po2 = po1 + sizeof(obj_begin_id) + obj->h_len + 2;//End of the object. Now it point to the end of the header
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
    po2 = memmem(po2, desc->size - (po2 - desc->in) , obj_end_id, sizeof(obj_end_id));
    //walk_steps(desc, po2 - po1);
    desc->in_pos += po2 - po1 + sizeof(obj_end_id);
    //*/
    return true;
}

void walk_write(walker *desc, u8 *mem, size_t size)
{
    memcpy(desc->out+desc->out_pos, mem, size);
    desc->out_pos += size;
}


void write_obj(walker *desc, object *obj)
{
    walk_write(desc, obj_begin_id, sizeof(obj_begin_id));
    walk_write(desc, obj->header, obj->h_len);
    walk_write(desc, (u8*)">>", 2);
    if(obj->b_len != 0){
        walk_write(desc, stream_begin_id, sizeof(stream_begin_id));
        walk_write(desc, obj->body, obj->b_len);
        walk_write(desc, stream_end_id, sizeof(stream_end_id)-1);
    }
    walk_write(desc, obj_end_id, sizeof(obj_end_id));
}

u8 hextohb(u8 c)
{
    if(('0' <= c) && (c <= '9')){
        return c - '0';
    } else if(('A' <= c) && (c <= 'F')){
        return c - 'A' + 10;
    } else if(('a' <= c) && (c <= 'f')){
        return c - 'a' + 10;
    }else{
        return 0;
    }
}

u8* dec_ascii_hex(u8* str, size_t *len)
{
    u8* buf = malloc(*len/2);
    *len = *len & (SIZE_MAX - 1);
    size_t true_len = 0;
    u8 val = 0;
    bool flag = false;
    for(size_t k = 0; k < *len; k++){
        if(str[k] != ' '){
            if(flag){
                flag = false;
                val <<= 4;
                val |= hextohb(str[k]);
                buf[true_len] = val;
                true_len++;
            } else{
                flag = true;
                val = hextohb(str[k]);
            }
            if(str[k] == '>'){
                break;
            }
        }
    }
    *len= true_len;
    return buf;
}

u8* dec_ascii85(u8* str, size_t *len)
{
    size_t t_len = *len * 4 / 5;
    for(size_t k = 0; k < *len; k++){
        if(str[k] == 'z'){
            t_len+=4;
        }
    }
    u8* buf = malloc(t_len);
    uint64_t val = 0;
    int flag = 0;
    t_len = 0;
    for(size_t k = 0; k < *len; k++){
        if(str[k] != ' '){
            if((0x21 <= str[k]) && (str[k] <= 0x75)){
                val *= 85;
                val += str[k] - 0x21;
                flag++;
            }else if(str[k] == 'z'){
                val = 0;
                flag = 5;
            } else if(str[k] == '~'){
                if((k == (*len - 1)) || (str[k+1] == '>')){
                    break;
                }
            }
            if(flag == 5){
                for(int i = 3; i >= 0; i--){
                    buf[t_len + i] = val & 0xFF;
                    val >>= 8;
                }
                t_len+=4;
                val = 0;
                flag = 0;
            }
        }
    }
    // Last block
    if(flag > 1){
        uint8_t shift = 0;
        // Add hiden zeroes
        for(int k = 0; k <= 4-flag; k++){
            val *= 85;
            shift += 8;
        }
        // Remove extra bytes
        val >>= shift;
        // Fix last byte
        val++;
        // Convert to base256 representation
        for(int i = flag - 2; i >= 0; i--){
            buf[t_len + i] = val & 0xFF;
            val >>= 8;
        }
        // New length
        t_len+=flag - 1;
    }
    *len = t_len;
    return buf;
}

u8* dec_rle(u8* str, size_t *len)
{
    size_t sz  = *len * 2;
    u8 *out  = malloc(sz);
    size_t out_pos = 0;
    for(size_t k = 0; k < *len;){
        if(str[k] == 128){
            break;
        } else if (str[k] < 128){
            if((out_pos + str[k] + 1) > sz){
                sz *= 2;
                out = realloc(out, sz);
            }
            // Copy folowing n bytes:
            memcpy(out + out_pos, str + k + 1, str[k] + 1);
            out_pos += str[k] + 1;
            k+= str[k];
        } else if(str[k] > 128){
            if((out_pos + 257 - str[k]) > sz){
                sz *= 2;
                out = realloc(out, sz);
            }
            //Copy next byte n times
            memset(out + out_pos, str[k+1], 257 - str[k]);
            out_pos += 257 - str[k];
            k+=2;
        }
    }
    *len = out_pos;
    return out;
}

u8* dec_deflate(u8* str, size_t *len)
{
    int ret;
    z_stream strm;
    size_t dlen = *len*2;
    u8 *out = malloc(dlen);

    // Init structure
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return NULL;

    strm.avail_in = *len;
    strm.next_in = str;
    strm.avail_out = dlen;
    strm.next_out = out;
    // Decompression loop
    do{
        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                inflateEnd(&strm);
                return NULL;  
            }
        if(ret != Z_STREAM_END){
            // Increase buffer and continue decompression.
            unsigned char *tmp = malloc(dlen*2);
            memcpy(tmp, out, dlen);
            free(out);
            out = tmp;
            strm.avail_out = dlen;
            strm.next_out = out + dlen;
            dlen*=2;
        }
    } while (ret != Z_STREAM_END);
    *len = dlen - strm.avail_out;
    inflateEnd(&strm);
    return out;
}


typedef enum FilterID{
    asciihex = 0,
    ascii85,
    rle,
    flate,
    unsupported
} FilterID;//*/

typedef struct Filter{
    FilterID id;
    u8* pointer;
    size_t len;
} Filter;

const char *asciihex_str = "/ASCIIHexDecode";
const char *ascii85_str = "/ASCII85Decode";
const char *rle_str = "/RunLengthDecode";
const char *flate_str = "/FlateDecode";

Filter filter_to_enum(u8* str, size_t size)
{
    size_t ah_l = strlen(asciihex_str);   
    size_t a85_l = strlen(ascii85_str);
    size_t fl_l = strlen(flate_str);
    size_t rl_l = strlen(rle_str);
    Filter ret;
    if((size >= ah_l) && (strncmp((const char*)str, asciihex_str,ah_l) == 0)){
        ret.id = asciihex;
        ret.pointer = str;
        ret.len = ah_l;
    } else if((size >= a85_l) && (strncmp((const char*)str, ascii85_str,a85_l) == 0)){
        ret.id = ascii85;
        ret.pointer = str;
        ret.len = a85_l;
    } else if((size >= fl_l) && (strncmp((const char*)str, flate_str,fl_l) == 0)){
        ret.id = flate;
        ret.pointer = str;
        ret.len = fl_l;
    } else if((size >= rl_l) && (strncmp((const char*)str, rle_str,rl_l) == 0)){
        ret.id = rle;
        ret.pointer = str;
        ret.len = rl_l;
    } else{
        ret.id = unsupported;
        ret.pointer = str;
        size_t k = 0;
        for(k = 0; k < size; k++){
            if(str[k] == ' '){
                break;
            }
        }
        ret.len = k;
    }
    return ret;
}

Filter* get_filters(const u8* header, size_t *count, u8 **filt_begin)
{
    u8* fp = memmem(header, *count, "/Filter", 7);
    *filt_begin = fp;
    if(fp == NULL){// No filters
        *count = 0;
        return NULL;
    }
    fp += 7;
    //Find filter enum
    while((*fp != '/') && (*fp != '[')){
        fp++;
    };
    Filter* fl = malloc(sizeof(Filter));
    size_t fl_count = 0;
    if(*fp == '/'){
        fl[0] = filter_to_enum(fp, *count - (fp - header));
        fl_count++;
    }else if(*fp == '['){
        do{
            fp++;
            if(*fp == '/'){
                fl[fl_count] = filter_to_enum(fp, *count - (fp - header));
                fl_count++;
                fl = realloc(fl, (fl_count + 1)*sizeof(Filter));
            }
        }while(*fp != ']');
    }else{

    }
    *count = fl_count;
    return fl;
}

void obj_compress(object *obj)
{
    size_t filt_count = obj->h_len;
    u8* filt_begin;
    Filter* filters = get_filters(obj->header, &filt_count, &filt_begin);
    size_t len = obj->b_len;
    // Unpack stream
    u8* un_comp = malloc(len);
    memcpy(un_comp, obj->body, len);
    bool is_uns = false;
    size_t k;
    for(k = 0; k < filt_count; k++){
        u8* tmp;
        is_uns = false;
        switch (filters[k].id){
            case asciihex:
                tmp = dec_ascii_hex(un_comp, &len);
                break;
            case ascii85:
                tmp = dec_ascii85(un_comp, &len);
                break;
            case rle:
                tmp = dec_rle(un_comp, &len);
                break;
            case flate:
                tmp = dec_deflate(un_comp, &len);
                break;
            case unsupported:
                is_uns = true;
                break;
        }
        if(!is_uns){
            free(un_comp);
            un_comp = tmp;
        } else{
            break;
        }
    }
    printf("->%llu", len);
    // Pack stream
    u8* packed;
    size_t pack_sz;
    ZopfliOptions options;
    ZopfliInitOptions(&options);
    options.numiterations=2000;
    ZopfliCompress(&options, ZOPFLI_FORMAT_ZLIB, un_comp, len, &packed, &pack_sz);
    printf("->%llu", pack_sz);
    if(pack_sz < obj->b_len){
        printf(" (%lf\%)", (double)pack_sz / (double)obj->b_len * 100);
        free(obj->body);
        obj->body = packed;
        obj->b_len = pack_sz;
        // Update header
        u8* deflate_str = "/Filter /FlateDecode ";
        u8 buf_for_len[21];
        snprintf(buf_for_len,21,"%llu", pack_sz);
        //TODO: Fix /Length
        size_t deflate_len = strlen(deflate_str);
        u8* tmp;
        switch (filt_count){
            case 0:
                tmp = malloc(obj->h_len + deflate_len + 1);
                memcpy(tmp, obj->header, obj->h_len);
                tmp[obj->h_len] = ' ';
                memcpy(tmp + obj->h_len + 1, deflate_str, deflate_len);//*/
                //printf("\n?????????????????n");
                break;
            case 1:
                tmp = malloc(obj->h_len - filters[0].len - 7 + deflate_len );
                memcpy(tmp, obj->header, filt_begin - obj->header);
                memcpy(tmp + (filt_begin - obj->header), deflate_str, deflate_len);
                //printf("\n!!!!!!!!!!!!\n");
                //fwrite(filters[0].pointer, 1, 5, stdout);
                //printf("\n!!!!!!!!!!!! %lu\n", filters[0].len);
                memcpy(tmp + (filt_begin - obj->header) + deflate_len, filters[0].pointer + filters[0].len, obj->h_len - (filters[0].pointer - obj->header) - filters[0].len);

                break;
            default:
                //TODO
                break;
        }
        //free(tmp);
        free(obj->header);
        obj->header = tmp;
    } else{
        free(packed);
        printf(" Skipped");
    }

    free(filters);
    free(un_comp);
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
            //printf("%s\n", obj.header);
        }
        if(obj.b_len != 0){
            printf("Found stream: length %llu", obj.b_len);
            obj_compress(&obj);
        }
        putc('\n', stdout);
        write_obj(&desc, &obj);
    };
    //delete_object(&obj);
    walk_finish(&desc);
    *in_size = desc.out_pos;
    return desc.out;
}


int main(int argc, char *argv[])
{
    printf("pdfopt version 0.1, Copyright (c) 2024, Sergey Astafiev\n");
    printf("pdfopt is a lossless pdf compressor.\n");
    
    long pdf_size;
    u8* pdf_orig =  load_pdf("in.pdf", &pdf_size);
    u8* pdf_comp = through_pdf(pdf_orig, &pdf_size);
    save_pdf("out.pdf", pdf_comp, pdf_size);
    free(pdf_comp);
    free(pdf_orig);//*/
    return 0;
}
