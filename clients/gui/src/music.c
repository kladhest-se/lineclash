/* A bounded stereo music buffer. Only the worker reads/decodes files;
 * the SDL audio callback consumes already decoded PCM and never does file I/O. */
#include "client.h"
#include "music_decoder.h"
#include "music_catalog.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define MUSIC_RATE 44100
#define RING_FRAMES (MUSIC_RATE*2)
#define BLOCK_FRAMES 1024
static SDL_mutex *lock;
static SDL_cond *changed;
static SDL_Thread *worker;
static float ring[RING_FRAMES*2];
static int read_at, buffered, quitting, generation, eof, failed, loading;
static int enabled=1, paused, shuffle, current, game_mode, selected=1;
static float volume=0.7f;
static double offset, duration;
static uint64_t consumed;
static char resource_dir[1024], base_dir[1024];
static uint32_t shuffle_seed=0x6d2b79f5u;
static void enter(void) { if(lock) SDL_LockMutex(lock); }
static void leave(void) { if(lock) SDL_UnlockMutex(lock); }
static void signal_worker(void) { if(changed) SDL_CondSignal(changed); }
int music_track_count(void) { return (int)(sizeof(music_catalog)/sizeof(music_catalog[0])); }
const MusicTrack *music_track(int index) { return index>=0 && index<music_track_count()?&music_catalog[index]:NULL; }

