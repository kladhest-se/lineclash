/* mod.h — Minimal ProTracker MOD player (4-channel, 31-instrument) */
#ifndef MOD_H
#define MOD_H
#include <stdint.h>

/* Call once after SDL audio is open */
void mod_init(void);

/* Load and start playing a .mod file. Returns 1 on success, 0 if file not found. */
int  mod_load(const char *path);

/* Stop and unload current module */
void mod_stop(void);

/* Is a module currently playing? */
int  mod_playing(void);

/* Mix next chunk of stereo 16-bit samples into buf (len = bytes).
   Call from SDL audio callback or from audio lock. */
void mod_mix(int16_t *buf, int samples, float volume);

#endif
