/* Render actual game/player/About screens with SDL's software renderer. */
#include "../clients/gui/src/render_sdl.c"
#include <assert.h>
static void capture(const char *name) {
    int w,h; assert(SDL_GetRendererOutputSize(g_rend,&w,&h)==0);
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888); assert(s);
    assert(SDL_RenderReadPixels(g_rend,NULL,s->format->format,s->pixels,s->pitch)==0);
    char path[2048];
#ifdef PLATFORM_WINDOWS
    char folder[1024]; DWORD n=GetTempPathA(sizeof(folder),folder); assert(n>0 && n<sizeof(folder));
    snprintf(path,sizeof(path),"%s%s",folder,name);
#else
    const char *folder=SDL_getenv("TMPDIR"); if(!folder || !folder[0]) folder="/tmp";
    snprintf(path,sizeof(path),"%s/%s",folder,name);
#endif
    assert(SDL_SaveBMP(s,path)==0); SDL_FreeSurface(s);
}

int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1); SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    assert(logo_phase(0,0)==logo_phase(500,0));
    assert(logo_phase(0,1)!=logo_phase(180,1));
    assert(logo_phase(0,1)==logo_phase(540,1));
    assert(sdl_init()); assert(sound_init()); music_set_res_prefix("../"); music_select(16);
    MusicPlayback p;
    for(int i=0;i<500;i++) { music_tick(); music_snapshot(&p); if(p.duration>0 && p.position>1)break; SDL_Delay(5); }
    ClientCtx c; memset(&c,0,sizeof(c)); c.running=1; c.state=CS_PLAYING; c.slot=1; c.room_id=1; c.priv_level=3;
    strcpy(c.nick,"Player"); c.hold.type=-1; c.km.hold=6; c.ping_ms=24;
    field_init(&c.my_field); piece_init(&c.cur,1); piece_init(&c.next,2);
    c.music_player_open=1; sdl_render_game(&c); capture("lineclash-music-player.bmp");
    c.music_player_open=0; c.about_popup_open=1; c.about_music_page=2;
    sdl_render_game(&c); capture("lineclash-music-about.bmp");
    c.about_popup_open=0; sdl_render_game(&c); capture("lineclash-music-game.bmp");
    /* Wheel and Escape route to the player without sending gameplay input. */
    c.music_player_open=1; c.music_scroll=0; SDL_Event e; SDL_zero(e);
    e.type=SDL_MOUSEWHEEL; e.wheel.y=-1;
#if SDL_VERSION_ATLEAST(2,0,18)
    e.wheel.preciseY=-1;
#endif
     e.wheel.windowID=SDL_GetWindowID(g_win); assert(SDL_PushEvent(&e)==1);
    for(int i=0;i<20 && c.music_scroll!=2;i++) { sdl_poll_events(&c); SDL_Delay(5); }
    assert(c.music_scroll==2);
    client_on_key(&c,41,0,0,""); assert(!c.music_player_open);
    c.state=CS_SERVER_LIST; client_on_button(&c,BTN_MUSIC_PLAYER);
    assert(!c.music_player_open);
    music_toggle_pause();
    c.music_player_open=1; client_tick(&c); assert(!c.music_player_open);
    music_snapshot(&p); assert(p.index==0 && !p.paused);
    c.state=CS_PLAYING; client_on_button(&c,BTN_MUSIC_PLAYER);
    assert(c.music_player_open);
    c.state=CS_GAMEOVER; client_tick(&c); assert(c.music_player_open);
    c.music_player_open=0;c.state=CS_SERVER_LIST;c.server_count=0;
    sdl_render_server_list(&c);capture("lineclash-modern-empty.bmp");
    c.server_count=8;c.server_cursor=0;c.server_filter=0;c.server_sort=1;
    strcpy(c.directory_message,"Found 8 public servers");
    for(int i=0;i<c.server_count;i++) {
        SavedServer *s=&c.server_list[i];snprintf(s->name,sizeof(s->name),"Public server %d",i+1);strcpy(s->host,"game.example.org");s->port=31457+i;s->discovered=1;s->online=1;s->ping_ms=20+i*13;s->capacity=8;s->users_idle=i;s->favourite=i%2;s->last_used=100+i;
    }
    sdl_render_server_list(&c);capture("lineclash-native-directory.bmp");
    c.server_filter=1;sdl_render_server_list(&c);capture("lineclash-favourites.bmp");
    assert(controller_key(&c,SDL_CONTROLLER_BUTTON_A)==c.km.rotate_cw);
    assert(controller_key(&c,SDL_CONTROLLER_BUTTON_Y)==c.km.hard_drop);
#if SDL_VERSION_ATLEAST(2,0,14)
    int virtual_pad=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    assert(virtual_pad>=0);controller_open();assert(g_controller);
    SDL_Joystick *virtual_joystick=SDL_GameControllerGetJoystick(g_controller);
    assert(SDL_JoystickSetVirtualButton(virtual_joystick,SDL_CONTROLLER_BUTTON_A,1)==0);
    SDL_GameControllerUpdate();assert(SDL_GameControllerGetButton(g_controller,SDL_CONTROLLER_BUTTON_A));
    assert(SDL_JoystickDetachVirtual(virtual_pad)==0);controller_open();assert(!g_controller);
#endif
    c.keymap_popup_open=1;c.keymap_capture_idx=-1;c.settings_tab=0;
    sdl_render_server_list(&c);capture("lineclash-settings-display.bmp");
    c.settings_tab=1;sdl_render_server_list(&c);capture("lineclash-settings-controls.bmp");
    c.settings_tab=2;sdl_render_server_list(&c);capture("lineclash-settings-audio.bmp");
    client_on_key(&c,41,0,0,"");assert(!c.keymap_popup_open);
    c.state=CS_SERVER_LIST;client_accounts_open(&c,0);assert(c.state==CS_ACCOUNT);
    client_on_key(&c,0,0,1,"alpha");assert(!strcmp(c.tf_account_email.text,"alpha"));
    client_on_key(&c,43,0,0,NULL);client_on_key(&c,0,0,1,"å-password");assert(!strcmp(c.tf_account_password.text,"å-password"));
    c.account_count=2;c.account_cursor=0;c.account_active=0;strcpy(c.accounts[0].handle,"alpha");strcpy(c.accounts[1].handle,"beta");strcpy(c.accounts[0].token,"test-session-not-real");c.accounts[0].expires=time(NULL)+600;
    sdl_render_accounts(&c);capture("lineclash-account-profiles.bmp");
    client_on_key(&c,41,0,0,NULL);assert(c.state==CS_SERVER_LIST && !c.tf_account_password.text[0]);
    client_accounts_open(&c,0);client_accounts_action(&c,ACCOUNT_CREATE_MODE);strcpy(c.tf_account_handle.text,"newplayer");c.tf_account_handle.len=9;strcpy(c.tf_account_email.text,"player@example.test");c.tf_account_email.len=19;
    sdl_render_accounts(&c);capture("lineclash-account-create.bmp");
    memset(c.account_recovery,'a',48);c.account_recovery[48]=0;sdl_render_accounts(&c);capture("lineclash-account-created.bmp");account_erase(c.account_recovery,sizeof(c.account_recovery));
    sound_shutdown(); sdl_shutdown(); return 0;
}
