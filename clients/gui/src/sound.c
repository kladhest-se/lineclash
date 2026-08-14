/* sound.c - Procedural sound effects via SDL audio + MOD music */
#include "client.h"
#include "mod.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SAMPLE_RATE     44100
#define CHANNELS        1
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
static int               g_music_enabled = 1;
static float             g_sfx_volume    = 0.16f; /* 0.0 – 1.0  default 16% */
static float             g_music_volume  = 1.0f;  /* 0.0 – 1.0  default 100% */
static char              g_music_last_path[512] = "";
static char              g_res_prefix[512] = ""; /* prepended to .mod paths on macOS bundle */

/* Set a directory prefix for all music file lookups (used on macOS app bundle) */
void music_set_res_prefix(const char *prefix) {
    if (!prefix || !prefix[0]) { g_res_prefix[0] = '\0'; return; }
    strncpy(g_res_prefix, prefix, sizeof(g_res_prefix)-1);
    g_res_prefix[sizeof(g_res_prefix)-1] = '\0';
}

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
    int i, n = len / (int)sizeof(Sint16);
    Sint16 *out = (Sint16*)stream;
    (void)userdata;
    memset(stream, 0, len);
    /* SFX voice */
    if (g_voice.active) {
        for (i = 0; i < n && g_voice.pos < g_voice.len; i++, g_voice.pos++) {
            float v = g_voice.samples[g_voice.pos];
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            out[i] = (Sint16)(v * 28000.0f * g_sfx_volume);
        }
        if (g_voice.pos >= g_voice.len) g_voice.active = 0;
    }
    /* MOD music mixed on top */
    if (g_music_enabled && mod_playing())
        mod_mix(out, n, 0.7f * g_music_volume);
}

void sound_set_sfx_volume(float v)   { if(v<0)v=0; if(v>1)v=1; g_sfx_volume=v; }
float sound_get_sfx_volume(void)     { return g_sfx_volume; }
void music_set_volume(float v)       { if(v<0)v=0; if(v>1)v=1; g_music_volume=v; }
float music_get_volume(void)         { return g_music_volume; }

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

    mod_init();
    for (i = 0; i < SFX_COUNT; i++) gen_sfx((SfxId)i);
    SDL_PauseAudioDevice(g_dev, 0);
    return 1;
}

void sound_shutdown(void) {
    if (g_dev) {
        SDL_LockAudioDevice(g_dev);
        mod_stop();
        SDL_UnlockAudioDevice(g_dev);
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
}

/* Load and start a MOD file if it exists; silently skips if not found */
void music_load(const char *path) {
    if (!g_dev) return;
    /* Prepend resource prefix if set (macOS bundle: Contents/Resources/) */
    char full[1024];
    if (g_res_prefix[0] && path && path[0] && path[0] != '/') {
        snprintf(full, sizeof(full), "%s%s", g_res_prefix, path);
        path = full;
    }
    /* Remember path so we can reload on re-enable */
    if (path && path[0]) {
        strncpy(g_music_last_path, path, sizeof(g_music_last_path)-1);
        g_music_last_path[sizeof(g_music_last_path)-1] = '\0';
    }
    SDL_LockAudioDevice(g_dev);
    mod_stop();
    if (g_music_enabled) mod_load(path);
    SDL_UnlockAudioDevice(g_dev);
}

void music_stop(void) {
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    mod_stop();
    SDL_UnlockAudioDevice(g_dev);
}

void music_set_enabled(int enabled) {
    g_music_enabled = enabled;
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    if (!enabled) {
        mod_stop();
    } else if (g_music_last_path[0]) {
        /* Reload the last track when re-enabling */
        mod_load(g_music_last_path);
    }
    SDL_UnlockAudioDevice(g_dev);
}

int music_get_enabled(void) { return g_music_enabled; }

/* No-op on POSIX/SDL — MOD is streamed from the audio callback thread */
void music_tick(void) {}

void sound_play(ClientCtx *c, SfxId id) {
    int len;
    if (!c->sound_enabled || !g_dev) return;
    if (id < 0 || id >= SFX_COUNT) return;
    len = g_sfx_len[id];
    if (len <= 0 || len > SFX_MAX_SAMPLES) return;  /* safety guard */
    SDL_LockAudioDevice(g_dev);
    memcpy(g_voice.samples, g_sfx[id], (size_t)len * sizeof(float));
    g_voice.len    = len;
    g_voice.pos    = 0;
    g_voice.active = 1;
    SDL_UnlockAudioDevice(g_dev);
}
