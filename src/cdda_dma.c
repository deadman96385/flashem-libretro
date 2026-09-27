#include "cdda_dma.h"

/* BIOS1006241C: +28/+2C left ring and +34/+38 right ring. End registers
 * address the final32-bit word; +24/+30 are live cursors. Control+40 bit5
 * selects split stereo and bit6 enables wrapping. Caller selects this path. */
static int ring_valid(uint32_t start, uint32_t end, uint32_t base, size_t size) {
    uint64_t limit = (uint64_t)end + 4;
    return !(start & 3) && !(end & 3) && start <= end && start >= base &&
           limit <= (uint64_t)base + size && limit <= UINT32_MAX;
}

int cdda_dma_sector(uint32_t *s, const uint8_t raw[2352],
                    uint8_t *ram, size_t size, uint32_t base) {
    uint32_t ls=s[0x28/4], le=s[0x2c/4], rs=s[0x34/4], re=s[0x38/4];
    uint32_t lp=s[0x24/4], rp=s[0x30/4], mark=s[0x80/4];
    unsigned i; int result=CDDA_DMA_DONE;
    if (!ram || !raw || (s[0x40/4]&0x78)!=0x60 ||
        !ring_valid(ls,le,base,size) || !ring_valid(rs,re,base,size)) return 0;
    le+=4; re+=4;
    if ((lp&1)||lp<ls||lp>=le)lp=ls;
    if ((rp&1)||rp<rs||rp>=re)rp=rs;
    for(i=0;i<588;i++) {
        uint32_t prev=lp;
        ram[lp-base]=raw[i*4]; ram[lp-base+1]=raw[i*4+1];
        ram[rp-base]=raw[i*4+2]; ram[rp-base+1]=raw[i*4+3];
        lp+=2;rp+=2;
        if(lp==le)lp=ls;
        if(rp==re)rp=rs;
        if((mark>prev&&(uint64_t)mark<=(uint64_t)prev+2)||mark==lp)
            result|=CDDA_DMA_MARK;
    }
    s[0x24/4]=lp; s[0x30/4]=rp;
    return result;
}
