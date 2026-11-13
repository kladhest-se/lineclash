/* server_api.c — Lightweight HTTP JSON status API for lineclashd
 *
 * Listens on a separate TCP port (default 31458).
 * Accepts any HTTP GET request and returns a JSON snapshot of server state.
 * No authentication — read-only, public status information only.
 *
 * Example:
 *   curl http://myserver:31458/
 *   curl http://myserver:31458/status
 *   curl http://myserver:31458/players
 *   curl http://myserver:31458/rooms
 *
 * All paths return the same full JSON document for simplicity.
 */
#include "../../shared/include/lineclash.h"
#include "server_internal.h"
#include "auth.h"
#include <sqlite3.h>
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ---- State shared with server.c ---- */
static sock_t g_api_sock = INVALID_SOCK;
#define API_CLIENTS 16
#define API_TIMEOUT 5
#define API_RESPONSE_LIMIT (512 * 1024)
typedef struct {
    int active;
    sock_t sock;
    time_t started;
    char request[512];
    int received;
    char *response;
    size_t length, sent;
} ApiClient;
static ApiClient api_clients[API_CLIENTS];

static void api_close(ApiClient *c) {
    if (c->active) sock_close(c->sock);
    free(c->response);
    memset(c, 0, sizeof(*c));
}


/* ---- JSON helpers ---- */
static char *g_jbuf = NULL;
static int   g_jlen = 0;
static int   g_jcap = 0;
static int   g_jfailed = 0;

static void j_reset(void) { g_jlen = 0; g_jfailed = 0; if (g_jbuf) g_jbuf[0] = 0; }
static int j_ensure(int extra) {
    if (g_jfailed) return 0;
    if (extra > API_RESPONSE_LIMIT - g_jlen) { g_jfailed = 1; return 0; }
    if (g_jlen + extra + 2 > g_jcap) {
        int cap = (g_jlen + extra + 2) * 2 + 4096;
        char *next = (char *)realloc(g_jbuf, cap);
        if (!next) { g_jfailed = 1; return 0; }
        g_jbuf = next; g_jcap = cap;
    }
    return 1;
}
static void j_raw(const char *s) {
    int l = (int)strlen(s);
    if (!j_ensure(l)) return;
    memcpy(g_jbuf + g_jlen, s, l); g_jlen += l; g_jbuf[g_jlen] = 0;
}
/* Preserve valid UTF-8 from config; legacy player text may be Latin-1. */
static int utf8_span(const unsigned char *s) {
    int n = s[0] >= 0xc2 && s[0] <= 0xdf ? 2 :
            s[0] >= 0xe0 && s[0] <= 0xef ? 3 :
            s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 : 0;
    int i;
    for (i = 1; i < n; i++) if ((s[i] & 0xc0) != 0x80) return 0;
    if (n == 3 && ((s[0] == 0xe0 && s[1] < 0xa0) || (s[0] == 0xed && s[1] >= 0xa0))) return 0;
    if (n == 4 && ((s[0] == 0xf0 && s[1] < 0x90) || (s[0] == 0xf4 && s[1] >= 0x90))) return 0;
    return n;
}
static void j_str(const char *s) {
    j_raw("\"");
    for (; *s; s++) {
        if      (*s == '"')  j_raw("\\\"");
        else if (*s == '\\') j_raw("\\\\");
        else if (*s == '\n') j_raw("\\n");
        else if (*s == '\r') j_raw("\\r");
        else if ((unsigned char)*s >= 0x80 && utf8_span((const unsigned char *)s)) {
            int n = utf8_span((const unsigned char *)s);
            if (!j_ensure(n)) return;
            memcpy(g_jbuf + g_jlen, s, n); g_jlen += n;
            g_jbuf[g_jlen] = 0; s += n - 1;
        }
        else if ((unsigned char)*s < 0x20 || (unsigned char)*s >= 0x80) {
            /* Player text uses Latin-1; emit valid Unicode JSON escapes. */
            char esc[7]; snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*s);
            j_raw(esc);
        }
        else { if (!j_ensure(1)) return; g_jbuf[g_jlen++] = *s; g_jbuf[g_jlen] = '\0'; }
    }
    j_raw("\"");
}
static void j_kv_str(const char *k, const char *v, int comma) {
    if (comma) j_raw(",");
    j_str(k); j_raw(":"); j_str(v);
}
static void j_kv_int(const char *k, int v, int comma) {
    char tmp[32]; snprintf(tmp, sizeof(tmp), "%d", v);
    if (comma) j_raw(",");
    j_str(k); j_raw(":"); j_raw(tmp);
}

