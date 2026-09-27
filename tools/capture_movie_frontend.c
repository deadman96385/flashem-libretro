/* Fresh Multisports boot and real movie handshake. No injected emulated state.
 * Usage: capture_movie_frontend disc.cue output-prefix */
#include "../src/vflash.h"
#include "../src/audio.h"
#include <assert.h>
#include <stdlib.h>
static void le16(FILE *f,unsigned v){fputc(v,f);fputc(v>>8,f);}
static void le32(FILE *f,unsigned v){le16(f,v);le16(f,v>>16);}
static uint32_t input(unsigned f) {
    uint32_t b=f>=2400 && f<2410?VFLASH_BTN_ENTER:0;
    for(unsigned i=0,at=3700;at<9000;at+=300,i++) {
        if(f>=at && f<at+10)b|=VFLASH_BTN_ENTER;
        if((i+1)%4==0 && f>=at-60 && f<at-50)b|=VFLASH_BTN_DOWN;
    }
    return b;
}
int main(int argc,char **argv) {
    assert(argc==3);vflash_set_bios_dir("..");VFlash *v=vflash_create(argv[1]);assert(v);
    Audio *a=vflash_get_audio(v);audio_init_external(a);audio_set_volume(a,256);
    char path[1024];snprintf(path,sizeof path,"%s.wav",argv[2]);FILE *wav=fopen(path,"wb+");assert(wav);
    fwrite("RIFF",1,4,wav);le32(wav,0);fwrite("WAVEfmt ",1,8,wav);le32(wav,16);
    le16(wav,1);le16(wav,2);le32(wav,44100);le32(wav,176400);le16(wav,4);le16(wav,16);
    fwrite("data",1,4,wav);le32(wav,0);
    unsigned captured=0,last=0,max=0,audio_samples=0,nonzero=0;int begun=0;
    for(unsigned f=0;f<9000 && captured<120;f++) {
        vflash_set_input(v,begun?0:input(f));vflash_run_frame(v);
        uint32_t message=vflash_read32(v,0x1083c080),movie_frame=vflash_read32(v,0x1083c09c);
        if(message==0x92 && movie_frame>=1 && movie_frame<1186) {
            if(!begun)printf("Movie begins at frontend frame %u\n",f);
            begun=1;if(movie_frame>max)max=movie_frame;
            if(movie_frame!=last){printf("Native frame %u at frontend frame %u\n",movie_frame,f);last=movie_frame;}
        }
        int16_t samples[4096];unsigned n=audio_pull_samples(a,samples,4096);
        if(!begun)continue;
        if(captured==40) {
            snprintf(path,sizeof path,"%s.ram",argv[2]);FILE *dump=fopen(path,"wb");assert(dump);
            for(uint32_t p=0x10000000;p<0x11000000;p+=4)le32(dump,vflash_read32(v,p));fclose(dump);
            snprintf(path,sizeof path,"%s.ve",argv[2]);dump=fopen(path,"wb");assert(dump);
            for(uint32_t p=0xb8000000;p<0xb8000800;p+=4)le32(dump,vflash_read32(v,p));fclose(dump);
        }
        for(unsigned i=0;i<n;i++){le16(wav,(uint16_t)samples[i]);nonzero+=samples[i]!=0;}
        audio_samples+=n;
        int w,h;vflash_get_screen_size(v,&w,&h);uint32_t *pixels=vflash_get_framebuffer(v);
        snprintf(path,sizeof path,"%s-%03u.ppm",argv[2],captured++);
        FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n%d %d\n255\n",w,h);
        for(int i=0;i<w*h;i++){fputc(pixels[i]>>16,out);fputc(pixels[i]>>8,out);fputc(pixels[i],out);}fclose(out);
    }
    fseek(wav,4,SEEK_SET);le32(wav,36+2*audio_samples);fseek(wav,40,SEEK_SET);le32(wav,2*audio_samples);fclose(wav);
    printf("Frontend movie capture: %u video frames, maximum native frame %u, %u nonzero audio samples\n",captured,max,nonzero);
    vflash_destroy(v);assert(captured==120 && max>=20 && nonzero>1000);
}
