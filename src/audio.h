#pragma once
#include <stdint.h>
#include <stdatomic.h>

/* V.Flash Audio
 * .snd files are PCM WAV
 * ZEVIO 1020 dedicated MIDI/sample core feeds the hardware PCM stream
 * Output: 16-bit stereo, 44100Hz
 */

#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2
#define AUDIO_BUF_SAMPLES  1024

typedef struct {
    int16_t  *buf;          /* Ring buffer */
    uint32_t  buf_size;     /* In samples */
    _Atomic uint32_t write_pos; /* producer publishes complete stereo frames */
    _Atomic uint32_t read_pos;
    int       initialized;
    uint32_t  device;      /* SDL device ID, zero for external output */
    int       discard;     /* producer-only: accelerated frames are inaudible */
    _Atomic uint32_t volume; /* 0-256, shared with SDL callback */
} Audio;

Audio*  audio_create(void);
void    audio_destroy(Audio *a);
int     audio_init_sdl(Audio *a);
void    audio_push_samples(Audio *a, const int16_t *samples, uint32_t count);
void    audio_set_volume(Audio *a, uint32_t vol);
/* Call from the producer thread. Flushes safely against the SDL consumer. */
void    audio_clear(Audio *a);
void    audio_set_discard(Audio *a, int discard);

/* Frontends that do their own output: mark the pipeline live, then drain it.
 * Push/pull preserve stereo pairs; odd trailing input/capacity is ignored. */
void     audio_init_external(Audio *a);
uint32_t audio_available(const Audio *a);
uint32_t audio_pull_samples(Audio *a, int16_t *out, uint32_t max);

/* .snd file decoder (PCM WAV) */
int     snd_decode(const uint8_t *data, uint32_t size,
                   int16_t **out_samples, uint32_t *out_count, uint32_t *out_rate);

/* Legacy WAV-style IMA helper (unused by the emulated hardware).
 * Actual MJP/effect Apple IMA4 decoding is in midi.c.
 * Data format: 4-byte header (predictor + step_index) + nibble-packed ADPCM.
 * Decodes to mono 22050Hz, auto-upsampled to 44100Hz stereo. */
void    audio_decode_ima_adpcm(Audio *a, const uint8_t *data, uint32_t size);

/* DMA audio queue: raw PCM from RAM (called from I/O write handler)
 * stereo: 1=stereo, 0=mono
 * s16:    1=16-bit signed, 0=8-bit unsigned */
void    audio_queue_pcm(Audio *a, const uint8_t *data, uint32_t size,
                        int stereo, int s16);
