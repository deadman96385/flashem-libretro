/* Real Dingo soundfont metadata plus frame-split/pitch/gain/end validation. */
#include "../src/midi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char ram[0x1000000];
static uint32_t word(unsigned o) {return ram[o]|(uint32_t)ram[o+1]<<8|(uint32_t)ram[o+2]<<16|(uint32_t)ram[o+3]<<24;}
static int read_sample(void*ctx,uint32_t a,int16_t*s) {
    unsigned o=a-0x10000000;(void)ctx;if(o>=sizeof(ram)-1)return 0;
    *s=(int16_t)(ram[o]|(uint16_t)ram[o+1]<<8);return 1;
}
int main(int argc,char**argv) {
    Midi m;FILE*f;unsigned i,j;int16_t out[24000];
    const unsigned pitches[]={0x400,0x4c2,0x800,0x1000};
    uint32_t r[]={0x108b2f16,0x108b9952,0x108b2f26,0,0x400,0xffffffff,0xf6ff0000,0xffffffff,0xffff80ff,0xffffff00};
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");assert(f);
    assert(fread(ram,1,sizeof(ram),f)==sizeof(ram));fclose(f);
    assert(word(0x808e60)==r[0]&&word(0x808e68)==r[1]-r[0]);
    assert(word(0x808e6c)==16&&ram[0x808e72]==57&&ram[0x808e74]==4);
    /* Root-note descriptor flags04 selects table index57 = pitch0400.
     * A near50-sample period at11025Hz is220.5Hz, matching root A3. */
    {
        double e50=0,e25=0;
        for(i=3000;i<10000;i++) {
            int16_t a,b,c;read_sample(NULL,r[0]+2*i,&a);read_sample(NULL,r[0]+2*(i+50),&b);read_sample(NULL,r[0]+2*(i+25),&c);
            e50+=(double)(a-b)*(a-b);e25+=(double)(a-c)*(a-c);
        }
        assert(e50<e25/3);
    }
    for(j=0;j<sizeof(pitches)/sizeof(pitches[0]);j++) {
        midi_reset(&m);midi_set_memory(&m,NULL,read_sample);r[4]=pitches[j];
        for(i=0;i<10;i++)midi_write(&m,i*4,r[i],0);
        midi_write(&m,0x1010,1,0);assert(m.active==1);
        midi_render(&m,out,731);midi_render(&m,out+1462,11269);
        for(i=0;i<12000;i++) {
            int16_t s;assert(read_sample(NULL,r[0]+2*((i*pitches[j])/4096),&s));
            assert(out[2*i]==s&&out[2*i+1]==(int32_t)s*128/255);
        }
        assert(m.cursor[0]==r[0]+2*(12000*pitches[j]/4096));
        assert(m.phase[0]==(12000*pitches[j]%4096));
    }
    midi_write(&m,0, r[1],0);midi_write(&m,0x1010,1,0);
    midi_render(&m,out,2);assert(m.active==0&&out[2]==0&&out[3]==0);
    midi_write(&m,0xc,0x1e6,0);midi_write(&m,0x1010,1,0);assert(m.active==0&&m.unsupported_starts==1);
    puts("Real soundfont metadata, pitch, split rendering, channel gains and end verified");return 0;
}
