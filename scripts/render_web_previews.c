/* SPDX-License-Identifier: GPL-3.0-or-later
 * Render illustrative website screenshots using the native client. No network.
 * Writes /tmp/lineclash-web-{arena,bots,rules,rooms}.bmp.
 */
#include "../clients/gui/src/render_sdl.c"
#include <assert.h>
static void shot(const char *name){int w,h;SDL_GetRendererOutputSize(g_rend,&w,&h);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(s);assert(!SDL_RenderReadPixels(g_rend,NULL,s->format->format,s->pixels,s->pitch));char p[256];snprintf(p,sizeof(p),"/tmp/lineclash-web-%s.bmp",name);assert(!SDL_SaveBMP(s,p));SDL_FreeSurface(s);}
static unsigned preview_random(unsigned *seed){*seed=*seed*1664525u+1013904223u;return *seed;}
/* Drop real tetrominoes with independent, repeatable sequences per board. */
static void stack(Field *f,int player){
 field_init(f); unsigned seed=719u+player*1031u;
 const int counts[]={19,31,13,25};
 for(int n=0;n<counts[player%4];n++){
  Piece best; int best_y=-100; unsigned tie=0; int type=(int)(preview_random(&seed)%7);
  for(int rotation=0;rotation<4;rotation++)for(int x=-1;x<FIELD_WIDTH;x++){
   Piece p;piece_init(&p,type);p.rotation=rotation;p.x=x;if(!field_valid(f,&p))continue;
   piece_drop(f,&p);unsigned choice=preview_random(&seed);
   if(p.y>best_y || (p.y==best_y && choice>tie)){best=p;best_y=p.y;tie=choice;}
  }
  if(best_y<0)break;field_lock(f,&best);field_clear_lines(f);
 }
 f->level=2+player;f->lines=11+player*9;f->score=4200+player*3600;
}
int main(void){SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);assert(sdl_init());ClientCtx c;memset(&c,0,sizeof(c));c.running=1;c.state=CS_PLAYING;c.slot=1;c.room_id=1;c.is_host=1;c.priv_level=1;c.hold_enabled=1;c.bots_allowed=1;c.garbage_system=1;c.last_player_wins=1;c.ping_ms=24;strcpy(c.nick,"PixelPilot");stack(&c.my_field,0);piece_init(&c.cur,2);c.cur.y=6;c.cur.x=3;piece_init(&c.next,4);piece_init(&c.hold,0);for(int i=1;i<4;i++){snprintf(c.opp_nicks[i],sizeof(c.opp_nicks[i]),"Player %d",i+1);c.opp_alive[i]=1;stack(&c.opp_fields[i],i);}for(int a=0;a<4;a++)for(int b=a+1;b<4;b++){Field *fa=a?&c.opp_fields[a]:&c.my_field;Field *fb=b?&c.opp_fields[b]:&c.my_field;assert(memcmp(fa->cells,fb->cells,sizeof(fa->cells))!=0);}
 sdl_render_game(&c);shot("arena");c.ai_players_popup_open=1;c.opp_is_bot[2]=1;c.opp_bot_diff[2]=1;strcpy(c.opp_nicks[2],"Bot Medium");sdl_render_game(&c);shot("bots");c.ai_players_popup_open=0;c.game_options_popup_open=1;sdl_render_game(&c);shot("rules");c.game_options_popup_open=0;c.state=CS_LOBBY_SELECT;c.lobby_list_count=3;const char *names[]={"Evening Arena","Practice with Bots","Team Challenge"};for(int i=0;i<3;i++){LobbyInfo *l=&c.lobby_list[i];l->id=i+1;strcpy(l->name,names[i]);l->max_players=8;l->player_count=3+i;l->hold_enabled=1;l->garbage_system=1;l->last_player_wins=1;}sdl_render_lobby_select(&c);shot("rooms");sdl_shutdown();return 0;}
