/* Sony CAV-W command decoding and elapsed-time/clock transitions. */
#include "../src/cdsp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void close_to(double actual, double expected) {
    assert(fabs(actual-expected)<0.000001);
}
int main(void) {
    CDSP d;cdsp_reset(&d,NULL);
    cdsp_command(&d,0,32,0x9B009000);close_to(cdsp_speed(&d),1);
    cdsp_command(&d,0,32,0x9F209000);close_to(cdsp_speed(&d),2);
    cdsp_command(&d,0,32,0xD0C00040);
    close_to(cdsp_speed(&d),2); /* VP has no effect in crystal CLV mode. */
    cdsp_command(&d,0,32,0xE6670000);close_to(cdsp_speed(&d),4);
    d.focused=1;d.tmode=4;d.lba=0;
    cdsp_advance(&d,1000000);close_to(d.lba,300);
    /* Advance time at the old rate before applying a new velocity. */
    cdsp_command(&d,2000000,20,0xD0D00);close_to(d.lba,600);
    close_to(cdsp_speed(&d),3);
    cdsp_advance(&d,3000000);close_to(d.lba,825);
    cdsp_command(&d,3000000,20,0xD0D80);close_to(cdsp_speed(&d),2.5);
    cdsp_advance(&d,4000000);close_to(d.lba,1012.5);
    cdsp_command(&d,4000000,20,0xD0F04);close_to(cdsp_speed(&d),2);
    cdsp_command(&d,4000000,16,0xE665);close_to(cdsp_speed(&d),2);
    cdsp_command(&d,4000000,8,0xD0);close_to(cdsp_speed(&d),2);
    /* Audio/ordinary CLV returns to DSPB's rate, irrespective of VP. */
    cdsp_command(&d,4000000,32,0xE6000000);
    cdsp_command(&d,4000000,32,0x9B009000);close_to(cdsp_speed(&d),1);
    cdsp_advance(&d,5000000);close_to(d.lba,1087.5);
    /* The two observed retail clock profiles select the same movie rate. */
    cdsp_command(&d,5000000,32,0x9F209000);
    cdsp_command(&d,5000000,32,0xAE001140);
    cdsp_command(&d,5000000,32,0xD1E00040);
    cdsp_command(&d,5000000,32,0xE6650000);close_to(cdsp_speed(&d),4);
    cdsp_command(&d,5000000,32,0xAE00200F);close_to(cdsp_speed(&d),4);
    cdsp_command(&d,5000000,32,0xAE0011D5);
    cdsp_command(&d,5000000,32,0xD0C00040);
    cdsp_command(&d,5000000,32,0xE6670000);close_to(cdsp_speed(&d),4);
    cdsp_reset(&d,NULL);close_to(cdsp_speed(&d),1);
    puts("PASS: CAV-W 4x, fractional rates, multiplier, clock transitions and CLV audio");
    return 0;
}
