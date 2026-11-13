#ifndef LINECLASH_MUSIC_DECODER_H
#define LINECLASH_MUSIC_DECODER_H
#include <stdint.h>
typedef struct MusicDecoder MusicDecoder;
MusicDecoder *music_decoder_open(const char *path);
MusicDecoder *music_decoder_open_sequence(const char *intro,const char *path);
void music_decoder_close(MusicDecoder *d);
int music_decoder_channels(const MusicDecoder *d);
int music_decoder_rate(const MusicDecoder *d);
double music_decoder_duration(const MusicDecoder *d);
uint64_t music_decoder_read(MusicDecoder *d, uint64_t frames, float *out);
int music_decoder_seek(MusicDecoder *d, double seconds);
#endif
