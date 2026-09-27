/* Real SDL clock/callback integration, using silent PCM only. */
#define SDL_MAIN_HANDLED
#include "../src/frame_pacer.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO) == 0);
    Audio *a = audio_create();
    assert(audio_init_sdl(a));
    FramePacer p;
    frame_pacer_reset(&p);
    uint32_t start = SDL_GetTicks(), peak = 0;
    int16_t silence[1470] = {0};
    for (unsigned i=0; i<180; i++) {
        audio_push_samples(a, silence, 1470);
        frame_pacer_wait(&p, a);
        uint32_t queued = audio_available(a);
        if (queued > peak) peak = queued;
        assert(queued <= 4410);
    }
    uint32_t elapsed = SDL_GetTicks() - start;
    printf("180 paced frames: %u ms, peak queue %u samples\n", elapsed, peak);
    assert(elapsed >= 2990 && elapsed < 3400);

    /* Device stops consuming: backpressure must time out, flush, and return. */
    SDL_PauseAudioDevice(a->device, 1);
    audio_clear(a);
    for (unsigned i=0; i<8; i++) audio_push_samples(a,silence,1470);
    frame_pacer_reset(&p); start = SDL_GetTicks();
    frame_pacer_wait(&p,a);
    assert(SDL_GetTicks()-start < 500 && audio_available(a)==0);

    /* Accelerated boot cannot accumulate audio even with a paused device. */
    audio_set_discard(a,1);
    for (unsigned i=0;i<10000;i++) audio_push_samples(a,silence,1470);
    assert(audio_available(a)==0);
    audio_set_discard(a,0);
    audio_push_samples(a,silence,1470);
    assert(audio_available(a)==1470);
    SDL_PauseAudioDevice(a->device,0);
    SDL_Delay(100);
    assert(audio_available(a)==0);
    audio_destroy(a); SDL_Quit();
    puts("PASS: pacing, bounded queue, stalled device, and boot-to-playback transition");
}
