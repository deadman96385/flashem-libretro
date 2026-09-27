/* Trace movie-to-menu scanout transitions during an ordinary frontend run. */
#include <assert.h>
#include "../src/vflash.c"
#include "../src/hw.c"
static VFlash*window_v;static unsigned window_frame;static const char*window_dir;
static int window_movie;static unsigned window_boot;
static int window_trace_movie,window_trace_lcd;
static unsigned window_stops;
static int window_quiet;
static void window_blob(const char*name,const void*p,size_t n){
    char path[1024];snprintf(path,sizeof path,"%s/%s",window_dir,name);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(p,1,n,f)==n);fclose(f);
}
static void window_write(void*ctx,uint32_t a,uint32_t value){
    HW*h=window_v->hw;uint32_t old=0;
    if(window_trace_movie && value==0x80000040 && window_frame>7000 && a>=RAM_BASE && a<RAM_BASE+RAM_SIZE) {
        printf("MOVIE_QUEUE_STOP f=%u address=%08x pc=%08x lr=%08x sp=%08x\n",window_frame,a,window_v->cpu.r[15]-8,window_v->cpu.r[14],window_v->cpu.r[13]);
        window_blob("queue-stop.ram",h->ram,RAM_SIZE);window_blob("queue-stop.regs",window_v->cpu.r,sizeof window_v->cpu.r);
    }
    if(window_trace_movie && (a==0x10abc9c8 || a==0x10abc9ec || a==0x10abcaac)) {
        uint32_t previous;memcpy(&previous,h->ram+a-RAM_BASE,4);
        if(previous!=value)printf("MOVIE_WATCH f=%u address=%08x old=%08x value=%08x pc=%08x lr=%08x\n",window_frame,a,previous,value,window_v->cpu.r[15]-8,window_v->cpu.r[14]);
    }
    if(window_trace_movie&&(a&0x1ffff)==0x1c000&&(value==0x80||value==0x81||value==0x82)){
        uint32_t base=(h->zsp.control[0x24/4]&0x7fff)<<17;
        if(a==base+0x1c000&&(value==0x80||value==0x81||value==0x82)){
            if(value==0x80 && getenv("VE_HOLD_LOADING"))window_movie=1;
            printf("MOVIE_PRODUCER f=%u cmd=%02x pc=%08x lr=%08x sp=%08x\n",window_frame,value,window_v->cpu.r[15]-8,window_v->cpu.r[14],window_v->cpu.r[13]);
            if(value==0x82 && window_stops++==0){window_blob("before-stop.ram",h->ram,RAM_SIZE);window_blob("before-stop.regs",window_v->cpu.r,sizeof window_v->cpu.r);window_blob("before-stop.control",h->zsp.control,sizeof h->zsp.control);}
        }
    }
    if(window_trace_lcd&&window_frame>=8899&&window_frame<=8903&&a>=0xa8000000&&a<0xa8001000)
        printf("WINDOW_LCD f=%u pc=%08x reg=%03x value=%08x\n",window_frame,window_v->cpu.r[15]-8,a-0xa8000000,value);
    if(a==0x900a0020&&(value&1)&&!h->zsp.running){
        char name[80];snprintf(name,sizeof name,"boot%u-program.bin",window_boot);window_blob(name,h->zsp.p,sizeof h->zsp.p);
        snprintf(name,sizeof name,"boot%u-data.bin",window_boot++);window_blob(name,h->zsp.d,sizeof h->zsp.d);
    }
    if(a>=VE_BASE&&a<VE_BASE+sizeof h->ve)old=h->ve[(a-VE_BASE)/4];
    hw_write32(ctx,a,value);
    if(window_frame>=1800&&old!=value&&(a==0xb8000100||a==0xb8000108||a==0xb8000138||a==0xb800013c||a==0xb8000164||a==0xb8000168||a==0xb800016c))
        printf("WINDOW_WRITE f=%u pc=%08x reg=%03x old=%08x value=%08x\n",window_frame,window_v->cpu.r[15]-8,a-VE_BASE,old,value);
    if(a==0x900a0108&&(value&1)){
        uint32_t base=(h->zsp.control[0x24/4]&0x7fff)<<17,o=base+0x1c000-RAM_BASE;
        if(o<RAM_SIZE-64){uint32_t cmd;memcpy(&cmd,h->ram+o,4);printf("WINDOW_DSP f=%u command=%08x\n",window_frame,cmd);if(cmd==0x81)window_movie=1;if(cmd==0x82)window_movie=0;}
    }
}
static VFlash*window_create(const char*disc){
    window_v=vflash_create(disc);assert(window_v);window_dir=getenv("VE_CAPTURE_DIR");assert(window_dir);
    window_trace_movie=getenv("VE_TRACE_MOVIE")!=NULL;window_trace_lcd=getenv("VE_TRACE_LCD")!=NULL;
    window_v->cpu.mem_write32=window_write;return window_v;
}
static void window_run(VFlash*v){
    vflash_run_frame(v);HW*h=v->hw;unsigned f=window_frame;
    if(!window_quiet && f>3000 && getenv("VE_HOLD_PINK_LOADING")) {
        int w,ht,pink=0,total=0;vflash_get_screen_size(v,&w,&ht);
        uint32_t*p=vflash_get_framebuffer(v);
        for(int y=0;y<ht;y+=8)for(int x=0;x<w;x+=8){
            uint32_t c=p[y*w+x];unsigned r=c>>16&255,g=c>>8&255,b=c&255;
            pink+=r>225 && g>160 && g<225 && b>185;total++;
        }
        if(pink*10>total*6){window_quiet=1;printf("WINDOW_INPUT_QUIET f=%u pink=%d/%d\n",f,pink,total);}
    }
    const char*extra_frame=getenv("VE_EXTRA_FRAME");int extra=extra_frame&&f==strtoul(extra_frame,NULL,0);
    if(f>=2100&&(f%300==0||f==8999||extra)){
        char name[80];snprintf(name,sizeof name,"f%05u.ve",f);window_blob(name,h->ve,sizeof h->ve);
        printf("WINDOW_STATE f=%u enable=%08x format=%08x win=%08x/%08x ptr=%08x/%08x/%08x dsp=%d\n",f,h->ve[0x100/4],h->ve[0x108/4],h->ve[0x138/4],h->ve[0x13c/4],h->ve[0x164/4],h->ve[0x168/4],h->ve[0x16c/4],h->zsp.running);
        printf("WINDOW_CORE f=%u fault=%u pc=%04x instructions=%llu dma=%llu\n",f,h->zsp.core.fault,h->zsp.core.pc,(unsigned long long)h->zsp.core.instructions,(unsigned long long)h->zsp.words);
        char path[1024];snprintf(path,sizeof path,"%s/f%05u.ppm",window_dir,f);FILE*shot=fopen(path,"wb");assert(shot);
        int w,ht;vflash_get_screen_size(v,&w,&ht);uint32_t*p=vflash_get_framebuffer(v);fprintf(shot,"P6\n%d %d\n255\n",w,ht);
        for(int i=0;i<w*ht;i++){fputc(p[i]>>16,shot);fputc(p[i]>>8,shot);fputc(p[i],shot);}fclose(shot);
        if(f==4500||f==5400||f==6000||f==7500||f==8999||extra||getenv("VE_ALL_RAM")){
            snprintf(name,sizeof name,"f%05u.ram",f);window_blob(name,h->ram,RAM_SIZE);
            snprintf(name,sizeof name,"f%05u.sram",f);window_blob(name,h->sram,sizeof h->sram);
        }
    }
    window_frame++;
}
#define vflash_create window_create
#define vflash_run_frame window_run
static void window_input(VFlash*v,uint32_t buttons){
    if(window_quiet)buttons=0;
    const char*stop=getenv("VE_PAUSE_INPUT_AFTER");
    if(stop&&window_frame>=strtoul(stop,NULL,0))buttons=0;
    if(getenv("VE_HOLD_MOVIES")&&window_movie)buttons=0;
    vflash_set_input(v,buttons);
}
#define vflash_set_input window_input
#include "audit_game_audio.c"
