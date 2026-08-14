/* server_cmd.c -- Unified admin command processor. */
#include "server_internal.h"
#include "ai.h"
#include "auth.h"
#include "config.h"
#include "../../shared/include/lineclash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/resource.h>

/* ── Network / activity stats ── */
unsigned long g_stat_bytes_sent    = 0;
unsigned long g_stat_msgs_sent     = 0;
unsigned long g_stat_bytes_recv    = 0;
unsigned long g_stat_msgs_recv     = 0;
unsigned long g_stat_field_updates = 0;
unsigned long g_stat_games_started = 0;
unsigned long g_stat_games_ended   = 0;
unsigned long g_stat_specials_fired= 0;
unsigned long g_stat_lines_cleared = 0;
unsigned long g_stat_kicks         = 0;
unsigned long g_stat_connects      = 0;
unsigned long g_stat_disconnects   = 0;
unsigned long g_stat_log_lines     = 0;
time_t        g_stat_start_time    = 0;

#define OUT(fmt,...) do{ char _ob[512]; snprintf(_ob,sizeof(_ob),fmt,##__VA_ARGS__); out(_ob,ctx); }while(0)

/* ── sha256 for mkusr ── */
typedef unsigned int _u32;typedef unsigned long long _u64;typedef unsigned char _u8;
typedef struct{_u32 h[8];_u8 buf[64];_u64 total;}_Sha256;
static const _u32 _K[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROTR(x,n)(((x)>>(n))|((x)<<(32-(n))))
#define S0(x)(ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define S1(x)(ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define G0(x)(ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define G1(x)(ROTR(x,17)^ROTR(x,19)^((x)>>10))
#define CH(x,y,z)(((x)&(y))^(~(x)&(z)))
#define MJ(x,y,z)(((x)&(y))^((x)&(z))^((y)&(z)))
static void _sinit(_Sha256*c){c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;c->total=0;}
static void _scomp(_Sha256*c,const _u8*b){_u32 w[64],a,b2,cc,d,e,f,g,h,t1,t2,i;for(i=0;i<16;i++)w[i]=(((_u32)b[i*4])<<24)|(((_u32)b[i*4+1])<<16)|(((_u32)b[i*4+2])<<8)|((_u32)b[i*4+3]);for(i=16;i<64;i++)w[i]=G1(w[i-2])+w[i-7]+G0(w[i-15])+w[i-16];a=c->h[0];b2=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];for(i=0;i<64;i++){t1=h+S1(e)+CH(e,f,g)+_K[i]+w[i];t2=S0(a)+MJ(a,b2,cc);h=g;g=f;f=e;e=d+t1;d=cc;cc=b2;b2=a;a=t1+t2;}c->h[0]+=a;c->h[1]+=b2;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;}
static void _supd(_Sha256*c,const _u8*data,size_t len){size_t i=c->total%64;c->total+=len;while(len--){c->buf[i++]=*data++;if(i==64){_scomp(c,c->buf);i=0;}}}
static void _sfin(_Sha256*c,_u8 d[32]){size_t i=c->total%64,j;c->buf[i++]=0x80;if(i>56){while(i<64)c->buf[i++]=0;_scomp(c,c->buf);i=0;}while(i<56)c->buf[i++]=0;_u64 bits=c->total*8;for(j=0;j<8;j++)c->buf[56+j]=(_u8)(bits>>((7-j)*8));_scomp(c,c->buf);for(i=0;i<8;i++){d[i*4]=(c->h[i]>>24)&0xff;d[i*4+1]=(c->h[i]>>16)&0xff;d[i*4+2]=(c->h[i]>>8)&0xff;d[i*4+3]=c->h[i]&0xff;}}
static void sha256_hex_cmd_(const char*input,char out[65]){_Sha256 ctx;_u8 dig[32];int i;_sinit(&ctx);_supd(&ctx,(const _u8*)input,strlen(input));_sfin(&ctx,dig);for(i=0;i<32;i++)snprintf(out+i*2,3,"%02x",(unsigned)dig[i]);out[64]='\0';}

static void lc(char*s){while(*s){*s=(char)tolower((unsigned char)*s);s++;}}

/* ── tokeniser ── */
static int tokenise(const char*line,char toks[][MAX_MSG_LEN],int max){
    int n=0; const char*p=line,*st=line;
    while(n<max){
        if(*p==' '||*p=='\0'){
            int len=(int)(p-st);
            if(len>=MAX_MSG_LEN)len=MAX_MSG_LEN-1;
            memcpy(toks[n],st,len);toks[n][len]='\0';n++;
            if(*p=='\0') { break; } else { st=p+1; }
        }
        p++;
    }
    return n;
}

/* ── kick helpers ── */
void kick_player_ip(const char *nick, char *ip_out, int ip_sz) {
    int li, pi;
    for (li=0; li<g_server.lobby_count; li++) {
        Lobby *lb = &g_server.lobbies[li];
        for (pi=0; pi<MAX_PLAYERS; pi++) {
            Player *p = &lb->players[pi];
            if (p->connected && !strcasecmp(p->nick, nick)) {
                strncpy(ip_out, p->ip, ip_sz-1); ip_out[ip_sz-1]='\0';
                return;
            }
        }
    }
    ip_out[0]='\0';
}

static int kick_player(const char *nick, const char *reason) {
    int li, pi, found=0;
    char buf[MAX_MSG_LEN];
    for (li=0; li<g_server.lobby_count; li++) {
        Lobby *lb = &g_server.lobbies[li];
        for (pi=0; pi<MAX_PLAYERS; pi++) {
            Player *p = &lb->players[pi];
            if (!p->connected || strcasecmp(p->nick,nick)) continue;
            snprintf(buf,sizeof(buf),"kicked %s",reason);
            net_send(p->sock,buf);
            net_send(p->sock,"noconnecting You have been kicked");
            sock_close(p->sock); p->sock=INVALID_SOCK;
            lobby_remove_player(lb,p);
            auth_add_kick_server(nick);
            found=1;
        }
    }
    for (int pk=0; pk<MAX_PENDING; pk++) {
        Pending *pend=&g_pending[pk];
        if (!pend->active||!pend->auth_ok||pend->is_tool) continue;
        if (strcasecmp(pend->pending_nick,nick)) continue;
        snprintf(buf,sizeof(buf),"kicked %s",reason);
        net_send(pend->sock,buf);
        sock_close(pend->sock); pend->active=0;
        found=1;
    }
    return found;
}

static void kick_ip(const char *ip){
    int li,pi;
    for(li=0;li<g_server.lobby_count;li++){
        Lobby *lb=&g_server.lobbies[li];
        for(pi=0;pi<MAX_PLAYERS;pi++){
            Player *p=&lb->players[pi];
            if(p->connected&&!strcmp(p->ip,ip)){
                net_send(p->sock,"kicked Your IP address has been banned");
                net_send(p->sock,"noconnecting Your IP has been banned");
                sock_close(p->sock); p->sock=INVALID_SOCK;
                lobby_remove_player(lb,p);
            }
        }
    }
    for(int pk=0;pk<MAX_PENDING;pk++){
        Pending *pend=&g_pending[pk];
        if(!pend->active||!strcmp(pend->ip,ip)) continue;
        if(strcmp(pend->ip,ip)) continue;
        net_send(pend->sock,"kicked Your IP address has been banned");
        net_send(pend->sock,"noconnecting Your IP has been banned");
        sock_close(pend->sock); pend->active=0;
    }
}

void schat_broadcast(const char *msg);

int kick_player_from_room(Lobby *lb, const char *nick, const char *by_nick, int *target_priv) {
    int pi, k;
    if (target_priv) *target_priv = 0;
    for (pi=0; pi<MAX_PLAYERS; pi++) {
        Player *tp = &lb->players[pi];
        if (!tp->connected || strcasecmp(tp->nick,nick)) continue;
        if (target_priv) *target_priv = tp->priv_level;
        if (tp->priv_level <= 2) return -1;
        char buf[MAX_MSG_LEN];
        snprintf(buf,sizeof(buf),"kicked Kicked from room by %s",by_nick);
        net_send(tp->sock,buf);
        net_send(tp->sock,"endgame");
        auth_add_kick_room(tp->nick);
        snprintf(buf,sizeof(buf),"pline [SERVER] %s was kicked from the room by %s",nick,by_nick);
        lobby_broadcast(lb,buf,-1);
        log_info("Room %d: %s kicked %s from room",lb->id,by_nick,nick);
        sock_t saved_sock = tp->sock;
        tp->sock = INVALID_SOCK;
        lobby_remove_player(lb,tp);
        for (k=0; k<MAX_PENDING; k++) {
            if (!g_pending[k].active) {
                memset(&g_pending[k],0,sizeof(g_pending[k]));
                strncpy(g_pending[k].pending_nick,nick,sizeof(g_pending[k].pending_nick)-1);
                g_pending[k].sock = saved_sock;
                g_pending[k].active = 1;
                g_pending[k].auth_ok = 1;
                g_pending[k].is_room_kicked = 1;
                auth_set_status(nick, "idle");
                send_lobby_list(saved_sock);
                return 1;
            }
        }
        sock_close(saved_sock);
        return 1;
    }
    return 0;
}

extern void schat_broadcast(const char *msg);


