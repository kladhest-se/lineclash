/* Bundled decoders retain their upstream MIT/public-domain notices. */
#include "music_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define DR_MP3_IMPLEMENTATION
#include "../third_party/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../third_party/dr_wav.h"
#include "../third_party/stb_vorbis.c"

struct MusicDecoder {
    int kind, channels, rate, intro_done;
    struct MusicDecoder *intro, *main;
    uint64_t frames;
    union { drmp3 mp3; drwav wav; stb_vorbis *ogg; } data;
};
MusicDecoder *music_decoder_open(const char *path) {
    MusicDecoder *d=(MusicDecoder *)calloc(1,sizeof(*d));
    const char *ext=strrchr(path,'.');
    if(!d || !ext) { free(d); return NULL; }
    if(!strcmp(ext,".mp3")) {
        if(!drmp3_init_file(&d->data.mp3,path,NULL)) { free(d); return NULL; }
        d->kind=1; d->channels=d->data.mp3.channels; d->rate=d->data.mp3.sampleRate;
        d->frames=drmp3_get_pcm_frame_count(&d->data.mp3);
        drmp3_seek_to_pcm_frame(&d->data.mp3,0);
    } else if(!strcmp(ext,".wav")) {
        if(!drwav_init_file(&d->data.wav,path,NULL)) { free(d); return NULL; }
        d->kind=2; d->channels=d->data.wav.channels; d->rate=d->data.wav.sampleRate;
        d->frames=d->data.wav.totalPCMFrameCount;
    } else if(!strcmp(ext,".ogg")) {
        int error=0;
        d->data.ogg=stb_vorbis_open_filename(path,&error,NULL);
        if(!d->data.ogg) { free(d); return NULL; }
        stb_vorbis_info info=stb_vorbis_get_info(d->data.ogg);
        d->kind=3; d->channels=info.channels; d->rate=info.sample_rate;
        d->frames=stb_vorbis_stream_length_in_samples(d->data.ogg);
        stb_vorbis_seek_start(d->data.ogg);
    } else { free(d); return NULL; }
    if(d->channels<1 || d->channels>8 || d->rate<8000 || d->rate>192000 || !d->frames) {
        music_decoder_close(d); return NULL;
    }
    return d;
}
MusicDecoder *music_decoder_open_sequence(const char *intro,const char *path) {
    MusicDecoder *opening=music_decoder_open(intro), *main=music_decoder_open(path);
    if(!opening || !main || opening->channels!=main->channels || opening->rate!=main->rate) {
        music_decoder_close(opening); music_decoder_close(main); return NULL;
    }
    MusicDecoder *d=(MusicDecoder *)calloc(1,sizeof(*d));
    if(!d) { music_decoder_close(opening); music_decoder_close(main); return NULL; }
    d->kind=4; d->intro=opening; d->main=main;
    d->channels=main->channels; d->rate=main->rate; d->frames=opening->frames+main->frames;
    return d;
}
void music_decoder_close(MusicDecoder *d) {
    if(!d) return;
    if(d->kind==4) { music_decoder_close(d->intro); music_decoder_close(d->main); }
    if(d->kind==1) drmp3_uninit(&d->data.mp3);
    if(d->kind==2) drwav_uninit(&d->data.wav);
    if(d->kind==3) stb_vorbis_close(d->data.ogg);
    free(d);
}
int music_decoder_channels(const MusicDecoder *d) { return d->channels; }
int music_decoder_rate(const MusicDecoder *d) { return d->rate; }
double music_decoder_duration(const MusicDecoder *d) { return (double)d->frames/d->rate; }
uint64_t music_decoder_read(MusicDecoder *d,uint64_t frames,float *out) {
    if(d->kind==4) {
        uint64_t first=0;
        if(!d->intro_done) {
            first=music_decoder_read(d->intro,frames,out);
            if(first==frames) return first;
            d->intro_done=1;
        }
        return first+music_decoder_read(d->main,frames-first,out+first*d->channels);
    }
    if(d->kind==1) return drmp3_read_pcm_frames_f32(&d->data.mp3,frames,out);
    if(d->kind==2) return drwav_read_pcm_frames_f32(&d->data.wav,frames,out);
    if(d->kind==3) return stb_vorbis_get_samples_float_interleaved(d->data.ogg,d->channels,out,(int)frames*d->channels);
    return 0;
}
int music_decoder_seek(MusicDecoder *d,double seconds) {
    if(!isfinite(seconds)) return 0;
    if(seconds<0) seconds=0;
    if(d->kind==4) {
        double length=music_decoder_duration(d->intro);
        d->intro_done=seconds>=length;
        if(!d->intro_done) return music_decoder_seek(d->intro,seconds) && music_decoder_seek(d->main,0);
        return music_decoder_seek(d->main,seconds-length);
    }
    uint64_t frame=(uint64_t)(seconds*d->rate);
    if(frame>=d->frames) frame=d->frames-1;
    if(d->kind==1) return drmp3_seek_to_pcm_frame(&d->data.mp3,frame)!=0;
    if(d->kind==2) return drwav_seek_to_pcm_frame(&d->data.wav,frame)!=0;
    return stb_vorbis_seek(d->data.ogg,(unsigned int)frame)!=0;
}
