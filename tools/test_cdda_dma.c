#include "../src/cdda_dma.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    uint32_t s[0x100/4]={0}; uint8_t raw[2352], ram[8192], before[8192];
    unsigned i;
    for(i=0;i<588;i++) {
        raw[i*4]=i;raw[i*4+1]=i>>8;
        raw[i*4+2]=~i;raw[i*4+3]=(~i)>>8;
    }
    memset(ram,0x55,sizeof(ram));
    s[0x40/4]=0x60;
    s[0x28/4]=0x1000;s[0x2c/4]=0x17fc;
    s[0x34/4]=0x2000;s[0x38/4]=0x27fc;
    s[0x24/4]=0x17f8;s[0x30/4]=0x2000;s[0x80/4]=0x1000;
    assert(cdda_dma_sector(s,raw,ram,sizeof(ram),0x1000)==3);
    assert(s[0x24/4]==0x1490&&s[0x30/4]==0x2498);
    for(i=0;i<588;i++) {
        unsigned l=(0x7f8+i*2)%0x800,r=0x1000+i*2;
        assert(ram[l]==raw[i*4]&&ram[l+1]==raw[i*4+1]);
        assert(ram[r]==raw[i*4+2]&&ram[r+1]==raw[i*4+3]);
    }
    assert(ram[0x800]==0x55&&ram[0x1800]==0x55);
    memcpy(before,ram,sizeof(ram));
    s[0x38/4]=0x4000; /* bad right ring must not partly write left */
    assert(cdda_dma_sector(s,raw,ram,sizeof(ram),0x1000)==0);
    assert(!memcmp(before,ram,sizeof(ram)));
    s[0x38/4]=0x27fc;s[0x40/4]=0;
    assert(cdda_dma_sector(s,raw,ram,sizeof(ram),0x1000)==0);
    puts("CDDA split, independent wrap, watermark and bounds tests passed");
    return 0;
}
