/* Interior PCM sustain loop, fractional pitch and firmware release semantics. */
#include "../src/midi.h"
#include <assert.h>
#include <stdio.h>
static const int16_t pcm[]={100,-200,300,-400};
static int rd(void *ctx,uint32_t a,int16_t *s) {
    (void)ctx;assert(a>=0x1000 && a<=0x1006 && !(a&1));
    *s=pcm[(a-0x1000)/2];return 1;
}
static void setup(Midi *m,unsigned pitch,unsigned loop) {
    uint32_t r[]={0x1000,0x1006,loop,0x10,pitch,0xffffffff,0xf6ff0000,0xffffffff,0xffff80ff,0xffffff00};
    midi_reset(m);midi_set_memory(m,NULL,rd);
    for(unsigned i=0;i<10;i++)midi_write(m,i*4,r[i],0);
    midi_write(m,0x1010,1,0);
}
int main(void) {
    Midi m;int16_t out[514];
    const unsigned pitches[]={0,0x400,0x4c2,0xa00,0x1000};
    for(unsigned p=0;p<sizeof pitches/sizeof pitches[0];p++) {
        setup(&m,pitches[p],0x1002);assert(m.active==1);
        midi_render(&m,out,7);midi_render(&m,out+14,250);
        /* Infinite source is attack[0], followed by repeating[1,2,3]. */
        for(unsigned i=0;i<257;i++) {
            unsigned k=i*pitches[p]/4096;unsigned idx=k?1+(k-1)%3:0;
            assert(out[2*i]==pcm[idx]);assert(out[2*i+1]==(int)pcm[idx]*128/255);
        }
        unsigned k=257*pitches[p]/4096;
        assert(m.cursor[0]==0x1000+2*(k?1+(k-1)%3:0));
        assert(m.phase[0]==257*pitches[p]%4096 && m.active==1);
        midi_write(&m,0x1018,1,0);midi_render(&m,out,1);assert(!m.active && !out[0] && !out[1]);
    }
    setup(&m,0x1000,0x1002);midi_render(&m,out,5);
    assert(m.cursor[0]==0x1004);midi_write(&m,0xc,0,0); /* live loop-bit clear */
    midi_render(&m,out,3);assert(out[0]==300 && out[2]==-400 && out[4]==0 && !m.active);
    assert(!m.unsupported_starts);
    setup(&m,0x600,0x1002);midi_render(&m,out,5);
    assert(m.phase[0]==0xe00 && m.cursor[0]==0x1002);
    midi_write(&m,0xc,0,0);midi_render(&m,out,7);
    const int16_t release[]={-200,300,300,-400,-400,-400,0};
    for(unsigned i=0;i<7;i++)assert(out[2*i]==release[i]);
    assert(!m.active);
    setup(&m,0x1000,0x1006);midi_render(&m,out,9);
    for(unsigned i=3;i<9;i++)assert(out[2*i]==-400);
    assert(m.active==1);
    setup(&m,0x1000,0x1000);midi_render(&m,out,9);
    for(unsigned i=0;i<9;i++)assert(out[2*i]==pcm[i%4]);
    const unsigned bad[]={0xffe,0x1001,0x1008};
    for(unsigned i=0;i<3;i++){setup(&m,0x1000,bad[i]);assert(!m.active && m.unsupported_starts==1);}
    puts("PASS: pitched PCM loops, split phase, inclusive end, release, key-off and invalid loops");
}
