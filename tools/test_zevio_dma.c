#include "../src/zevio_dsp.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static void put(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=v>>(8*i); }
static void descriptor(uint8_t *ram,unsigned at,uint32_t src,uint32_t dst,uint32_t next,unsigned n) {
    uint32_t words[]={0x400000,0x1f004800,src,dst,next,n,2,2};
    for(unsigned i=0;i<8;i++)put(ram+at+4*i,words[i]);
}
int main(void) {
    ZevioDSP *z=calloc(1,sizeof *z);uint8_t *ram=calloc(1,65536);assert(z && ram);
    zevio_dsp_init(z,ram,0x10000000,65536);
    ram[0x100]=0x34;ram[0x101]=0x12;ram[0x102]=0xcd;ram[0x103]=0xab;
    /* Deliberately non-contiguous links; two uploads to different DSP banks. */
    descriptor(ram,0x200,0x10000100,0xdc020020,0x10000500,2);
    descriptor(ram,0x500,0x10000100,0xdc000040,0,2);
    zevio_dsp_dma_write(z,0x88,0x10000200);zevio_dsp_dma_write(z,0x80,2);
    assert(z->p[0x20]==0x34 && z->d[0x42]==0xcd && z->transfers==2 && z->words==4);
    assert(zevio_dsp_dma_read(z,0)==1 && zevio_dsp_dma_read(z,0x84)==16);
    /* Independent second-channel completion and W1C acknowledgement. */
    descriptor(ram,0x200,0xdc000040,0x10000600,0,2);
    zevio_dsp_dma_write(z,0xc8,0x10000200);zevio_dsp_dma_write(z,0xc0,2);
    assert(ram[0x600]==0x34 && ram[0x603]==0xab && zevio_dsp_dma_read(z,0)==3);
    zevio_dsp_dma_write(z,0x10,2);assert(zevio_dsp_dma_read(z,0)==1);
    /* Out-of-bounds source fails, never advertises successful completion. */
    descriptor(ram,0x200,0x10010000,0xdc000040,0,2);
    zevio_dsp_dma_write(z,0xc0,2);
    assert(zevio_dsp_dma_read(z,0)==1 && zevio_dsp_dma_read(z,0x40)==2);
    zevio_dsp_dma_write(z,0x50,2);assert(!zevio_dsp_dma_read(z,0x40));
    /* Cyclic descriptor chains terminate with an error. Zero words avoid
     * changing memory while exercising the actual bounded traversal. */
    descriptor(ram,0x200,0,0,0x10000200,0);zevio_dsp_dma_write(z,0x80,2);
    assert(!(zevio_dsp_dma_read(z,0)&1) && (zevio_dsp_dma_read(z,0x40)&1));
    free(ram);free(z);puts("PASS Zevio DMA links, channels, acknowledgement and invalid descriptors");
}
