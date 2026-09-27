/* Real firmware streaming-input regression. User-supplied assets only.
 * Usage: program.bin data.bin movie.MJP reference-prefix [frames]
 * Refills only whole sectors behind the consumed pointer reported by DSP.
 */
#include "../src/zevio_dsp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t load(const char *path,uint8_t *p,size_t max) {
    FILE *f=fopen(path,"rb");assert(f);size_t n=fread(p,1,max,f);
    assert(n && fgetc(f)==EOF);fclose(f);return n;
}
static void put(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t get(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(int argc,char**argv) {
    assert(argc==5||argc==6);unsigned frames=argc==6?strtoul(argv[5],0,0):1035;
    ZevioDSP*z=calloc(1,sizeof *z);uint8_t*ram=calloc(1,0x2000000),*source=calloc(1,0x1000000),*ref=malloc(0x60000);
    assert(z&&ram&&source&&ref);zevio_dsp_init(z,ram,0x10000000,0x2000000);
    load(argv[1],z->p,sizeof z->p);load(argv[2],z->d,sizeof z->d);
    size_t size=load(argv[3],source,0x1000000);
    enum { RING=0x96000, BASE=0x11000000 };
    memcpy(ram+0x1000000,source,size<RING?size:RING);
    uint16_t boot[]={0xcf00,0xcf00,0x2600,0x3600,0xbb06,0xcf00,0xcf00,0xcf00};
    for(unsigned i=0;i<8;i++){ram[0x81f000+2*i]=boot[i];ram[0x81f001+2*i]=boot[i]>>8;}
    zevio_dsp_control_write(z,0x24,0x08400841);zevio_dsp_control_write(z,0x20,1);zevio_dsp_run(z,100000);
    assert(!z->core.fault&&z->control[0x114/4]==3);
    put(ram+0x83c000,0x80);zevio_dsp_control_write(z,0x108,1);zevio_dsp_run(z,100000);
    uint32_t args[]={0x81,0,BASE,BASE+RING,0x10820000,0xa000,0x10040000,0x10080000,0x10090000,0x100c0000,0x100d0000,0x4268,0};
    for(unsigned i=0;i<13;i++)put(ram+0x83c000+4*i,args[i]);zevio_dsp_control_write(z,0x108,1);
    unsigned completed=0,events=0,skipped=0;size_t consumed=0;
    while(completed<frames&&events++<frames*4+8){
        unsigned batches=0;
        while(!z->control[0x104/4]&&!z->core.fault&&batches++<1000)zevio_dsp_run(z,10000);
        assert(!z->core.fault&&z->control[0x104/4]);uint8_t*m=ram+0x83c080;
        uint32_t flags=get(m+12),frame=get(m+28),ptr=get(m+44);
        printf("event=%u flags=%x frame=%u ptr=%08x consumed=%zu\n",events,flags,frame,ptr,consumed);fflush(stdout);
        if(!(flags&0x1a)){
            assert(frame==completed+1);char path[1024];snprintf(path,sizeof path,"%s-%03u.yuv",argv[4],completed);
            unsigned n=get(ram+0x83c048)*get(ram+0x83c04c);assert(n<=0x40000);
            assert(load(path,ref,0x60000)==n+n/2);
            if(!(flags&4)){assert(!memcmp(ref,ram+0x40000,n));assert(!memcmp(ref+n,ram+0x80000,n/4));assert(!memcmp(ref+n+n/4,ram+0x90000,n/4));}
            else skipped++;
            completed++;
        }
        assert(ptr>=BASE&&ptr<BASE+RING);
        unsigned delta=(ptr-BASE+RING-(consumed%RING))%RING;
        unsigned sectors=delta/2048;
        while(sectors--){
            size_t from=consumed+RING;unsigned off=consumed%RING;
            memset(ram+0x1000000+off,0,2048);
            if(from<size)memcpy(ram+0x1000000+off,source+from,size-from<2048?size-from:2048);
            consumed+=2048;
        }
        uint32_t reply[]={0x8092,0,0,0x10040000,0x10080000,0x10090000,0,0};
        if(getenv("CAR_TEST_SKIP") && (completed==6||completed==7) && !(flags&2))reply[7]=1;
        for(unsigned i=0;i<8;i++)put(m+64+4*i,reply[i]);zevio_dsp_control_write(z,0x104,0);
    }
    assert(completed==frames);printf("PASS: %u decoded frames match all reference Y/U/V bytes, %u requested skips, %zu input-ring wraps\n",completed-skipped,skipped,consumed/RING);
    free(z);free(ram);free(source);free(ref);return 0;
}
