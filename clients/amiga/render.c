/* Native 16-colour Intuition frontend. No SDL, FPU, RTG or encrypted game stream. */
#include "../gui/src/client.h"
#include <intuition/intuition.h>
#include <graphics/displayinfo.h>
#include <devices/inputevent.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
static struct Screen *screen;
static struct Window *window;
struct Library *KeymapBase;
static int click_x=-1,click_y=-1;
static void box(int x,int y,int w,int h,int pen){SetAPen(window->RPort,pen);RectFill(window->RPort,x,y,x+w-1,y+h-1);}
static void text(int x,int y,const char *s,int pen){SetAPen(window->RPort,pen);SetBPen(window->RPort,0);SetDrMd(window->RPort,JAM1);Move(window->RPort,x,y+7);Text(window->RPort,s,(ULONG)strlen(s));}
static int button(int x,int y,int w,const char *label){box(x,y,w,14,2);text(x+4,y+3,label,1);return click_x>=x && click_x<x+w && click_y>=y && click_y<y+14;}
static void begin(const char *title){box(0,0,640,256,0);box(0,0,640,20,2);text(8,6,"LINECLASH",5);text(104,6,title,1);text(560,6,"v" TETRINETZ_VERSION,3);}
static void end(void){click_x=click_y=-1;}
int sdl_init(void){log_info("Opening native Intuition frontend");KeymapBase=OpenLibrary("keymap.library",37);if(!KeymapBase)return 0;
 screen=OpenScreenTags(NULL,SA_Width,640,SA_Height,256,SA_Depth,4,SA_DisplayID,HIRES_KEY,SA_Title,"LineClash",SA_ShowTitle,FALSE,TAG_DONE);if(!screen)return 0;
 const UWORD palette[]={0x112,0xeee,0x334,0x679,0xa8d,0x6dc,0xdc6,0xc67,0x89d,0xd95,0x6b8,0x7cd,0xb9d,0xaaa,0x789,0xddd};LoadRGB4(&screen->ViewPort,(UWORD*)palette,16);
 window=OpenWindowTags(NULL,WA_CustomScreen,(ULONG)screen,WA_Left,0,WA_Top,0,WA_Width,640,WA_Height,256,WA_Borderless,TRUE,WA_Activate,TRUE,WA_RMBTrap,TRUE,WA_IDCMP,IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS|IDCMP_CLOSEWINDOW, TAG_DONE);log_info("Intuition window: %s",window?"opened":"failed");return window!=NULL;
}
void sdl_shutdown(void){if(window)CloseWindow(window);if(screen)CloseScreen(screen);if(KeymapBase)CloseLibrary(KeymapBase);window=0;screen=0;KeymapBase=0;}
void sdl_text_start(void){}void sdl_text_stop(void){}const char *sdl_get_res_dir(void){return "PROGDIR:";}
void sdl_set_ui_options(int scale,int patterns,int contrast){(void)scale;(void)patterns;(void)contrast;}
void sdl_open_account_code(const char *code){printf("Approve central sign-in at https://lineclash.kladhest.se/account/  Code: %s\n",code);}

static int raw_sc(UWORD raw,char ch){if(raw>=0x50&&raw<=0x59)return 58+raw-0x50;switch(raw){case 0x4c:return 82;case 0x4d:return 81;case 0x4e:return 79;case 0x4f:return 80;case 0x44:return 40;case 0x45:return 41;case 0x41:return 42;case 0x42:return 43;case 0x40:return 44;case 0x60:return 225;case 0x61:return 229;}if(ch>='A'&&ch<='Z')ch+=32;if(ch>='a'&&ch<='z')return ch-'a'+4;if(ch>='1'&&ch<='9')return ch-'1'+30;if(ch=='0')return 39;return 0;}
int sdl_poll_events(ClientCtx *c){struct IntuiMessage *message;
 while((message=(struct IntuiMessage*)GetMsg(window->UserPort))){ULONG cls=message->Class;UWORD code=message->Code,qual=message->Qualifier;int x=message->MouseX,y=message->MouseY;ReplyMsg((struct Message*)message);
 if(cls==IDCMP_CLOSEWINDOW)return 0;
 if(cls==IDCMP_MOUSEBUTTONS && code==SELECTDOWN){click_x=x;click_y=y;}
 if(cls==IDCMP_RAWKEY && !(code&0x80)){struct InputEvent event;char chars[8]={0};memset(&event,0,sizeof(event));event.ie_Class=IECLASS_RAWKEY;event.ie_Code=code;event.ie_Qualifier=qual;int count=MapRawKey(&event,chars,7,NULL);int sc=raw_sc(code,chars[0]);if(sc)client_on_key(c,sc,0,0,NULL);if(count>0 && (unsigned char)chars[0]>=32){char utf[16];int j=0;for(int i=0;i<count && j<13;i++){unsigned char ch=chars[i];if(ch>=128){utf[j++]=0xc0|(ch>>6);utf[j++]=0x80|(ch&63);}else utf[j++]=ch;}utf[j]=0;client_on_key(c,0,0,1,utf);}}
 }return c->running;
}
static void overlay(ClientCtx *c){if(c->about_popup_open){box(64,48,512,156,2);text(80,64,"LineClash - Tommy Froessman",1);text(80,88,"GPLv3 or later / keyboard and mouse",1);text(80,112,"Soundtrack credits: see CREDITS.md",1);text(80,144,"Native AmigaOS 3.x / bsdsocket.library",5);if(button(80,176,96,"Close"))c->about_popup_open=0;}
 if(c->keymap_popup_open){box(64,40,512,180,2);text(80,56,"CONTROLS",5);text(80,80,"Arrows: move / rotate     Space: hard drop",1);text(80,104,"Z: reverse rotate        C / Shift: hold",1);text(80,128,"R: ready   Tab: chat   1-8: special target",1);text(80,152,"F5: pause   Esc: back   F2: close settings",1);if(button(80,188,96,"Close"))c->keymap_popup_open=0;}}
