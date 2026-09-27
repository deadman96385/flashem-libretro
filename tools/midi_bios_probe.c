/* Execute the actual BIOS streaming driver against its saved RAM image.
 * gcc -O2 tools/midi_bios_probe.c src/arm9.c src/cp15.c -lm -o /tmp/midi_probe */
#include "../src/arm9.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char ram[0x1000000];
static uint32_t regs[0x1300/4];
static uint32_t cdregs[0x100/4];
static ARM9 cpu;
static unsigned writes;
static uint8_t r8(void *c,uint32_t a) {
    (void)c;
    if(a>=0x10000000 && a<0x11000000)return ram[a-0x10000000];
    if(a>=0xb0000000 && a<0xb0001300)return ((uint8_t*)regs)[a-0xb0000000];
    if(a>=0xc4000000 && a<0xc4000100)return ((uint8_t*)cdregs)[a-0xc4000000];
    return 0;
}
static uint16_t r16(void*c,uint32_t a){return r8(c,a)|(uint16_t)r8(c,a+1)<<8;}
static uint32_t r32(void*c,uint32_t a){return r16(c,a)|(uint32_t)r16(c,a+2)<<16;}
static void w8(void*c,uint32_t a,uint8_t v){(void)c;if(a>=0x10000000&&a<0x11000000)ram[a-0x10000000]=v;}
static void w16(void*c,uint32_t a,uint16_t v){w8(c,a,v);w8(c,a+1,v>>8);}
static void w32(void*c,uint32_t a,uint32_t v){
    if(a>=0xb0000000&&a<0xb0001300&&!(a&3)){
        regs[(a-0xb0000000)/4]=v;writes++;
        printf("%08X = %08X PC=%08X\n",a,v,cpu.r[15]-8);
    }else if(a>=0xc4000000&&a<0xc4000100&&!(a&3)){
        cdregs[(a-0xc4000000)/4]=v;
        printf("%08X = %08X PC=%08X\n",a,v,cpu.r[15]-8);
    }else{w16(c,a,v);w16(c,a+2,v>>16);}
}
static void invoke(uint32_t pc,uint32_t a,uint32_t b){
    unsigned n;
    cpu.r[0]=a;cpu.r[1]=b;cpu.r[13]=0x10fff000;
    cpu.r[14]=0x10fffffc;cpu.r[15]=pc;
    for(n=0;n<1000000&&cpu.r[15]!=0x10fffffc;n++)arm9_step(&cpu);
    if(n==1000000){fprintf(stderr,"limit PC=%08X\n",cpu.r[15]);exit(2);}
    fprintf(stderr,"call %08X: %u steps r0=%08X\n",pc,n,cpu.r[0]);
}
int main(int argc,char**argv){
    FILE*f;
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(ram,1,sizeof(ram),f)!=sizeof(ram))return 2;
    fclose(f);arm9_reset(&cpu);
    cpu.mem_read8=r8;cpu.mem_read16=r16;cpu.mem_read32=r32;
    cpu.mem_write8=w8;cpu.mem_write16=w16;cpu.mem_write32=w32;
    invoke(0x1008aa84,0x10156b78,0x10156b8c);
    invoke(0x1008abfc,0,0);
    invoke(0x1008b9f8,0,0);
    /* Isolated low-level split/ring setter with documented ABI arguments;
     * this call is a probe, not a claim of full CD startup execution. */
    cpu.r[2]=0x102521b0;cpu.r[3]=0x1024c590;
    w32(NULL,0x10fff000,11760);
    invoke(0x1006241c,1,1);
    fprintf(stderr,"writes=%u; original firmware arguments from100149F0/F4\n",writes);
    return 0;
}
