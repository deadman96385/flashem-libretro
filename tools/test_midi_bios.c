/* Render actual BIOS UI sample through emulated registers, compare every
 * output sample with the embedded RIFF PCM and optionally export a WAV. */
#include "../src/midi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char ram[0x1000000];
static uint32_t get32(uint32_t a) {
    unsigned char *p=ram+a-0x10000000;
    return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int read_sample(void *ctx,uint32_t a,int16_t *out) {
    unsigned char*p;(void)ctx;
    if(a<0x10000000||a>0x10fffffe)return 0;
    p=ram+a-0x10000000;*out=(int16_t)(p[0]|(uint16_t)p[1]<<8);return 1;
}
static void put32(FILE*f,uint32_t v){unsigned i;for(i=0;i<4;i++)fputc(v>>(8*i),f);}
static void put16(FILE*f,uint16_t v){fputc(v,f);fputc(v>>8,f);}
int main(int argc,char**argv) {
    Midi m;FILE*f;uint32_t start,end,bytes,frames,i;int16_t*out;
    if(argc<2||argc>3)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(ram,1,sizeof(ram),f)!=sizeof(ram))return 2;
    fclose(f);
    midi_reset(&m);midi_set_memory(&m,NULL,read_sample);
    start=get32(0x100b2eb4);end=get32(0x100b2eb8);bytes=end-start+2;
    assert(!memcmp(ram+start-0x10000000-44,"RIFF",4));
    assert(get32(start-24)==0x00010001); /* format1,mono */
    assert(get32(start-20)==22050&&get32(start-12)==0x00100002);
    assert(get32(start-4)==bytes);
    for(i=0;i<10;i++)midi_write(&m,5*64+i*4,get32(0x100b2eb4+i*4),0);
    midi_write(&m,0x1010,1<<5,0);
    frames=bytes; /* mono22050 to stereo44100: two output frames/sample */
    out=calloc((size_t)frames*2+2,sizeof(*out));if(!out)return 2;
    midi_render(&m,out,frames+1);
    for(i=0;i<frames;i++) {
        int16_t sample;assert(read_sample(NULL,start+(i/2)*2,&sample));
        assert(out[i*2]==sample&&out[i*2+1]==sample);
    }
    assert(m.active==0&&out[frames*2]==0&&out[frames*2+1]==0);
    if(argc==3){
        f=fopen(argv[2],"wb");if(!f)return 2;
        fwrite("RIFF",1,4,f);put32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);
        put32(f,16);put16(f,1);put16(f,2);put32(f,44100);put32(f,176400);
        put16(f,4);put16(f,16);fwrite("data",1,4,f);put32(f,frames*4);
        for(i=0;i<frames*2;i++)put16(f,(uint16_t)out[i]);
        fclose(f);
    }
    printf("BIOS WAV verified: %u input PCM bytes, %u stereo output frames, exact match\n",bytes,frames);
    free(out);return 0;
}