void sdl_render_server_list(ClientCtx *c){static char last[128];if(strcmp(last,c->directory_message)){snprintf(last,sizeof(last),"%s",c->directory_message);log_info("Directory: %s",last);}begin("SERVERS");if(button(8,26,112,"Public"))client_on_button(c,BTN_PUBLIC_SERVERS);if(button(128,26,112,"Saved"))client_on_button(c,BTN_SAVED_SERVERS);if(button(248,26,112,"Local network"))client_on_button(c,BTN_LAN_SERVERS);if(button(368,26,100,"Settings"))c->keymap_popup_open=1;if(button(476,26,80,"Account"))client_accounts_open(c,0);
 int indices[MAX_SAVED_SERVERS],count=client_server_order(c,indices);
 for(int i=0;i<count && i<10;i++){int y=48+i*15,index=indices[i];SavedServer *s=&c->server_list[index];char label[90];snprintf(label,sizeof(label),"%.20s  %.30s:%d  %.16s",s->name,s->host,s->port,client_server_status(s));if(c->server_cursor==index)box(4,y,628,14,2);text(8,y+2,label,c->server_cursor==index?5:1);if(click_y>=y&&click_y<y+14){if(c->server_cursor==index)client_on_button(c,BTN_SRV_CONNECT);else c->server_cursor=index;}}
 if(button(8,206,108,"Refresh")){if(c->server_tab==2)client_on_button(c,BTN_LAN_SERVERS);else client_on_button(c,BTN_DISCOVER);}if(button(124,206,108,"Connect"))client_on_button(c,BTN_SRV_CONNECT);if(button(240,206,112,"Direct / local")){c->central_mode=0;c->state=CS_CONNECT;}if(button(360,206,100,"Quit"))c->running=0;text(8,234,c->directory_message,3);overlay(c);end();}
void sdl_render_connect(ClientCtx *c){begin("CONNECT");
 if(c->state==CS_AUTH){text(24,56,c->central_mode?"Signing in over HTTPS with AmiSSL...":"Signing in...",1);text(24,88,c->central_profile,5);text(24,120,"Passwords and sessions stay out of the game connection.",1);if(button(24,192,120,"Back"))client_on_button(c,BTN_BACK_CONNECT);end();return;}
 TextField *fields[]={&c->tf_host,&c->tf_port,&c->tf_nick,&c->tf_pass};const char *labels[]={"Host","Port","Local name","Local password"};
 for(int i=0;i<4;i++){int y=42+i*28;text(16,y,labels[i],1);box(152,y-2,456,18,2);char value[256];if(i==3){memset(value,'*',fields[i]->len);value[fields[i]->len]=0;}else snprintf(value,sizeof(value),"%.54s",fields[i]->text);text(160,y,value,fields[i]->focused?5:1);if(click_y>=y-2&&click_y<y+16){for(int j=0;j<4;j++)fields[j]->focused=i==j;}}
 if(button(16,168,168,"Local account"))client_on_button(c,BTN_CONNECT);if(button(192,168,176,"Central account"))client_on_button(c,BTN_CENTRAL_CONNECT);if(button(376,168,120,"Back"))client_on_button(c,BTN_BACK_CONNECT);text(16,216,c->connect_error,7);end();}
