#include "../src/midi.h"
#include <assert.h>
#include <stdio.h>

static unsigned calls, last_voice;
static uint32_t last_sample;
static int sample_read(void *ctx, uint32_t addr, int16_t *out) {
    static const int16_t samples[] = {1000, -2000, 3000};
    (void)ctx;
    if (addr < 0x1000 || addr > 0x1004 || (addr & 1)) return 0;
    *out = samples[(addr - 0x1000) / 2]; return 1;
}
static void trace(void *ctx, unsigned v, const uint32_t *r, uint32_t pc, int on) {
    (void)ctx; (void)on;
    assert(pc == 0x1008ba54);
    calls++; last_voice = v; last_sample = r[0];
}
int main(void) {
    Midi m;
    midi_reset(&m);
    /* Initialization ACK must not create pending interrupts. */
    midi_write(&m, 0x1004, 0x3f, 0);
    assert(midi_read(&m, 0x1004) == 0);
    /* Inject pending sources independently of the firmware write path. */
    m.regs[0x1004 / 4] = 0x25;
    midi_write(&m, 0x1004, 0x12340204, 0);
    assert(midi_read(&m, 0x1004) == 0x12340221);
    midi_write(&m, 0x1004, 0x200, 0);
    assert(midi_read(&m, 0x1004) == 0x221);
    midi_write(&m, 0x1004, 0x23f, 0);
    assert(midi_read(&m, 0x1004) == 0x200);
    midi_set_trace(&m, NULL, trace);
    /* Reproduce BIOS flush order and upper-bank channel addressing. */
    midi_write(&m, 63 * 64, 0x10203040, 0);
    midi_write(&m, 0x1014, 0x80000001, 0x1008ba54);
    assert(calls == 2 && last_voice == 63 && last_sample == 0x10203040);
    assert(m.requested == ((UINT64_C(1) << 63) | (UINT64_C(1) << 32)));
    assert(m.unsupported_starts == 2);
    assert(midi_read(&m, 0x1014) == 0);
    midi_write(&m, 0x101c, 0x80000000, 0x1008ba54);
    assert(m.requested == (UINT64_C(1) << 32));
    /* Unsupported playback must not falsely claim an active decoder. */
    midi_write(&m, 0x34, 0x1237abcd, 0);
    assert(midi_read(&m, 0x34) == 0x1230abcd);
    midi_write(&m, 0x1188, 0x7fff7fff, 0);
    assert(midi_read(&m, 0x1188) == 0x7fff7fff);
    midi_write(&m, 0xffffffff, 123, 0);
    assert(midi_read(&m, 0xffffffff) == 0);
    midi_reset(&m);
    assert(m.requested == 0 && m.trace == NULL);
    {
        int16_t out[10]; unsigned j;
        const uint32_t setup[] = {0x1000,0x1004,0x1000,0x10,0x1000,
            0xffffffff,0xffff0000,0xffffffff,0xff,0};
        for(j=0;j<10;j++) midi_write(&m,j*4,setup[j],0);
        midi_set_memory(&m,NULL,sample_read);
        midi_write(&m,0x1010,1,0);
        assert(midi_read(&m,0x34)&0x70000);
        midi_render(&m,out,5);
        assert(out[0]==1000&&out[2]==-2000&&out[4]==3000&&out[6]==1000&&out[8]==-2000);
        assert(out[1]==0&&out[9]==0&&midi_read(&m,0x28)==0x1004);
        midi_write(&m,0x20,0xff00,0);
        midi_render(&m,out,1);
        assert(out[0]==0&&out[1]==3000);
        midi_write(&m,0x1018,1,0);
        midi_render(&m,out,1);
        assert(out[0]==0&&out[1]==0&&!(midi_read(&m,0x34)&0x70000));
        midi_write(&m,0xc,0x11,0); /* unknown format must remain silent */
        midi_write(&m,0x1010,1,0);
        assert(m.active==0&&m.unsupported_starts==1);
        midi_write(&m,0xc,0x10,0);
        midi_write(&m,0,0x2000,0);
        midi_write(&m,4,0x2004,0);
        midi_write(&m,8,0x2000,0);
        midi_write(&m,0x1010,1,0);
        midi_render(&m,out,1);
        assert(m.active==0&&out[0]==0&&out[1]==0);
        midi_write(&m,0,0x1000,0);midi_write(&m,4,0x1002,0);
        midi_write(&m,8,0x1000,0);midi_write(&m,0xc,0x1e4,0);
        midi_write(&m,0x20,0xffff,0);midi_write(&m,0x1010,1,0);
        midi_render(&m,out,5);
        assert(out[0]==1000&&out[1]==1000&&out[2]==1000&&out[3]==1000);
        assert(out[4]==-2000&&out[5]==-2000&&out[6]==-2000&&out[7]==-2000);
        assert(out[8]==0&&out[9]==0&&m.active==0);
    }
    puts("midi register/capture and narrow BIOS streaming tests passed");
    return 0;
}