static int locate_track(int index,int intro,char *path,size_t size) {
    const MusicTrack *track=music_track(index);
    const char *filename=intro?track->intro_file:track->file;
    if(!filename || !filename[0]) return 0;
    char prefix[1024]; const char *override=SDL_getenv("LINECLASH_MUSIC_DIR");
    enter(); snprintf(prefix,sizeof(prefix),"%s",resource_dir); leave();
    const char *dirs[]={override,prefix,base_dir,"",NULL};
    const char *relative[]={"music/","../music/","../../music/",""};
    for(int i=0;i<4;i++) {
        if(!dirs[i] || (i<3 && !dirs[i][0])) continue;
        for(int j=0;j<4;j++) {
            snprintf(path,size,"%s%s%s%s",dirs[i],dirs[i][0] && dirs[i][strlen(dirs[i])-1]!='/'?"/":"",relative[j],filename);
            FILE *f=fopen(path,"rb"); if(f) { fclose(f); return 1; }
        }
    }
#ifndef PLATFORM_WINDOWS
    const char *installed[]={"/usr/share/lineclash/music/","/usr/local/share/lineclash/music/"};
    for(int i=0;i<2;i++) {
        snprintf(path,size,"%s%s",installed[i],filename);
        FILE *f=fopen(path,"rb"); if(f) { fclose(f); return 1; }
    }
#endif
    return 0;
}
static void request_track(int index,int mode) {
    current=index; game_mode=mode;
    if(index>0) selected=index;
    read_at=buffered=eof=failed=0; loading=1; consumed=0; offset=duration=0;
    generation++; signal_worker();
}
static float duck_gain=1.0f;
static int duck_frames=0;
void music_duck(void) { enter(); duck_frames=44100; leave(); }
static int decode_thread(void *unused) {
    MusicDecoder *decoder=NULL; SDL_AudioStream *converter=NULL;
    float input[BLOCK_FRAMES*8], output[BLOCK_FRAMES*2];
    int local_generation=-1, decoder_end=0;
    (void)unused;
    for(;;) {
        enter();
        if(quitting) { leave(); break; }
        if(local_generation!=generation) {
            int target=current, next_generation=generation; double seek=offset;
            local_generation=next_generation; leave();
            music_decoder_close(decoder); decoder=NULL;
            if(converter) SDL_FreeAudioStream(converter);
            converter=NULL; decoder_end=0;
            char path[2048];
            if(locate_track(target,0,path,sizeof(path))) {
                if(music_track(target)->intro_file[0]) {
                    char intro[2048];
                    if(locate_track(target,1,intro,sizeof(intro))) decoder=music_decoder_open_sequence(intro,path);
                } else decoder=music_decoder_open(path);
            }
            if(decoder) {
                converter=SDL_NewAudioStream(AUDIO_F32SYS,(Uint8)music_decoder_channels(decoder),music_decoder_rate(decoder),AUDIO_F32SYS,2,MUSIC_RATE);
                if(converter && seek>0 && !music_decoder_seek(decoder,seek)) { SDL_FreeAudioStream(converter); converter=NULL; }
            }
            enter();
            if(generation==local_generation) {
                failed=!(decoder && converter); loading=0;
                duration=decoder?music_decoder_duration(decoder):0;
            }
            leave(); continue;
        }
        if(failed || eof || !decoder || !converter || buffered>RING_FRAMES-BLOCK_FRAMES) {
            SDL_CondWaitTimeout(changed,lock,50); leave(); continue;
        }
        leave();
        if(SDL_AudioStreamAvailable(converter)<(int)sizeof(output) && !decoder_end) {
            uint64_t frames=music_decoder_read(decoder,BLOCK_FRAMES,input);
            if(frames) {
                if(SDL_AudioStreamPut(converter,input,(int)(frames*music_decoder_channels(decoder)*sizeof(float)))<0) decoder_end=1;
            } else { decoder_end=1; SDL_AudioStreamFlush(converter); }
        }
        int bytes=SDL_AudioStreamGet(converter,output,sizeof(output));
        enter();
        if(local_generation==generation) {
            if(bytes<0) failed=1;
            else if(bytes==0 && decoder_end) eof=1;
            else {
                int frames=bytes/(2*(int)sizeof(float));
                for(int i=0;i<frames;i++) {
                    int at=(read_at+buffered+i)%RING_FRAMES;
                    ring[at*2]=output[i*2]; ring[at*2+1]=output[i*2+1];
                }
                buffered+=frames;
            }
        }
        leave();
    }
    music_decoder_close(decoder); if(converter) SDL_FreeAudioStream(converter);
    return 0;
}
int music_engine_init(void) {
    lock=SDL_CreateMutex(); changed=SDL_CreateCond();
    if(!lock || !changed) { music_engine_shutdown(); return 0; }
    char *base=SDL_GetBasePath(); if(base) { snprintf(base_dir,sizeof(base_dir),"%s",base); SDL_free(base); }
    quitting=0; enter(); request_track(current,game_mode); leave();
    worker=SDL_CreateThread(decode_thread,"LineClash music",NULL);
    if(!worker) { music_engine_shutdown(); return 0; }
    return 1;
}
void music_engine_shutdown(void) {
    enter(); quitting=1; signal_worker(); leave();
    if(worker) SDL_WaitThread(worker,NULL); worker=NULL;
    if(changed) SDL_DestroyCond(changed); changed=NULL;
    if(lock) SDL_DestroyMutex(lock); lock=NULL;
    read_at=buffered=0; loading=0;
}
void music_mix(short *samples,int frames) {
    if(!lock) return;
    enter();
    float target=duck_frames>0?0.35f:1.0f;
    duck_gain+=(target-duck_gain)*0.08f;
    if(duck_frames>0) {duck_frames-=frames;if(duck_frames<0)duck_frames=0;}
    if(enabled && !paused) {
        int take=frames<buffered?frames:buffered;
        for(int i=0;i<take;i++) {
            int at=(read_at+i)%RING_FRAMES;
            for(int channel=0;channel<2;channel++) {
                int pos=i*2+channel;
                float v=(float)samples[pos]+ring[at*2+channel]*volume*duck_gain*22000.0f;
                if(v>32767)v=32767; if(v< -32768)v= -32768;
                samples[pos]=(short)v;
            }
        }
        read_at=(read_at+take)%RING_FRAMES; buffered-=take; consumed+=take;
        if(take) signal_worker();
    }
    leave();
}
void music_set_res_prefix(const char *prefix) {
    enter(); snprintf(resource_dir,sizeof(resource_dir),"%s",prefix?prefix:""); request_track(current,game_mode); leave();
}
void music_start_menu(void) { enter(); if(game_mode || current!=0) { paused=0; request_track(0,0); } leave(); }
void music_start_game(void) { enter(); if(!game_mode) request_track(selected,1); leave(); }
void music_load(const char *path) { if(path && strstr(path,"game")) music_start_game(); else music_start_menu(); }
void music_select(int index) {
    if(index<0 || index>=music_track_count()) return;
    enter(); enabled=1; paused=0; request_track(index,index>0); leave();
}
void music_previous(void) { enter(); int next=current<=1?music_track_count()-1:current-1; leave(); music_select(next); }
void music_next(void) {
    enter(); int next=current+1;
    if(shuffle && current>0) {
        shuffle_seed^=shuffle_seed<<13; shuffle_seed^=shuffle_seed>>17; shuffle_seed^=shuffle_seed<<5;
        next=1+(int)(shuffle_seed%(music_track_count()-1));
        if(next==current) next=next%(music_track_count()-1)+1;
    }
    if(next>=music_track_count()) next=1;
    leave(); music_select(next);
}
void music_tick(void) {
    enter(); int advance=enabled && !paused && eof && buffered==0 && !failed;
    int menu=!game_mode;
    if(advance && menu) request_track(0,0);
    leave(); if(advance && !menu) music_next();
}
void music_seek(double seconds) {
    if(!isfinite(seconds)) return;
    enter(); if(seconds<0) seconds=0; if(seconds>duration) seconds=duration;
    offset=seconds; consumed=0; buffered=read_at=eof=failed=0; loading=1;
    generation++; signal_worker(); leave();
}
void music_toggle_pause(void) { enter(); paused=!paused; signal_worker(); leave(); }
void music_stop(void) { enter(); paused=1; leave(); }
void music_set_enabled(int value) { enter(); enabled=value!=0; signal_worker(); leave(); }
int music_get_enabled(void) { enter(); int value=enabled; leave(); return value; }
void music_set_shuffle(int value) { enter(); shuffle=value!=0; leave(); }
int music_get_shuffle(void) { enter(); int value=shuffle; leave(); return value; }
void music_set_volume(float value) { if(!isfinite(value)) return; if(value<0)value=0; if(value>1)value=1; enter(); volume=value; leave(); }
float music_get_volume(void) { enter(); float value=volume; leave(); return value; }
void music_snapshot(MusicPlayback *out) {
    enter(); out->index=current; out->enabled=enabled; out->paused=paused; out->shuffle=shuffle;
    out->loading=loading; out->failed=failed; out->available=worker!=NULL;
    out->position=offset+(double)consumed/MUSIC_RATE; out->duration=duration; leave();
}