void sdl_render_lobby_select(ClientCtx *c){begin("CHOOSE A ROOM");for(int i=0;i<c->lobby_list_count && i<8;i++){char label[80];snprintf(label,sizeof(label),"Room %d  %s  (%d players)",c->lobby_list[i].id,c->lobby_list[i].name,c->lobby_list[i].player_count);if(button(16,42+i*20,600,label)){c->lobby_cursor=i;client_on_key(c,40,0,0,NULL);}}if(button(16,222,120,"Disconnect"))client_on_button(c,BTN_BACK_CONNECT);end();}
static void field(int x,int y,int size,Field *board,Piece *piece){for(int row=0;row<FIELD_HEIGHT;row++)for(int col=0;col<FIELD_WIDTH;col++){int v=board->cells[row][col];box(x+col*size,y+row*size,size-1,size-1,v?4+(v%8):2);}if(piece){const PieceShape *shape=&PIECES[piece->type][piece->rotation];for(int i=0;i<4;i++){int px=piece->x+shape->c[i].dx,py=piece->y+shape->c[i].dy;if(px>=0&&px<FIELD_WIDTH&&py>=0&&py<FIELD_HEIGHT)box(x+px*size,y+py*size,size-1,size-1,4+piece->type);}}}
void sdl_render_game(ClientCtx *c){begin(c->nick);if(c->state==CS_IN_LOBBY){text(16,42,"R: ready / Enter: start as host / Esc: leave",1);if(button(16,70,120,"Ready"))client_on_key(c,c->km.ready,0,0,NULL);if(button(144,70,120,"Start"))client_on_key(c,40,0,0,NULL);}else field(8,42,8,&c->my_field,&c->cur);
 char stats[100];snprintf(stats,sizeof(stats),"LEVEL %d  LINES %d  SCORE %d",c->my_field.level,c->my_field.lines,c->my_field.score);text(160,30,stats,5);
 for(int i=0;i<MAX_PLAYERS;i++)if(i+1!=c->slot && c->opp_nicks[i][0]){int x=160+i*58;text(x,48,c->opp_nicks[i],1);field(x,60,3,&c->opp_fields[i],NULL);}
 int start=c->chat_count>7?c->chat_count-7:0;for(int i=start;i<c->chat_count;i++)text(160,142+(i-start)*11,c->chat[i].text,3);
 text(8,230,c->status,5);if(c->chat_input.active)text(160,230,c->chat_input.text,1);overlay(c);end();}

void sdl_render_accounts(ClientCtx *c){begin("ACCOUNT");int busy=account_https_busy();const int x=112,w=416;
 if(!busy && button(360,26,148,"Activation help"))snprintf(c->account_message,sizeof(c->account_message),"Resend: lineclash.kladhest.se/account/verify/");
 if(!busy && button(516,26,100,"Back"))client_accounts_action(c,ACCOUNT_BACK);
 if(client_account_ready(c)){
  text(x,66,"You're signed in.",5);text(x,96,c->central_profile,1);char email[53];snprintf(email,sizeof(email),"%.52s",c->account.email);text(x,120,email,3);
  if(!busy && button(x,160,w,"Browse servers"))client_accounts_action(c,ACCOUNT_CONTINUE);
  if(!busy && button(x,190,w,"Sign out"))client_accounts_action(c,ACCOUNT_LOGOUT);
 }else if(c->account_pin_mode){
  text(x,58,"Sign in with a PIN",5);text(48,86,"Open lineclash.kladhest.se/account/pin in a browser",1);
  char display[24];snprintf(display,sizeof(display),"PIN: %.4s %.4s",c->account_pin,c->account_pin+4);text(x,114,display,5);
  text(48,142,"Enter the PIN, sign in, then approve this client.",1);
  if(button(x,174,w,"Use username / password"))client_accounts_action(c,ACCOUNT_PIN_CANCEL);
  if(!busy && !c->account_pin_secret[0] && button(x,200,w,"Get a new PIN"))client_accounts_action(c,ACCOUNT_PIN_START);
 }else{
  TextField *fields[]={&c->tf_account_handle,&c->tf_account_email,&c->tf_account_password,&c->tf_account_confirm};const char *labels[]={"Username","Email","Password","Confirm password"};
  int creating=c->account_create_mode,first=creating?0:1,last=creating?3:2;
  for(int i=first;i<=last;i++){int y=(creating?44:74)+(i-first)*(creating?38:52);text(x,y-12,labels[i],1);box(x,y,w,18,2);char value[128];if(i>=2){int visible=fields[i]->len<50?fields[i]->len:50;memset(value,'*',visible);value[visible]=0;}else snprintf(value,sizeof(value),"%.50s",fields[i]->text);text(x+8,y+3,value,fields[i]->focused?5:1);if(!busy && click_y>=y && click_y<y+18 && click_x>=x && click_x<x+w){for(int j=0;j<4;j++)fields[j]->focused=i==j;}}
  if(!creating && !busy && button(x,212,w,"Sign in with a PIN"))client_accounts_action(c,ACCOUNT_PIN_START);
  if(!creating && !busy && button(8,26,140,"Forgot password?"))snprintf(c->account_message,sizeof(c->account_message),"Visit lineclash.kladhest.se/account/recover/ in a browser.");
  if(!busy){if(button(x,creating?190:164,w,creating?"Create account":"Sign in"))client_accounts_action(c,creating?ACCOUNT_REGISTER:ACCOUNT_SIGNIN);if(button(x,creating?214:190,w,creating?"Back to sign in":"Create a new account"))client_accounts_action(c,creating?ACCOUNT_LOGIN_MODE:ACCOUNT_CREATE_MODE);}
 }
 char message[78];snprintf(message,sizeof(message),"%.77s",busy?"Working securely with AmiSSL...":c->account_message);text(8,240,message,5);end();}
