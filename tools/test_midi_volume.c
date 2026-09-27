/* Execute original BIOS volume/fade routines against the sound device.
 * This validates the firmware contract, not unmeasured hardware arithmetic. */
#define main old_probe_main
#include "midi_bios_probe.c"
#undef main
#include "../src/midi.h"
#include <assert.h>
static Midi sound;
static uint32_t gain_read(void*c,uint32_t a) {
    if(a>=0xb0000000&&a<0xb0001300)return midi_read(&sound,a-0xb0000000);
    return r32(c,a);
}
static void gain_write(void*c,uint32_t a,uint32_t v) {
    if(a>=0xb0000000&&a<0xb0001300)midi_write(&sound,a-0xb0000000,v,cpu.r[15]-8);
    else w32(c,a,v);
}
static int16_t amplitude=30000;
static int constant(void*c,uint32_t a,int16_t*out) {
    (void)c;(void)a;*out=amplitude;return 1;
}
int main(int argc,char**argv) {
    FILE*f;int16_t out[8];unsigned v,j;
    const uint32_t setup[]={0x1000,0x1000,0x1000,0x10,0x1000,
        0xffffffff,0xffff0000,0xffffffff,0xff,0};
    assert(argc==2);f=fopen(argv[1],"rb");assert(f);
    assert(fread(ram,1,sizeof ram,f)==sizeof ram);fclose(f);
    midi_reset(&sound);arm9_reset(&cpu);
    cpu.mem_read8=r8;cpu.mem_read16=r16;cpu.mem_read32=gain_read;
    cpu.mem_write8=w8;cpu.mem_write16=w16;cpu.mem_write32=gain_write;
    /* Actual BIOS setter, up/down, mute and endpoint saturation. */
    invoke(0x10013d90,0x4800,0);
    assert(midi_read(&sound,0x1184)==0xffffffff);
    assert(midi_read(&sound,0x1248)==0x48004800);
    invoke(0x10013dd8,0,0);assert(midi_read(&sound,0x1248)==0x50005000);
    invoke(0x10013e58,0,0);assert(midi_read(&sound,0x1248)==0x48004800);
    invoke(0x10013d90,0x7fff,0);
    invoke(0x10013dd8,0,0);assert(midi_read(&sound,0x1248)==0x7fff7fff);
    invoke(0x10013d90,0,0);
    invoke(0x10013e58,0,0);assert(midi_read(&sound,0x1248)==0);
    /* Firmware fade-in initializes zero, requests full scale, polls current.
     * FFFF is the supported immediate case; finite ramp timing is unknown. */
    invoke(0x1008cdcc,0xffff,0);
    invoke(0x1008ce34,0,0);assert(midi_read(&sound,0x1248)==0);
    invoke(0x1008ce34,0,0);assert(midi_read(&sound,0x1248)==0x7fff7fff);
    invoke(0x1008ce34,0,0);assert(r32(NULL,0x101a64c4)==0);
    invoke(0x1008ce00,0xffff,0);
    invoke(0x1008ce34,0,0);assert(midi_read(&sound,0x1248)==0);
    invoke(0x1008ce34,0,0);assert(r32(NULL,0x101a64c4)==0);
    /* Mixer headroom: two30000 voices at quarter gain must not clip first. */
    for(v=0;v<2;v++)for(j=0;j<10;j++)midi_write(&sound,v*64+j*4,setup[j],0);
    midi_set_memory(&sound,NULL,constant);midi_write(&sound,0x1010,3,0);
    midi_write(&sound,0x1188,0x7fff2000,0);
    midi_render(&sound,out,4);
    for(j=0;j<4;j++)assert(out[j*2]==(int64_t)60000*0x2000/0x7fff&&out[j*2+1]==0);
    midi_write(&sound,0x1188,0,0);midi_render(&sound,out,1);assert(out[0]==0);
    /* Channels independently qualify for immediate update. */
    midi_write(&sound,0x1184,0x1234ffff,0);
    midi_write(&sound,0x1188,0x70001234,0);assert(midi_read(&sound,0x1248)==0x1234);
    midi_write(&sound,0x1184,0xffffffff,0);assert(midi_read(&sound,0x1248)==0x70001234);
    /* Independent right gain and negative samples. */
    midi_write(&sound,64+0x20,0xff00,0);amplitude=-30000;
    midi_write(&sound,0x1188,0x40002000,0);midi_render(&sound,out,1);
    assert(out[0]==(int64_t)-30000*0x2000/0x7fff);
    assert(out[1]==(int64_t)-30000*0x4000/0x7fff);
    /*64 voices exercise a master product wider than signed32 bits. */
    for(v=0;v<64;v++)for(j=0;j<10;j++)midi_write(&sound,v*64+j*4,setup[j],0);
    midi_write(&sound,0x1010,0xffffffff,0);midi_write(&sound,0x1014,0xffffffff,0);
    midi_write(&sound,0x1188,0x7fff7fff,0);midi_render(&sound,out,1);
    assert(out[0]==-32768);
    midi_write(&sound,0x1188,0x01000100,0);midi_render(&sound,out,1);
    assert(out[0]==(int64_t)-30000*64*256/0x7fff);
    puts("BIOS volume controls, immediate fade state machine, stereo gain and headroom passed");
    return 0;
}