/* ---- Build JSON response ---- */
static void build_json(void) {
    int i, j, li;
    time_t now = time(NULL);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    /* Count totals */
    int total_connected = 0, total_playing = 0, total_bots = 0;
    for (li = 0; li < g_server.lobby_count; li++) {
        Lobby *lb = &g_server.lobbies[li];
        for (j = 0; j < MAX_PLAYERS; j++) {
            if (!lb->players[j].connected) continue;
            if (lb->players[j].is_bot) { total_bots++; continue; }
            total_connected++;
            if (lb->game_state == STATE_PLAYING && lb->players[j].alive)
                total_playing++;
        }
    }
    for (i = 0; i < MAX_PENDING; i++)
        if (g_pending[i].active && g_pending[i].auth_ok) total_connected++;

    j_reset();
    j_raw("{");

    /* Server info */
    j_kv_str("server_name",    g_cfg.server_name,        0);
    j_kv_str("version",        TETRINETZ_VERSION,         1);
    j_kv_str("motd",           g_cfg.motd[0] ? g_cfg.motd : "", 1);
    j_kv_int("port",           g_cfg.port,                1);
    j_kv_int("status_port",    g_cfg.status_port,         1);
    j_kv_str("timestamp",      ts,                        1);

    /* Player counts */
    j_raw(","); j_str("players"); j_raw(":{");
    j_kv_int("online",      total_connected,                  0);
    j_kv_int("playing",     total_playing,                    1);
    j_kv_int("idle",        total_connected - total_playing,  1);
    j_kv_int("bots_active", total_bots,                       1);
    j_kv_int("registered",  auth_count_users(),               1);
    j_raw("}");

    /* Rooms */
    j_raw(","); j_str("rooms"); j_raw(":[");
    for (li = 0; li < g_server.lobby_count; li++) {
        Lobby *lb = &g_server.lobbies[li];
        int room_players = 0, room_playing = 0, room_specs = 0;
        int room_bots = 0, room_bots_easy = 0, room_bots_med = 0, room_bots_hard = 0;
        for (j = 0; j < MAX_PLAYERS; j++) {
            Player *rp = &lb->players[j];
            if (!rp->connected) continue;
            if (rp->is_bot) {
                room_bots++;
                if (rp->bot_difficulty == 0) room_bots_easy++;
                else if (rp->bot_difficulty == 1) room_bots_med++;
                else room_bots_hard++;
                continue;
            }
            if (rp->spectator) room_specs++;
            else room_players++;
            if (lb->game_state == STATE_PLAYING && rp->alive)
                room_playing++;
        }
        if (li) j_raw(",");
        j_raw("{");
        j_kv_int("id",         lb->id,          0);
        j_kv_str("state",      lb->game_state == STATE_PLAYING ? "playing" : "open", 1);
        j_kv_int("players",    room_players,    1);
        j_kv_int("spectators", room_specs,      1);
        j_kv_int("playing",    room_playing,    1);
        j_kv_int("bots",       room_bots,       1);
        j_kv_int("bots_easy",  room_bots_easy,  1);
        j_kv_int("bots_medium",room_bots_med,   1);
        j_kv_int("bots_hard",  room_bots_hard,  1);
        j_kv_int("max_players",g_cfg.max_players_per_room, 1);
        j_kv_int("round",      lb->round_num,   1);
        j_kv_int("paused",     lb->paused,      1);
        j_kv_int("no_attacks", lb->no_attacks,  1);
        j_kv_int("no_taunts",  lb->no_taunts,   1);
        j_kv_int("last_player_wins", lb->last_player_wins, 1);
        j_kv_int("speedup_enabled", lb->speedup_enabled,  1);
        j_kv_int("garbage_system",  lb->garbage_system,   1);
        j_kv_int("garbage_targeting", lb->garbage_targeting, 1);
        j_kv_int("lock_delay_ms",   lb->lock_delay_ms,    1);

        /* Per-room all-time player wins (persistent DB) */
        j_raw(","); j_str("room_player_wins"); j_raw(":[");
        {LbRow rw[20]; int rn=auth_get_room_player_wins(lb->id,rw,20),ri;
         for(ri=0;ri<rn;ri++){
             if(ri) j_raw(",");
             j_raw("{"); j_kv_str("nick",rw[ri].nick,0);
             j_kv_str("team",rw[ri].team,1); j_kv_int("wins",rw[ri].wins,1);
             j_raw("}");
         }}
        j_raw("]");

        /* Per-room all-time team wins (persistent DB) */
        j_raw(","); j_str("room_team_wins"); j_raw(":[");
        {TeamLbRow tw[20]; int tn=auth_get_room_team_wins(lb->id,tw,20),ti;
         for(ti=0;ti<tn;ti++){
             if(ti) j_raw(",");
             j_raw("{"); j_kv_str("team",tw[ti].team,0);
             j_kv_int("wins",tw[ti].wins,1);
             j_raw("}");
         }}
        j_raw("]");

        /* Session (in-memory) leaderboards — reset when room empties */
        j_raw(","); j_str("session_player_wins"); j_raw(":[");
        {int si;
         for(si=0;si<lb->lb_count;si++){
             if(si) j_raw(",");
             j_raw("{"); j_kv_str("nick",lb->lb_nicks[si],0);
             j_kv_int("wins",lb->lb_wins[si],1); j_raw("}");
         }}
        j_raw("]");
        j_raw(","); j_str("session_team_wins"); j_raw(":[");
        {int si;
         for(si=0;si<lb->lb_team_count;si++){
             if(si) j_raw(",");
             j_raw("{"); j_kv_str("team",lb->lb_team_nicks[si],0);
             j_kv_int("wins",lb->lb_team_wins[si],1); j_raw("}");
         }}
        j_raw("]");

        /* Player list for this room */
        j_raw(","); j_str("player_list"); j_raw(":[");
        int pfirst = 1;
        for (j = 0; j < MAX_PLAYERS; j++) {
            Player *p = &lb->players[j];
            if (!p->connected || !p->nick[0]) continue;
            if (!pfirst) j_raw(",");
            pfirst = 0;
            j_raw("{");
            j_kv_str("nick",   p->nick,       0);
            j_kv_str("team",   p->team[0] ? p->team : "", 1);
            j_kv_int("slot",   p->slot,       1);
            j_kv_int("is_bot", p->is_bot,     1);
            if (p->is_bot) {
                const char *dstr = p->bot_difficulty==0?"easy":
                                   p->bot_difficulty==1?"medium":"hard";
                j_raw(","); j_str("bot_difficulty"); j_raw(":");
                j_str(dstr);
            }
            j_kv_str("status", p->is_bot ? (lb->game_state==STATE_PLAYING?
                               (p->alive?"playing":"eliminated"):"idle") :
                               p->spectator ? "spectator" :
                               (lb->game_state == STATE_PLAYING ?
                                (p->alive ? "playing" : "eliminated") : "idle"), 1);
            j_kv_int("level",  p->field.level, 1);
            j_kv_int("lines",  p->field.lines, 1);
            j_kv_int("score",  p->field.score, 1);
            j_raw("}");
        }
        j_raw("]}");
    }
    j_raw("]");

    /* Lobby-select (not in a room) players */
    j_raw(","); j_str("lobby_players"); j_raw(":[");
    int pfirst = 1;
    for (i = 0; i < MAX_PENDING; i++) {
        if (!g_pending[i].active || !g_pending[i].auth_ok || !g_pending[i].pending_nick[0]) continue;
        if (!pfirst) j_raw(",");
        pfirst = 0;
        j_raw("{");
        j_kv_str("nick",   g_pending[i].pending_nick, 0);
        j_kv_str("status", "lobby",                   1);
        j_raw("}");
    }
    j_raw("]");

    /* Player leaderboard — top LEADERBOARD_MAX players by wins */
    {
        LbRow lb_rows[LEADERBOARD_MAX];
        int lb_count = auth_get_leaderboard(lb_rows, LEADERBOARD_MAX);
        j_raw(","); j_str("leaderboard"); j_raw(":[");
        for (i = 0; i < lb_count; i++) {
            char rank[8]; snprintf(rank, sizeof(rank), "%d", i + 1);
            if (i) j_raw(",");
            j_raw("{");
            j_kv_str("rank",    rank,               0);
            j_kv_str("nick",    lb_rows[i].nick,    1);
            j_kv_str("team",    lb_rows[i].team,    1);
            j_kv_int("wins",    lb_rows[i].wins,    1);
            j_kv_int("attacks", lb_rows[i].attacks, 1);
            j_raw("}");
        }
        j_raw("]");
    }

    /* Team leaderboard — top teams by total wins */
    {
        TeamLbRow tlb_rows[LEADERBOARD_MAX];
        int tlb_count = auth_get_team_leaderboard(tlb_rows, LEADERBOARD_MAX);
        j_raw(","); j_str("team_leaderboard"); j_raw(":[");
        for (i = 0; i < tlb_count; i++) {
            char rank[8]; snprintf(rank, sizeof(rank), "%d", i + 1);
            if (i) j_raw(",");
            j_raw("{");
            j_kv_str("rank",    rank,                  0);
            j_kv_str("team",    tlb_rows[i].team,      1);
            j_kv_int("wins",    tlb_rows[i].wins,      1);
            j_kv_int("members", tlb_rows[i].members,   1);
            j_raw("}");
        }
        j_raw("]");
    }

    /* User statistics — all users with any activity */
    {sqlite3_stmt *us=NULL;
     j_raw(","); j_str("user_stats"); j_raw(":[");
     {sqlite3*_db=auth_get_db();
     if(_db&&sqlite3_prepare_v2(_db,
         "SELECT nick,team,wins,attacks,taunts_sent,specials_sent,"
         "blocks_placed,lines_cleared,messages_sent,"
         "kicks_from_server,kicks_from_room,bans_received"
         " FROM users WHERE wins+attacks+taunts_sent+specials_sent+"
         "blocks_placed+lines_cleared+messages_sent+"
         "kicks_from_server+kicks_from_room+bans_received > 0"
         " ORDER BY wins DESC LIMIT 100;"
         ,-1,&us,NULL)==SQLITE_OK){
         int ufirst=1;
         while(sqlite3_step(us)==SQLITE_ROW){
             if(!ufirst){ j_raw(","); } ufirst=0;
             j_raw("{");
             {const char*n=(const char*)sqlite3_column_text(us,0);
              const char*t=(const char*)sqlite3_column_text(us,1);
              j_kv_str("nick",   n?n:"",0);
              j_kv_str("team",   t?t:"",1);}
             j_kv_int("wins",           sqlite3_column_int(us,2),1);
             j_kv_int("attacks",        sqlite3_column_int(us,3),1);
             j_kv_int("taunts_sent",    sqlite3_column_int(us,4),1);
             j_kv_int("specials_sent",  sqlite3_column_int(us,5),1);
             j_kv_int("blocks_placed",  sqlite3_column_int(us,6),1);
             j_kv_int("lines_cleared",  sqlite3_column_int(us,7),1);
             j_kv_int("messages_sent",  sqlite3_column_int(us,8),1);
             j_kv_int("kicks_from_server",sqlite3_column_int(us,9),1);
             j_kv_int("kicks_from_room",  sqlite3_column_int(us,10),1);
             j_kv_int("bans_received",    sqlite3_column_int(us,11),1);
             j_raw("}");
         }
         sqlite3_finalize(us);
     }}  /* close if+_db block */
    }    /* close outer stmt block */
    j_raw("]");

    /* Recent completed games */
    {RecentGame rg[50]; int rn=auth_get_recent_games(50,rg,50),ri;
     j_raw(","); j_str("recent_games"); j_raw(":[");
     for(ri=0;ri<rn;ri++){
         if(ri) j_raw(",");
         j_raw("{");
         j_kv_int("game_id",   rg[ri].game_id,   0);
         j_kv_int("room_id",   rg[ri].room_id,   1);
         j_kv_str("started_at",rg[ri].started_at,1);
         j_kv_str("ended_at",  rg[ri].ended_at,  1);
         j_kv_str("winner",    rg[ri].winner_nick,1);
         j_kv_str("winner_team",rg[ri].winner_team,1);
         j_kv_int("players",   rg[ri].player_count,1);
         j_raw("}");
     }}
    j_raw("]");

    /* Runtime server stats */
    j_raw(","); j_str("server_stats"); j_raw(":{");
    {long uptime=(long)(time(NULL)-g_stat_start_time);
     j_kv_int("uptime_seconds",    (int)uptime,                   0);
     j_kv_int("games_started",     (int)g_stat_games_started,     1);
     j_kv_int("games_ended",       (int)g_stat_games_ended,       1);
     j_kv_int("specials_fired",    (int)g_stat_specials_fired,    1);
     j_kv_int("lines_cleared",     (int)g_stat_lines_cleared,     1);
     j_kv_int("connects",          (int)g_stat_connects,          1);
     j_kv_int("disconnects",       (int)g_stat_disconnects,       1);
     j_kv_int("msgs_sent",         (int)g_stat_msgs_sent,         1);
     j_kv_int("msgs_recv",         (int)g_stat_msgs_recv,         1);
     j_kv_int("bytes_sent",        (int)g_stat_bytes_sent,        1);
     j_kv_int("bytes_recv",        (int)g_stat_bytes_recv,        1);
     j_kv_int("kicks",             (int)g_stat_kicks,             1);
     j_kv_int("bots_allowed",      g_cfg.allow_bots,              1);
     j_kv_int("default_garbage_system",   g_cfg.garbage_system,    1);
     j_kv_int("default_garbage_targeting",g_cfg.garbage_targeting,  1);
     j_kv_int("btb_bonus",         g_cfg.btb_bonus,               1);
     j_kv_int("combo_cap",         g_cfg.combo_cap,               1);
     j_kv_int("perfect_clear_bonus",g_cfg.perfect_clear_bonus,    1);
     j_kv_int("garbage_hole_style",g_cfg.garbage_hole_style,      1);
     j_kv_int("lock_delay_ms",     g_cfg.lock_delay_ms,           1);
    }
    j_raw("}}"); /* server_stats and root */
}

