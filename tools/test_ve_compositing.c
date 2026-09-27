/* Movie plane behind RGB, including opaque black and transparent windows.
 * Compile without src/hw.c. No proprietary assets are required. */
#include "../src/hw.c"
#include <assert.h>
static void pixel(HW*h,unsigned x,unsigned y,uint16_t c,int tiled){
    unsigned off=tiled?y*64+x*2:(y*8+x)*2;
    h->ram[0x1000+off]=c;h->ram[0x1001+off]=c>>8;
}
int main(void){
    ARM9 cpu={0};static uint32_t fb[VFLASH_FB_MAX_W*VFLASH_FB_MAX_H];
    HW*h=hw_create(&cpu,NULL,0,NULL,fb);assert(h);
    h->ve[0x0c/4]=7|(3<<16);h->ve[0x10c/4]=0x7c00;
    h->ve[0x130/4]=0;h->ve[0x134/4]=7|(3<<16);
    h->ve[0x138/4]=2|(1<<16);h->ve[0x13c/4]=5|(2<<16);
    h->ve[0x160/4]=RAM_BASE+0x1000;
    h->ve[0x164/4]=RAM_BASE+0x6000;h->ve[0x168/4]=RAM_BASE+0x6100;h->ve[0x16c/4]=RAM_BASE+0x6200;
    memset(h->ram+0x6000,150,8);memset(h->ram+0x6100,128,2);memset(h->ram+0x6200,128,2);
    for(int tiled=0;tiled<=1;tiled++){
        h->ve[0x100/4]=0x30;h->ve[0x108/4]=tiled?4:0;
        for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++)pixel(h,x,y,0x001f,tiled);
        pixel(h,3,1,0x8000,tiled);pixel(h,4,1,0,tiled);pixel(h,0,0,0x8000,tiled);
        ve_render(h);
        assert(fb[1*8+2]==0xffff0000); /* opaque menu covers valid movie data */
        assert(fb[1*8+3]==0xff969696); /* transparent window reveals movie */
        assert(fb[1*8+4]==0xff000000); /* black is still opaque */
        assert(fb[0]==0xff0000ff); /* outside movie, reveal background */
        h->ve[0x100/4]=0x20;ve_render(h);assert(fb[1*8+2]==0xff969696);
        h->ve[0x100/4]=0x10;ve_render(h);assert(fb[1*8+3]==0xff0000ff);
    }
    hw_destroy(h);puts("PASS: RGB menus cover movie planes; transparent windows reveal video in linear and tiled RGB modes");return 0;
}
