/* Ring overflow must preserve stereo; libretro must retain partial batches. */
#include "../src/flashem_libretro.c"
#include <assert.h>
#include <pthread.h>
static int16_t received[32];static unsigned received_count,accept;
static size_t batch(const int16_t*p,size_t frames) {
    if(frames>accept)frames=accept;
    memcpy(received+received_count,p,frames*4);received_count+=(unsigned)frames*2;return frames;
}
static void single(int16_t l,int16_t r){received[received_count++]=l;received[received_count++]=r;}
static atomic_int done;
static void*producer(void*ctx) {
    Audio*a=ctx;for(int i=1;i<=50000;i++) {int16_t p[2]={(int16_t)(i%30000+1),(int16_t)-(i%30000+1)};audio_push_samples(a,p,2);}
    atomic_store(&done,1);return NULL;
}
int main(void) {
    Audio*a=audio_create();audio_set_volume(a,256);a->buf_size=8;
    int16_t initial[]={1,101,2,102,3,103,4,104},more[]={5,105,6,106,7},out[254];
    audio_push_samples(a,initial,8);assert(audio_available(a)==6);
    assert(audio_pull_samples(a,out,3)==2 && out[0]==1 && out[1]==101);
    audio_push_samples(a,more,2);assert(audio_pull_samples(a,out,8)==6);
    const int16_t expected[]={2,102,3,103,5,105};assert(!memcmp(out,expected,sizeof expected));
    audio_push_samples(a,more+2,3);assert(audio_available(a)==2);
    assert(audio_pull_samples(a,out,1)==0);assert(audio_pull_samples(a,out,8)==2 && out[0]==6&&out[1]==106);
    audio_push_samples(a,initial,6);audio_batch_cb=batch;accept=0;
    flashem_audio_output(a);assert(!received_count && s_audio_count==6 && !s_audio_offset);
    audio_push_samples(a,more,2);accept=1;flashem_audio_output(a);assert(received_count==2);
    accept=8;flashem_audio_output(a);assert(received_count==6 && !memcmp(received,initial,12));
    flashem_audio_output(a);assert(received_count==8 && received[6]==5 && received[7]==105);
    audio_batch_cb=NULL;audio_cb=single;audio_push_samples(a,more+2,2);flashem_audio_output(a);
    assert(received_count==10 && received[8]==6 && received[9]==106);
    a->buf_size=64;
    const uint8_t high[]={255,127,88,0,0x77},low[]={0,128,88,0,0xff};
    audio_decode_ima_adpcm(a,high,sizeof high);assert(audio_pull_samples(a,out,254)==8);
    for(unsigned i=0;i<8;i++)assert(out[i]==32767);
    audio_decode_ima_adpcm(a,low,sizeof low);assert(audio_pull_samples(a,out,254)==8);
    for(unsigned i=0;i<8;i++)assert(out[i]==-32768);
    pthread_t thread;atomic_init(&done,0);assert(!pthread_create(&thread,NULL,producer,a));
    unsigned total=0;
    while(!atomic_load(&done)||audio_available(a)) {
        unsigned n=audio_pull_samples(a,out,254);assert(!(n&1));total+=n;
        for(unsigned i=0;i<n;i+=2)assert(out[i]>0 && out[i+1]==-out[i]);
    }
    assert(!pthread_join(thread,NULL));assert(total>0);
    audio_push_samples(a,initial,8);assert(audio_available(a)==8);
    audio_set_discard(a,1);assert(audio_available(a)==0);
    for(unsigned i=0;i<10000;i++)audio_push_samples(a,initial,8);
    assert(audio_available(a)==0);
    audio_set_discard(a,0);audio_push_samples(a,initial,8);
    assert(audio_pull_samples(a,out,8)==8 && !memcmp(out,initial,sizeof initial));
    audio_push_samples(a,initial,8);audio_clear(a);assert(audio_available(a)==0);
    audio_destroy(a);
    puts("PASS: overflow/wrap stereo pairs, partial/zero batch acceptance, single callback and concurrent queue");
}
