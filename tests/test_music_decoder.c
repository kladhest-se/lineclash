#include "../clients/gui/src/client.h"
#include "../clients/gui/src/music_decoder.h"
#include "../clients/gui/src/music_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void) {
    assert((int)(sizeof(music_catalog)/sizeof(music_catalog[0]))==19);
    float pcm[4096*8];
    for(int i=0;i<(int)(sizeof(music_catalog)/sizeof(music_catalog[0]));i++) {
        const MusicTrack *t=&music_catalog[i]; char path[1024],intro[1024];
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
puts("All audio decoder sanitizer checks passed"); return 0; }
