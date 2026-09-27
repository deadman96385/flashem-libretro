/* Replay captured scanout state with the current production compositor.
 * Usage: ram ve output.ppm [palette-sram]. Compile without src/hw.c. */
#include "../src/hw.c"
#include <assert.h>
static void load(const char*name,void*p,size_t n){FILE*f=fopen(name,"rb");assert(f);assert(fread(p,1,n,f)==n);fclose(f);}
int main(int argc,char**argv){
    assert(argc==4||argc==5);ARM9 cpu={0};static uint32_t fb[VFLASH_FB_MAX_W*VFLASH_FB_MAX_H];
    HW*h=hw_create(&cpu,NULL,0,NULL,fb);assert(h);
    load(argv[1],h->ram,RAM_SIZE);load(argv[2],h->ve,sizeof h->ve);
    if(argc==5)load(argv[4],h->sram,sizeof h->sram);
    ve_render(h);FILE*f=fopen(argv[3],"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",h->scr_w,h->scr_h);
    for(int i=0;i<h->scr_w*h->scr_h;i++){fputc(fb[i]>>16,f);fputc(fb[i]>>8,f);fputc(fb[i],f);}fclose(f);
    printf("Replayed VE enables=%02x format=%02x\n",h->ve[0x100/4],h->ve[0x108/4]);hw_destroy(h);return 0;
}
