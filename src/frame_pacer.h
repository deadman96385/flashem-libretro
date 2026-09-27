#pragma once
#include <SDL2/SDL.h>
#include "audio.h"

typedef struct {
    double frequency, deadline;
} FramePacer;

static void frame_pacer_reset(FramePacer *p) {
    p->frequency = (double)SDL_GetPerformanceFrequency();
    p->deadline = (double)SDL_GetPerformanceCounter();
}

/* Main-thread pacing, independent of the display refresh rate. */
static void frame_pacer_wait(FramePacer *p, Audio *audio) {
    double period = p->frequency / 60.0;
    p->deadline += period;
    double now = (double)SDL_GetPerformanceCounter();
    /* Bound catch-up after slow rendering or a scheduler stall. */
    if (now > p->deadline + period) p->deadline = now;
    while (now < p->deadline) {
        double ms = (p->deadline - now) * 1000.0 / p->frequency;
        SDL_Delay(ms > 1.0 ? (uint32_t)(ms - 1.0) : 0);
        now = (double)SDL_GetPerformanceCounter();
    }
    /* Hardware clocks drift. Backpressure limits latency to three frames.
     * Bound the wait so a stalled device cannot hang the event loop. */
    uint32_t start = SDL_GetTicks();
    const uint32_t max_queue = (AUDIO_SAMPLE_RATE / 60) * AUDIO_CHANNELS * 3;
    while (audio && audio->device && audio_available(audio) > max_queue) {
        SDL_Delay(1);
        if (SDL_GetTicks() - start >= 100) {
            audio_clear(audio);
            break;
        }
    }
    /* Keep the fractional schedule through short callback waits. Resetting
     * it on every wait would accumulate callback jitter as permanent drift. */
    now = (double)SDL_GetPerformanceCounter();
    if (now > p->deadline + period) p->deadline = now;
}
