#include "../src/zevio_dsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
static void load(const char *path,uint8_t *p,size_t max) {
    FILE *f=fopen(path,"rb"); assert(f); size_t n=fread(p,1,max,f); assert(n); fclose(f);
}
int main(int argc,char **argv) {
    assert(argc==3 || argc==4);
    ZevioDSP *z=calloc(1,sizeof *z); uint8_t *ram=calloc(1,0x1000000);
    zevio_dsp_init(z,ram,0x10000000,0x1000000);
    load(argv[1],z->p,sizeof z->p); load(argv[2],z->d,sizeof z->d);
    uint16_t boot[]={0xcf00,0xcf00,0x2600,0x3600,0xbb06,0xcf00,0xcf00,0xcf00};
    for(unsigned i=0;i<8;i++) { ram[0x81f000+2*i]=boot[i]; ram[0x81f001+2*i]=boot[i]>>8; }
    zevio_dsp_control_write(z,0x24,0x08400841);
    zevio_dsp_control_write(z,0x20,1);
    zevio_dsp_run(z,100000);
    printf("PC=%04x steps=%llu ready=%x fault=%d\n",z->core.pc,(unsigned long long)z->core.instructions,z->control[0x114/4],z->core.fault);
    for(unsigned i=0;i<16;i++) printf("r%u=%04x%c",i,z->core.r[i],i==15?'\n':' ');
    /* Actual firmware command 80: query decoder's input-buffer word address. */
    ram[0x83c000]=0x80;
    zevio_dsp_control_write(z,0x108,1);
    zevio_dsp_run(z,100000);
    printf("after notification: PC=%04x steps=%llu fault=%d\n",z->core.pc,(unsigned long long)z->core.instructions,z->core.fault);
    printf("reply:"); for(unsigned i=0;i<48;i+=2) printf(" %04x",ram[0x83c040+i]|ram[0x83c041+i]<<8); puts("");
    if(argc==4) {
        /* Exploratory stream fixture following ARM command81's field layout.
         * This is not yet an acceptance test for video output. */
        load(argv[3],ram+0xa0000,0x10000);
        uint32_t args[]={0x81,0,0x100a0000,0x100a4268,0x10820000,0xa000,
                         0x10040000,0x10080000,0x10090000,0x100c0000,0x100d0000,0x4268,0};
        for(unsigned i=0;i<sizeof args/sizeof args[0];++i)
            for(unsigned j=0;j<4;++j) ram[0x83c000+4*i+j]=args[i]>>(8*j);
        zevio_dsp_control_write(z,0x108,1);
        zevio_dsp_run(z,10000000);
        printf("setup81: PC=%04x steps=%llu fault=%d\n",z->core.pc,(unsigned long long)z->core.instructions,z->core.fault);
        for(unsigned i=0;i<16;i++) printf("r%u=%04x%c",i,z->core.r[i],i==15?'\n':' ');
        printf("reply81:"); for(unsigned i=0;i<48;i+=2) printf(" %04x",ram[0x83c040+i]|ram[0x83c041+i]<<8); puts("");
        FILE *frame=fopen("/tmp/zsp-first-frame.yuv","wb"); assert(frame);
        fwrite(ram+0x40000,1,320*160,frame); fwrite(ram+0x80000,1,160*80,frame); fwrite(ram+0x90000,1,160*80,frame); fclose(frame);
        for(unsigned event=0;event<4 && !z->core.fault;event++) {
            printf("event%u:",event); for(unsigned i=0;i<64;i+=2) printf(" %04x",ram[0x83c080+i]|ram[0x83c081+i]<<8); puts("");
            uint32_t response[]={0x8092,0,0,0x10040000,0x10080000,0x10090000,0,0};
            for(unsigned i=0;i<8;++i) for(unsigned j=0;j<4;++j) ram[0x83c0c0+4*i+j]=response[i]>>(8*j);
            zevio_dsp_control_write(z,0x104,0);
            zevio_dsp_run(z,2000000);
            printf("post event PC=%04x imask=%04x ip0=%04x ip1=%04x ireq=%04x\n",z->core.pc,z->core.c[2],z->core.c[3],z->core.c[4],z->core.c[9]);
        }
        FILE *out=fopen("/tmp/zsp-probe-ram.bin","wb"); assert(out); fwrite(ram,1,0x1000000,out); fclose(out);
        out=fopen("/tmp/zsp-probe-data.bin","wb"); assert(out); fwrite(z->d,1,sizeof z->d,out); fclose(out);
    }
    free(z); free(ram);
}