int server_exec_command(const char *line, const char *caller,
                        void (*out)(const char*, void*), void *ctx) {
    char toks[32][MAX_MSG_LEN];
    int n = tokenise(line, toks, 32);
    if (n == 0) return 0;
    char verb[64]; strncpy(verb, toks[0], 63); verb[63]='\0'; lc(verb);
    /* Strip leading slash — "remoteadmin" protocol prefixes commands with "/" */
    if (verb[0] == '/') memmove(verb, verb+1, strlen(verb));
    int li, pi;

    /* ================================================================
     * HELP
     * ================================================================ */
    if(!strcmp(verb,"help")||!strcmp(verb,"?")){
        OUT("LineClash v%s — Admin Command Reference", TETRINETZ_VERSION);
        out("", ctx);
        OUT("SERVER");
        OUT("  server version                      Version, platform and build info");
        OUT("  server path                         Config, database and log file paths");
        OUT("  server status                       Server status snapshot");
        OUT("  server motd set <text>              Change MOTD (persisted to config)");
        OUT("  server shutdown                     Shut down immediately");
        OUT("  server shutdown <minutes> [reason]  Timed shutdown with countdown");
        OUT("  server shutdown cancel              Cancel pending timed shutdown");
        OUT("  server set debug 1|0                Toggle verbose debug logging");
        out("", ctx);
        OUT("CONNECTIONS");
        OUT("  server connection list              List active connections with ID");
        OUT("  server connection kill <id>         Kill a connection by ID");
        out("", ctx);
        OUT("BOTS  (allow_bots in lineclashd.cfg controls availability)");
        OUT("  bot add <room_id> easy|medium|hard  Add a bot to a room");
        OUT("  bot remove <room_id> <slot>         Remove a bot by slot");
        OUT("  bot list [room_id]                  List active bots");
        out("", ctx);
        OUT("ROOMS");
        OUT("  room list                           Show all room status");
        OUT("  room set <room_id> specials 0|1     Enable/disable specials");
        OUT("  room set <room_id> last_wins 0|1    Enable/disable last-player-wins");
        OUT("  room set <room_id> taunts 0|1       Enable/disable taunts");
        OUT("  room set <room_id> speedup 0|1      Enable/disable speed-up lines");
        OUT("  room set <room_id> topic <text>     Set room topic");
        out("", ctx);
        OUT("USERS");
        OUT("  user list                           List online + registered users");
        OUT("  user list <username>                Show details for one user");
        OUT("  user add <username>                 Create user account (prompts for password)");
        OUT("  user remove <username>              Delete user account");
        OUT("  user set <username> password <pw>   Change password");
        OUT("  user set <username> hash <sha256>   Set password from hash");
        OUT("  user set <username> privilege <1-3> Change privilege (1=admin)");
        OUT("  user set <username> active 1|0      Enable or disable account");
        OUT("  user kick server <username> [reason]      Kick from server");
        OUT("  user kick room <room_id> <username> [reason]  Kick from room");
        out("", ctx);
        OUT("MESSAGING");
        OUT("  say server <msg>                    Send to lobby/server chat");
        OUT("  say room <room_id> <msg>            Send to players in a room");
        OUT("  say global <msg>                    Send to all rooms and lobby");
        out("", ctx);
        OUT("BANS");
        OUT("  ban list                            List all bans");
        OUT("  ban add user <username> [reason]    Ban user account");
        OUT("  ban add address <ip> [reason]       Ban IP address");
        OUT("  ban remove user <username>          Unban user");
        OUT("  ban remove address <ip>             Unban IP");
        out("", ctx);
        OUT("  Privilege levels: 1=admin  2=manager  3=user");
        return 0;
    }

    /* ================================================================
     * LSVERSION / LSPATH
     * ================================================================ */
    /* ================================================================
     * "server" subcommands:
     *   server version | paths | motd set <x> | shutdown [...]
     *   server set debug 1|0
     * Legacy single-word aliases also kept for scripting.
     * ================================================================ */

    /* Detect "server X" two-word commands: verb=="server", toks[1]=subcommand */
    const char *sub = (n>=2 && !strcmp(verb,"server")) ? toks[1] : "";

    /* server version */
    if(!strcmp(verb,"version")||!strcmp(verb,"lsversion")||
       (!strcmp(sub,"version")||!strcmp(sub,"ver"))) {
        OUT("LineClash Server v%s", TETRINETZ_VERSION);
#if defined(__APPLE__)
        OUT("Platform : macOS (POSIX)");
#elif defined(_WIN32)
        OUT("Platform : Windows");
#else
        OUT("Platform : Linux/POSIX");
#endif
        OUT("Built    : " __DATE__ " " __TIME__);
        OUT("Database : %s", auth_get_db_path());
        return 0;
    }

    /* server paths */
    if(!strcmp(verb,"path")||!strcmp(verb,"lspath")||!strcmp(verb,"dbpath")||
       !strcmp(sub,"path")||!strcmp(sub,"paths")) {
        OUT("Paths:");
        OUT("  Config  : %s", CFG_FILE);
        OUT("  Database: %s", auth_get_db_path());
        OUT("  Log     : %s", g_cfg.log_file[0] ? g_cfg.log_file : "(none)");
        return 0;
    }

    /* server motd set <text> */
    if((!strcmp(verb,"chmotd") && n>=2) ||
       (!strcmp(sub,"motd") && n>=4 && !strcasecmp(toks[2],"set"))) {
        const char *new_motd = !strcmp(verb,"chmotd") ? toks[1] : toks[3];
        strncpy(g_cfg.motd, new_motd, sizeof(g_cfg.motd)-1);
        g_cfg.motd[sizeof(g_cfg.motd)-1] = '\0';
        config_save_motd(g_cfg.motd); /* persist to config file */
        OUT("MOTD updated: %s", g_cfg.motd);
        log_info("[ADMIN %s] motd set: %s", caller, g_cfg.motd);
        return 0;
    }

    /* server set debug 1|0 */
    if(!strcmp(verb,"chdebug") ||
       (!strcmp(sub,"set") && n>=4 && !strcasecmp(toks[2],"debug"))) {
        int on;
        if (!strcmp(verb,"chdebug") && n>=3 && !strcasecmp(toks[1],"set"))
            on = atoi(toks[2]);
        else if (!strcmp(sub,"set") && n>=4)
            on = atoi(toks[3]);
        else {
            OUT("Debug: auth=%s  ingame=%s",
                g_cfg.debug_auth?"on":"off", g_cfg.debug_ingame?"on":"off");
            OUT("Usage: server set debug 1|0");
            return 0;
        }
        g_cfg.debug_auth = on; g_cfg.debug_ingame = on;
        OUT("Debug logging %s.", on?"ENABLED":"DISABLED");
        log_info("[ADMIN %s] debug set %d", caller, on);
        return 0;
    }
    /* legacy debug aliases */
    if(!strcmp(verb,"debug_auth")){
        if(n>=2){int on=(!strcmp(toks[1],"on")||!strcmp(toks[1],"1"));g_cfg.debug_auth=on;OUT("Auth debug %s.",on?"on":"off");}
        else OUT("Auth debug: %s",g_cfg.debug_auth?"on":"off");
        return 0;
    }
    if(!strcmp(verb,"debug_ingame")){
        if(n>=2){int on=(!strcmp(toks[1],"on")||!strcmp(toks[1],"1"));g_cfg.debug_ingame=on;OUT("In-game debug %s.",on?"on":"off");}
        else OUT("In-game debug: %s",g_cfg.debug_ingame?"on":"off");
        return 0;
    }

    /* ================================================================
     * CONNECTIONS (was lsconn)
     * ================================================================ */
    /* "server connection list" or legacy "connections" */
    int _is_conn_cmd = (!strcmp(verb,"connections") || !strcmp(verb,"lsconn")
        || (!strcmp(verb,"server") && n>=2 && !strcasecmp(toks[1],"connection")));
    const char *_conn_sub = (!strcmp(verb,"server")&&n>=3) ? toks[2] : "";
    int _do_conn_list = _is_conn_cmd && (!strcmp(_conn_sub,"")||!strcasecmp(_conn_sub,"list"));
    int _do_conn_kill = _is_conn_cmd && !strcasecmp(_conn_sub,"kill") && n>=4;

    if (_do_conn_kill) {
        /* server connection kill <id>  — id is the socket fd */
        int kill_id = atoi(toks[3]);
        int killed = 0;
        for(li=0;li<g_server.lobby_count && !killed;li++){
            Lobby *lb2=&g_server.lobbies[li];
            for(pi=0;pi<MAX_PLAYERS && !killed;pi++){
                Player *pp=&lb2->players[pi];
                if(!pp->connected || (int)pp->sock!=kill_id) continue;
                OUT("Killing connection %d (%s).", kill_id, pp->nick);
                log_info("[ADMIN %s] kill connection %d (%s)", caller, kill_id, pp->nick);
                net_send(pp->sock, "noconnecting Your connection was terminated by an administrator");
                sock_close(pp->sock); pp->sock=INVALID_SOCK;
                lobby_remove_player(lb2, pp);
                killed = 1;
            }
        }
        for(pi=0;pi<MAX_PENDING && !killed;pi++){
            Pending *pend=&g_pending[pi];
            if(!pend->active || (int)pend->sock!=kill_id) continue;
            OUT("Killing connection %d (%s).", kill_id, pend->pending_nick);
            log_info("[ADMIN %s] kill connection %d (%s)", caller, kill_id, pend->pending_nick);
            net_send(pend->sock, "noconnecting Your connection was terminated by an administrator");
            sock_close(pend->sock);
            memset(pend, 0, sizeof(*pend));
            killed = 1;
        }
        if(!killed) OUT("No connection with ID %d found.", kill_id);
        return 0;
    }
    if (_do_conn_list || _is_conn_cmd) {
        int cnt=0;
        OUT("%4s  %-20s %-8s %-12s %-8s %-10s %s",
            "ID","Username","Room","Status","Client","Platform","IP");
        OUT("%4s  %-20s %-8s %-12s %-8s %-10s %s",
            "----","--------","----","------","------","--------","--");
        for(li=0;li<g_server.lobby_count;li++){
            Lobby *lb2=&g_server.lobbies[li];
            for(pi=0;pi<MAX_PLAYERS;pi++){
                Player *pp=&lb2->players[pi];
                if(!pp->connected||pp->is_bot) continue;
                const char *st=lb2->game_state==STATE_PLAYING?(pp->alive?"Playing":"Eliminated"):"Idle";
                OUT("%4d  %-20s %-8s %-12s %-8s %-10s %s",
                    (int)pp->sock, pp->nick, lb2->name, st,
                    pp->client_type[0]?pp->client_type:"?",
                    pp->client_platform[0]?pp->client_platform:"?",
                    pp->ip); cnt++;
            }
        }
        for(pi=0;pi<MAX_PENDING;pi++){
            Pending *pend=&g_pending[pi];
            if(!pend->active||!pend->auth_ok) continue;
            OUT("%4d  %-20s %-8s %-12s %-8s %-10s %s",
                (int)pend->sock, pend->pending_nick, "-", "LobbySelect",
                pend->client_type[0]?pend->client_type:"?",
                pend->client_platform[0]?pend->client_platform:"?",
                pend->ip); cnt++;
        }
        OUT("%d connection(s). Use: server connection kill <id>", cnt);
        return 0;
    }

    /* ================================================================
     * MONITOR (live stats — interactive via admin tool)
     * ================================================================ */
    /* "server status" (two words) or just "serverstatus" */
    int is_server_status = (!strcmp(verb,"serverstatus") || !strcmp(verb,"lsmonitor") ||
        (n>=2 && !strcmp(verb,"server") &&
         (!strcmp(toks[1],"status")||!strcmp(toks[1],"stat"))));
    if(is_server_status){
        time_t now=time(NULL);
        long uptime=(long)(now-g_stat_start_time);
        int uph=uptime/3600, upm=(uptime%3600)/60, ups=uptime%60;
        double ups_d = uptime > 0 ? (double)uptime : 1.0;

        /* Count players and bots */
        int humans_online=0, bots_online=0, playing=0, spectators=0, pending_cnt=0;
        for(li=0;li<g_server.lobby_count;li++){
            Lobby *lb2=&g_server.lobbies[li];
            for(pi=0;pi<MAX_PLAYERS;pi++){
                Player *pp2=&lb2->players[pi];
                if(!pp2->connected) continue;
                if(pp2->is_bot) { bots_online++; continue; }
                if(pp2->spectator) spectators++;
                humans_online++;
                if(lb2->game_state==STATE_PLAYING&&pp2->alive) playing++;
            }
        }
        for(int pk=0;pk<MAX_PENDING;pk++) if(g_pending[pk].active) pending_cnt++;

        /* System resource usage */
        struct rusage ru; getrusage(RUSAGE_SELF,&ru);
        long mem_kb = ru.ru_maxrss;
#if defined(__APPLE__)
        mem_kb /= 1024;
#endif
        /* CPU time used by process */
        double cpu_user = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec/1e6;
        double cpu_sys  = ru.ru_stime.tv_sec + ru.ru_stime.tv_usec/1e6;
        double cpu_pct  = uptime > 0 ? (cpu_user+cpu_sys)/uptime*100.0 : 0.0;

        /* Games in progress */
        int rooms_active=0, rooms_idle=0, rooms_paused=0;
        for(li=0;li<g_server.lobby_count;li++){
            Lobby *lb2=&g_server.lobbies[li];
            if(lb2->game_state==STATE_PLAYING) rooms_active++;
            else rooms_idle++;
            if(lb2->paused) rooms_paused++;
        }

        OUT("+----------------------------------------------------------+");
        OUT("|  LineClash  Server  Status  v%-10s                  |", TETRINETZ_VERSION);
        OUT("+----------------------------------------------------------+");
        OUT("|  Uptime : %02dh %02dm %02ds                                    |", uph, upm, ups);
        OUT("|  Listen  : port %-5d                                   |", g_cfg.port);
        OUT("+----------------------------------------------------------+");
        OUT("+-- PLAYERS -----------------------------------------------+");
        OUT("|  Human  : %-4d   Bots : %-4d   Pending: %-4d             |", humans_online, bots_online, pending_cnt);
        OUT("|  Playing: %-4d   Spectators : %-4d                     |", playing, spectators);
        OUT("+----------------------------------------------------------+");
        OUT("+-- ROOMS -------------------------------------------------+");
        OUT("|  Active : %-4d   Idle : %-4d   Paused : %-4d             |", rooms_active, rooms_idle, rooms_paused);
        for(li=0;li<g_server.lobby_count;li++){
            Lobby *lb2=&g_server.lobbies[li];
            int ra=0, rb2=0, pi3;
            for(pi3=0;pi3<MAX_PLAYERS;pi3++){
                if(!lb2->players[pi3].connected) continue;
                if(lb2->players[pi3].is_bot) rb2++; else ra++;
            }
            char rstate[8];
            strncpy(rstate, lb2->game_state==STATE_PLAYING?(lb2->paused?"PAUSED":"PLAYING"):"IDLE", sizeof(rstate)-1);
            rstate[sizeof(rstate)-1]='\0';
            char rtopic[20]="";
            if(lb2->topic[0]) snprintf(rtopic,sizeof(rtopic),"[%.17s]",lb2->topic);
            OUT("|  Room %-2d  %-7s  %dH+%dB/%-2d  %-19s         |",
                lb2->id, rstate, ra, rb2, lb2->max_players, rtopic);
        }
        OUT("+----------------------------------------------------------+");
        OUT("+-- SYSTEM ------------------------------------------------+");
        OUT("|  RAM     : %6ld KB                                      |", mem_kb);
        OUT("|  CPU usr : %5.1f s  sys: %5.1f s  avg: %5.1f%%              |", cpu_user, cpu_sys, cpu_pct);
         OUT("|  IO r/w  : %-8lu / %-8lu blks                      |", (unsigned long)ru.ru_inblock, (unsigned long)ru.ru_oublock);
        OUT("+----------------------------------------------------------+");
        OUT("+-- NETWORK / LIFETIME ------------------------------------+");
        OUT("|  Connects   : %-8lu  Disconnects: %-8lu         |", g_stat_connects, g_stat_disconnects);
        OUT("|  Msgs  out  : %-8lu  (%.1f/s)                        |", g_stat_msgs_sent, g_stat_msgs_sent/ups_d);
        OUT("|  Bytes out  : %-8lu KB (%.1f KB/s)                   |", g_stat_bytes_sent/1024, (g_stat_bytes_sent/1024.0)/ups_d);
        OUT("|  Msgs  in   : %-8lu  (%.1f/s)                        |", g_stat_msgs_recv, g_stat_msgs_recv/ups_d);
        OUT("|  Bytes in   : %-8lu KB (%.1f KB/s)                   |", g_stat_bytes_recv/1024, (g_stat_bytes_recv/1024.0)/ups_d);
        OUT("|  Field upds : %-8lu  (%.1f/s)                        |", g_stat_field_updates, g_stat_field_updates/ups_d);
        OUT("+----------------------------------------------------------+");
        OUT("+-- GAMEPLAY ----------------------------------------------+");
        OUT("|  Games started : %-8lu                                  |", g_stat_games_started);
        OUT("|  Games ended   : %-8lu                                  |", g_stat_games_ended);
        OUT("|  Specials fired: %-8lu                                  |", g_stat_specials_fired);
        OUT("|  Lines cleared : %-8lu                                  |", g_stat_lines_cleared);
        OUT("|  Kicks         : %-8lu                                  |", g_stat_kicks);
        OUT("|  Log lines     : %-8lu  (%.1f/s)                        |", g_stat_log_lines, g_stat_log_lines/ups_d);
        OUT("|  Debug mode    : %-3s                                    |", g_cfg.debug_ingame?"ON":"off");
        OUT("+----------------------------------------------------------+");
        return 0;
    }


    /* ================================================================
     * LSUSR — list/show users
     * ================================================================ */
    if(!strcmp(verb,"dedup")){
        /* Delete duplicate/corrupt rows for a nick (case-insensitive).
         * Use when a nick has multiple rows due to case-mismatch re-registration. */
        if(n<2||!toks[1][0]){OUT("Usage: dedup <nick>");return 0;}
        const char *dnick=toks[1];
        char listbuf[2048]="";
        auth_list_all_nicks(listbuf,sizeof(listbuf),dnick);
        OUT("Rows matching '%s':\n%s",dnick,listbuf[0]?listbuf:"(none)");
        int deleted=auth_delete_corrupt_dupes(dnick);
        if(deleted<0) OUT("DB error during dedup.");
        else if(deleted==0) OUT("No corrupt rows deleted (0 or 1 row found).");
        else { OUT("Deleted %d corrupt row(s).",deleted);
                log_info("[ADMIN %s] dedup %s: deleted %d corrupt rows",caller,dnick,deleted); }
        return 0;
    }
    if(!strcmp(verb,"lsversion")||!strcmp(verb,"version")||
       (!strcmp(verb,"server")&&n>=2&&(!strcasecmp(toks[1],"version")||!strcasecmp(toks[1],"ver")))){
        OUT("LineClash Server v%s", TETRINETZ_VERSION);
#if defined(__APPLE__)
        OUT("Platform : macOS (POSIX)");
#elif defined(_WIN32)
        OUT("Platform : Windows");
#else
        OUT("Platform : Linux/POSIX");
#endif
        OUT("Database : %s", auth_get_db_path());
        OUT("Built    : " __DATE__ " " __TIME__);
        return 0;
    }
    if(!strcmp(verb,"dbpath")||!strcmp(verb,"lspath")||
       (!strcmp(verb,"server")&&n>=2&&(!strcasecmp(toks[1],"paths")||!strcasecmp(toks[1],"path")))){
        OUT("Paths:");
        OUT("  Config  : %s", CFG_FILE);
        OUT("  Database: %s", auth_get_db_path());
        OUT("  Log     : %s", g_cfg.log_file[0] ? g_cfg.log_file : "(none)");
        return 0;
    }
    /* "user X" dispatcher — new command style */
    int _is_user_cmd = !strcmp(verb,"user");
    const char *_user_sub = (_is_user_cmd && n>=2) ? toks[1] : "";
    /* Map new → old internally */
    if(_is_user_cmd){
        if(!strcasecmp(_user_sub,"list"))  { /* fall through to lsusr */ goto _do_lsusr; }
        if(!strcasecmp(_user_sub,"add")) {
            /* user add <username>  — create with privilege 3 */
            if(n<3){OUT("Usage: user add <username>");return 0;}
            const char *nick=toks[2];
            if(auth_nick_exists(nick)){OUT("User '%s' already exists.",nick);return 0;}
            /* Generate a random temp password so account exists; admin sets pw next */
            char tmpbuf[32]; snprintf(tmpbuf,sizeof(tmpbuf),"tmp%lu",(unsigned long)time(NULL));
            char hash[65]; sha256_hex_cmd_(tmpbuf,hash);
            if(auth_register(nick,hash)==AUTH_NEW_USER){
                auth_set_priv_level(nick,3);
                OUT("User '%s' created (privilege 3). Set password with: user set %s password <pw>",nick,nick);
                log_info("[ADMIN %s] user add %s",caller,nick);
            } else OUT("Failed to create '%s'.",nick);
            return 0;
        }
        if(!strcasecmp(_user_sub,"remove")) {
            if(n<3){OUT("Usage: user remove <username>");return 0;}
            const char *nick=toks[2];
            if(!auth_nick_exists(nick)){OUT("User '%s' not found.",nick);return 0;}
            int tpriv=auth_get_priv_level(nick);
            if(tpriv>0&&tpriv<=2){OUT("Cannot remove '%s': protected (priv %d).",nick,tpriv);return 0;}
            kick_player(nick,"Your account has been removed");
            auth_delete_user(nick);
            OUT("User '%s' removed.",nick);
            log_info("[ADMIN %s] user remove %s",caller,nick);
            return 0;
        }
        if(!strcasecmp(_user_sub,"set") && n>=5) {
            /* user set <username> password|hash|privilege|active <value> */
            const char *nick=toks[2];
            char prop[32]; strncpy(prop,toks[3],31); lc(prop);
            /* Translate "active" → "activated" for chusr logic */
            if(!strcmp(prop,"active")) strncpy(prop,"activated",31);
            if(!auth_nick_exists(nick)){OUT("User '%s' not found.",nick);return 0;}
            /* Inject chusr-style: shift toks so chusr handler can reuse */
            /* Build fake toks: [chusr, nick, set, prop, value] */
            char fake[6][MAX_MSG_LEN];
            strncpy(fake[0],"chusr",MAX_MSG_LEN-1);
            strncpy(fake[1],nick,MAX_MSG_LEN-1);
            strncpy(fake[2],"set",MAX_MSG_LEN-1);
            strncpy(fake[3],prop,MAX_MSG_LEN-1);
            strncpy(fake[4],n>=5?toks[4]:"",MAX_MSG_LEN-1);
            /* Call chusr logic directly */
            if(!strcmp(prop,"activated")){int val=atoi(fake[4]);int deactivate=!val;
                if(deactivate) {
                    auth_ban_nick(nick); kick_player(nick,"Account deactivated");
                    OUT("Account '%s' deactivated.",nick);
                    log_info("[ADMIN %s] user set %s active 0",caller,nick);
                } else {
                    auth_unban_nick(nick);
                    OUT("Account '%s' activated.",nick);
                    log_info("[ADMIN %s] user set %s active 1",caller,nick);
                }
                return 0;}
            if(!strcmp(prop,"password")){char hash2[65];sha256_hex_cmd_(fake[4],hash2);
                if(auth_reset_password(nick,hash2)){OUT("Password updated for '%s'.",nick);log_info("[ADMIN %s] user set %s password",caller,nick);}
                else {OUT("Failed to update password.");} return 0;}
            if(!strcmp(prop,"hash")){if(strlen(fake[4])!=64){OUT("Invalid hash (need 64 hex chars).");return 0;}
                if(auth_reset_password(nick,fake[4])){OUT("Hash updated for '%s'.",nick);log_info("[ADMIN %s] user set %s hash",caller,nick);}
                else {OUT("Failed.");} return 0;}
            if(!strcmp(prop,"privilege")){int priv=atoi(fake[4]);
                if(priv<1||priv>3){OUT("Privilege must be 1-3.");return 0;}
                /* Reuse chusr privilege logic by falling through */
                /* (copy-free: just set toks and jump) */
                for(int _li2=0;_li2<g_server.lobby_count;_li2++){
                    Lobby *_lb2=&g_server.lobbies[_li2];
                    for(int _pi2=0;_pi2<MAX_PLAYERS;_pi2++){
                        Player *_p2=&_lb2->players[_pi2];
                        if(!_p2->connected||strcasecmp(_p2->nick,nick)) continue;
                        _p2->priv_level=priv; _p2->is_admin=(priv==1)?1:0;
                        if(priv==1){net_send(_p2->sock,"adminok");net_send(_p2->sock,"gmsg *** Admin access granted");}
                        else{char _pb[32];snprintf(_pb,sizeof(_pb),"privlevel %d",priv);net_send(_p2->sock,_pb);
                            if(priv==2)net_send(_p2->sock,"gmsg *** Manager access granted");
                            else net_send(_p2->sock,"gmsg *** Privilege level updated");}
                    }
                }
                auth_set_priv_level(nick,priv);
                OUT("Set privilege %d for '%s'.",priv,nick);
                log_info("[ADMIN %s] user set %s privilege %d",caller,nick,priv);
                return 0;}
            OUT("Usage: user set <username> password|hash|privilege|active <value>");
            return 0;
        }
        if(!strcasecmp(_user_sub,"kick")) {
            /* user kick server <username> [reason] */
            /* user kick room <room_id> <username> [reason] */
            if(n<4){OUT("Usage: user kick server <username> [reason]\n       user kick room <room_id> <username> [reason]");return 0;}
            const char *kmode=toks[2]; /* "server" or "room" */
            int kick_from_server=(!strcasecmp(kmode,"server")||!strcasecmp(kmode,"srv"));
            int kick_from_room=(!strcasecmp(kmode,"room"));
            if(kick_from_server) {
                const char *knick=toks[3];
                const char *kreason=(n>=5)?toks[4]:"Kicked by admin";
                int ktpriv=auth_get_priv_level(knick);
                if(ktpriv>0&&ktpriv<=2){OUT("Cannot kick '%s': protected.",knick);return 0;}
                /* Kick from server — disconnect and remove */
                int kicked_count=kick_player(knick,kreason);
                if(kicked_count>0){OUT("Kicked '%s' from server.",knick);log_info("[ADMIN %s] user kick server %s",caller,knick);}
                else OUT("Player '%s' not found online.",knick);
                return 0;
            }
            if(kick_from_room && n>=5) {
                int kroom_id=atoi(toks[3]);
                const char *knick=toks[4];
                /* Delegate to existing kick room logic */
                /* Find the room, find the player, kick */
                Lobby *klb=server_find_lobby(&g_server,kroom_id);
                if(!klb){OUT("Room %d not found.",kroom_id);return 0;}
                int kfound=kick_player_from_room(klb,knick,caller,NULL);
                (void)kfound;
                OUT("Kick sent for '%s' from room %d.",knick,kroom_id);
                log_info("[ADMIN %s] user kick room %d %s",caller,kroom_id,knick);
                return 0;
            }
            OUT("Usage: user kick server <username> [reason]  OR  user kick room <room_id> <username> [reason]");
            return 0;
        }
        if(!strcasecmp(_user_sub,"list") || !_user_sub[0]) { /* show user list */ }
        else { OUT("Unknown user subcommand '%s'. See 'help'.",_user_sub); return 0; }
    }
_do_lsusr:;
    if(!strcmp(verb,"lsusr")){
        if(n>=2&&toks[1][0]){
            /* Detail view for a single user */
            const char *nick=toks[1];
            if(!auth_nick_exists(nick)){OUT("User '%s' not found.",nick);return 0;}
            char team[64]=""; auth_get_team(nick,team,sizeof(team));
            int wins=auth_get_wins(nick), priv=auth_get_priv_level(nick);
            OUT("  %-15s | wins:%-4d | priv:%d (%s)%s",
                nick, wins, priv,
                priv==1?"admin":priv==2?"mgr":"normal",
                auth_is_banned_nick(nick)?" [BANNED]":"");
            OUT("Nick      : %s",nick);
            OUT("Team      : %s",team[0]?team:"(none)");
            OUT("Wins      : %d",wins);
            OUT("Privilege : %d (%s)",priv,priv==1?"admin":priv==2?"mgr":"normal");
            OUT("Activated : %s",auth_is_activated(nick)?"yes":"NO -- account is deactivated");
            int found_online=0;
            for(li=0;li<g_server.lobby_count;li++){
                Lobby *lb=&g_server.lobbies[li];
                for(pi=0;pi<MAX_PLAYERS;pi++){
                    Player *p=&lb->players[pi];
                    if(p->connected&&!strcasecmp(p->nick,nick)){
                        OUT("Online    : %s  P%d  %s",lb->name,p->slot,
                            p->spectator?"Spectator":lb->game_state==STATE_PLAYING?"Playing":"Idle");
                        found_online=1;
                    }
                }
            }
            for(pi=0;pi<MAX_PENDING;pi++){
                Pending *pend=&g_pending[pi];
                if(pend->active&&pend->auth_ok&&!pend->is_tool&&!strcasecmp(pend->pending_nick,nick)){
                    OUT("Online    : [selecting lobby]"); found_online=1;
                }
            }
            if(!found_online) OUT("Online    : no (disconnected)");
            return 0;
        }

        /* List ALL registered users, cross-referenced with online state */
        char db_buf[16384];
        int db_count = auth_list_users(db_buf, sizeof(db_buf));

        /* Build online lookup: nick -> room ("Lobby" or "Room N") + status */
        typedef struct {
            char nick[MAX_NICK_LEN];
            char room[24];    /* "Lobby" | "Room N" */
            char conn[16];    /* "Online" | "Offline" */
            char status[16];  /* "Idle" | "Playing" | "Spectator" */
            char client[16];  /* "gui" | "cli" | "" */
            char platform[16];/* "macos" | "linux" | "windows" | "" */
        } OnlineEntry;
        OnlineEntry online[MAX_PLAYERS*MAX_LOBBIES+MAX_PENDING];
        int online_count=0;
        for(li=0;li<g_server.lobby_count;li++){
            Lobby *lb=&g_server.lobbies[li];
            for(pi=0;pi<MAX_PLAYERS;pi++){
                Player *p=&lb->players[pi];
                if(!p->connected||!p->nick[0]) continue;
                OnlineEntry *oe=&online[online_count++];
                memset(oe,0,sizeof(*oe));
                strncpy(oe->nick,p->nick,MAX_NICK_LEN-1);
                snprintf(oe->room,sizeof(oe->room),"Room %d",lb->id);
                strncpy(oe->conn,"Online",16);
                strncpy(oe->status,
                    p->spectator?"Spectator":lb->game_state==STATE_PLAYING?"Playing":"Idle",16);
                strncpy(oe->client,  p->client_type[0]?p->client_type:"-",    16);
                strncpy(oe->platform,p->client_platform[0]?p->client_platform:"-",16);
            }
        }
        for(pi=0;pi<MAX_PENDING;pi++){
            Pending *pend=&g_pending[pi];
            if(pend->active&&pend->auth_ok&&pend->pending_nick[0]&&!pend->is_tool){
                OnlineEntry *oe=&online[online_count++];
                memset(oe,0,sizeof(*oe));
                strncpy(oe->nick,pend->pending_nick,MAX_NICK_LEN-1);
                strncpy(oe->room,"Lobby",16);
                strncpy(oe->conn,"Online",16);
                strncpy(oe->status,"Idle",16);
                strncpy(oe->client,  pend->client_type[0]?pend->client_type:"-",    16);
                strncpy(oe->platform,pend->client_platform[0]?pend->client_platform:"-",16);
            }
        }

        /* Column header */
        OUT("%-20s %-8s %-12s %-12s %-6s %-8s %-4s",
            "Username","Room","Connection","Status","Client","Platform","Priv");
        OUT("%-20s %-8s %-12s %-12s %-6s %-8s %-4s",
            "--------","----","----------","------","------","--------","----");

        /* Walk DB, cross-reference online table */
        char *line=db_buf, *nl;
        int shown=0;
        while(line&&*line){
            nl=strchr(line,'\n'); if(nl)*nl='\0';
            if(!line[0]){line=nl?nl+1:NULL; continue;}

            char row_nick[MAX_NICK_LEN]={0};
            int ni=0;
            while(line[ni]&&line[ni]!=' '&&ni<MAX_NICK_LEN-1){row_nick[ni]=line[ni];ni++;}
            row_nick[ni]='\0';

            /* Defaults for offline user */
            const char *room_col   = "-";
            const char *conn_col   = "Offline";
            const char *status_col = "-";
            int oi;
            for(oi=0;oi<online_count;oi++){
                if(!strcasecmp(online[oi].nick,row_nick)){
                    room_col   = online[oi].room;
                    conn_col   = online[oi].conn;
                    status_col = online[oi].status;
                    break;
                }
            }

            int is_banned=(strstr(line,"[BANNED]")!=NULL);
            int priv=auth_get_priv_level(row_nick);
            const char *priv_label=priv==1?"Admin":priv==2?"Manager":"User";

            /* Find client info for this nick */
            const char *client_col="-", *platform_col="-";
            int oi2; for(oi2=0;oi2<online_count;oi2++)
                if(!strcasecmp(online[oi2].nick,row_nick))
                    {client_col=online[oi2].client;platform_col=online[oi2].platform;break;}
            OUT("%-20s %-8s %-12s %-12s %-6s %-8s %d (%s)%s",
                row_nick, room_col, conn_col, status_col,
                client_col, platform_col,
                priv, priv_label,
                !auth_is_activated(row_nick)?" [DEACTIVATED]":is_banned?" [BANNED]":"");
            shown++;
            line=nl?nl+1:NULL;
        }
        if(!shown&&!db_count){
            OUT("No registered users in database.");
        } else {
            int online_total=0;
            for(li=0;li<g_server.lobby_count;li++)
                for(pi=0;pi<MAX_PLAYERS;pi++)
                    if(g_server.lobbies[li].players[pi].connected) online_total++;
            OUT("%d registered user(s), %d online.",shown,online_total);
        }
        return 0;
    }

    /* ================================================================
     * MKUSR
     * ================================================================ */
    if(!strcmp(verb,"mkusr")){
        if(n<6){OUT("Usage: mkusr \"<nick>\" password \"<pw>\" privilege <1-3>");return 0;}
        const char *nick=toks[1], *pw=NULL; int priv=3, i;
        for(i=2;i<n-1;i++){
            char kw[32]; strncpy(kw,toks[i],31); lc(kw);
            if(!strcmp(kw,"password"))  pw=toks[i+1];
            if(!strcmp(kw,"privilege")) priv=atoi(toks[i+1]);
        }
        if(!pw){OUT("Missing: password \"<pw>\"");return 0;}
        if(priv<1||priv>3) priv=3;
        if(auth_nick_exists(nick)){OUT("User '%s' already exists.",nick);return 0;}
        char hash[65]; sha256_hex_cmd_(pw,hash);
        if(auth_register(nick,hash)==AUTH_NEW_USER){
            auth_set_priv_level(nick,priv); /* stores 1, 2, or 0 (for 3) */
            OUT("User '%s' created (privilege %d).",nick,priv);
            log_info("[ADMIN %s] mkusr %s priv=%d",caller,nick,priv);
        } else OUT("Failed to create '%s'.",nick);
        return 0;
    }

    /* ================================================================
     * CHUSR — change user properties
     * chusr "<nick>" set ban 1|0
     * chusr "<nick>" set password "<pw>"
     * chusr "<nick>" set privilege <1-3>
     * ================================================================ */
    if(!strcmp(verb,"chusr")){
        if(n<4){OUT("Usage: chusr \"<nick>\" set <property> <value>");return 0;}
        const char *nick=toks[1];
        char op[32],prop[32];
        strncpy(op,toks[2],31); lc(op);
        strncpy(prop,toks[3],31); lc(prop);
        if(!auth_nick_exists(nick)){OUT("User '%s' not found.",nick);return 0;}

        if((!strcmp(prop,"activated")||!strcmp(prop,"ban"))&&n>=5){
            int val=atoi(toks[4]);
            /* activated 1=enabled, 0=disabled; "ban" is legacy: 1=banned */
            int deactivate = !strcmp(prop,"ban") ? val : !val;
            if(deactivate){
                auth_ban_nick(nick);
                kick_player(nick,"Your account has been deactivated");
                OUT("Account '%s' deactivated.",nick);
                log_info("[ADMIN %s] chusr %s set activated 0",caller,nick);
            } else {
                auth_unban_nick(nick);
                OUT("Account '%s' activated.",nick);
                log_info("[ADMIN %s] chusr %s set activated 1",caller,nick);
            }
            return 0;
        }
        if(!strcmp(prop,"password")&&n>=5){
            char hash[65]; sha256_hex_cmd_(toks[4],hash);
            if(auth_reset_password(nick,hash)){
                OUT("Password updated for '%s'.",nick);
                log_info("[ADMIN %s] chusr %s set password",caller,nick);
            } else OUT("Failed to update password.");
            return 0;
        }
        if(!strcmp(prop,"hash")&&n>=5){
            /* Set password directly from a pre-computed sha256 hex string.
             * Useful when the plaintext password is unknown but the hash is
             * available (e.g. from ~/.lineclash_servers on the game client). */
            const char *hex=toks[4];
            if(strlen(hex)!=64){OUT("Invalid hash: must be exactly 64 hex characters.");return 0;}
            if(auth_reset_password(nick,hex)){
                OUT("Hash updated for '%s'.",nick);
                log_info("[ADMIN %s] chusr %s set hash",caller,nick);
            } else OUT("Failed to update hash (invalid hex?).");
            return 0;
        }
        if(!strcmp(prop,"fixhash")&&n>=5){
            /* Force-update hash using case-insensitive nick match.
             * Use when set password silently fails due to nick encoding mismatch. */
            const char *hex=toks[4];
            if(strlen(hex)!=64){OUT("Invalid hash: must be exactly 64 hex chars.");return 0;}
            int rows=auth_fix_hash(nick,hex);
            if(rows>0) OUT("Hash force-updated for '%s' (%d row(s)).",nick,rows);
            else        OUT("FAILED: nick '%s' not found (tried case-insensitive match).",nick);
            return 0;
        }
        if(!strcmp(prop,"dbhash")){
            /* Diagnostic: show the raw hash bytes stored in DB for this nick.
             * Use to detect corruption: a valid hash is always exactly 64 hex chars.
             * Also lists all nicks so you can spot case/encoding differences. */
            char rawhash[256]=""; int rawlen=0;
            if(auth_get_raw_hash(nick,rawhash,sizeof(rawhash),&rawlen)){
                OUT("DB hash for '%s': [%s] (%d bytes)",nick,rawhash,rawlen);
                if(rawlen==64) OUT("Hash looks valid (64 bytes).");
                else           OUT("WARNING: hash is %d bytes, expected 64 — CORRUPTED.",rawlen);
            } else { OUT("Nick '%s' not found (exact match). Listing all nicks:",nick);
                char nickbuf[4096]=""; auth_list_all_nicks(nickbuf,sizeof(nickbuf),NULL);
                OUT("%s",nickbuf);
            }
            return 0;
        }
        if(!strcmp(prop,"privilege")&&n>=5){
            int priv=atoi(toks[4]);
            if(priv<1||priv>3){OUT("Privilege must be 1-3.");return 0;}
            auth_set_priv_level(nick,priv); /* stores actual value in admin column */
            /* Update online player if present — in a room */
            for(li=0;li<g_server.lobby_count;li++){
                Lobby *lb=&g_server.lobbies[li];
                for(pi=0;pi<MAX_PLAYERS;pi++){
                    Player *p=&lb->players[pi];
                    if(!p->connected||strcasecmp(p->nick,nick)) continue;
                    p->priv_level=priv;
                    p->is_admin=(priv==1)?1:0;
                    if(priv==1){
                        net_send(p->sock,"adminok");
                        net_send(p->sock,"gmsg *** You have been granted admin access");
                    } else {
                        char plbuf[32];
                        snprintf(plbuf,sizeof(plbuf),"privlevel %d",priv);
                        net_send(p->sock,plbuf);
                        if(priv==2)
                            net_send(p->sock,"gmsg *** You have been granted manager access");
                        else
                            net_send(p->sock,"gmsg *** Your admin/manager access has been revoked");
                    }
                }
            }
            /* Also update player in lobby-select (pending) state */
            {int pk3;
             for(pk3=0;pk3<MAX_PENDING;pk3++){
                 Pending *pend2=&g_pending[pk3];
                 if(!pend2->active||!pend2->auth_ok||pend2->is_tool) continue;
                 if(strcasecmp(pend2->pending_nick,nick)) continue;
                 if(priv==1){
                     net_send(pend2->sock,"adminok");
                     net_send(pend2->sock,"gmsg *** You have been granted admin access");
                 } else {
                     char plbuf[32];
                     snprintf(plbuf,sizeof(plbuf),"privlevel %d",priv);
                     net_send(pend2->sock,plbuf);
                     if(priv==2)
                         net_send(pend2->sock,"gmsg *** You have been granted manager access");
                     else
                         net_send(pend2->sock,"gmsg *** Your admin/manager access has been revoked");
                 }
             }}
            OUT("Set privilege %d for '%s'.",priv,nick);
            log_info("[ADMIN %s] chusr %s set privilege %d",caller,nick,priv);
            return 0;
        }
        OUT("Usage: chusr \"<nick>\" set activated 1|0 | set password \"<pw>\" | set privilege <1-3>");
        return 0;
    }

    /* ================================================================
     * KICK
     * kick server <nick> [reason]   disconnect from server
     * kick <room_id> <nick> [reason] kick back to lobby
     * ================================================================ */
    if(!strcmp(verb,"kick")){
        if(n<3){OUT("Usage: kick server <nick> [<reason>]  OR  kick <room_id> <nick> [<reason>]");return 0;}
        int from_server=(!strcasecmp(toks[1],"server")||!strcasecmp(toks[1],"srv"));
        int room_id=from_server?0:atoi(toks[1]);
        const char *nick=toks[2];
        const char *reason=(n>=4)?toks[3]:"Kicked by admin";
        int tpriv=auth_get_priv_level(nick);
        if(tpriv>0&&tpriv<=2){OUT("Cannot kick '%s': protected (priv %d).",nick,tpriv);return 0;}
        if(from_server){
            int found=kick_player(nick,reason);
            if(found){
                char gbuf[MAX_MSG_LEN];
                snprintf(gbuf,sizeof(gbuf),"schat [SERVER] *** %s was kicked%s%s",
                         nick,reason[0]?" -- ":"",reason);
                schat_broadcast(gbuf);
                OUT("Kicked '%s' from server.",nick);
                log_info("[ADMIN %s] kick server %s reason='%s'",caller,nick,reason);
            } else { OUT("'%s' not online.",nick); }
        } else {
            Lobby *lb=server_find_lobby(&g_server,room_id);
            if(!lb){OUT("Room %d not found.",room_id);return 0;}
            int tpriv2=0;
            int found=kick_player_from_room(lb,nick,caller,&tpriv2);
            if(found==-1)  OUT("Cannot kick '%s': protected (priv %d).",nick,tpriv2);
            else if(found) { OUT("Kicked '%s' from Room %d.",nick,room_id); log_info("[ADMIN %s] kick room %d %s",caller,room_id,nick); }
            else           OUT("'%s' not found in Room %d.",nick,room_id);
        }
        return 0;
    }

    /* ================================================================
     * CHBAN — unified ban management
     * chban add user <nick> [reason]     ban user account
     * chban add address <ip> [reason]   ban IP address
     * chban remove user <nick>           unban user account
     * chban remove address <ip>          unban IP address
     * lsban                              list all bans
     * ================================================================ */
    /* "ban X" dispatcher */
    int _is_ban_cmd = !strcmp(verb,"ban");
    const char *_ban_sub = (_is_ban_cmd && n>=2) ? toks[1] : "";
    if(_is_ban_cmd) {
        if(!strcasecmp(_ban_sub,"list")) { strncpy(verb,"lsban",63); goto _do_lsban; }
        if(!strcasecmp(_ban_sub,"add")||!strcasecmp(_ban_sub,"remove")) {
            /* ban add|remove user|address <target> [reason] */
            /* Remap to chban: toks[0]=chban toks[1]=add/remove toks[2]=user/address toks[3]=target */
            if(n<4){OUT("Usage: ban add|remove user|address <target> [reason]");return 0;}
            strncpy(toks[0],"chban",MAX_MSG_LEN-1);
            /* toks[1]=sub toks[2]=what toks[3]=target already aligned */
            strncpy(verb,"chban",63);
            goto _do_chban;
        }
        OUT("Usage: ban list | ban add user|address <target> [reason] | ban remove user|address <target>");
        return 0;
    }
_do_lsban:;
    if(!strcmp(verb,"lsban")){
        OUT("=== User Account Bans ===");
        char dbuf[8192]; auth_list_users(dbuf,sizeof(dbuf));
        int found_banned=0;
        char *ln=dbuf,*nl;
        while((nl=strchr(ln,'\n'))){*nl='\0';
            if(ln[0]&&!auth_is_activated(ln)){
                OUT("  [DEACTIVATED] %s",ln); found_banned=1;
            }
            ln=nl+1;
        }
        if(!found_banned) OUT("  (no deactivated accounts)");
        out("", ctx);
        OUT("=== IP Address Bans ===");
        char ibuf[8192]; int icnt=auth_list_banned_ips(ibuf,sizeof(ibuf));
        if(!icnt) OUT("  (no banned IPs)");
        else {
            OUT("  %-18s %-12s %-16s %s","IP","Banned by","Date","Reason");
            char *iln=ibuf,*inl;
            while((inl=strchr(iln,'\n'))){*inl='\0';if(iln[0])OUT("  %s",iln);iln=inl+1;}
        }
        return 0;
    }
_do_chban:;
    if(!strcmp(verb,"chban")){
        if(n<3){
            OUT("Usage: chban add user <nick> [reason]");
            OUT("       chban add address <ip> [reason]");
            OUT("       chban remove user <nick>");
            OUT("       chban remove address <ip>");
            return 0;
        }
        char op[32]; strncpy(op,toks[1],31); lc(op);
        char what[32]; strncpy(what,toks[2],31); lc(what);
        int is_add=(!strcmp(op,"add")||!strcmp(op,"ban"));
        int is_remove=(!strcmp(op,"remove")||!strcmp(op,"unban"));
        int is_user=(!strcmp(what,"user")||!strcmp(what,"nick")||!strcmp(what,"account"));
        int is_addr=(!strcmp(what,"address")||!strcmp(what,"ip")||!strcmp(what,"addr"));
        if((!is_add&&!is_remove)||(!is_user&&!is_addr)||n<4){
            OUT("Usage: chban add|remove user|address <target> [reason]"); return 0;
        }
        const char *target=toks[3];
        const char *reason=(n>=5)?toks[4]:"Banned by admin";
        if(is_user){
            if(is_add){
                if(!auth_nick_exists(target)){OUT("User '%s' not found.",target);return 0;}
                int tpriv=auth_get_priv_level(target);
                if(tpriv>0&&tpriv<=2){OUT("Cannot ban '%s': protected.",target);return 0;}
                auth_ban_nick(target); auth_add_ban(target);
                kick_player(target,"Your account has been banned");
                OUT("User '%s' banned.",target);
                log_info("[ADMIN %s] chban add user %s reason='%s'",caller,target,reason);
                char gbuf[MAX_MSG_LEN];
                snprintf(gbuf,sizeof(gbuf),"schat [SERVER] *** %s has been banned",target);
                schat_broadcast(gbuf);
            } else {
                auth_unban_nick(target);
                OUT("User '%s' unbanned.",target);
                log_info("[ADMIN %s] chban remove user %s",caller,target);
            }
        } else { /* address */
            if(is_add){
                auth_ban_ip(target,reason,caller);
                kick_ip(target);
                OUT("IP %s banned and existing connections kicked.",target);
                log_info("[ADMIN %s] chban add address %s reason='%s'",caller,target,reason);
            } else {
                auth_unban_ip(target);
                OUT("IP ban removed: %s.",target);
                log_info("[ADMIN %s] chban remove address %s",caller,target);
            }
        }
        return 0;
    }
    /* legacy aliases */
    if(!strcmp(verb,"mkban")||!strcmp(verb,"banip")||!strcmp(verb,"rmban")||!strcmp(verb,"unbanip")){
        OUT("Use 'chban' command. See 'help' for usage."); return 0;
    }


    /* ================================================================
     * CHROOM / room set — room settings
     * ================================================================ */
_do_chroom:;
    if(!strcmp(verb,"chroom")){
        if(n<5){OUT("Usage: chroom <id> set specials|last_wins|taunts|speedup|topic on|off|\"text\"");return 0;}
        int room_id=atoi(toks[1]);
        char prop[32]; strncpy(prop,toks[3],31); lc(prop);
        Lobby *lb=server_find_lobby(&g_server,room_id);
        if(!lb){OUT("Room %d not found.",room_id);return 0;}

        if(!strcmp(prop,"specials")||!strcmp(prop,"attacks")||!strcmp(prop,"attack")){
            int on=(!strcasecmp(toks[4],"on")||!strcmp(toks[4],"1"));
            lb->no_attacks=on?0:1;
            auth_set_lobby_settings(room_id,lb->no_attacks,lb->last_player_wins,lb->speedup_enabled);
            char sbuf[MAX_MSG_LEN];
            snprintf(sbuf,sizeof(sbuf),"lobbysettings %d %d %d %d %d",
                     room_id,lb->no_attacks,lb->last_player_wins,lb->no_taunts,lb->speedup_enabled);
            lobby_broadcast(lb,sbuf,-1);
            OUT("Room %d: specials = %s",room_id,on?"on":"off");
            log_info("[ADMIN %s] chroom %d set specials %s",caller,room_id,on?"on":"off");
        } else if(!strcmp(prop,"last_wins")||!strcmp(prop,"last_player_wins")){
            int on=(!strcasecmp(toks[4],"on")||!strcmp(toks[4],"1"));
            lb->last_player_wins=on?1:0;
            auth_set_lobby_settings(room_id,lb->no_attacks,lb->last_player_wins,lb->speedup_enabled);
            char sbuf[MAX_MSG_LEN];
            snprintf(sbuf,sizeof(sbuf),"lobbysettings %d %d %d %d %d",
                     room_id,lb->no_attacks,lb->last_player_wins,lb->no_taunts,lb->speedup_enabled);
            lobby_broadcast(lb,sbuf,-1);
            OUT("Room %d: last_wins = %s",room_id,on?"on":"off");
            log_info("[ADMIN %s] chroom %d set last_wins %s",caller,room_id,on?"on":"off");
        } else if(!strcmp(prop,"taunts")){
            int on=(!strcasecmp(toks[4],"on")||!strcmp(toks[4],"1"));
            lb->no_taunts=on?0:1;
            char sbuf[MAX_MSG_LEN];
            snprintf(sbuf,sizeof(sbuf),"lobbysettings %d %d %d %d %d",
                     room_id,lb->no_attacks,lb->last_player_wins,lb->no_taunts,lb->speedup_enabled);
            lobby_broadcast(lb,sbuf,-1);
            OUT("Room %d: taunts = %s",room_id,on?"on":"off");
            log_info("[ADMIN %s] chroom %d set taunts %s",caller,room_id,on?"on":"off");
        } else if(!strcmp(prop,"topic")){
            /* Reconstruct topic from remaining tokens */
            char topic[256]=""; int ti;
            for(ti=4;ti<n&&strlen(topic)<250;ti++){
                if(ti>4) strncat(topic," ",sizeof(topic)-strlen(topic)-1);
                strncat(topic,toks[ti],sizeof(topic)-strlen(topic)-1);
            }
            strncpy(lb->topic,topic,sizeof(lb->topic)-1);
            auth_set_room_topic(room_id,topic);
            /* Broadcast topic to room players */
            {char sbuf[MAX_MSG_LEN];
             snprintf(sbuf,sizeof(sbuf),"gmsg *** Room topic: %s",topic[0]?topic:"(cleared)");
             lobby_broadcast(lb,sbuf,-1);}
            OUT("Room %d topic set: %s",room_id,topic[0]?topic:"(cleared)");
            log_info("[ADMIN %s] chroom %d set topic '%s'",caller,room_id,topic);
        } else if(!strcmp(prop,"speedup")||!strcmp(prop,"speed_up")){
            int on=(!strcasecmp(toks[4],"on")||!strcmp(toks[4],"1"));
            lb->speedup_enabled=on?1:0;
            auth_set_lobby_settings(room_id,lb->no_attacks,lb->last_player_wins,lb->speedup_enabled);
            /* Announce change to room */
            {char sbuf[MAX_MSG_LEN];
             snprintf(sbuf,sizeof(sbuf),"gmsg *** Speed-up: %s",on?"enabled":"disabled");
             lobby_broadcast(lb,sbuf,-1);}
            OUT("Room %d: speedup = %s (delay=%dmin interval=%ds lines=%d)",
                room_id,on?"on":"off",lb->speedup_delay,lb->speedup_interval,lb->speedup_lines);
            log_info("[ADMIN %s] chroom %d set speedup %s",caller,room_id,on?"on":"off");
        } else {
            OUT("Unknown property '%s'. Use: specials, last_wins, taunts, speedup, topic",prop);
        }
        return 0;
    }

    if(!strcmp(verb,"say")){
        /*
         * say server <msg>         — send to server/lobby chat
         * say room <id> <msg>      — send to players in a room
         * say global <msg>         — send to all rooms and lobby
         *
         * Sender displayed as [SERVER] in all cases.
         */
        if(n<3){
            OUT("Usage:");
            OUT("  say server <msg>             Send to lobby/server chat");
            OUT("  say room <room_id> <msg>     Send to players in a room");
            OUT("  say global <msg>             Send to all rooms and lobby");
            return 0;
        }
        char sbuf[MAX_MSG_LEN];
        const char *target=toks[1];

        /* say server (aliases: srv) */
        if(!strcasecmp(target,"server")||!strcasecmp(target,"srv")){
            const char *msg=toks[2];
            snprintf(sbuf,sizeof(sbuf),"schat [SERVER] %s",msg);
            schat_lobby(sbuf);
            OUT("-> server: %s",msg);
            log_info("[ADMIN say-server] %s",msg);
            return 0;
        }

        /* say global (aliases: all) */
        if(!strcasecmp(target,"global")||!strcasecmp(target,"all")){
            const char *msg=toks[2];
            snprintf(sbuf,sizeof(sbuf),"schat [SERVER] %s",msg);
            schat_lobby(sbuf);
            {int li2; char rbuf[MAX_MSG_LEN];
             snprintf(rbuf,sizeof(rbuf),"pline [SERVER] %s",msg);
             for(li2=0;li2<g_server.lobby_count;li2++)
                 lobby_broadcast(&g_server.lobbies[li2],rbuf,-1);}
            OUT("-> global: %s",msg);
            log_info("[ADMIN say-global] %s",msg);
            return 0;
        }

        /* say room <room_id> <msg> (new style) */
        if(!strcasecmp(target,"room")&&n>=4){
            int room_id=atoi(toks[2]);
            const char *msg=toks[3];
            Lobby *lb=server_find_lobby(&g_server,room_id);
            if(!lb){OUT("Room %d not found.",room_id);return 0;}
            snprintf(sbuf,sizeof(sbuf),"pline [SERVER] %s",msg);
            lobby_broadcast(lb,sbuf,-1);
            OUT("-> Room %d: %s",room_id,msg);
            log_info("[ADMIN say-room %d] %s",room_id,msg);
            return 0;
        }

        /* say <room_id> <msg> (legacy numeric room id) */
        {
            int room_id=atoi(target);
            const char *msg=toks[2];
            if(room_id<=0){OUT("Usage: say server|global <msg>  OR  say room <id> <msg>");return 0;}
            Lobby *lb=server_find_lobby(&g_server,room_id);
            if(!lb){OUT("Room %d not found.",room_id);return 0;}
            snprintf(sbuf,sizeof(sbuf),"pline [SERVER] %s",msg);
            lobby_broadcast(lb,sbuf,-1);
            OUT("-> Room %d: %s",room_id,msg);
            log_info("[ADMIN say-room %d] %s",room_id,msg);
        }
        return 0;
    }

    if(!strcmp(verb,"mkban")){
        if(n<2){OUT("Usage: mkban <nick> [<reason>]");return 0;}
        const char *nick=toks[1];
        const char *reason=(n>=3)?toks[2]:"Banned by admin";
        if(!auth_nick_exists(nick)){OUT("User '%s' not found.",nick);return 0;}
        int tpriv=auth_get_priv_level(nick);
        if(tpriv>0&&tpriv<=2){OUT("Cannot ban '%s': privilege %d accounts are protected.",nick,tpriv);return 0;}
        /* Deactivate account (set admin column to -1 = deactivated/banned) */
        auth_ban_nick(nick);
        auth_add_ban(nick);
        /* Get IP and ban it */
        char tip[48]="";
        kick_player_ip(nick,tip,sizeof(tip));
        if(tip[0]){
            auth_ban_ip(tip,reason,caller);
            kick_ip(tip);
        }
        kick_player(nick,reason);
        char gbuf[MAX_MSG_LEN];
        snprintf(gbuf,sizeof(gbuf),"schat [SERVER] *** %s has been banned by %s",nick,caller);
        schat_broadcast(gbuf);
        OUT("Banned '%s' (account deactivated%s).",nick,tip[0]?" + IP banned":"");
        log_info("[ADMIN %s] mkban %s reason='%s'%s",caller,nick,reason,tip[0]?" ip-banned":"");
        return 0;
    }
    if(!strcmp(verb,"rmban")||!strcmp(verb,"unbanip")){
        if(n<2){OUT("Usage: rmban <ip>");return 0;}
        auth_unban_ip(toks[1]);
        OUT("Removed IP ban: %s",toks[1]);
        log_info("[ADMIN %s] rmban %s",caller,toks[1]);
        return 0;
    }
    if(!strcmp(verb,"banip")||!strcmp(verb,"chblock")){
        /* Legacy aliases — kept for backwards compat */
        const char *ip,*reason2;
        if(!strcmp(verb,"chblock")){
            if(n<3){OUT("Usage: banip <ip> [<reason>] (or: mkban <nick>)");return 0;}
            char op[32]; strncpy(op,toks[1],31); lc(op);
            if(!strcmp(op,"remove")){auth_unban_ip(toks[2]);OUT("Removed IP ban: %s",toks[2]);return 0;}
            ip=toks[2]; reason2=(n>=4)?toks[3]:"banned by admin";
        } else {
            if(n<2){OUT("Usage: banip <ip> [<reason>]");return 0;}
            ip=toks[1]; reason2=(n>=3)?toks[2]:"banned by admin";
        }
        auth_ban_ip(ip,reason2,caller);
        kick_ip(ip);
        OUT("Banned IP %s.",ip);
        log_info("[ADMIN %s] banip %s reason='%s'",caller,ip,reason2);
        return 0;
    }
    if(!strcmp(verb,"debug_auth")){
        if(n>=2){
            int on=(!strcmp(toks[1],"on")||!strcmp(toks[1],"1"));
            g_cfg.debug_auth=on;
            OUT("Auth debug logging %s",on?"ENABLED":"DISABLED");
        } else {
            OUT("Auth debug: %s",g_cfg.debug_auth?"on":"off");
        }
        return 0;
    }
    if(!strcmp(verb,"debug_ingame")){
        if(n>=2){
            int on=(!strcmp(toks[1],"on")||!strcmp(toks[1],"1"));
            g_cfg.debug_ingame=on;
            OUT("In-game debug logging %s",on?"ENABLED":"DISABLED");
        } else {
            OUT("In-game debug: %s",g_cfg.debug_ingame?"on":"off");
        }
        return 0;
    }
    /* "room X" dispatcher */
    int _is_room_cmd = !strcmp(verb,"room");
    const char *_room_sub = (_is_room_cmd && n>=2) ? toks[1] : "";
    if(_is_room_cmd && !strcasecmp(_room_sub,"set") && n>=5){
        /* room set <id> <prop> <val> → reuse chroom logic */
        /* Shift args: toks[2]=id toks[3]=prop toks[4]=val */
        char _rt[6][MAX_MSG_LEN];
        strncpy(_rt[0],"chroom",MAX_MSG_LEN-1);
        strncpy(_rt[1],toks[2],MAX_MSG_LEN-1); /* id */
        strncpy(_rt[2],"set",MAX_MSG_LEN-1);
        strncpy(_rt[3],toks[3],MAX_MSG_LEN-1); /* prop */
        strncpy(_rt[4],n>=5?toks[4]:"",MAX_MSG_LEN-1); /* val */
        /* patch toks in place for chroom handler */
        for(int _ri=0;_ri<5;_ri++) strncpy(toks[_ri],_rt[_ri],MAX_MSG_LEN-1);
        strncpy(verb,"chroom",63); n=5;
        /* fall through to chroom */
        goto _do_chroom;
    }
    if(_is_room_cmd && (!strcasecmp(_room_sub,"list")||!_room_sub[0])){
        strncpy(verb,"lsroom",63); /* fall through to lsroom */
    }
    if(!strcmp(verb,"lsroom")||!strcmp(verb,"rooms")){
        for(li=0;li<g_server.lobby_count;li++){
            Lobby *lb=&g_server.lobbies[li]; int cnt=0;
            for(pi=0;pi<MAX_PLAYERS;pi++) if(lb->players[pi].connected) cnt++;
            OUT("Room %-2d  %d/%d  %-8s  Spec:%-4s  Last:%-4s  Taunt:%-4s  SpeedUp:%-3s%s%s",
                lb->id,cnt,lb->max_players,
                lb->game_state==STATE_PLAYING?"PLAYING":"WAITING",
                lb->no_attacks?"off":"on",
                lb->last_player_wins?"on":"off",
                lb->no_taunts?"off":"on",
                lb->speedup_enabled?"on":"off",
                lb->topic[0]?"  Topic: ":"",
                lb->topic[0]?lb->topic:"");
        }
        return 0;
    }

    /* ================================================================
     * SHUTDOWN — /shutdown [<minutes>] ["<reason>"]
     *   No args     : immediate shutdown, no message
     *   <minutes>   : timed countdown with periodic warnings
     *   <minutes> "<reason>" : countdown + reason in broadcast
     * ================================================================ */
    if(!strcmp(verb,"shutdown")||!strcmp(verb,"quit")||!strcmp(verb,"exit")||
       (!strcmp(verb,"server")&&n>=2&&!strcasecmp(toks[1],"shutdown"))){
        /* Arg offset for "server shutdown" */
        int _soff=(!strcmp(verb,"server"))?1:0;
        (void)_soff; /* used below */
        /* shutdown cancel — cancel a pending timed shutdown */
        if(n>=2 && !strcmp(toks[1],"cancel")){
            if(g_server.shutdown_ticks > 0){
                g_server.shutdown_ticks = 0;
                memset(g_server.shutdown_warned, 0, sizeof(g_server.shutdown_warned));
                g_server.shutdown_reason[0] = '\0';
                schat_broadcast("gmsg *** Server shutdown has been cancelled.");
                log_info("[ADMIN %s] shutdown cancelled", caller);
                OUT("Shutdown cancelled.");
            } else {
                OUT("No shutdown is currently scheduled.");
            }
            return 0;
        }
        int minutes = 0;
        char reason[256] = "";

        /* toks[1] = optional minutes or reason, toks[2] = optional reason */
        if(n >= 2 && toks[1][0] >= '0' && toks[1][0] <= '9') {
            minutes = atoi(toks[1]);
            if(n >= 3) strncpy(reason, toks[2], sizeof(reason)-1);
        } else if(n >= 2) {
            strncpy(reason, toks[1], sizeof(reason)-1);
        }

        log_info("[ADMIN %s] shutdown: minutes=%d reason='%s'",
                 caller, minutes, reason);

        if(minutes <= 0) {
            /* Immediate — no broadcast */
            OUT("Server shutting down immediately.");
            g_server.running = 0;
        } else {
            g_server.shutdown_ticks = minutes * 60 * 20; /* 20 ticks/s */
            memset(g_server.shutdown_warned, 0, sizeof(g_server.shutdown_warned));
            strncpy(g_server.shutdown_reason, reason,
                    sizeof(g_server.shutdown_reason)-1);

            char gbuf[MAX_MSG_LEN];
            if(reason[0])
                snprintf(gbuf, sizeof(gbuf),
                    "gmsg *** Server will shut down in %d minute%s — %s",
                    minutes, minutes==1?"":"s", reason);
            else
                snprintf(gbuf, sizeof(gbuf),
                    "gmsg *** Server will shut down in %d minute%s",
                    minutes, minutes==1?"":"s");
            schat_broadcast(gbuf);
            log_info("Shutdown scheduled in %d minute(s)", minutes);
            OUT("Shutdown scheduled in %d minute(s). Reason: %s",
                minutes, reason[0] ? reason : "(none)");
        }
        return 0;
    }

    /* ================================================================
     * BOT subcommands
     *   bot add <room_id> easy|medium|hard
     *   bot remove <room_id> <slot>
     *   bot list [room_id]
     * Legacy single-word aliases kept for scripting.
     * allow_bots is controlled via lineclashd.cfg (allow_bots = on|off)
     * ================================================================ */

    /* Normalise: "bot add ..." → verb="addbot" style */
    int _is_bot_cmd = !strcmp(verb,"bot");
    const char *_bot_sub = (_is_bot_cmd && n>=2) ? toks[1] : "";
    int _do_addbot    = !strcmp(verb,"addbot")
                     || (_is_bot_cmd && !strcasecmp(_bot_sub,"add"));
    int _do_removebot = !strcmp(verb,"removebot")
                     || (_is_bot_cmd && (!strcasecmp(_bot_sub,"remove")||!strcasecmp(_bot_sub,"rm")));
    int _do_listbots  = !strcmp(verb,"lsbots") || !strcmp(verb,"listbots")
                     || (_is_bot_cmd && (!strcasecmp(_bot_sub,"list")||!strcasecmp(_bot_sub,"ls")));

    if (_do_addbot) {
        if (!g_cfg.allow_bots) { OUT("Bots are disabled (allow_bots in lineclashd.cfg)."); return 0; }
        /* Arg offset: "bot add <room> <diff>" vs "addbot <room> <diff>" */
        int _aoff = (_is_bot_cmd && n>=4) ? 1 : 0; /* skip "add" token */
        if (n < 3+_aoff) { OUT("Usage: bot add <room_id> easy|medium|hard"); return 0; }
        int rid = atoi(toks[1+_aoff]);
        int diff = AI_EASY;
        if (!strcasecmp(toks[2+_aoff],"medium")) diff = AI_MEDIUM;
        else if (!strcasecmp(toks[2+_aoff],"hard")) diff = AI_HARD;
        Lobby *lb2 = server_find_lobby(&g_server, rid);
        if (!lb2) { OUT("Room %d not found.", rid); return 0; }
        if (lb2->game_state != STATE_MENU) { OUT("Cannot add bot during a game."); return 0; }
        int count = 0;
        for (int ci=0; ci<MAX_PLAYERS; ci++) if (lb2->players[ci].connected) count++;
        if (count >= lb2->max_players) { OUT("Room %d is full (%d/%d).", rid, count, lb2->max_players); return 0; }
        int slot = lobby_add_bot(lb2, diff);
        if (slot < 0) { OUT("Could not add bot."); return 0; }
        Player *bp = NULL;
        for (int ci=0; ci<MAX_PLAYERS; ci++)
            if (lb2->players[ci].connected && lb2->players[ci].slot==slot) { bp=&lb2->players[ci]; break; }
        if (bp) {
            /* Announce bot join to room */
            char jmsg[MAX_MSG_LEN];
            snprintf(jmsg,sizeof(jmsg),"playerjoin %d %s -",bp->slot,bp->nick);
            lobby_broadcast(lb2,jmsg,-1);
            /* Auto-ready bot */
            bp->ready = 1;
            snprintf(jmsg,sizeof(jmsg),"readystatus %d 1",bp->slot);
            lobby_broadcast(lb2,jmsg,-1);
            OUT("Added %s bot '%s' to room %d (slot %d).",
                diff==AI_EASY?"easy":diff==AI_MEDIUM?"medium":"hard",
                bp->nick, rid, slot);
            log_info("[ADMIN %s] addbot room=%d diff=%s slot=%d nick=%s",
                     caller, rid,
                     diff==AI_EASY?"easy":diff==AI_MEDIUM?"medium":"hard",
                     slot, bp->nick);
        }
        return 0;
    }
    if (!strcmp(verb,"removebot") || _do_removebot) {
        int _roff = (_is_bot_cmd && n>=4) ? 1 : 0;
        if (n < 3+_roff) { OUT("Usage: bot remove <room_id> <slot>"); return 0; }
        int rid = atoi(toks[1+_roff]), slot = atoi(toks[2+_roff]);
        Lobby *lb2 = server_find_lobby(&g_server, rid);
        if (!lb2) { OUT("Room %d not found.", rid); return 0; }
        int found = 0;
        for (int ci=0; ci<MAX_PLAYERS; ci++) {
            if (lb2->players[ci].connected && lb2->players[ci].is_bot && lb2->players[ci].slot==slot) {
                OUT("Removed bot '%s' from room %d.", lb2->players[ci].nick, rid);
                log_info("[ADMIN %s] removebot room=%d slot=%d nick=%s",
                         caller, rid, slot, lb2->players[ci].nick);
                lobby_remove_bot(lb2, slot);
                found = 1; break;
            }
        }
        if (!found) OUT("No bot in slot %d of room %d.", slot, rid);
        return 0;
    }
    if (_do_listbots) {
        int _loff = (_is_bot_cmd) ? 1 : 0;
        int rid = (n>=2+_loff) ? atoi(toks[1+_loff]) : 0;
        int any = 0;
        for (li=0; li<g_server.lobby_count; li++) {
            if (rid > 0 && g_server.lobbies[li].id != rid) continue;
            Lobby *lb2 = &g_server.lobbies[li];
            for (pi=0; pi<MAX_PLAYERS; pi++) {
                Player *bp = &lb2->players[pi];
                if (!bp->connected || !bp->is_bot) continue;
                OUT("Room %-2d  Slot %-2d  %-20s  %s  %s",
                    lb2->id, bp->slot, bp->nick,
                    bp->bot_difficulty==AI_EASY?"easy  ":
                    bp->bot_difficulty==AI_MEDIUM?"medium":"hard  ",
                    bp->alive?"playing":"waiting");
                any++;
            }
        }
        if (!any) { if(rid) OUT("No bots active in room %d.", rid); else OUT("No bots active."); }
        return 0;
    }

    return 1;  /* not recognised */
}
