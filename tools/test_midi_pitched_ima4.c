/* Pitched original IMA4 asset versus independent decoded reference samples. */
#include "../src/midi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned char data[65536];static unsigned size;
static int rd(void*ctx,uint32_t a,int16_t*s) {
    unsigned o=a-0x1000;(void)ctx;if(o+1>=size)return 0;
    *s=(int16_t)(data[o]|(unsigned)data[o+1]<<8);return 1;
}
int main(int argc,char**argv) {
    assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f);
    size=(unsigned)fread(data,1,sizeof data,f);fclose(f);assert(size && size%34==0);
    unsigned frames=size/34*64;int16_t*ref=malloc(frames*2);assert(ref);
    f=fopen(argv[2],"rb");assert(f);assert(fread(ref,2,frames,f)==frames);assert(fgetc(f)==EOF);fclose(f);
    const unsigned pitches[]={0x400,0x4c2,0x800,0x1000};Midi m;int16_t out[254];
    for(unsigned p=0;p<4;p++) {
        unsigned pitch=pitches[p];uint32_t r[]={0x1000,0x1000+size-2,0x1000,2,pitch,0xffffffff,0xf6ff0000,0xffffffff,0xffff80ff,0xffffff00};
        midi_reset(&m);midi_set_memory(&m,NULL,rd);
        for(unsigned i=0;i<10;i++)midi_write(&m,i*4,r[i],0);
        midi_write(&m,0x1010,1,0);assert(m.active==1);
        unsigned total=(frames*4096+pitch-1)/pitch;
        for(unsigned i=0;i<total;) {
            unsigned n=total-i;if(n>127)n=127;midi_render(&m,out,n);
            for(unsigned j=0;j<n;j++) {
                int16_t s=ref[(i+j)*pitch/4096];
                assert(out[j*2]==s && out[j*2+1]==(int)s*128/255);
            }
            i+=n;if(i<total) {
                unsigned decoded=i*pitch/4096,expected=0x1000+(decoded/64)*34+2+(decoded%64)/2;
                if(decoded%64==0 && (i-1)*pitch/4096<decoded)expected-=2;
                assert(m.cursor[0]==expected && m.phase[0]==i*pitch%4096);
            }
        }
        assert(!m.active && m.cursor[0]==0x1000+size);midi_render(&m,out,1);assert(!out[0]&&!out[1]);
        /* Fixed-rate final-byte endpoint is not valid for soundfont mode2. */
        midi_write(&m,4,0x1000+size-1,0);midi_write(&m,0x1010,1,0);assert(!m.active);
        printf("PASS: mode2 pitch%X, %u FFmpeg samples, gains, cursor and final-word end\n",pitch,frames);
    }
    free(ref);
}