/* ── HTTP listener ────────────────────────────────────────────────────────── */

void server_api_init(void) {
    if (!g_cfg.status_api || g_cfg.status_port <= 0) return;
    g_api_sock = net_listen(g_cfg.status_port);
    if (g_api_sock == INVALID_SOCK)
        log_info("API: failed to listen on port %d", g_cfg.status_port);
    else
        log_info("API: listening on port %d", g_cfg.status_port);
}

void server_api_shutdown(void) {
    int i; for (i = 0; i < API_CLIENTS; i++) api_close(&api_clients[i]);
    if (g_api_sock != INVALID_SOCK) { sock_close(g_api_sock); g_api_sock = INVALID_SOCK; }
    free(g_jbuf); g_jbuf = NULL; g_jcap = g_jlen = 0;
}

sock_t server_api_get_sock(void) { return g_api_sock; }

/* All accepted sockets are explicitly nonblocking on every platform. */
void server_api_accept(void) {
    struct sockaddr_in sa; socklen_t sl = sizeof(sa);
    sock_t client = accept(g_api_sock, (struct sockaddr *)&sa, &sl);
    int i;
    if (client == INVALID_SOCK) return;
    net_set_nonblocking(client);
    for (i = 0; i < API_CLIENTS; i++) {
        ApiClient *c = &api_clients[i];
        if (c->active) continue;
        c->active = 1; c->sock = client; c->started = time(NULL);
        return;
    }
    sock_close(client);
}

