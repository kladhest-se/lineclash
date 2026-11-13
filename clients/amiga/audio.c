/* Audio is optional in the initial native frontend; no SDL or decoder overhead. */
#include "../gui/src/client.h"
static float music_volume=.7f,sfx_volume=.7f;static int enabled,shuffle;
int sound_init(void){return 1;}void sound_shutdown(void){}void sound_play(ClientCtx*c,SfxId id){(void)c;(void)id;}
void sound_set_sfx_volume(float v){sfx_volume=v;}float sound_get_sfx_volume(void){return sfx_volume;}
void music_set_volume(float v){music_volume=v;}float music_get_volume(void){return music_volume;}
void music_set_enabled(int v){enabled=v;}int music_get_enabled(void){return enabled;}
int music_track_count(void){return 1;}const MusicTrack *music_track(int i){(void)i;static MusicTrack track={"Audio unavailable","","","","","","",""};return &track;}
void music_snapshot(MusicPlayback*out){memset(out,0,sizeof(*out));out->enabled=enabled;out->shuffle=shuffle;}
void music_select(int i){(void)i;}void music_previous(void){}void music_next(void){}void music_toggle_pause(void){}void music_duck(void){}
void music_set_shuffle(int v){shuffle=v;}int music_get_shuffle(void){return shuffle;}void music_seek(double v){(void)v;}
void music_start_menu(void){}void music_start_game(void){}int music_engine_init(void){return 1;}void music_engine_shutdown(void){}void music_mix(short*s,int n){memset(s,0,n*4);}void music_load(const char*p){(void)p;}void music_stop(void){}void music_tick(void){}void music_set_res_prefix(const char*p){(void)p;}
