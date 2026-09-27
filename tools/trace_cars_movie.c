/* Cars movie lifecycle capture: original boot and firmware, no injected state. */
#include <assert.h>
#include "../src/vflash.c"
#include "../src/hw.c"
static VFlash *car_v;static unsigned car_frame,plane_seq,boot_seq,ack_seq;
static const char*car_dir;
static uint32_t word(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void blob(const char*name,const void*p,size_t n) {
    char path[1024];snprintf(path,sizeof path,"%s/%s",car_dir,name);
    FILE*f=fopen(path,"wb");assert(f);assert(fwrite(p,1,n,f)==n);fclose(f);
}
static void planes(const char*name) {
    HW*h=car_v->hw;uint32_t base=(h->zsp.control[0x24/4]&0x7fff)<<17;
    uint32_t off=base+0x1c040-RAM_BASE;
    if(off>RAM_SIZE-64)return;
    unsigned w=word(h->ram+off+8),ht=word(h->ram+off+12);
    if(!w||!ht||w*ht>0x40000)return;
    uint32_t a[3]={h->ve[0x164/4],h->ve[0x168/4],h->ve[0x16c/4]};
    char path[1024];snprintf(path,sizeof path,"%s/%s.yuv",car_dir,name);
    FILE*f=fopen(path,"wb");assert(f);
    for(unsigned i=0;i<3;i++) {
        unsigned n=w*ht/(i?4:1),o=a[i]-RAM_BASE;
        assert(o<RAM_SIZE&&n<=RAM_SIZE-o);fwrite(h->ram+o,1,n,f);
    }fclose(f);
    printf("CAR_PLANE file=%s f=%u size=%ux%u ptr=%08x/%08x/%08x status=%08x cmd=%08x reply=%08x frameword=%08x\n",name,car_frame,w,ht,a[0],a[1],a[2],h->zsp.control[0x104/4],word(h->ram+off-64),word(h->ram+off),word(h->ram+off+64+28));
}
static void car_write(void*ctx,uint32_t a,uint32_t value) {
    HW*h=car_v->hw;
    if(a==0x900a0104 && !(value&1) && (h->zsp.control[0x104/4]&1)) {
        uint32_t base=(h->zsp.control[0x24/4]&0x7fff)<<17,o=base+0x1c080-RAM_BASE;
        if(o<RAM_SIZE-128) {
            printf("CAR_ACK f=%u pc=%08x msg=",car_frame,car_v->cpu.r[15]-8);
            for(unsigned i=0;i<16;i++)printf(" %08x",word(h->ram+o+4*i));
            printf(" reply=");for(unsigned i=0;i<16;i++)printf(" %08x",word(h->ram+o+64+4*i));
            printf(" stream=");for(unsigned i=0;i<18;i++)printf(" %08x",word(h->ram+0xb8f8a0+4*i));
            printf(" readidx=%u ser=",word(h->ram+0xbb9f80));for(unsigned i=0x1c/4;i<0x88/4;i++)printf(" %08x",h->ser[i]);puts("");
            if(ack_seq<16) {char snap[80];snprintf(snap,sizeof snap,"ack-%04u.input",ack_seq);blob(snap,h->ram+0x5bd000,0x96000);}
            ack_seq++;
        }
    }
    if(a==0x900a0020&&(value&1)&&!h->zsp.running) {
        char name[128];snprintf(name,sizeof name,"boot%u-program.bin",boot_seq);blob(name,h->zsp.p,sizeof h->zsp.p);
        snprintf(name,sizeof name,"boot%u-data.bin",boot_seq);blob(name,h->zsp.d,sizeof h->zsp.d);boot_seq++;
    }
    if(a==0x900a0108&&(value&1)) {
        uint32_t base=(h->zsp.control[0x24/4]&0x7fff)<<17,o=base+0x1c000-RAM_BASE;
        if(o<RAM_SIZE-64) {
            printf("CAR_COMMAND f=%u",car_frame);for(unsigned i=0;i<13;i++)printf(" %08x",word(h->ram+o+4*i));puts("");
            if(word(h->ram+o)==0x82)blob("before-stop.ram",h->ram,RAM_SIZE);
        }
    }
    hw_write32(ctx,a,value);
    if(car_frame>=6000 && (a==0xb8000100||a==0xb8000108||a==0xb8000138||a==0xb800013c||a==0xb8000164||a==0xb8000168||a==0xb800016c))
        printf("CAR_VE f=%u pc=%08x reg=%03x value=%08x\n",car_frame,car_v->cpu.r[15]-8,a-VE_BASE,value);
    if(a==0xb800016c && car_frame>=6000) {
        if(plane_seq>=349 && plane_seq<=350) {
            char snap[80];snprintf(snap,sizeof snap,"present-%04u.ram",plane_seq);blob(snap,h->ram,RAM_SIZE);
        }
        char name[64];snprintf(name,sizeof name,"present-%04u",plane_seq++);planes(name);
    }
}
static VFlash*car_create(const char*disc) {
    car_v=vflash_create(disc);if(!car_v)return NULL;
    car_dir=getenv("CAR_CAPTURE_DIR");assert(car_dir);
    car_v->cpu.mem_write32=car_write;return car_v;
}
static void car_run(VFlash*v) {
    vflash_run_frame(v);
    if(car_frame>=6300 && car_frame%60==0) {
        char name[64];snprintf(name,sizeof name,"tick-%04u",car_frame);planes(name);
    }
    car_frame++;
}
static void car_input(VFlash*v,uint32_t b){vflash_set_input(v,b&~VFLASH_BTN_DOWN);}
#define vflash_create car_create
#define vflash_run_frame car_run
#define vflash_set_input car_input
#include "audit_game_audio.c"
