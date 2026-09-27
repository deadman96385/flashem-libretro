/* Real BIOS driver -> HW MMIO -> emulated time -> frontend Audio ring. */
#include "../src/vflash.h"
#include "../src/audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static unsigned char ram[0x1000000];
static uint32_t rd(uint32_t a) {
    unsigned char*p=ram+a-0x10000000;
    return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void invoke(VFlash*v,uint32_t pc,uint32_t r0,uint32_t r1) {
    unsigned n;
    vflash_set_reg(v,0,r0);vflash_set_reg(v,1,r1);
    vflash_set_reg(v,13,0x10fff000);vflash_set_reg(v,14,0x10fffffc);
    vflash_set_reg(v,15,pc);
    for(n=0;n<10000&&vflash_get_pc(v)!=0x10fffffc;n++)vflash_step(v);
    assert(n<10000);
}
int main(int argc,char**argv) {
    FILE*f;VFlash*v;Audio*a;unsigned i,got,shift,match=0;int16_t out[8192];
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(ram,1,sizeof(ram),f)!=sizeof(ram))return 2;
    fclose(f);vflash_set_bios_dir("..");v=vflash_create(NULL);assert(v);
    a=vflash_get_audio(v);audio_init_external(a);audio_set_volume(a,256);
    for(i=0;i<sizeof(ram);i+=4)vflash_write32(v,0x10000000+i,rd(0x10000000+i));
    invoke(v,0x1008c4c0,0x100b2eb4,5);
    invoke(v,0x1008c0fc,5,0);
    audio_pull_samples(a,out,8192); /* discard pre-key-on silence */
    invoke(v,0x1008b9f8,0,0);
    vflash_write32(v,0x10ffe000,0xeafffffe);vflash_set_reg(v,15,0x10ffe000);
    for(i=0;i<3;i++)vflash_run_frame(v);
    got=audio_pull_samples(a,out,8192);assert(got>3000);
    /* At most a few output frames accrue during the actual key-on routine.
     * Match a long nonzero sequence, allowing that bounded startup phase. */
    for(shift=0;shift<16&&!match;shift++) {
        unsigned nonzero=0,ok=1;
        for(i=32;i<1000;i++) {
            uint32_t addr=0x1011944c+((i+shift)/2)*2;
            int16_t s=(int16_t)(ram[addr-0x10000000]|(uint16_t)ram[addr-0x10000000+1]<<8);
            nonzero+=s!=0;
            if(out[i*2]!=s||out[i*2+1]!=s){ok=0;break;}
        }
        if(ok&&nonzero>100)match=1;
    }
    assert(match);
    printf("Frontend audio verified: %u stereo frames, BIOS PCM match, phase %u\n",got/2,shift-1);
    vflash_destroy(v);return 0;
}
