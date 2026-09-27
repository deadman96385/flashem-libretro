/* Execute the unmodified game's ARM loader in a captured RAM image.
 * This is a focused firmware integration test, not a save-state resume. */
#include "../src/hw.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static void call(HW *h, ARM9 *c, uint32_t pc, uint32_t a, uint32_t b) {
    (void)h;
    c->r[0]=a; c->r[1]=b; c->r[13]=0x10fff000;
    c->r[14]=0x10ffff00; c->r[15]=pc; c->fetch_ptr=NULL;
    unsigned n=0;
    while (c->r[15]!=0x10ffff00 && ++n<2000000) arm9_step(c);
    printf("ARM call %08x: instructions=%u result=%08x pc=%08x\n",pc,n,c->r[0],c->r[15]);
    assert(c->r[15]==0x10ffff00);
}
int main(int argc, char **argv) {
    if(argc!=2 && argc!=4) { fprintf(stderr,"usage: %s multi.ram [movie.MJP output.yuv]\n",argv[0]); return 2; }
    FILE *f=fopen(argv[1],"rb"); if(!f) return 2;
    ARM9 c={0}; uint32_t *fb=calloc(1024*512,4);
    HW *h=hw_create(&c,NULL,0,NULL,fb);
    for(unsigned i=0; i<0x1000000; ++i) {
        int v=fgetc(f); assert(v!=EOF); hw_write8(h,0x10000000+i,(uint8_t)v);
    }
    fclose(f);
    /* Populate external data, upload the two real firmware sections. */
    call(h,&c,0x109fe608,0x10820000,0x10820000);
    assert(c.r[0]==0);
    assert(hw_read32(h,0xc0000040)==0);
    assert(hw_read32(h,0xc0000084)&0x10);
    /* Read back through a separate reverse-direction DMA descriptor. */
    uint32_t desc[]={0x400000,0x1f004800,0xdc020000,0x10700000,0,0x1ed0,2,2};
    for(unsigned i=0;i<8;++i) hw_write32(h,0x10710000+i*4,desc[i]);
    hw_write32(h,0xc0000088,0x10710000); hw_write32(h,0xc0000080,2);
    assert(hw_read32(h,0xc0000000)&1);
    for(unsigned i=0;i<0x3da0;++i) assert(hw_read8(h,0x10700000+i)==hw_read8(h,0x10820000+i));
    /* Real reset-stub installer. */
    call(h,&c,0x109fe808,0x1081f000,0);
    assert(hw_read16(h,0x1081f000)==0xcf00);
    assert(hw_read16(h,0x1081f008)==0xbb06);
    puts("PASS: actual ARM loader, linked upload, reverse DMA and reset stub");
    hw_write32(h,0x900a0024,((0x10800000u&0xfffe0000)>>1)|(0x10820000u>>17));
    hw_write32(h,0x900a0020,1);
    /* ARM branch-to-self lets debugger stepping advance device time. */
    hw_write32(h,0x10ffff00,0xeafffffe); c.r[15]=0x10ffff00; c.fetch_ptr=NULL;
    unsigned steps=0;
    while(!(hw_read32(h,0x900a0114)&3) && ++steps<100000) hw_step(h);
    printf("DSP ready=%x after %u ARM steps\n",hw_read32(h,0x900a0114),steps);
    assert(hw_read32(h,0x900a0114)==3);
    for(unsigned i=0;i<100000;++i) hw_step(h);
    /* Host request80, real DSP ISR/command/reply, ARM IRQ18 and W1C ack. */
    hw_write32(h,0x900a0118,3);
    hw_write32(h,0xdc000204,0x60000);
    hw_write32(h,0xdc000008,0x60000);
    hw_write32(h,0x1083c000,0x80);
    hw_write32(h,0x900a0108,1);
    steps=0;
    while(!(hw_read32(h,0x900a0110)&1) && ++steps<100000) hw_step(h);
    assert(hw_read32(h,0x1083c040)==0x8080);
    assert(hw_read32(h,0x1083c048)==0x7000);
    assert(hw_read32(h,0xdc000000)&0x40000);
    hw_write32(h,0x900a0110,1);
    hw_write32(h,0xdc000004,0x40000);
    assert(!(hw_read32(h,0xdc000000)&0x40000));
    puts("PASS: actual DSP boot, command80 reply, IRQ18 delivery and acknowledgment");
    if(argc==4) {
        /* Let the firmware retire the previous reply before posting another
         * request; real ARM dispatch naturally has this intervening work. */
        for(unsigned i=0;i<10000;i++) hw_step(h);
        f=fopen(argv[2],"rb");assert(f);
        for(unsigned i=0;i<0x10000;i++) { int v=fgetc(f);assert(v!=EOF);hw_write8(h,0x100a0000+i,v); }
        fclose(f);
        uint32_t args[]={0x81,0,0x100a0000,0x100b0000,0x10820000,0xa000,
            0x10040000,0x10080000,0x10090000,0x100c0000,0x100d0000,0x4268,0};
        for(unsigned i=0;i<sizeof args/sizeof args[0];i++) hw_write32(h,0x1083c000+4*i,args[i]);
        hw_write32(h,0x900a0108,1);
        steps=0;
        while(!(hw_read32(h,0x900a0104)&1) && ++steps<10000000) hw_step(h);
        assert(hw_read32(h,0x1083c040)==0x8081);
        assert(hw_read32(h,0x1083c048)==320 && hw_read32(h,0x1083c04c)==160);
        assert(hw_read32(h,0x1083c080)==0x92 && hw_read32(h,0x1083c09c)==1);
        assert(hw_read32(h,0xdc000000)&0x20000);
        f=fopen(argv[3],"wb");assert(f);
        const uint32_t addresses[]={0x10040000,0x10080000,0x10090000};
        for(unsigned plane=0;plane<3;plane++) for(unsigned i=0;i<(plane?12800u:51200u);i++) fputc(hw_read8(h,addresses[plane]+i),f);
        fclose(f);
        printf("PASS: full HW scheduler decoded frame1 and delivered IRQ17 after %u ARM steps\n",steps);
    }
    hw_destroy(h); free(fb); return 0;
}
