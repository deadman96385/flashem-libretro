/* Replay actual movie RAM/VE registers through scanout, without another boot.
 * Including hw.c gives this focused test access to the scanout boundary. */
#include "../src/hw.c"
#include <assert.h>
int main(int argc,char **argv) {
    assert(argc==4);
    uint32_t *fb=calloc(VFLASH_FB_MAX_W*VFLASH_FB_MAX_H,sizeof(*fb));assert(fb);
    ARM9 cpu={0};HW *h=hw_create(&cpu,NULL,0,NULL,fb);assert(h);
    FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(h->ram,1,RAM_SIZE,f)==RAM_SIZE);fclose(f);
    f=fopen(argv[2],"rb");assert(f);assert(fread(h->ve,1,sizeof h->ve,f)==sizeof h->ve);fclose(f);
    for(unsigned i=0x100;i<0x170;i+=4)printf("VE %03X = %08X\n",i,h->ve[i/4]);
    assert(h->ve[0x100/4]&32);
    assert(ve_yuv(0,128,128)==0xff000000 && ve_yuv(255,128,128)==0xffffffff);
    assert(ve_yuv(76,85,255)==0xfffe0000); /* JPEG red, rounded coefficients */
    ve_render(h);int w=h->scr_w,ht=h->scr_h;
    f=fopen(argv[3],"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",w,ht);
    for(int i=0;i<w*ht;i++){fputc(fb[i]>>16,f);fputc(fb[i]>>8,f);fputc(fb[i],f);}fclose(f);
    /* A truncated plane must be rejected, even when the window is valid. */
    h->ve[0x100/4]=32;h->ve[0x164/4]=RAM_BASE+RAM_SIZE-1;
    ve_render(h);for(int i=0;i<w*ht;i++)assert(fb[i]==rgb555(h->ve[0x10c/4]));
    hw_destroy(h);free(fb);puts("PASS: movie scanout and invalid-plane rejection");
}
