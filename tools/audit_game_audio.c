/* Fresh-disc audio audit. Usage: disc.cue output-prefix [frames]. */
#include "../src/vflash.h"
#include "../src/audio.h"
#include <assert.h>
#include <stdlib.h>
static void le16(FILE*f,unsigned v){fputc(v,f);fputc(v>>8,f);}
static void le32(FILE*f,unsigned v){le16(f,v);le16(f,v>>16);}
int main(int argc,char**argv) {
    assert(argc>=3);unsigned limit=argc>3?(unsigned)atoi(argv[3]):6500;
    vflash_set_bios_dir("..");VFlash*v=vflash_create(argv[1]);assert(v);
    Audio*a=vflash_get_audio(v);audio_init_external(a);audio_set_volume(a,256);
    char path[1024];snprintf(path,sizeof path,"%s.wav",argv[2]);FILE*f=fopen(path,"wb+");assert(f);
    fwrite("RIFF",1,4,f);le32(f,0);fwrite("WAVEfmt ",1,8,f);le32(f,16);
    le16(f,1);le16(f,2);le32(f,44100);le32(f,176400);le16(f,4);le16(f,16);fwrite("data",1,4,f);le32(f,0);
    unsigned count=0,nz=0,game_nz=0,game_frames=0,peak=0,first=0;
    unsigned modes[512]={0};double square=0;
    unsigned asset_addresses[128]={0},asset_count=0;int ram_saved=0;
    for(unsigned frame=0;frame<limit;frame++) {
        unsigned buttons=frame>=2400&&frame<2410?VFLASH_BTN_ENTER:0;
        for(unsigned at=3700,k=0;at<limit;at+=300,k++) {
            if(frame>=at&&frame<at+10)buttons|=VFLASH_BTN_ENTER;
            if((k+1)%4==0&&frame>=at-60&&frame<at-50)buttons|=VFLASH_BTN_DOWN;
        }
        vflash_set_input(v,buttons);vflash_run_frame(v);
        unsigned game=0;
        for(unsigned voice=0;voice<64;voice++) {
            unsigned base=0xb0000000+voice*64,mode=vflash_read32(v,base+12);
            if(getenv("VFLASH_AUDIO_ASSETS") && (mode==2 || mode==0x12 || mode==0x1ea)) {
                unsigned start=vflash_read32(v,base),end=vflash_read32(v,base+4),known=0;
                for(unsigned k=0;k<asset_count;k++)if(asset_addresses[k]==start)known=1;
                if(!known && asset_count<128 && start>=0x10000000 && end>=start && end<0x11000000) {
                    asset_addresses[asset_count++]=start;
                    unsigned bytes=end-start+(mode==0x1ea?1:2);
                    snprintf(path,sizeof path,"%s-%03x-%08x.bin",argv[2],mode,start);
                    FILE*asset=fopen(path,"wb");assert(asset);
                    for(unsigned k=0;k<bytes;k++)fputc(vflash_read32(v,(start+k)&~3u)>>(((start+k)&3)*8),asset);
                    fclose(asset);printf("ASSET mode=%03x start=%08x bytes=%u frame=%u\n",mode,start,bytes,frame);
                    if(mode==2 && !ram_saved) {
                        snprintf(path,sizeof path,"%s.ram",argv[2]);asset=fopen(path,"wb");assert(asset);
                        for(unsigned p=0x10000000;p<0x11000000;p+=4)le32(asset,vflash_read32(v,p));
                        fclose(asset);ram_saved=1;
                    }
                }
            }
            if(vflash_read32(v,base+0x34)&0x70000) {
                if(mode<512)modes[mode]++;
                if(mode!=0x1e4)game=1;
            }
        }
        int16_t samples[4096];unsigned n=audio_pull_samples(a,samples,4096);
        if(frame>=1800) {
            for(unsigned i=0;i<n;i++) {
                unsigned mag=samples[i]<0?-(int)samples[i]:samples[i];
                le16(f,(uint16_t)samples[i]);nz+=samples[i]!=0;
                if(game)game_nz+=samples[i]!=0;
                if(mag>peak)peak=mag;square+=(double)samples[i]*samples[i];
            }
            count+=n;if(game){game_frames++;if(!first){first=frame;printf("Game audio active at frame %u\n",frame);}}
        }
        if(frame==limit-1||frame==4500||frame==5500) {
            snprintf(path,sizeof path,"%s-%u.ppm",argv[2],frame);FILE*shot=fopen(path,"wb");assert(shot);
            int w,h;vflash_get_screen_size(v,&w,&h);uint32_t*p=vflash_get_framebuffer(v);
            fprintf(shot,"P6\n%d %d\n255\n",w,h);
            for(int i=0;i<w*h;i++){fputc(p[i]>>16,shot);fputc(p[i]>>8,shot);fputc(p[i],shot);}fclose(shot);
        }
    }
    fseek(f,4,SEEK_SET);le32(f,36+count*2);fseek(f,40,SEEK_SET);le32(f,count*2);fclose(f);
    printf("AUDIT frames=%u stereo_samples=%u nonzero=%u game_nonzero=%u game_active_frames=%u first_game_frame=%u peak=%u mean_square=%.2f\n",limit,count/2,nz,game_nz,game_frames,first,peak,count?square/count:0);
    for(unsigned i=0;i<512;i++)if(modes[i])printf("MODE %03X active_voice_frames=%u\n",i,modes[i]);
    vflash_destroy(v);return 0;
}
