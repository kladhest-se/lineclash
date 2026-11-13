/* Procedural sound effects mixed with the stereo streaming music player. */
#include "client.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SAMPLE_RATE     44100
#define CHANNELS        2
#define AUDIO_BUF       512
/* Large enough for the longest sound (GAMEOVER at 0.5s = 22050 samples) */
#define SFX_MAX_SAMPLES 24000

typedef struct {
    float   samples[SFX_MAX_SAMPLES];
    int     len;
    int     pos;
    int     active;
} SfxVoice;

static SDL_AudioDeviceID g_dev   = 0;
static SfxVoice          g_voice;
static float             g_sfx[SFX_COUNT][SFX_MAX_SAMPLES];
static int               g_sfx_len[SFX_COUNT];
static float g_sfx_volume = 0.16f;

/* Generate into buf[0..SFX_MAX_SAMPLES-1], returns number of samples written. */
static int gen_tone(float *buf, int buf_max,
                    float freq, float dur_s, float vol,
                    int waveform,   /* 0=sine 1=square 2=noise */
                    float freq2, float sweep) {
    int i, n = (int)(dur_s * SAMPLE_RATE);
    if (n > buf_max) n = buf_max;
    for (i = 0; i < n; i++) {
        float t   = (float)i / SAMPLE_RATE;
        float f   = freq + (freq2 - freq) * sweep * t / dur_s;
        float env = 1.0f - (float)i / n;
        float v;
        if (waveform == 1) {
            v = (sinf(2.0f * 3.14159f * f * t) >= 0) ? 1.0f : -1.0f;
        } else if (waveform == 2) {
            v = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        } else {
            v = sinf(2.0f * 3.14159f * f * t);
        }
        buf[i] = v * vol * env;
    }
    return n;
}

static void gen_sfx(SfxId id) {
    float *b  = g_sfx[id];
    int    bmax = SFX_MAX_SAMPLES;
    int    total = 0;

    switch (id) {
    case SFX_MOVE:
        total = gen_tone(b, bmax, 300, 0.04f, 0.3f, 0, 300, 0);
        break;
    case SFX_ROTATE:
        total = gen_tone(b, bmax, 440, 0.05f, 0.3f, 0, 550, 0);
        break;
    case SFX_LAND:
        total = gen_tone(b, bmax, 180, 0.08f, 0.5f, 1, 100, 1);
        break;
    case SFX_LINE_CLEAR:
        total = gen_tone(b, bmax, 600, 0.18f, 0.6f, 0, 1000, 1);
        break;
    case SFX_TETRIS: {
        /* Three-note jingle — each segment written directly into b with
           remaining capacity tracked so total never exceeds SFX_MAX_SAMPLES */
        int n1 = gen_tone(b,            bmax,          600,  0.08f, 0.6f, 0, 600,  0);
        int n2 = gen_tone(b + n1,       bmax - n1,     800,  0.08f, 0.6f, 0, 800,  0);
        int n3 = gen_tone(b + n1 + n2,  bmax - n1 - n2, 1000, 0.12f, 0.7f, 0, 1200, 1);
        total = n1 + n2 + n3;
        break;
    }
    case SFX_SPECIAL_SEND:
        total = gen_tone(b, bmax, 800, 0.10f, 0.5f, 1, 400, 1);
        break;
    case SFX_SPECIAL_RECV:
        total = gen_tone(b, bmax, 200, 0.14f, 0.6f, 2, 200, 0);
        break;
    case SFX_GAMEOVER:
        total = gen_tone(b, bmax, 300, 0.5f, 0.7f, 1, 60, 1);
        break;
    case SFX_WIN: {
        /* Ascending four-note arpeggio */
        float notes[] = {523, 659, 784, 1047};
        int k;
        for (k = 0; k < 4 && total < bmax; k++) {
            int nn = gen_tone(b + total, bmax - total,
                              notes[k], 0.12f, 0.7f, 0, notes[k] * 1.02f, 0);
            total += nn;
        }
        break;
    }
    case SFX_CHAT:
        /* Soft two-tone ping — unobtrusive */
        total  = gen_tone(b,       bmax,      880, 0.04f, 0.18f, 0, 880, 0);
        total += gen_tone(b+total, bmax-total,1100, 0.04f, 0.12f, 0,1100, 0);
        break;
    case SFX_TAUNT: {
        /* Silly descending "boing" */
        int n1 = gen_tone(b,       bmax,       600, 0.06f, 0.5f, 0, 200, 1);
        int n2 = gen_tone(b+n1,    bmax-n1,    300, 0.10f, 0.4f, 1, 150, 1);
        total = n1+n2;
        break;
    }
    case SFX_ROUND_START: {
        /* Three rising beeps — "ready, set, go!" feel */
        int n1 = gen_tone(b,             bmax,             440, 0.08f, 0.5f, 0, 440, 0);
        int n2 = gen_tone(b+n1,          bmax-n1,          554, 0.08f, 0.5f, 0, 554, 0);
        int n3 = gen_tone(b+n1+n2,       bmax-n1-n2,       659, 0.08f, 0.5f, 0, 659, 0);
        int n4 = gen_tone(b+n1+n2+n3,    bmax-n1-n2-n3,    880, 0.16f, 0.7f, 0, 940, 1);
        total = n1+n2+n3+n4;
        break;
    }
    case SFX_ROUND_END: {
        /* Short descending "round over" — two down notes then low thud */
        int n1 = gen_tone(b,       bmax,       660, 0.10f, 0.6f, 0, 600, 1);
        int n2 = gen_tone(b+n1,    bmax-n1,    440, 0.10f, 0.6f, 0, 380, 1);
        int n3 = gen_tone(b+n1+n2, bmax-n1-n2, 220, 0.20f, 0.7f, 1, 120, 1);
        total = n1+n2+n3;
        break;
    }
    default:
        total = 0;
        break;
    }
    g_sfx_len[id] = total;
}