static int api_retry(void) {
#ifdef PLATFORM_WINDOWS
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

/* Called every game-loop iteration, even when the listener has no activity.
 * Slots, header size, response size and total lifetime are bounded. */
void server_api_poll(void) {
    int i;
    for (i = 0; i < API_CLIENTS; i++) {
        ApiClient *c = &api_clients[i];
        if (!c->active) continue;
        if (time(NULL) - c->started >= API_TIMEOUT) { api_close(c); continue; }
        if (!c->response) {
            int n = (int)recv(c->sock, c->request + c->received,
                             sizeof(c->request) - c->received - 1, 0);
            if (n <= 0) {
                if (n < 0 && api_retry()) continue;
                api_close(c); continue;
            }
            if (memchr(c->request + c->received, 0, n)) { api_close(c); continue; }
            c->received += n; c->request[c->received] = 0;
            if (!strstr(c->request, "\r\n\r\n")) {
                if (c->received == sizeof(c->request) - 1) api_close(c);
                continue;
            }
            build_json();
            if (g_jfailed || g_jlen > API_RESPONSE_LIMIT) { api_close(c); continue; }
            {
                char hdr[256];
                int hlen = snprintf(hdr, sizeof(hdr),
                    "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\nContent-Length: %d\r\n"
                    "Connection: close\r\n\r\n", g_jlen);
                c->length = hlen + g_jlen;
                c->response = (char *)malloc(c->length);
                if (!c->response) { api_close(c); continue; }
                memcpy(c->response, hdr, hlen);
                memcpy(c->response + hlen, g_jbuf, g_jlen);
            }
        }
        {
#ifdef PLATFORM_WINDOWS
            int n = send(c->sock, c->response + c->sent, (int)(c->length - c->sent), 0);
#else
            int n = (int)send(c->sock, c->response + c->sent, c->length - c->sent, MSG_NOSIGNAL);
#endif
            if (n > 0) {
                c->sent += n;
                if (c->sent == c->length) api_close(c);
            } else if (n == 0 || !api_retry()) api_close(c);
        }
    }
}
