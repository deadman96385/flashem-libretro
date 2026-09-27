/* CD data-ring backpressure, notification-only marks, wrap and restart.
 * Compile without src/hw.c; synthetic sectors require no game assets. */
#include "../src/hw.c"
#include <assert.h>
int main(void) {
    ARM9 cpu={0};CDROM cd={0};static uint32_t fb[512*512];uint8_t sector[2048];
    cd.fp=tmpfile();assert(cd.fp);cd.raw_sector_size=2048;cd.sector_count=8;cd.is_open=1;
    for(unsigned i=0;i<8;i++){memset(sector,0x40+i,sizeof sector);assert(fwrite(sector,1,sizeof sector,cd.fp)==sizeof sector);}fflush(cd.fp);
    HW*h=hw_create(&cpu,NULL,0,&cd,fb);assert(h);h->ic.noninverted=~0u;
    uint32_t base=RAM_BASE+0x10000;memset(h->ram+0x10000,0xa5,8192);
    h->ser[0x1c/4]=0x1000000;h->ser[0x20/4]=0x1000000;
    h->ser[0x24/4]=h->ser[0x28/4]=base;h->ser[0x2c/4]=base+8192-4;
    h->ser[0x3c/4]=RAM_BASE+0x8000;h->ser[0x40/4]=0x14;
    h->ser[0x68/4]=CD_END;h->ser[0x80/4]=base+4096;h->ser[0x84/4]=1;
    cd_deliver(h,0);assert(h->ser[0x40/4]&4);cd_release(h,1);
    cd_deliver(h,1);cd_int_check(h);
    assert(!(h->ser[0x40/4]&4)&&!h->cd_xfer);
    assert((h->ser[0x64/4]&(CD_END|0x10000|CD_SLOT))==(CD_END|0x10000|CD_SLOT));
    assert(h->ic.raw&(1u<<14));assert(h->cd_sectors==2&&h->ser[0x24/4]==base+4096);
    assert(h->ser[0x20/4]==0x1000000); /* an unlimited read still stops */
    for(unsigned i=0;i<4096;i++)assert(h->ram[0x11000+i]==0xa5);
    cd_release(h,2);ser_write(h,0xc4000064,~0u);
    /* Moving the guard and rearming consumes the untouched tail, stopping
     * as the write address wraps to the protected first sector. */
    ser_write(h,0xc4000080,base);ser_write(h,0xc4000040,0x14);
    cd_deliver(h,2);cd_release(h,1);cd_deliver(h,3);
    assert(!(h->ser[0x40/4]&4)&&h->ser[0x24/4]==base);
    assert(h->ram[0x10000]==0x40&&h->ram[0x10800]==0x41);
    assert(h->ram[0x11000]==0x42&&h->ram[0x11800]==0x43);
    cd_release(h,2);ser_write(h,0xc4000064,~0u);
    /* With automatic stop disabled the same mark only notifies. */
    ser_write(h,0xc4000084,0);ser_write(h,0xc4000080,base+2048);ser_write(h,0xc4000040,0x14);
    cd_deliver(h,4);
    assert(h->ser[0x40/4]&4);assert(h->ser[0x64/4]&0x10000);assert(!(h->ser[0x64/4]&CD_END));
    assert(h->ram[0x10000]==0x44);cd_release(h,2);ser_write(h,0xc4000064,~0u);
    ser_write(h,0xc4000084,1);ser_write(h,0xc4000080,base+4096);ser_write(h,0xc4000020,3);
    cd_deliver(h,5);
    assert(!(h->ser[0x40/4]&4)&&h->ser[0x20/4]==2); /* preserve unread transfer length */
    assert(h->ram[0x11000]==0x42);
    /* Decoder cadence uses the same programmable rate as pickup motion;
     * in particular, fractional CAV rates must not truncate to integers. */
    cdsp_reset(&h->dsp,NULL);cpu.cycles=0;h->cd_next_us=0;
    cdsp_command(&h->dsp,0,32,0x9F209000);
    cdsp_command(&h->dsp,0,32,0xD0C00040);
    cdsp_command(&h->dsp,0,32,0xE6670000);
    cd_tick(h);assert(h->cd_next_us==3333);
    cdsp_command(&h->dsp,0,20,0xD0D80);h->cd_next_us=0;
    cd_tick(h);assert(h->cd_next_us==5333);
    cdsp_command(&h->dsp,0,32,0xE6000000);
    cdsp_command(&h->dsp,0,32,0x9B009000);h->cd_next_us=0;
    cd_tick(h);assert(h->cd_next_us==13333);
    hw_destroy(h);fclose(cd.fp);
    puts("PASS: CD ring guard stops before protected data, wraps, resumes, and supports notification-only marks");return 0;
}
