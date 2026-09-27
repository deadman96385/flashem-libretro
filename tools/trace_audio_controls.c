/* Research-only CPU MMIO interception; no device semantics are changed.
 * Compile in place of vflash.c and audit_game_audio.c. Set VFLASH_AUDIO_TRACE. */
#include "../src/vflash.c"
static VFlash *traced_vf;
static FILE *trace_file;
static unsigned trace_frame,reads[0x1300/4],writes[0x1300/4];
static uint32_t previous[0x1300/4];
static uint32_t pc(void){return traced_vf->cpu.r[15]-(traced_vf->cpu.thumb?4:8);}
static void trace_summary(void) {
    if(!trace_file)return;
    for(unsigned i=0;i<0x1300/4;i++)if(reads[i]||writes[i])
        fprintf(trace_file,"COUNT %04x reads=%u writes=%u\n",i*4,reads[i],writes[i]);
    fclose(trace_file);
}
static uint32_t traced_read(void*ctx,uint32_t a) {
    uint32_t value=hw_read32(ctx,a);
    if(a>=0xb0000000&&a<0xb0001300) {
        unsigned off=a-0xb0000000,i=off/4;reads[i]++;
        if(reads[i]<=2 || previous[i]!=value)
            fprintf(trace_file,"R f=%u pc=%08x reg=%04x value=%08x\n",trace_frame,pc(),off,value);
        previous[i]=value;
    }
    return value;
}
static void traced_write(void*ctx,uint32_t a,uint32_t value) {
    if(a>=0xb0000000&&a<0xb0001300) {
        unsigned off=a-0xb0000000;writes[off/4]++;
        fprintf(trace_file,"W f=%u pc=%08x reg=%04x value=%08x\n",trace_frame,pc(),off,value);
    }
    hw_write32(ctx,a,value);
}
static VFlash *traced_create(const char*disc) {
    VFlash*v=vflash_create(disc);if(!v)return NULL;
    const char*path=getenv("VFLASH_AUDIO_TRACE");if(!path)return v;
    trace_file=fopen(path,"w");if(!trace_file)return v;
    traced_vf=v;v->cpu.mem_read32=traced_read;v->cpu.mem_write32=traced_write;
    atexit(trace_summary);return v;
}
static void traced_run(VFlash*v){vflash_run_frame(v);trace_frame++;}
#define vflash_create traced_create
#define vflash_run_frame traced_run
#include "audit_game_audio.c"
