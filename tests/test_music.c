/* Decode every bundled track and exercise streaming controls without speakers. */
#include "../clients/gui/src/client.h"
#include "../clients/gui/src/music_decoder.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
static MusicPlayback ready(int index) {
    MusicPlayback p;
    for(int i=0;i<1000;i++) {
        music_tick(); music_snapshot(&p);
        assert(!p.failed);
        if(p.index==index && !p.loading && p.duration>0 && p.position>0.02) return p;
        SDL_Delay(5);
    }
    fprintf(stderr,"Music never became ready: %d\n",index); assert(0); return p;
}
int main(void) {
    assert(music_track_count()==19);
    float pcm[4096*8];
    for(int i=0;i<music_track_count();i++) {
        const MusicTrack *t=music_track(i); char path[1024],intro[1024];
        snprintf(path,sizeof(path),"../music/%s",t->file);
        MusicDecoder *d;
        if(t->intro_file[0]) {
            snprintf(intro,sizeof(intro),"../music/%s",t->intro_file);
            d=music_decoder_open_sequence(intro,path);
        } else d=music_decoder_open(path);
        assert(d); assert(music_decoder_duration(d)>5 && music_decoder_duration(d)<3600);
        assert(music_decoder_seek(d,music_decoder_duration(d)*0.4));
        uint64_t n=music_decoder_read(d,4096,pcm); assert(n>0);
        double energy=0;
        for(uint64_t k=0;k<n*(uint64_t)music_decoder_channels(d);k++) { assert(isfinite(pcm[k])); energy+=pcm[k]*pcm[k]; }
        assert(energy>0.000001);
        printf("Decoded: %s (%.1fs, %d Hz, %d channels)\n",t->title,music_decoder_duration(d),music_decoder_rate(d),music_decoder_channels(d));
        if(t->intro_file[0]) {
            MusicDecoder *a=music_decoder_open(intro), *b=music_decoder_open(path); assert(a && b);
            double first=music_decoder_duration(a);
            assert(fabs(music_decoder_duration(d)-first-music_decoder_duration(b))<0.001);
            assert(music_decoder_seek(d,first-0.01));
            assert(music_decoder_read(d,4096,pcm)==4096); /* crosses opening -> loop */
            music_decoder_close(a); music_decoder_close(b);
        }
        music_decoder_close(d);
    }
    assert(!music_decoder_open("../music/does-not-exist.mp3"));
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    SDL_SetMainReady(); assert(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)==0); assert(sound_init());
    music_set_res_prefix("../"); music_set_enabled(1); music_select(0); ready(0);
    for(int i=1;i<music_track_count();i++) { music_select(i); ready(i); }
    music_select(1); MusicPlayback p=ready(1);
    music_toggle_pause(); music_snapshot(&p); double stopped=p.position;
    SDL_Delay(100); music_snapshot(&p); assert(p.position==stopped && p.paused);
    music_set_enabled(0); music_toggle_pause(); SDL_Delay(100); music_snapshot(&p); assert(p.position==stopped);
    music_set_enabled(1); ready(1);
    music_set_volume(-1); assert(music_get_volume()==0);
    music_set_volume(2); assert(music_get_volume()==1);
    music_set_volume(NAN); assert(music_get_volume()==1);
    music_set_shuffle(1); music_next(); music_snapshot(&p); assert(p.index>0 && p.index!=1);
    music_set_shuffle(0); music_select(1); p=ready(1);
    music_seek(p.duration-0.08); ready(2); /* EOF advances the bank. */
    music_select(0); p=ready(0); music_seek(p.duration-0.08);
    int looped=0;
    for(int i=0;i<1000;i++) {
        music_tick(); music_snapshot(&p);
        if(!p.loading && p.duration>0 && p.position<1) { looped=1; break; }
        SDL_Delay(5);
    }
    assert(looped && p.index==0);
    music_start_game(); music_snapshot(&p); assert(p.index>0);
    music_start_menu(); music_snapshot(&p); assert(p.index==0);
    sound_shutdown(); assert(sound_init()); music_set_res_prefix("../"); music_select(1); ready(1);
    sound_shutdown(); SDL_Quit(); puts("Music decoding and player regressions passed"); return 0;
}
