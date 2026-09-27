/* Research harness: device counters and every rejected start, no production edits.
 * Includes implementation files solely to inspect private device state. */
#include <assert.h>
#include "../src/vflash.c"
#include "../src/hw.c"
static VFlash *stress_v;
static unsigned stress_frame,max_voices,starts[512],rejected[512],other_modes;
static unsigned finite_ramps,current_reads,dsp_requests,dsp_fault_frames;
static uint64_t dsp_instructions,dsp_previous,dma_words,dma_previous;
static uint64_t rejected_total,last_rejected;
static unsigned pop64(uint64_t bits){unsigned n=0;while(bits){n++;bits&=bits-1;}return n;}
static uint32_t stress_read(void*ctx,uint32_t a) {
    if(a==0xb0001248)current_reads++;
    return hw_read32(ctx,a);
}
static void stress_write(void*ctx,uint32_t a,uint32_t value) {
    hw_write32(ctx,a,value);
    Midi*m=&stress_v->hw->midi;
    if(a==0xb0001184 && value!=0xffffffff) {
        finite_ramps++;
        printf("STRESS_RAMP frame=%u value=%08x pc=%08x\n",stress_frame,value,stress_v->cpu.r[15]-8);
    }
    if(a==0xb0001188)printf("STRESS_VOLUME frame=%u target=%08x\n",stress_frame,value);
    if(a==0x900a0108 && (value&1))dsp_requests++;
    if(a==0xb0001010||a==0xb0001014) {
        for(unsigned n=0;n<32;n++)if(value&(1u<<n)) {
            unsigned voice=n+(a==0xb0001014?32:0),*r=&m->regs[voice*16],mode=r[3];
            int accepted=!!(m->active&(UINT64_C(1)<<voice));
            if(mode<512){starts[mode]++;if(!accepted)rejected[mode]++;}else other_modes++;
            if(!accepted) {
                printf("STRESS_REJECT frame=%u voice=%u pc=%08x regs",stress_frame,voice,stress_v->cpu.r[15]-8);
                for(unsigned j=0;j<10;j++)printf(" %08x",r[j]);puts("");
            }
        }
        unsigned n=pop64(m->active);if(n>max_voices)max_voices=n;
    }
}
static void stress_summary(void) {
    printf("STRESS max_voices=%u finite_ramps=%u current_reads=%u dsp_requests=%u dsp_fault_frames=%u dsp_instructions=%llu dsp_dma_words=%llu unsupported_total=%llu other_modes=%u\n",max_voices,finite_ramps,current_reads,dsp_requests,dsp_fault_frames,(unsigned long long)dsp_instructions,(unsigned long long)dma_words,(unsigned long long)rejected_total,other_modes);
    for(unsigned i=0;i<512;i++)if(starts[i])printf("STRESS_MODE %03X starts=%u rejected=%u\n",i,starts[i],rejected[i]);
}
static VFlash*stress_create(const char*disc) {
    stress_v=vflash_create(disc);if(!stress_v)return NULL;
    stress_v->cpu.mem_read32=stress_read;stress_v->cpu.mem_write32=stress_write;
    atexit(stress_summary);return stress_v;
}
static void stress_run(VFlash*v) {
    vflash_run_frame(v);
    uint64_t now=v->hw->zsp.core.instructions;
    dsp_instructions+=now>=dsp_previous?now-dsp_previous:now;dsp_previous=now;
    now=v->hw->zsp.words;dma_words+=now>=dma_previous?now-dma_previous:now;dma_previous=now;
    now=v->hw->midi.unsupported_starts;
    rejected_total+=now>=last_rejected?now-last_rejected:now;last_rejected=now;
    dsp_fault_frames+=v->hw->zsp.core.fault!=0;
    if(stress_frame%1000==0)printf("STRESS_PROGRESS frame=%u voices=%u dsp=%llu fault=%d\n",stress_frame,pop64(v->hw->midi.active),(unsigned long long)dsp_instructions,v->hw->zsp.core.fault);
    stress_frame++;
}
static void stress_input(VFlash*v,uint32_t buttons) {
    /* Cars' established audit route: down taps otherwise enter Options. */
    if(getenv("STRESS_NO_DOWN"))buttons&=~VFLASH_BTN_DOWN;
    vflash_set_input(v,buttons);
}
static void stress_destroy(VFlash*v) {
    const char *prefix=getenv("STRESS_SNAPSHOT");
    if(prefix) {
        char path[1024];FILE*f;
        snprintf(path,sizeof path,"%s.ram",prefix);f=fopen(path,"wb");assert(f);
        fwrite(v->hw->ram,1,RAM_SIZE,f);fclose(f);
        snprintf(path,sizeof path,"%s.ve",prefix);f=fopen(path,"wb");assert(f);
        fwrite(v->hw->ve,1,sizeof v->hw->ve,f);fclose(f);
        snprintf(path,sizeof path,"%s.dsp-control",prefix);f=fopen(path,"wb");assert(f);
        fwrite(v->hw->zsp.control,1,sizeof v->hw->zsp.control,f);fclose(f);
    }
    vflash_destroy(v);
}
#define vflash_create stress_create
#define vflash_run_frame stress_run
#define vflash_set_input stress_input
#define vflash_destroy stress_destroy
#include "audit_game_audio.c"
