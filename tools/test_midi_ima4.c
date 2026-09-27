/* Compare complete native assets to an independent FFmpeg IMA4 decode. */
#include "../src/midi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned char data[1048576];static unsigned size;
static int rd(void*ctx,uint32_t a,int16_t*s) {
    unsigned o=a-0x1000;(void)ctx;if(o+1>=size)return 0;
    *s=(int16_t)(data[o]|(unsigned)data[o+1]<<8);return 1;
}
int main(int argc,char**argv) {
    Midi m;FILE*f;unsigned i,j,frames;int mode;int16_t out[254],*ref;
    uint32_t r[]={0x1000,0,0x1000,0,0x1000,0xffffffff,0xffff0000,0xffffffff,0xffff,0};
    if(argc!=4 && argc!=5)return 2;
    f=fopen(argv[1],"rb");assert(f);size=(unsigned)fread(data,1,sizeof(data),f);fclose(f);
    assert(size&&size%34==0);frames=size/34*64;
    ref=malloc(frames*2);assert(ref);f=fopen(argv[2],"rb");assert(f);
    assert(fread(ref,2,frames,f)==frames);assert(fgetc(f)==EOF);fclose(f);
    mode=(int)strtol(argv[3],NULL,16);r[1]=0x1000+size-1;r[3]=(unsigned)mode;
    if(argc==5) {assert(mode==0x12 || mode==0x16);r[1]--; } /* word endpoint */
    unsigned divisor=mode==0x12?1:(mode==0x1ea || mode==0x1fa)?4:2;
    midi_reset(&m);midi_set_memory(&m,NULL,rd);
    for(i=0;i<10;i++)midi_write(&m,i*4,r[i],0);
    midi_write(&m,0x1010,1,0);assert(m.active==1);
    for(i=0;i<frames*divisor;) {
        unsigned n=frames*divisor-i;if(n>127)n=127;
        midi_render(&m,out,n);
        for(j=0;j<n;j++)assert(out[j*2]==ref[(i+j)/divisor]&&out[j*2+1]==ref[(i+j)/divisor]);
        i+=n;
        if(i<frames*divisor) {
            unsigned decoded=i/divisor,expected=0x1000+(decoded/64)*34+2+(decoded%64)/2;
            /* A boundary has consumed the previous block but not read nextheader. */
            if(decoded%64==0 && i%divisor==0)expected-=2;
            assert(m.cursor[0]==expected);
        }
    }
    if(mode&0x10) {
        assert(m.active==1&&m.cursor[0]==0x1000);
        midi_render(&m,out,127);
        for(i=0;i<127;i++)assert(out[i*2]==ref[i/divisor]);
        /* Release a fixed-rate looping effect mid-block and finish its tail. */
        if(mode==0x1f6 || mode==0x1fa) {
            midi_write(&m,12,(unsigned)mode&~0x10u,0);
            for(i=127;i<frames*divisor;) {
                unsigned n=frames*divisor-i;if(n>127)n=127;
                midi_render(&m,out,n);
                for(j=0;j<n;j++)assert(out[j*2]==ref[(i+j)/divisor]);
                i+=n;
            }
            assert(m.active==0);
        }
    } else assert(m.active==0);
    /* Accepting a word endpoint must not admit partial blocks or loops. */
    midi_write(&m,4,r[1]-2,0);midi_write(&m,0x1010,1,0);assert(m.active==0);
    midi_write(&m,4,r[1],0);midi_write(&m,8,r[2]+2,0);
    midi_write(&m,0x1010,1,0);assert(m.active==0);
    midi_write(&m,8,r[2],0);
    /* Invalid packed index must never index outside the step table. */
    data[1]=127;midi_write(&m,0x1010,1,0);midi_render(&m,out,1);assert(m.active==0&&out[0]==0);
    /* Mode12 streaming support must not admit the unverified soundfont tuple. */
    midi_write(&m,12,0x12,0);midi_write(&m,0x18,0xf6ff0000,0);
    midi_write(&m,0x20,0xffff7f7f,0);midi_write(&m,0x24,0xffffff00,0);
    midi_write(&m,0x1010,1,0);assert(m.active==0);
    free(ref);printf("IMA4 mode%X: %u samples exactly match FFmpeg; cursor/end/loop verified\n",mode,frames);return 0;
}
