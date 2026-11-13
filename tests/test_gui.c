/* Real GUI network/handler lifecycle without opening a display or audio. */
#include "../clients/gui/src/client.h"
#include <assert.h>
static sock_t listener;
static int port;
static sock_t connect_pair(sock_t *remote) {
    sock_t s=net_connect("127.0.0.1",port); assert(s!=INVALID_SOCK);
    *remote=INVALID_SOCK;
    for(int i=0;i<1000 && *remote==INVALID_SOCK;i++) {*remote=accept(listener,NULL,NULL);if(*remote==INVALID_SOCK)SLEEP_MS(1);}
    assert(*remote!=INVALID_SOCK);
    net_set_nonblocking(*remote); return s;
}
int main(void) {
    SavedServer rows[4];
    char feed[]="LC-DIRECTORY-1\nmain\tMain server\t127.0.0.1\t31457\t2\t8\n";
    assert(directory_parse(feed,rows,4)==1 && rows[0].port==31457 && rows[0].capacity==8 && rows[0].discovered);
    assert(!rows[0].pw_hash[0] && !rows[0].nick[0]);
    char bad[]="LC-DIRECTORY-1\nevil\tEvil\tbad@host\t31457\t2\t8\n";assert(directory_parse(bad,rows,4)==-1);
    assert(!directory_url_valid("http://example.com/api/",1));assert(!directory_url_valid("https://user:password@example.com/api/",0));
    assert(directory_url_valid("https://example.com/api/",0));
    assert(net_init()); listener=net_listen(0); assert(listener!=INVALID_SOCK);
    struct sockaddr_in addr; socklen_t al=sizeof(addr);
    assert(getsockname(listener,(struct sockaddr *)&addr,&al)==0); port=ntohs(addr.sin_port);
    ClientCtx c; memset(&c,0,sizeof(c)); sock_t remote;
    c.server_count=2;c.server_list[0]=rows[0];strcpy(c.server_list[1].host,"second");c.server_list[1].port=31457;c.server_list[1].favourite=1;c.server_list[1].last_used=10;
    int order[MAX_SAVED_SERVERS];c.server_filter=1;assert(client_server_order(&c,order)==1 && order[0]==1);
    c.server_filter=2;assert(client_server_order(&c,order)==1 && order[0]==1);
    c.server_list[1].online=1;c.server_list[1].ping_ms=30;c.server_filter=4;assert(client_server_order(&c,order)==1 && order[0]==1);
    c.server_filter=0;c.server_sort=2;assert(client_server_order(&c,order)==2 && order[0]==1);
    c.server_cursor=0;c.have_saved_creds=1;strcpy(c.auth_pass_hash,"old password hash");strcpy(c.tf_pass.text,"old plaintext");
    client_on_button(&c,BTN_SRV_CONNECT);assert(c.state==CS_CONNECT && !c.auth_pass_hash[0] && !c.tf_pass.text[0] && !c.have_saved_creds);
    memset(&c,0,sizeof(c));
    c.sock=connect_pair(&remote); c.state=CS_PLAYING; c.room_id=1; c.slot=1;
    c.reconnect_window=3; c.reconnect_round=7; c.running=1;
    strcpy(c.auth_nick,"tester"); strcpy(c.nick,"tester"); memset(c.auth_pass_hash,'a',64);
    snprintf(c.tf_port.text,sizeof(c.tf_port.text),"%d",port);
    strcpy(c.tf_host.text,"127.0.0.1"); field_init(&c.my_field);
    c.my_field.cells[21][0]=1; piece_init(&c.cur,2); c.cur.x=4;
    Piece before=c.cur; sock_close(remote);
    for(int i=0;i<100 && !c.reconnect.active;i++) { client_network_tick(&c); SLEEP_MS(1); }
    assert(c.reconnect.active && c.state==CS_AUTH && c.my_field.cells[21][0]==1);
    assert(!memcmp(&before,&c.cur,sizeof(before)));
    c.reconnect.due=net_now_ms(); remote=INVALID_SOCK;
    for(int i=0;i<200 && remote==INVALID_SOCK;i++) {
        client_network_tick(&c); remote=accept(listener,NULL,NULL); SLEEP_MS(1);
    }
    assert(remote!=INVALID_SOCK); net_set_nonblocking(remote);
    for(int i=0;i<100 && c.sock==INVALID_SOCK;i++) { client_network_tick(&c); SLEEP_MS(1); }
    assert(c.sock!=INVALID_SOCK);
    client_handle_msg(&c,"authok existing");
    char rb[RECV_BUF_SIZE]="", line[MAX_MSG_LEN]; int len=0,joined=0;
    for(int i=0;i<100 && !joined;i++) {
        int n=net_recv(remote,rb,&len,line,sizeof(line));
        if(n==1 && !strcmp(line,"lineclash_join tester - 1 resume 7")) joined=1;
        SLEEP_MS(1);
    }
    assert(joined);
    client_handle_msg(&c,"playernum 1"); client_handle_msg(&c,"resume_ok 7 1");
    client_handle_msg(&c,"ingame"); client_handle_msg(&c,"resume_inventory n");
    assert(!c.reconnect.active && c.state==CS_PLAYING && c.my_field.special_count==1);
    assert(!memcmp(&before,&c.cur,sizeof(before)));
    sock_close(remote);
    for(int i=0;i<100 && !c.reconnect.active;i++) { client_network_tick(&c); SLEEP_MS(1); }
    assert(c.reconnect.active); client_on_key(&c,41,0,0,"");
    assert(!c.reconnect.active && c.sock==INVALID_SOCK && c.state==CS_SERVER_LIST);
    c.sock=connect_pair(&remote); c.state=CS_PLAYING; c.reconnect_blocked=0;
    client_handle_msg(&c,"kicked Test kick"); sock_close(remote);
    for(int i=0;i<100 && c.sock!=INVALID_SOCK;i++) { client_network_tick(&c); SLEEP_MS(1); }
    assert(!c.reconnect.active && c.sock==INVALID_SOCK);
    c.sock=connect_pair(&remote); c.state=CS_PLAYING; c.reconnect_blocked=0;
    c.last_received=net_now_ms()-5001; client_network_tick(&c);
    assert(c.reconnect.active); net_reconnect_stop(&c.reconnect); sock_close(remote);
    c.sock=connect_pair(&remote); c.state=CS_AUTH;
    net_reconnect_begin(&c.reconnect,net_now_ms(),3);
    client_handle_msg(&c,"autherr Wrong password");
    assert(!c.reconnect.active && c.sock==INVALID_SOCK); sock_close(remote);
    sock_close(listener); net_cleanup(); puts("GUI reconnect lifecycle passed"); return 0;
}
