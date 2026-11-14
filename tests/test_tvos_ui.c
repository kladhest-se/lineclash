/* SPDX-License-Identifier: GPL-3.0-or-later
 * Offscreen render checks for TV screens, using real widgets and game boards.
 * UIKit entry is checked separately in the tvOS simulator. No network or account.
 */
#define PLATFORM_TVOS
#include "../clients/gui/src/render_sdl.c"
#include <assert.h>
void lc_tvos_attach_window(SDL_Window *window){(void)window;}
int lc_tvos_editing(void){return 0;}
void lc_tvos_edit(char *text,size_t capacity,int *length,int password,const char *title){(void)text;(void)capacity;(void)length;(void)password;(void)title;abort();}
static void shot(const char *name){
    LCFocusRect focus;if(lc_focus_current(&focus))assert(focus.x>=0 && focus.y>=0 && focus.x+focus.w<=WIN_W && focus.y+focus.h<=WIN_H);
    int w,h;SDL_GetRendererOutputSize(g_rend,&w,&h);SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
    assert(SDL_RenderReadPixels(g_rend,NULL,surface->format->format,surface->pixels,surface->pitch)==0);
    char path[256];snprintf(path,sizeof(path),"/tmp/lineclash-tv-%s.bmp",name);assert(SDL_SaveBMP(surface,path)==0);SDL_FreeSurface(surface);
}
static void stack(Field *field,int index){
    field_init(field);
    for(int i=0;i<14+index*3;i++){Piece piece;piece_init(&piece,(i*3+index)%7);piece.rotation=i%4;piece.x=(i*5+index)%8;
        if(field_valid(field,&piece)){piece_drop(field,&piece);field_lock(field,&piece);field_clear_lines(field);}}
}
int main(void){
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(sdl_init());
    static ClientCtx c;c.running=1;c.sock=INVALID_SOCK;c.srv_test_sock=INVALID_SOCK;c.server_edit_idx=-1;
    c.state=CS_SERVER_LIST;c.server_count=3;for(int i=0;i<3;i++){SavedServer *s=&c.server_list[i];snprintf(s->name,sizeof(s->name),"Community arena %d",i+1);snprintf(s->host,sizeof(s->host),"server%d.example.org",i+1);s->port=31457;s->discovered=1;s->online=1;}
    sdl_render_server_list(&c);shot("servers");
    c.state=CS_ACCOUNT;sdl_render_accounts(&c);shot("account");c.account_create_mode=1;sdl_render_accounts(&c);shot("register");c.account_create_mode=0;
    c.state=CS_LOBBY_SELECT;c.lobby_list_count=3;for(int i=0;i<3;i++){c.lobby_list[i].id=i+1;c.lobby_list[i].player_count=i+2;c.lobby_list[i].max_players=8;}
    sdl_render_lobby_select(&c);shot("rooms");
    c.state=CS_IN_LOBBY;c.slot=1;c.room_id=1;c.is_host=1;c.bots_allowed=1;c.hold_enabled=1;strcpy(c.nick,"PixelPilot");
    for(int i=0;i<MAX_PLAYERS;i++){snprintf(c.opp_nicks[i],sizeof(c.opp_nicks[i]),"Player%d",i+1);c.opp_alive[i]=1;c.opp_ready[i]=i%2;stack(&c.opp_fields[i],i);}
    sdl_render_game(&c);shot("room");tv_room_options=1;sdl_render_game(&c);shot("rules");tv_room_options=0;
    c.state=CS_PLAYING;stack(&c.my_field,0);piece_init(&c.cur,2);c.cur.y=3;piece_init(&c.next,4);piece_init(&c.hold,0);c.attack_target=2;c.my_field.special_count=3;c.my_field.specials[0]=1;c.my_field.specials[1]=2;c.my_field.specials[2]=3;
    sdl_render_game(&c);shot("arena");g_tv_actions=1;sdl_render_game(&c);shot("actions");g_tv_actions=0;
    c.keymap_popup_open=1;c.settings_tab=1;sdl_render_game(&c);shot("controls");c.keymap_popup_open=0;c.music_player_open=1;sdl_render_game(&c);shot("music");
    sdl_shutdown();puts("TV screen render checks passed");return 0;
}
