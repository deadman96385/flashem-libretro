/* Regression for a transparent movie mask followed by depth-tested sprites.
 * No proprietary assets. Uses the SDK's observed absolute/local D3 geometry.
 */
#include <assert.h>
#include "../src/ge.c"
static void list(GE*g,const uint32_t*words,size_t count){
    memcpy(g->ram+0x200000,words,count*4);ge_run(g,RAM_BASE+0x200000);
}
int main(void){
    assert(c555(0)==0);assert(c555(0x8000)==0);assert(c555(0x1f)==0x10001f);
    GE g={0};g.ram=calloc(1,0x1000000);assert(g.ram);g.ram_size=0x1000000;
    g.surface=RAM_BASE;g.top=128;g.clip_x0=512;g.clip_y0=240;g.clip_x1=1023;g.clip_y1=479;
    const uint32_t mask[]={0x1e008000,0xd3000003,0x01130260,0x00a00140,0x8000,
        0xd3000003,0x00230060,0x00a00140,0x4e00,0x1400fd01};
    list(&g,mask,sizeof mask/4);
    assert(*px(&g,608,275)==0x8000);
    /* The farther sprite must not overwrite the movie opening. */
    const uint32_t back[]={0xc8000004,0x4d33,0x00f00200,0x00f00200,0x1f,0x1400fd01};
    list(&g,back,sizeof back/4);
    assert(*px(&g,608,275)==0x8000);assert(*px(&g,600,270)==0x1f);
    /* A nearer foreground element can still cover video. */
    const uint32_t front[]={0xc8000004,0x5000,0x01130260,0x00010001,0x3e0,0x1400fd01};
    list(&g,front,sizeof front/4);assert(*px(&g,608,275)==0x3e0);
    /* A far depth clear makes subsequent background drawing possible again. */
    const uint32_t clear[]={0xd3000003,0,0x00f00200,0,0x1400fd01};
    list(&g,clear,sizeof clear/4);list(&g,back,sizeof back/4);assert(*px(&g,609,276)==0x1f);
    /* Clipped colour fills stay colour fills, and cannot write below top. */
    const uint32_t colour[]={0xd3000003,0x00ef01ff,0x00020002,0x7c00,0x1400fd01};
    list(&g,colour,sizeof colour/4);assert(*px(&g,512,240)==0x7c00);
    assert(px(&g,0,0)==NULL);
    free(g.ram);puts("PASS: local depth mask protects video from background and allows nearer foreground");
}
