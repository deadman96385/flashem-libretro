/* Exercise actual sector delivery, slot/IRQ state and planar MIDI consumption.
 * Include the implementation to reach the physical sector-arrival boundary;
 * no production test hooks or changes are needed. Compile without src/hw.c. */
#include "../src/hw.c"
#include <assert.h>
int main(void) {
    ARM9 cpu={0}; CDROM cd={0}; uint8_t raw[2352]; int16_t out[1176];
    uint32_t fb[512*512]={0}; unsigned i,j; HW *h;
    cd.fp=tmpfile();assert(cd.fp);cd.raw_sector_size=2352;cd.sector_count=1;cd.is_open=1;
    for(i=0;i<588;i++) {
        int16_t l=(int16_t)(i*29-8500),r=(int16_t)(12000-i*31);
        raw[i*4]=(uint8_t)l;raw[i*4+1]=(uint16_t)l>>8;
        raw[i*4+2]=(uint8_t)r;raw[i*4+3]=(uint16_t)r>>8;
    }
    assert(fwrite(raw,1,sizeof(raw),cd.fp)==sizeof(raw));fflush(cd.fp);
    h=hw_create(&cpu,NULL,0,&cd,fb);assert(h);
    h->ic.noninverted=~0u;
    h->ser[0x1c/4]=0x1000000;h->ser[0x20/4]=1;h->ser[0x3c/4]=RAM_BASE+0x8000;
    h->ser[0x40/4]=0x64;h->ser[0x68/4]=CD_SLOT;
    h->ser[0x24/4]=h->ser[0x28/4]=RAM_BASE+0x1000;h->ser[0x2c/4]=RAM_BASE+0x1ffc;
    h->ser[0x30/4]=h->ser[0x34/4]=RAM_BASE+0x3000;h->ser[0x38/4]=RAM_BASE+0x3ffc;
    h->ser[0x80/4]=RAM_BASE+0x1200;
    cd_deliver(h,0);cd_int_check(h); /* sector scheduler updates IRQ after delivery */
    assert(h->cd_sectors==1&&h->ser[0x70/4]==4);
    assert((h->ser[0x64/4]&(CD_FOUND|CD_SLOT|CD_END|0x10000))==(CD_FOUND|CD_SLOT|CD_END|0x10000));
    assert(h->ser[0x20/4]==0&&!(h->ser[0x40/4]&4));
    assert((h->ser[0x1c/4]&0xffffff)==0x000201);
    assert(h->ser[0x24/4]==RAM_BASE+0x1498&&h->ser[0x30/4]==RAM_BASE+0x3498);
    assert(h->ic.raw&(1u<<14));
    assert(h->ram[0x8014]==0&&h->ram[0x8015]==2&&h->ram[0x8016]==0);
    for(i=0x8018;i<0x8138;i++)assert(h->ram[i]==0); /* no fake raw audio ECC */
    for(j=0;j<2;j++) {
        uint32_t off=j*0x40,start=RAM_BASE+0x1000+j*0x2000;
        midi_write(&h->midi,off,start,0);midi_write(&h->midi,off+4,start+1174,0);
        midi_write(&h->midi,off+8,start,0);midi_write(&h->midi,off+12,0x10,0);
        midi_write(&h->midi,off+16,0x1000,0);midi_write(&h->midi,off+20,~0u,0);
        midi_write(&h->midi,off+24,0xffff0000,0);midi_write(&h->midi,off+28,~0u,0);
        midi_write(&h->midi,off+32,j?0xff00:0xff,0);
    }
    midi_write(&h->midi,0x1010,3,0);midi_render(&h->midi,out,588);
    for(i=0;i<1176;i++)assert(out[i]==(int16_t)(raw[i*2]|(uint16_t)raw[i*2+1]<<8));
    hw_write32(h,0xc4000074,2);hw_write32(h,0xc4000064,CD_SLOT);
    assert((h->ser[0x70/4]&0x1c)==0&&!(h->ic.raw&(1u<<14)));
    hw_destroy(h);fclose(cd.fp);
    puts("CDDA HW delivery, slots, IRQ14, transfer end, and stereo PCM verified");return 0;
}
