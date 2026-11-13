/* Input protocol v1: only the server owns ranked boards, locks and clears. */
#include "server_internal.h"
#include "auth.h"
#include <limits.h>
#include <errno.h>

static int interval(Player *p) {
    static const int frames[]={48,43,38,33,28,23,18,13,8,6,5,5,5,4,4,4,3,3,3,2};
    int n=p->field.level-1;if(n<0)n=0;if(n>19)n=19;return frames[n]*16;
}
static int ready(Lobby *lb,Player *p) {
    return p->authority && p->connected && p->alive && !p->spectator && lb->game_state==STATE_PLAYING;
}
static void lost(Lobby *lb,Player *p) {
    char msg[40];snprintf(msg,sizeof(msg),"playerlost %d",p->slot);
    net_send(p->sock,msg);p->authority_internal=1;lobby_handle_message(lb,p,msg);p->authority_internal=0;
}
static int next_piece(Lobby *lb,Player *p) {
    if(p->sv_piece_tail-p->sv_piece_head<16)sv_gen_pieces(p,lb,32);
    if(p->sv_piece_head>=p->sv_piece_tail)return 0;
    piece_init(&p->sv_cur,p->sv_piece_queue[p->sv_piece_head++%64]);
    p->authority_drop=(long long)net_now_ms()+interval(p);p->authority_lock=0;p->authority_resets=0;
    return piece_spawn(&p->field,&p->sv_cur);
}
void authority_send(Lobby *lb,Player *p) {
    if(!p->authority || !p->connected)return;
    char board[FIELD_WIDTH*FIELD_HEIGHT+1],inventory[65],msg[MAX_MSG_LEN];int count=0;
    for(int i=p->sv_specials_head;i!=p->sv_specials_tail;i=(i+1)%64)inventory[count++]=SPECIAL_CHARS[p->sv_specials[i%64]-1];
    inventory[count]=0;field_encode(&p->field,board,sizeof(board));
    long long now=(long long)net_now_ms();
    int flags=(p->immunity_until>now?1:0)|(p->darkness_until>now?2:0)|(p->confusion_until>now?4:0)|(p->mutate_until>now?8:0);
    int next=p->sv_piece_queue[p->sv_piece_head%64];
    snprintf(msg,sizeof(msg),"statev1 %u %d %d %d %d %d %d %d %d %d %d %s %s %d",p->input_sequence,p->sv_cur.type,p->sv_cur.rotation,p->sv_cur.x,p->sv_cur.y,next,p->hold_type,p->hold_used,p->field.lines,p->field.score,p->field.level,count?inventory:"-",board,flags);
    net_send(p->sock,msg);
    Field shown=p->field;
    if(p->alive && field_valid(&shown,&p->sv_cur))field_lock(&shown,&p->sv_cur);
    field_encode(&shown,board,sizeof(board));snprintf(msg,sizeof(msg),"f %d %s",p->slot,board);lobby_broadcast(lb,msg,p->slot);
}
void authority_start(Lobby *lb,Player *p) {
    p->authority=p->input_capable;
    if(!p->authority)return;
    p->input_sequence=0;p->input_count=0;p->input_window=0;p->authority_last_lock=0;
    p->immunity_until=p->darkness_until=p->confusion_until=p->mutate_until=0;
    p->field.level=lb->starting_level;p->sv_piece_head=0;next_piece(lb,p);p->authority_tick=(long long)net_now_ms();
    authority_send(lb,p);
}
static void lock_piece(Lobby *lb,Player *p) {
    long long now=(long long)net_now_ms();
    if(now-p->authority_last_lock<50)return;
    p->authority_last_lock=now;
    if(!field_valid(&p->field,&p->sv_cur)){lost(lb,p);return;}
    field_lock(&p->field,&p->sv_cur);int cleared=field_clear_lines(&p->field);
    p->field.lines+=cleared;p->field.score+=cleared*cleared*100*p->field.level;
    auth_add_blocks(p->nick,4);
    if(cleared){char msg[32];snprintf(msg,sizeof(msg),"lineclear %d",cleared);p->authority_internal=1;lobby_handle_message(lb,p,msg);p->authority_internal=0;}
    else p->sv_combo=0;
    p->hold_used=0;
    if(!next_piece(lb,p)){lost(lb,p);return;}
}
void authority_input(Lobby *lb,Player *p,const char *sequence,const char *action) {
    if(!ready(lb,p)||lb->paused)return;
    if(strcmp(action,"left")&&strcmp(action,"right")&&strcmp(action,"cw")&&strcmp(action,"ccw")&&strcmp(action,"down")&&strcmp(action,"drop")&&strcmp(action,"hold"))return;
    if(sequence[0]<'0'||sequence[0]>'9')return;
    errno=0;char *end;unsigned long seq=strtoul(sequence,&end,10);
    if(errno || *end || seq>INT_MAX || seq<=p->input_sequence)return;
    p->input_sequence=(unsigned)seq;
    long long now=(long long)net_now_ms();
    if(now-p->input_window>=1000){p->input_window=now;p->input_count=0;}
    if(p->input_count>=120)return;
    p->input_count++;
    int changed=0,confused=p->confusion_until>now;
    if(!strcmp(action,"left"))changed=confused?piece_move_right(&p->field,&p->sv_cur):piece_move_left(&p->field,&p->sv_cur);
    else if(!strcmp(action,"right"))changed=confused?piece_move_left(&p->field,&p->sv_cur):piece_move_right(&p->field,&p->sv_cur);
    else if(!strcmp(action,"cw"))changed=confused?piece_rotate_ccw(&p->field,&p->sv_cur):piece_rotate_cw(&p->field,&p->sv_cur);
    else if(!strcmp(action,"ccw"))changed=confused?piece_rotate_cw(&p->field,&p->sv_cur):piece_rotate_ccw(&p->field,&p->sv_cur);
    else if(!strcmp(action,"down")){changed=piece_move_down(&p->field,&p->sv_cur);p->authority_drop=now+interval(p);}
    else if(!strcmp(action,"drop")){
        if(now-p->authority_last_lock<50)return;
        piece_drop(&p->field,&p->sv_cur);lock_piece(lb,p);
    } else if(!strcmp(action,"hold")&&lb->hold_enabled&&!p->hold_used){
        int type=p->sv_cur.type;
        if(p->hold_type<0){if(!next_piece(lb,p)){lost(lb,p);return;}}
        else {piece_init(&p->sv_cur,p->hold_type);if(!piece_spawn(&p->field,&p->sv_cur)){lost(lb,p);return;}}
        p->hold_type=type;p->hold_used=1;p->authority_lock=0;p->authority_drop=now+interval(p);
    }
    if(changed && p->authority_lock && p->authority_resets<15){p->authority_lock=now+lb->lock_delay_ms;p->authority_resets++;}
    if(ready(lb,p))authority_send(lb,p);
}
void authority_tick(Lobby *lb,Player *p) {
    if(!ready(lb,p))return;
    long long now=(long long)net_now_ms();
    if(now-p->authority_tick<16)return;
    p->authority_tick=now;
    if(lb->paused){p->authority_drop=now+interval(p);if(p->authority_lock)p->authority_lock=now+lb->lock_delay_ms;return;}
    /* Board-changing attacks can intersect the active piece; lift it or top out. */
    for(int n=0;n<FIELD_HEIGHT && !field_valid(&p->field,&p->sv_cur) && p->sv_cur.y>0;n++)p->sv_cur.y--;
    if(!field_valid(&p->field,&p->sv_cur)){lost(lb,p);return;}
    Piece below=p->sv_cur;below.y++;
    if(field_valid(&p->field,&below))p->authority_lock=0;
    else if(!p->authority_lock)p->authority_lock=now+lb->lock_delay_ms;
    if(p->authority_lock && now>=p->authority_lock){lock_piece(lb,p);}
    else if(!p->authority_lock){
        for(int n=0;n<16 && now>=p->authority_drop;n++){
            p->authority_drop+=interval(p);
            if(!piece_move_down(&p->field,&p->sv_cur)){p->authority_lock=now+lb->lock_delay_ms;break;}
        }
    }
    if(ready(lb,p))authority_send(lb,p);
}
