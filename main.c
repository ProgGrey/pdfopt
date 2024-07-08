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
    // End block
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


void obj_compress(object *obj)
{
    
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
        write_obj(&desc, &obj);
    };
    //delete_object(&obj);
    walk_finish(&desc);
    *in_size = desc.out_pos;
    return desc.out;
}


int main(int argc, char *argv[])
{
    u8* str = (u8*)"9jqo^BlbD-BleB1DJ+*+F(f,q/0JhKF<GL>Cj@.4Gp$d7F!,L7@<6@)/0JDEF<G%<+EV:2F!,O<DJ+*.@<*K0@<6L(Df-\\0Ec5e;DffZ(EZee.Bl.9pF\"AGXBPCsi+DGm>@3BB/F*&OCAfu2/AKYi(DIb:@FD,*)+C]U=@3BN#EcYf8ATD3s@q?d$AftVqCh[NqF<G:8+EV:.+Cf>-FD5W8ARlolDIal(DId<j@<?3r@:F%a+D58'ATD4$Bl@l3De:,-DJs`8ARoFb/0JMK@qB4^F!,R<AKZ&-DfTqBG%G>uD.RTpAKYo'+CT/5+Cei#DII?(E,9)oF*2M7/c~>";
    size_t len = strlen((const char*)str);
    printf("%lu\n", len);
    str = dec_ascii85(str, &len);
    fwrite(str, 1, len, stdout);
    free(str);
    printf("\n%lu\n", len);
    printf("pdfopt version 0.1, Copyright (c) 2024, Sergey Astafiev\n");
    printf("pdfopt is a lossless pdf compressor.\n");
    /*
    long pdf_size;
    u8* pdf_orig =  load_pdf("in.pdf", &pdf_size);
    u8* pdf_comp = through_pdf(pdf_orig, &pdf_size);
    save_pdf("out.pdf", pdf_comp, pdf_size);
    free(pdf_comp);
    free(pdf_orig);//*/
    return 0;
}