/* SDL audio callback — called from audio thread */
static void sfx_callback(void *userdata, unsigned char *stream, int len) {
    int i, n = len / (CHANNELS * (int)sizeof(Sint16));
    Sint16 *out = (Sint16*)stream;
    (void)userdata;
    memset(stream, 0, len);
    /* SFX voice */
    if (g_voice.active) {
        for (i = 0; i < n && g_voice.pos < g_voice.len; i++, g_voice.pos++) {
            float v = g_voice.samples[g_voice.pos];
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            out[i*2] = out[i*2+1] = (Sint16)(v * 28000.0f * g_sfx_volume);
        }
        if (g_voice.pos >= g_voice.len) g_voice.active = 0;
    }
    music_mix(out,n);
}

void sound_set_sfx_volume(float v) {
    if(!isfinite(v)) return; if(v<0)v=0; if(v>1)v=1;
    if(g_dev) SDL_LockAudioDevice(g_dev);
    g_sfx_volume=v;
    if(g_dev) SDL_UnlockAudioDevice(g_dev);
}
float sound_get_sfx_volume(void) { return g_sfx_volume; }

int sound_init(void) {
    SDL_AudioSpec want, got;
    int i;
    memset(&want, 0, sizeof(want));
    want.freq     = SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = CHANNELS;
    want.samples  = AUDIO_BUF;
    want.callback = sfx_callback;

    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!g_dev) return 0;

    memset(&g_voice,0,sizeof(g_voice));
    music_engine_init();
    for (i = 0; i < SFX_COUNT; i++) gen_sfx((SfxId)i);
    SDL_PauseAudioDevice(g_dev, 0);
    return 1;
}

void sound_shutdown(void) {
    if(g_dev) {
        SDL_PauseAudioDevice(g_dev,1);
        music_engine_shutdown();
        SDL_CloseAudioDevice(g_dev); g_dev=0;
    }
}

void sound_play(ClientCtx *c, SfxId id) {
    int len;
    if (!c->sound_enabled || !g_dev) return;
    if (id < 0 || id >= SFX_COUNT) return;
    if(id==SFX_SPECIAL_RECV || id==SFX_GAMEOVER || id==SFX_WIN || id==SFX_ROUND_START || id==SFX_ROUND_END) music_duck();
    len = g_sfx_len[id];
    if (len <= 0 || len > SFX_MAX_SAMPLES) return;  /* safety guard */
    SDL_LockAudioDevice(g_dev);
    memcpy(g_voice.samples, g_sfx[id], (size_t)len * sizeof(float));
    g_voice.len    = len;
    g_voice.pos    = 0;
    g_voice.active = 1;
    SDL_UnlockAudioDevice(g_dev);
}
