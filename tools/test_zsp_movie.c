/* Executes the original DSP firmware, including both DMA channels. Assets are
 * supplied by the user; no proprietary firmware or movie is embedded here.
 * Usage: test_zsp_movie program.bin data.bin movie.MJP output-prefix [frames]
 */
#include "../src/zevio_dsp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static size_t load(const char *path,uint8_t *p,size_t max) {
    FILE *f=fopen(path,"rb"); assert(f); size_t n=fread(p,1,max,f);
    assert(n && fgetc(f)==EOF); fclose(f); return n;
}
static void put(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=v>>(8*i); }
static uint32_t get(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
int main(int argc,char **argv) {
    assert(argc==5 || argc==6);
    unsigned frames=argc==6?strtoul(argv[5],NULL,0):3;
    ZevioDSP *z=calloc(1,sizeof *z); uint8_t *ram=calloc(1,0x2000000); assert(z && ram);
    /* Larger fixture RAM keeps the full source movie separate from all DSP
     * windows. This tests decoding; the physical console has streaming input. */
    zevio_dsp_init(z,ram,0x10000000,0x2000000);
    load(argv[1],z->p,sizeof z->p); load(argv[2],z->d,sizeof z->d);
    size_t size=load(argv[3],ram+0x1000000,0x1000000);
    uint16_t boot[]={0xcf00,0xcf00,0x2600,0x3600,0xbb06,0xcf00,0xcf00,0xcf00};
    for(unsigned i=0;i<8;i++) { ram[0x81f000+2*i]=boot[i];ram[0x81f001+2*i]=boot[i]>>8; }
    zevio_dsp_control_write(z,0x24,0x08400841); zevio_dsp_control_write(z,0x20,1);
    zevio_dsp_run(z,100000); assert(!z->core.fault && z->control[0x114/4]==3);
    put(ram+0x83c000,0x80); zevio_dsp_control_write(z,0x108,1); zevio_dsp_run(z,100000);
    assert(get(ram+0x83c040)==0x8080 && get(ram+0x83c048)==0x7000);
    uint32_t args[]={0x81,0,0x11000000,0x11000000+(uint32_t)size,0x10820000,0xa000,
        0x10040000,0x10080000,0x10090000,0x100c0000,0x100d0000,0x4268,0};
    for(unsigned i=0;i<sizeof args/sizeof args[0];i++) put(ram+0x83c000+4*i,args[i]);
    zevio_dsp_control_write(z,0x108,1);
    unsigned completed=0,events=0;
    while(completed<frames && events++<frames*4+8) {
        unsigned batches=0;
        while(!z->control[0x104/4] && !z->core.fault && batches++<1000) zevio_dsp_run(z,10000);
        assert(!z->core.fault && z->control[0x104/4]);
        uint8_t *message=ram+0x83c080;
        assert(get(message)==0x92);
        uint32_t flags=get(message+12);
        printf("event flags=%x frame=%u instructions=%llu\n",flags,get(message+28),(unsigned long long)z->core.instructions);
        if(!(flags&0x1a)) {
            assert(get(message+28)==completed+1);
            unsigned width=get(ram+0x83c048),height=get(ram+0x83c04c);
            assert(width && height && width*height<=0x40000);
            char path[1024]; snprintf(path,sizeof path,"%s-%03u.yuv",argv[4],completed);
            FILE *f=fopen(path,"wb"); assert(f);
            fwrite(ram+0x40000,1,width*height,f); fwrite(ram+0x80000,1,width*height/4,f);
            fwrite(ram+0x90000,1,width*height/4,f); fclose(f); ++completed;
        }
        uint32_t reply[]={0x8092,0,0,0x10040000,0x10080000,0x10090000,0,0};
        for(unsigned i=0;i<8;i++) put(message+64+4*i,reply[i]);
        zevio_dsp_control_write(z,0x104,0);
    }
    assert(completed==frames);
    printf("PASS: %u native firmware frames, %llu DMA words\n",completed,(unsigned long long)z->words);
    free(z);free(ram);
}
