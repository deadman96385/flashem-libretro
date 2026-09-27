/* Boot a real disc and record the actual frontend Audio ring after music starts. */
#include "../src/vflash.h"
#include "../src/audio.h"
#include <assert.h>
#include <stdlib.h>
static void le16(FILE*f,unsigned v){fputc(v,f);fputc(v>>8,f);}
static void le32(FILE*f,unsigned v){le16(f,v);le16(f,v>>16);}
int main(int argc,char**argv) {
    VFlash*v;Audio*a;FILE*f;int16_t samples[4096];unsigned frame,n,i,voice;
    unsigned started=0,frames_recorded=0,count=0,nonzero=0,peak=0;double square=0;
    if(argc!=3)return 2;
    vflash_set_bios_dir("..");v=vflash_create(argv[1]);assert(v);
    a=vflash_get_audio(v);audio_init_external(a);audio_set_volume(a,256);
    f=fopen(argv[2],"wb+");assert(f);
    fwrite("RIFF",1,4,f);le32(f,0);fwrite("WAVEfmt ",1,8,f);le32(f,16);
    le16(f,1);le16(f,2);le32(f,44100);le32(f,176400);le16(f,4);le16(f,16);
    fwrite("data",1,4,f);le32(f,0);
    for(frame=0;frame<5600&&frames_recorded<480;frame++) {
        vflash_set_input(v,((frame>=4200&&frame<4210)||(frame>=5000&&frame<5010))?VFLASH_BTN_ENTER:0);
        vflash_run_frame(v);n=audio_pull_samples(a,samples,4096);
        if(!started)for(voice=16;voice<64;voice++) {
            unsigned base=0xb0000000+voice*64;
            if(vflash_read32(v,base+12)==0&&(vflash_read32(v,base+0x34)&0x70000)) {
                started=1;printf("Music capture begins at frame %u voice %u\n",frame,voice);break;
            }
        }
        if(started) {
            for(i=0;i<n;i++) {
                unsigned mag=samples[i]<0?-(int)samples[i]:samples[i];
                le16(f,(uint16_t)samples[i]);nonzero+=samples[i]!=0;
                if(mag>peak)peak=mag;
                square+=(double)samples[i]*samples[i];
            }
            count+=n;frames_recorded++;
        }
    }
    fseek(f,4,SEEK_SET);le32(f,36+count*2);fseek(f,40,SEEK_SET);le32(f,count*2);fclose(f);
    printf("Captured stereo frames=%u nonzero samples=%u peak=%u mean_square=%.1f video_frames=%u\n",count/2,nonzero,peak,count?square/count:0,frames_recorded);
    vflash_destroy(v);assert(started&&nonzero>10000&&peak>100);return 0;
}
