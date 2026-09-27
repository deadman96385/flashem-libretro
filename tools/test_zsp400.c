#include "../src/zsp400.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t p[65536],d[65536];
static uint16_t rd(void *ctx,uint16_t a,int prog) { (void)ctx; return (prog?p:d)[a]; }
static void wr(void *ctx,uint16_t a,uint16_t v,int prog) { (void)ctx; (prog?p:d)[a]=v; }
static void one(ZSP400 *s,uint16_t w) { p[s->pc]=w; assert(zsp400_step(s)); }
int main(void) {
    ZSP400 s={0}; s.read=rd; s.write=wr; zsp400_reset(&s);
    assert(s.pc==0xf800);
    s.r[4]=0xabcd; one(&s,0x2412); assert(s.r[4]==0xab12);
    one(&s,0x3434); assert(s.r[4]==0x3412);
    /* SUB example in manual8-163: signed overflow but positive wrapped result. */
    s.r[13]=0x8f34;s.r[14]=0x7734;one(&s,0x86de);
    assert(s.r[13]==0x1800 && (s.flags&0x578)==0x570);
    /* CMP signed ordering must not use the wrapped subtraction's sign. */
    s.r[4]=0x8000;s.r[5]=1;one(&s,0x8145);assert(!(s.flags&32));
    /* Logical instructions clear carry/overflow, preserve sticky overflow. */
    s.r[4]=0xffff;s.r[5]=0;one(&s,0x8845);
    assert((s.flags&0x578)==0x128);
    s.c[0]=16;s.r[4]=0x7fff;s.r[5]=1;one(&s,0x8045);assert(s.r[4]==0x7fff);
    s.c[0]=0;
    s.r[4]=1;one(&s,0x8764);assert(s.r[6]==14);
    s.r[4]=0xffff;one(&s,0x8764);assert(s.r[6]==15);
    s.r[4]=0xffff;s.r[5]=0;s.r[7]=0x1234;one(&s,0x9764);
    assert(s.r[6]==15 && s.r[7]==0x1234);
    s.r[4]=1;s.r[5]=0;s.r[13]=16;one(&s,0x924d);assert(s.r[4]==0 && s.r[5]==1);
    s.r[4]=0x8000;one(&s,0x8c64);assert(s.r[6]==0x7fff);
    s.r[4]=0x8f34;s.r[5]=0x7734;one(&s,0x8d45);
    assert(s.r[4]==0x8f34 && (s.flags&0x478)==0x40);
    s.r[3]=0x8000;s.r[4]=0x8020;one(&s,0xe434);
    assert(s.r[0]==0 && s.r[1]==0x3ff0);
    /* Manual MAC.A example: q15 plus post-accumulation rounding. */
    s.r[0]=s.r[1]=s.c[7]=0;s.c[0]=5;s.r[6]=0x6b85;s.r[8]=0x2b85;
    one(&s,0x5068);assert(s.r[0]==0xe632 && s.r[1]==0x248e && s.c[7]==0);
    s.c[0]=0;
    /* Circular buffer1: begin selector20, end selector22; split double. */
    s.c[15]=4;s.c[20]=0x500;s.c[22]=0x504;s.r[15]=0x503;
    d[0x503]=0x1234;d[0x500]=0xabcd;one(&s,0x784f);
    assert(s.r[4]==0x1234 && s.r[5]==0xabcd && s.r[15]==0x501);
    s.c[15]=0;
    s.pc=0x100;one(&s,0x1ffe);assert(s.pc==0xfc && s.c[17]==0x101);
    one(&s,0xbf01);assert(s.pc==0x101);
    one(&s,0x0ffe);assert(s.pc==0xff);
    /* Downward doubles use [pointer-1,pointer], then decrement by2. */
    s.r[4]=0x1234;s.r[5]=0xabcd;s.r[12]=0x800;one(&s,0x6b4c);
    assert(d[0x7ff]==0x1234 && d[0x800]==0xabcd && s.r[12]==0x7fe);
    s.r[12]++;one(&s,0x784c);assert(s.r[4]==0x1234 && s.r[5]==0xabcd && s.r[12]==0x801);
    s.c[5]=0;s.pc=0x200;one(&s,0x4eff);assert(s.pc==0x201 && s.c[5]==0xffff);
    s.r[2]=0x1234;s.r[0]=0x1000;one(&s,0xbaf0);assert(s.r[2]==0);
    s.r[2]=0x5678;s.r[0]=0;one(&s,0xbaf0);assert(s.r[2]==0x1234);
    /* Masked request survives; IRQ4 uses firmware's0x60 vector and RETI. */
    s.pc=0x300;s.c[15]=128;zsp400_request_irq(&s,4);one(&s,0xcf00);
    assert(s.pc==0x301 && (s.c[9]&16));
    s.c[2]=0x8010;p[0x60]=0xcf00;assert(zsp400_step(&s));
    assert(s.pc==0x61 && s.c[18]==0x301 && !(s.c[9]&16));
    one(&s,0xbf02);assert(s.pc==0x301 && (s.c[2]&0x8000));
    /* Illegal instruction is a visible stop, not fabricated forward progress. */
    p[s.pc]=0xf000;assert(!zsp400_step(&s));assert(s.fault && s.fault_pc==0x301);
    puts("PASS ZSP400 architectural bootstrap cases");
}
