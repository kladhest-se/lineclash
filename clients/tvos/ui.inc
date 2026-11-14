/* SPDX-License-Identifier: MIT
 * TV-specific screens. Included by the shared SDL renderer to reuse drawing,
 * audio and client actions without forking the game/network implementation. */
static void tv_text(int x,int y,const char *text,uint32_t colour){dt(x,y,text,colour,2);}
static void tv_base(const char *section){
    SDL_SetRenderDrawColor(g_rend,11,17,29,255);SDL_RenderClear(g_rend);g_rtick++;
    for(int i=0;i<14;i++){int x=(i*173+41)%WIN_W,y=(i*97+(int)(SDL_GetTicks()/90))%WIN_H;fr(x,y,24,24,i%2?0x142337ff:0x152c31ff);}
    brick_logo(64,44,16,4,directory_busy()||account_https_busy());dt(130,38,"LineClash",COL_WHITE,3);
    tv_text(132,84,section,0x8eabc6ff);fr(64,124,WIN_W-128,1,0x304057ff);
}
static int tv_button(int x,int y,int w,const char *label,int active,int enabled){return dbtn(x,y,w,58,label,enabled?(active?BS_ACTIVE:BS_NORMAL):BS_DISABLED);}
static int tv_field(int x,int y,int w,const char *label,TextField *field,int password,int enabled){
    tv_text(x,y,label,COL_LTGRAY);char value[128];
    if(password)snprintf(value,sizeof(value),"%s",field->len?"********":"Enter password");
    else snprintf(value,sizeof(value),"%.48s",field->len?field->text:"Select to enter");
    while(strlen(value)>3 && tw(value,2)>w-40)value[strlen(value)-1]=0;
    return tv_button(x,y+32,w,value,0,enabled) ? (lc_tvos_edit(field->text,sizeof(field->text),&field->len,password,label),1):0;
}
static void tv_notice(int x,int y,int width,const char *message){
    char line[160];const char *p=message;
    for(int row=0;*p && row<2;row++){
        size_t n=0,space=0;while(p[n] && n<sizeof(line)-1){line[n]=p[n];line[n+1]=0;if(tw(line,2)>width)break;if(p[n]==' ')space=n;n++;}
        if(p[n] && space)n=space;if(!n)n=1;memcpy(line,p,n);line[n]=0;tv_text(x,y+row*30,line,COL_LTGRAY);p+=n;while(*p==' ')p++;
    }
}
static void tv_settings(ClientCtx *c,int click){
    g_popup_blocked=0;g_mouse.clicked=click;tv_base("SETTINGS");
    if(tv_button(WIN_W-244,44,180,"Close",0,1))c->keymap_popup_open=0;
    const char *tabs[]={"Display","Controls","Audio"};
    for(int i=0;i<3;i++)if(tv_button(64+i*330,158,306,tabs[i],c->settings_tab==i,1))c->settings_tab=i;
    if(c->settings_tab==0){
        if(tv_button(64,280,600,c->piece_patterns?"Piece patterns: On":"Piece patterns: Off",0,1)){c->piece_patterns=!c->piece_patterns;client_save_settings(c);}
        if(tv_button(64,376,600,c->high_contrast?"High contrast: On":"High contrast: Off",0,1)){c->high_contrast=!c->high_contrast;client_save_settings(c);}
        if(tv_button(64,472,600,c->controller_enabled?"Gamepad input: On":"Gamepad input: Off",0,1)){c->controller_enabled=!c->controller_enabled;client_save_settings(c);}
        tv_notice(746,300,580,"Use either an Apple remote or an Xbox / PlayStation gamepad. The remote stays available when gamepad input is off.");
    }else if(c->settings_tab==1){
        tv_text(64,270,"APPLE REMOTE",0x75dabbff);tv_text(746,270,"GAMEPAD",0xbba5f4ff);
        const char *remote[]={"Left / Right: move","Down: soft drop   Up: hard drop","Click: rotate clockwise","Play/Pause: hold","Back / Menu: game actions"};
        const char *pad[]={"D-pad / left stick: move","A: rotate   B: reverse","X: hold   Y: hard drop","Right shoulder: next target","Right stick click: use special","Back / Share: game actions","Start / Options: pause"};
        for(int i=0;i<5;i++)tv_text(64,326+i*48,remote[i],COL_WHITE);
        for(int i=0;i<7;i++)tv_text(746,326+i*48,pad[i],COL_WHITE);
    }else{
        if(tv_button(64,280,600,music_get_enabled()?"Music: On":"Music: Off",0,1)){music_set_enabled(!music_get_enabled());client_save_settings(c);}
        if(tv_button(64,376,600,c->sound_enabled?"Sound effects: On":"Sound effects: Off",0,1)){c->sound_enabled=!c->sound_enabled;client_save_settings(c);}
        for(int i=0;i<2;i++){float v=i?sound_get_sfx_volume():music_get_volume();int y=280+i*160;char label[60];snprintf(label,sizeof(label),"%s volume: %d%%",i?"Effects":"Music",(int)(v*100));tv_text(746,y,label,COL_WHITE);
            int change=0;if(tv_button(746,y+46,180,"-",0,1))change=-1;if(tv_button(960,y+46,180,"+",0,1))change=1;
            if(change){v+=change*.1f;if(v<0)v=0;if(v>1)v=1;if(i)sound_set_sfx_volume(v);else music_set_volume(v);client_save_settings(c);}}
    }
    sdl_set_ui_options(100,c->piece_patterns,c->high_contrast);
}
static void tv_music(ClientCtx *c,int click){
    g_popup_blocked=0;g_mouse.clicked=click;tv_base("MUSIC PLAYER");MusicPlayback p;music_snapshot(&p);
    if(tv_button(WIN_W-244,44,180,"Close",0,1))c->music_player_open=0;
    if(tv_button(64,158,240,"Previous",0,1))music_previous();
    if(tv_button(328,158,240,p.paused?"Play":"Pause",1,1)){if(!p.enabled)music_set_enabled(1);else music_toggle_pause();client_save_settings(c);}
    if(tv_button(592,158,240,"Next",0,1))music_next();
    if(tv_button(856,158,300,p.shuffle?"Shuffle: On":"Shuffle: Off",0,1)){music_set_shuffle(!p.shuffle);client_save_settings(c);}
    int count=music_track_count()-1;if(c->music_scroll<0)c->music_scroll=0;if(c->music_scroll>=count)c->music_scroll=0;
    for(int row=0;row<4 && row+c->music_scroll<count;row++){int i=1+c->music_scroll+row;const MusicTrack *t=music_track(i);char label[160];snprintf(label,sizeof(label),"%.34s  /  %.26s",t->title,t->artist);if(tv_button(64,252+row*82,WIN_W-128,label,p.index==i,1)){music_select(i);client_save_settings(c);}}
    if(tv_button(64,606,220,"Earlier",0,c->music_scroll>0))c->music_scroll-=4;
    if(tv_button(308,606,220,"More tracks",0,c->music_scroll+4<count))c->music_scroll+=4;
    const MusicTrack *t=music_track(p.index);char credit[240];snprintf(credit,sizeof(credit),"%.60s / %.60s / %.24s",t->title,t->artist,t->license);tv_notice(64,700,WIN_W-128,credit);dt(64,764,t->source_url,COL_GRAY,1);
}
static void tv_overlays(ClientCtx *c,int click){
    static int previous;int current=c->keymap_popup_open?1:c->about_popup_open?2:c->music_player_open?3:0;
    if(current!=previous)click=0;previous=current;
    if(c->keymap_popup_open)tv_settings(c,click);
    else if(c->about_popup_open)render_about_popup(c,click);
    else if(c->music_player_open)tv_music(c,click);
    g_popup_blocked=0;
}
static int tv_has_overlay(ClientCtx *c){return c->keymap_popup_open||c->about_popup_open||c->music_player_open;}
static void tv_header_buttons(ClientCtx *c){
    if(tv_button(WIN_W-694,44,190,client_account_ready(c)?"Account":"Sign in",0,1)){tv_account_form=0;client_accounts_open(c,0);}
    if(tv_button(WIN_W-480,44,190,"Settings",0,1))c->keymap_popup_open=1;
    if(tv_button(WIN_W-266,44,202,"About",0,1))client_on_button(c,BTN_SRV_ABOUT);
}
static void tv_servers(ClientCtx *c){
    int click=g_mouse.clicked,overlay=tv_has_overlay(c)||c->server_popup_open;g_popup_blocked=overlay;if(overlay)g_mouse.clicked=0;
    tv_base("CHOOSE YOUR ARENA");tv_header_buttons(c);
    const char *tabs[]={"Public servers","Saved servers","Local network"};const BtnId actions[]={BTN_PUBLIC_SERVERS,BTN_SAVED_SERVERS,BTN_LAN_SERVERS};
    for(int i=0;i<3;i++)if(tv_button(64+i*330,158,306,tabs[i],c->server_tab==i,1))client_on_button(c,actions[i]);
    if(tv_button(1054,158,306,c->server_tab==1?"Add server":"Refresh",0,1))client_on_button(c,c->server_tab==1?BTN_SRV_ADD:c->server_tab==2?BTN_LAN_SERVERS:BTN_DISCOVER);
    int rows[64],count=0;for(int i=0;i<c->server_count;i++){SavedServer *ss=&c->server_list[i];if((c->server_tab==0&&ss->discovered&&!ss->lan)||(c->server_tab==1&&ss->locally_saved)||(c->server_tab==2&&ss->lan))rows[count++]=i;}
    if(c->srv_page<0)c->srv_page=0;if(c->srv_page*4>=count)c->srv_page=0;
    int selected_visible=0;for(int i=c->srv_page*4;i<count && i<c->srv_page*4+4;i++)if(rows[i]==c->server_cursor)selected_visible=1;
    if(count && !selected_visible)c->server_cursor=rows[c->srv_page*4];
    for(int row=0;row<4 && c->srv_page*4+row<count;row++){int i=rows[c->srv_page*4+row];SavedServer *ss=&c->server_list[i];char label[160];snprintf(label,sizeof(label),"%.32s    %.28s:%d",ss->name[0]?ss->name:ss->host,ss->host,ss->port);
        if(tv_button(64,260+row*82,WIN_W-128,label,c->server_cursor==i,1)){if(c->server_cursor==i)client_on_button(c,BTN_SRV_CONNECT);else c->server_cursor=i;}}
    if(!count){brick_logo(WIN_W/2-45,316,28,8,directory_busy());dtc(WIN_W/2,440,directory_busy()?"Finding servers...":"Your next match starts here.",COL_WHITE,3);}
    if(tv_button(64,614,210,"Previous",0,c->srv_page>0))c->srv_page--;
    if(tv_button(298,614,210,"Next page",0,(c->srv_page+1)*4<count))c->srv_page++;
    if(c->server_tab==1){if(tv_button(532,614,210,"Edit server",0,count>0))client_on_button(c,BTN_SRV_EDIT);if(tv_button(766,614,210,"Remove",0,count>0))client_on_button(c,BTN_SRV_DEL);}
    if(tv_button(WIN_W-388,614,324,"Join server",1,count>0))client_on_button(c,BTN_SRV_CONNECT);
    tv_notice(64,712,WIN_W-128,c->directory_message);
    if(c->server_popup_open){g_popup_blocked=0;g_mouse.clicked=click;tv_base(c->server_edit_idx>=0?"EDIT SAVED SERVER":"ADD A SERVER");int busy=c->srv_test_state==1||c->srv_test_state==2;
        tv_field(64,176,600,"Host / IP address",&c->tf_srv_host,0,!busy);tv_field(746,176,600,"Port",&c->tf_srv_port,0,!busy);
        tv_field(64,320,600,"Local username (private servers)",&c->tf_srv_nick,0,!busy);tv_field(746,320,600,"Local password",&c->tf_srv_pass,1,!busy);
        if(tv_button(64,516,600,"Save server",1,!busy))client_on_button(c,BTN_SRV_ADD_OK);
        if(tv_button(746,516,600,"Cancel",0,1)){c->server_popup_open=0;if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}c->srv_test_state=0;}
        tv_notice(64,644,WIN_W-128,c->srv_test_msg);
    }
    tv_overlays(c,click);tv_present();
}
static void tv_account(ClientCtx *c){
    int busy=account_https_busy();tv_base("YOUR ACCOUNT");g_popup_blocked=0;
    if(tv_button(WIN_W-274,44,210,"Back",0,!busy))client_accounts_action(c,ACCOUNT_BACK);
    if(client_account_ready(c)){
        dt(64,200,"Ready to play.",COL_WHITE,4);tv_text(64,292,c->central_profile,0x75dabbff);
        if(tv_button(64,414,600,"Browse servers",1,!busy))client_accounts_action(c,ACCOUNT_CONTINUE);
        if(tv_button(64,516,600,"Sign out",0,!busy))client_accounts_action(c,ACCOUNT_LOGOUT);
    }else if(c->account_pin_mode){
        dt(64,194,"Sign in with your phone.",COL_WHITE,3);tv_text(64,270,"Visit this address and enter the PIN:",COL_LTGRAY);
        dt(64,330,"lineclash.kladhest.se/pin",0xbba5f4ff,3);
        char pin[24];snprintf(pin,sizeof(pin),"%.4s  %.4s",c->account_pin,c->account_pin+4);dt(64,430,c->account_pin[0]?pin:"Getting a PIN...",0x75dabbff,5);
        if(tv_button(64,592,600,"Use email and password",0,1)){client_accounts_action(c,ACCOUNT_PIN_CANCEL);tv_account_form=1;}
        if(!c->account_pin_secret[0] && tv_button(746,592,600,"Get a new PIN",0,!busy))client_accounts_action(c,ACCOUNT_PIN_START);
    }else if(!tv_account_form && !c->account_create_mode){
        dt(64,194,"One account. Every arena.",COL_WHITE,3);tv_notice(64,278,1200,"Use your phone to sign in, or enter your details with the Apple TV keyboard.");
        if(tv_button(64,398,600,"Sign in with a phone / PIN",1,!busy))client_accounts_action(c,ACCOUNT_PIN_START);
        if(tv_button(64,496,600,"Sign in with email and password",0,!busy))tv_account_form=1;
        if(tv_button(64,594,600,"Create an account",0,!busy))client_accounts_action(c,ACCOUNT_CREATE_MODE);
    }else{
        int creating=c->account_create_mode;
        tv_field(64,172,600,creating?"Username":"Email or username",creating?&c->tf_account_handle:&c->tf_account_email,0,!busy);
        tv_field(746,172,600,creating?"Email":"Password",creating?&c->tf_account_email:&c->tf_account_password,!creating,!busy);
        if(creating){tv_field(64,320,600,"Password",&c->tf_account_password,1,!busy);tv_field(746,320,600,"Confirm password",&c->tf_account_confirm,1,!busy);}
        if(tv_button(64,496,600,busy?"Please wait...":creating?"Create account":"Sign in",1,!busy))client_accounts_action(c,creating?ACCOUNT_REGISTER:ACCOUNT_SIGNIN);
        if(tv_button(746,496,600,"Sign in with a phone / PIN",0,!busy)){client_accounts_action(c,ACCOUNT_LOGIN_MODE);tv_account_form=0;client_accounts_action(c,ACCOUNT_PIN_START);}
    }
    tv_notice(64,708,WIN_W-128,c->account_message);tv_present();
}
static void tv_connect(ClientCtx *c){
    tv_base(c->state==CS_AUTH?"CONNECTING":"JOIN SERVER");int busy=c->state==CS_AUTH;
    if(tv_button(WIN_W-274,44,210,"Back",0,1))client_on_button(c,BTN_BACK_CONNECT);
    char destination[192];snprintf(destination,sizeof(destination),"%.80s : %s",c->tf_host.text,c->tf_port.text);dt(64,190,destination,COL_WHITE,3);
    tv_field(64,300,600,"Local username",&c->tf_nick,0,!busy);tv_field(746,300,600,"Local password",&c->tf_pass,1,!busy);
    if(tv_button(64,480,600,busy?"Connecting...":"Connect",1,!busy))client_on_button(c,BTN_CONNECT);
    if(tv_button(746,480,600,"Use central account",0,!busy))client_on_button(c,BTN_CENTRAL_CONNECT);
    tv_notice(64,620,WIN_W-128,c->connect_error);tv_present();
}
static void tv_rooms(ClientCtx *c){
    int click=g_mouse.clicked;g_popup_blocked=tv_has_overlay(c);if(g_popup_blocked)g_mouse.clicked=0;tv_base("CHOOSE A ROOM");
    if(tv_button(WIN_W-274,44,210,"Back",0,1))client_on_button(c,BTN_BACK_CONNECT);
    if(!c->lobby_list_count)tv_text(64,220,"Waiting for the server...",COL_LTGRAY);
    int pages=(c->lobby_list_count+3)/4;if(c->lobby_page>=pages)c->lobby_page=0;
    for(int row=0;row<4 && c->lobby_page*4+row<c->lobby_list_count;row++){
        int i=c->lobby_page*4+row;LobbyInfo *room=&c->lobby_list[i];char label[160];snprintf(label,sizeof(label),"Room %d     %d / %d players     %s",room->id,room->player_count,room->max_players,room->in_game?"Playing":"Waiting");
        if(tv_button(64,196+row*94,WIN_W-128,label,c->lobby_cursor==i,1)){if(c->lobby_cursor==i)client_on_button(c,BTN_LOBBY_JOIN);else c->lobby_cursor=i;}}
    if(tv_button(64,626,250,"Previous",0,c->lobby_page>0))c->lobby_page--;
    if(tv_button(338,626,250,"Next page",0,c->lobby_page+1<pages))c->lobby_page++;
    if(tv_button(WIN_W-664,626,600,"Join room",1,c->lobby_list_count>0))client_on_button(c,BTN_LOBBY_JOIN);
    tv_overlays(c,click);tv_present();
}
static void tv_room(ClientCtx *c){
    int click=g_mouse.clicked;g_popup_blocked=tv_has_overlay(c);if(g_popup_blocked)g_mouse.clicked=0;
    char title[80];snprintf(title,sizeof(title),"ROOM %d / %s",c->room_id,tv_room_options?"ROOM RULES":"READY TO CLASH");tv_base(title);
    if(tv_button(WIN_W-274,44,210,"Leave room",0,1))client_on_button(c,BTN_BACK_TO_LOBBIES);
    if(tv_room_options){
        if(tv_button(64,190,600,c->no_attacks?"Specials: Off":"Specials: On",0,c->is_host))client_on_button(c,BTN_TOGGLE_ATTACKS);
        if(tv_button(746,190,600,c->hold_enabled?"Hold piece: On":"Hold piece: Off",0,c->is_host))client_on_button(c,BTN_TOGGLE_HOLD);
        if(tv_button(64,292,600,c->garbage_system?"Garbage: Guideline":"Garbage: Classic",0,c->is_host))client_on_button(c,BTN_TOGGLE_GARBAGE_SYSTEM);
        if(tv_button(746,292,600,c->last_player_wins?"Last player wins":"Endless round",0,c->is_host))client_on_button(c,BTN_TOGGLE_ENDLESS);
        if(tv_button(64,394,400,"Add easy bot",0,c->is_host&&c->bots_allowed))client_on_button(c,BTN_ADD_BOT_EASY);
        if(tv_button(488,394,400,"Add medium bot",0,c->is_host&&c->bots_allowed))client_on_button(c,BTN_ADD_BOT_MEDIUM);
        if(tv_button(912,394,434,"Add hard bot",0,c->is_host&&c->bots_allowed))client_on_button(c,BTN_ADD_BOT_HARD);
        if(tv_button(64,496,600,"Remove a bot",0,c->is_host))client_on_button(c,BTN_REMOVE_BOT);
        if(tv_button(746,496,600,"Return to room",1,1))tv_room_options=0;
    }else{
        tv_text(64,190,"PLAYERS",0x75dabbff);int row=0;
        for(int i=0;i<MAX_PLAYERS;i++)if(c->opp_nicks[i][0]){char label[100];snprintf(label,sizeof(label),"%.14s    %s%s",c->opp_nicks[i],c->opp_is_bot[i]?"AI / ":"",c->opp_ready[i]?"Ready":"Waiting");tv_text(64,246+row++*54,label,COL_WHITE);}
        if(tv_button(746,190,600,c->is_ready?"Ready — waiting for rivals":"I'm ready",1,!c->is_spectator))client_on_button(c,BTN_READY);
        if(c->is_host && tv_button(746,282,600,"Start round",1,1))client_on_button(c,BTN_START_GAME);
        if(tv_button(746,374,600,"Room rules & AI players",0,1))tv_room_options=1;
        if(tv_button(746,466,600,"Settings",0,1))c->keymap_popup_open=1;
        if(tv_button(746,558,600,"About LineClash",0,1))client_on_button(c,BTN_SRV_ABOUT);
        tv_field(64,664,600,"Team",&c->tf_team_lobby,0,1);if(tv_button(746,696,600,"Set team",0,1))client_on_button(c,BTN_SET_TEAM);
    }
    tv_overlays(c,click);tv_present();
}
static void tv_play(ClientCtx *c){
    int click=g_mouse.clicked;g_popup_blocked=tv_has_overlay(c)||g_tv_actions;if(g_popup_blocked)g_mouse.clicked=0;tv_base(c->paused?"ROUND PAUSED":"IN THE ARENA");
    char title[100];snprintf(title,sizeof(title),"%.14s  /  Room %d",c->nick,c->room_id);tv_text(746,70,title,0xbba5f4ff);
    dfield(64,156,25,&c->my_field,c->is_spectator?NULL:&c->cur,1);dr(62,154,304,554,0x75dabbff);
    dnext(412,156,280,112,&c->next);dhold(412,298,280,112,c->hold.type,c->hold_used,"Play/Pause / X");
    char stat[80];snprintf(stat,sizeof(stat),"Level %d   Lines %d",c->my_field.level,c->my_field.lines);tv_text(64,736,stat,COL_WHITE);
    tv_text(412,460,"SPECIALS",0x75dabbff);for(int i=0;i<c->my_field.special_count && i<6;i++){char sp[2]={SPECIAL_CHARS[c->my_field.specials[i]-1],0};dt(412+i*44,506,sp,COL_WHITE,3);}
    const char *target=c->attack_target>0?c->opp_nicks[c->attack_target-1]:"Choose in game menu";tv_text(412,580,"TARGET",0x75dabbff);tv_notice(412,624,300,target);
    tv_text(412,736,"Back / Menu: actions",COL_LTGRAY);
    int row=0;for(int i=0;i<MAX_PLAYERS;i++)if(c->opp_nicks[i][0] && i+1!=c->slot && !c->opp_spectator[i]){
        int x=746+(row%4)*158,y=170+(row/4)*300;char name[12];snprintf(name,sizeof(name),"%.10s",c->opp_nicks[i]);tv_text(x,y-30,name,c->opp_alive[i]?COL_WHITE:COL_GRAY);dfield(x,y,12,&c->opp_fields[i],NULL,0);if(c->attack_target==i+1)dr(x-3,y-3,150,270,0xbba5f4ff);row++;
    }
    if(!row)tv_text(746,210,"Waiting for rivals...",COL_GRAY);
    if(c->paused)dtc(214,400,"PAUSED",COL_WHITE,3);
    if(g_tv_actions){g_mouse.clicked=click;tv_actions(c);}
    tv_overlays(c,click);tv_present();
}
