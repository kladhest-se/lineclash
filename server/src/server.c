/* server.c - LineClash server */
#include "../../shared/include/lineclash.h"
#include "auth.h"
#include "config.h"
#include "server_internal.h"
#include "ai.h"
#include "server_cmd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/* MAX_PENDING defined in server_internal.h */

Server  g_server;          /* non-static: shared with server_cmd.c */
Pending g_pending[MAX_PENDING];

/* ---- In-game debug helpers ---- */
static const char *sp_name(char sp_char) {
    switch (sp_char) {
        case 'a': return "AddLine";
        case 'c': return "ClearLine";
        case 'b': return "ClearSpecial";
        case 'r': return "RandomClear";
        case 'o': return "BlockBomb";
        case 'q': return "QuakeField";
        case 'g': return "Gravity";
        case 's': return "SwitchField";
        case 'n': return "NukeField";
        case 'i': return "Immunity";
        case 'v': return "ClearColumn";
        case 'm': return "Mutate";
        case 'd': return "Darkness";
        case 'f': return "Confusion";
        default:   return "Unknown";
    }
}

/* ---- Forward declarations ---- */
void    lobby_remove_player(Lobby *lb, Player *p);
static Player *lobby_add_player(Lobby *lb, sock_t sock,
                                const char *nick, const char *team,
                                char *recv_buf, int recv_len);
void lobby_broadcast(Lobby *lb, const char *msg, int exclude_slot);
void lobby_send_field(Lobby *lb, Player *sender);
void lobby_start_game(Lobby *lb);
void lobby_end_game(Lobby *lb, const char *winner);
static void lobby_announce_host(Lobby *lb);
static void lobby_assign_host(Lobby *lb);
static void broadcast_lobby_list_update(void);
static void lobby_send_settings(Lobby *lb, sock_t sock);

/* ---- Announce host ---- */
static void lobby_announce_host(Lobby *lb) {
    char buf[MAX_MSG_LEN];
    int i;
    if (lb->host_slot <= 0) return;
    for (i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &lb->players[i];
        if (p->connected && p->slot == lb->host_slot) {
            snprintf(buf, sizeof(buf), "host %d %s", lb->host_slot, p->nick);
            lobby_broadcast(lb, buf, -1);
            log_info("Lobby %d: Host is now %s (slot %d)", lb->id, p->nick, lb->host_slot);
            return;
        }
    }
}

/* ---- Assign host to first non-spectator connected player.
         Falls back to spectators if no active player available. ---- */
static void lobby_assign_host(Lobby *lb) {
    int i;
    lb->host_slot = 0;
    /* Prefer non-spectator human players */
    for (i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected && !lb->players[i].spectator
                && !lb->players[i].is_bot) {
            lb->host_slot = lb->players[i].slot;
            return;
        }
    }
    /* Fallback: promote first human spectator to host */
    for (i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected && !lb->players[i].is_bot) {
            lb->players[i].spectator = 0;
            lb->host_slot = lb->players[i].slot;
            net_send(lb->players[i].sock, "spectator 0");
            return;
        }
    }
    /* No humans — no host (bots cannot be host) */
}

/* ---- Lobby defaults ---- */
static void lobby_init_defaults(Lobby *lb) {
    int j;
    lb->game_state        = STATE_MENU;
    lb->round_num         = 0;
    lb->lb_count          = 0;
    memset(lb->lb_nicks, 0, sizeof(lb->lb_nicks));
    memset(lb->lb_wins,  0, sizeof(lb->lb_wins));
    lb->lb_team_count     = 0;
    memset(lb->lb_team_nicks, 0, sizeof(lb->lb_team_nicks));
    memset(lb->lb_team_wins,  0, sizeof(lb->lb_team_wins));
    lb->host_slot         = 0;
    lb->starting_level    = g_cfg.initial_level;
    lb->lines_per_level   = g_cfg.lines_per_level;
    lb->level_increment   = g_cfg.level_inc;
    lb->average_levels    = g_cfg.average_levels;
    lb->lines_per_special = g_cfg.special_lines;
    lb->specials_added    = g_cfg.special_count;
    lb->specials_capacity = g_cfg.special_capacity;
    lb->no_attacks        = 0;
    lb->last_player_wins  = 1;   /* default: game ends when last player stands */
    lb->no_taunts         = 0;   /* taunts enabled by default */
    lb->speedup_enabled   = g_cfg.speedup_enabled;
    lb->speedup_delay     = g_cfg.speedup_delay;
    lb->speedup_interval  = g_cfg.speedup_interval;
    lb->speedup_lines     = g_cfg.speedup_lines;
    lb->lock_delay_ms     = g_cfg.lock_delay_ms;
    lb->garbage_system    = g_cfg.garbage_system;
    lb->garbage_targeting = g_cfg.garbage_targeting;
    lb->hold_enabled      = g_cfg.hold_enabled;
    lb->game_start_time   = 0;
    lb->speedup_last_add  = 0;
    memset(lb->lb_streaks, 0, sizeof(lb->lb_streaks));
    for (j = 0; j < NUM_PIECES;   j++) lb->piece_frequencies[j]   = 14;
    lb->piece_frequencies[0] = 16;
    for (j = 0; j < NUM_SPECIALS; j++) lb->special_frequencies[j] = 11;
}

/* ---- Server init ---- */
void server_init(Server *srv) {
    int i;
    memset(srv, 0, sizeof(*srv));
    srv->listen_sock = INVALID_SOCK;
    srv->running = 1;

    for (i = 0; i < 3; i++) {
        Lobby *lb = &srv->lobbies[i];
        lb->id = i + 1;
        snprintf(lb->name, MAX_LOBBY_NAME, "Room %d", i + 1);
        lb->max_players = MAX_PLAYERS;
        lb->player_count = 0;
        lobby_init_defaults(lb);
    }
    srv->lobby_count = 3;
    memset(g_pending, 0, sizeof(g_pending));
}

Lobby *server_find_lobby(Server *srv, int id) {
    int i;
    for (i = 0; i < srv->lobby_count; i++)
        if (srv->lobbies[i].id == id) return &srv->lobbies[i];
    return NULL;
}

Lobby *server_create_lobby(Server *srv, const char *name) {
    Lobby *lb;
    if (srv->lobby_count >= MAX_LOBBIES) return NULL;
    lb = &srv->lobbies[srv->lobby_count++];
    memset(lb, 0, sizeof(*lb));
    lb->id = srv->lobby_count;
    strncpy(lb->name, name, MAX_LOBBY_NAME - 1);
    lb->max_players = MAX_PLAYERS;
    lobby_init_defaults(lb);
    return lb;
}

/* ---- Send settings to one socket ---- */
static void lobby_send_settings(Lobby *lb, sock_t sock) {
    char reconnect_msg[48];
    snprintf(reconnect_msg,sizeof(reconnect_msg),"reconnectwindow %d",g_cfg.reconnect_window);
    net_send(sock,reconnect_msg);

    char buf[MAX_MSG_LEN];
    snprintf(buf, sizeof(buf), "lobbysettings %d %d %d %d %d %d %d %d",
             lb->id, lb->no_attacks, lb->last_player_wins, lb->no_taunts,
             lb->speedup_enabled, lb->garbage_system, lb->garbage_targeting,
             lb->hold_enabled);
    net_send(sock, buf);
}

/* ---- Build lobbyinfo/lobbyupdate line ---- */
static void format_lobbyinfo(char *buf, int bufsz, Lobby *lb, int is_update) {
    int active = 0, i;
    for (i = 0; i < MAX_PLAYERS; i++)
        if (lb->players[i].connected && !lb->players[i].spectator)
            active++;
    /* Format: cmd id active max state no_attacks last_player_wins no_taunts paused speedup garbage_system garbage_targeting name */
    snprintf(buf, bufsz, "%s %d %d %d %s %d %d %d %d %d %d %d %d %s",
             is_update ? "lobbyupdate" : "lobbyinfo",
             lb->id,
             active, lb->max_players,
             lb->game_state == STATE_PLAYING ? "ingame" : "open",
             lb->no_attacks, lb->last_player_wins, lb->no_taunts,
             lb->paused, lb->speedup_enabled,
             lb->garbage_system, lb->garbage_targeting, lb->hold_enabled,
             lb->name);
}

/* ---- Send server-wide individual leaderboard to a socket ---- */
static void send_server_leaderboard(sock_t sock) {
    LbRow rows[LEADERBOARD_MAX];
    int cnt = auth_get_leaderboard(rows, LEADERBOARD_MAX);
    /* Send registered user count */
    {char lbmsg[64];
     int total=auth_count_users();
     snprintf(lbmsg,sizeof(lbmsg),"usercount %d",total);
     net_send(sock,lbmsg);}
    /* Send total currently connected users (all lobbies + lobby-select) */
    {char lbmsg[64]; int connected=0, li2, pi2, pk2;
     for(li2=0;li2<g_server.lobby_count;li2++)
         for(pi2=0;pi2<MAX_PLAYERS;pi2++)
             if(g_server.lobbies[li2].players[pi2].connected) connected++;
     for(pk2=0;pk2<MAX_PENDING;pk2++)
         if(g_pending[pk2].active&&g_pending[pk2].auth_ok&&!g_pending[pk2].is_tool) connected++;
     snprintf(lbmsg,sizeof(lbmsg),"conncount %d",connected);
     net_send(sock,lbmsg);}
    char lbmsg[MAX_MSG_LEN];
    int pos = snprintf(lbmsg, sizeof(lbmsg), "serverleaderboard %d", cnt);
    int ri;
    for (ri = 0; ri < cnt && pos < (int)sizeof(lbmsg)-160; ri++)
        pos += snprintf(lbmsg+pos, sizeof(lbmsg)-pos, " %s %s %d %d",
                        rows[ri].nick,
                        rows[ri].team[0] ? rows[ri].team : "-",
                        rows[ri].wins,
                        rows[ri].attacks);
    net_send(sock, lbmsg);
}

/* ---- Send team leaderboard to a socket ---- */
static void send_server_team_leaderboard(sock_t sock) {
    TeamLbRow rows[LEADERBOARD_MAX];
    int cnt = auth_get_team_leaderboard(rows, LEADERBOARD_MAX);
    char lbmsg[MAX_MSG_LEN];
    /* format: serverteamleaderboard <count> <team> <wins> <members> ... */
    int pos = snprintf(lbmsg, sizeof(lbmsg), "serverteamleaderboard %d", cnt);
    int ri;
    for (ri = 0; ri < cnt && pos < (int)sizeof(lbmsg)-128; ri++)
        pos += snprintf(lbmsg+pos, sizeof(lbmsg)-pos, " %s %d %d",
                        rows[ri].team, rows[ri].wins, rows[ri].members);
    net_send(sock, lbmsg);
}

/* ---- Send lobby list to a socket ---- */
void send_lobby_list(sock_t sock) {
    char buf[MAX_MSG_LEN];
    int li;
    /* Send counts pre-auth so the server browser probe can pick them up */
    {int total=auth_count_users();
     snprintf(buf,sizeof(buf),"usercount %d",total); net_send(sock,buf);}
    {int connected=0,li2,pi2,pk2;
     for(li2=0;li2<g_server.lobby_count;li2++)
         for(pi2=0;pi2<MAX_PLAYERS;pi2++)
             if(g_server.lobbies[li2].players[pi2].connected) connected++;
     for(pk2=0;pk2<MAX_PENDING;pk2++)
         if(g_pending[pk2].active&&g_pending[pk2].auth_ok&&!g_pending[pk2].is_tool) connected++;
     snprintf(buf,sizeof(buf),"conncount %d",connected); net_send(sock,buf);}
    for (li = 0; li < g_server.lobby_count; li++) {
        format_lobbyinfo(buf, sizeof(buf), &g_server.lobbies[li], 0);
        net_send(sock, buf);
    }
    net_send(sock, "lobbylistend");
    send_server_leaderboard(sock);
    send_server_team_leaderboard(sock);
}

/* ---- Push lobby list update to every connected player ---- */
static void broadcast_lobby_list_update(void) {
    int i, j;
    char buf[MAX_MSG_LEN];
    for (i = 0; i < g_server.lobby_count; i++) {
        format_lobbyinfo(buf, sizeof(buf), &g_server.lobbies[i], 1);
        /* Send to all in-room players */
        for (j = 0; j < g_server.lobby_count; j++)
            lobby_broadcast(&g_server.lobbies[j], buf, -1);
        /* Also send to pending (lobby-select) players so their list stays live */
        for (j = 0; j < MAX_PENDING; j++) {
            if (g_pending[j].active && g_pending[j].auth_ok && !g_pending[j].is_tool)
                net_send(g_pending[j].sock, buf);
        }
    }
}

/* ---- Broadcast ---- */
void lobby_broadcast(Lobby *lb, const char *msg, int exclude_slot) {
    auth_replay_record(lb->id,msg);
    if(!strncmp(msg,"playerlost ",11)) {
        int slot=atoi(msg+11),alive=0;
        for(int j=0;j<MAX_PLAYERS;j++) if(lb->players[j].connected && !lb->players[j].spectator && lb->players[j].alive) alive++;
        if(slot>=1 && slot<=MAX_PLAYERS) auth_game_place(lb->id,lb->players[slot-1].nick,alive+1);
    }
    int i;
    for (i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &lb->players[i];
        if (p->connected && p->sock != INVALID_SOCK && p->slot != exclude_slot) {
            g_stat_msgs_sent++;
            g_stat_bytes_sent += strlen(msg);
            net_send(p->sock, msg);
        }
    }
}

/* ---- Field broadcast ---- */
void lobby_send_field(Lobby *lb, Player *sender) {
    char encoded[FIELD_WIDTH * FIELD_HEIGHT + 8];
    char msg[MAX_MSG_LEN];
    field_encode(&sender->field, encoded, sizeof(encoded));
    snprintf(msg, sizeof(msg), "f %d %s", sender->slot, encoded);
    lobby_broadcast(lb, msg, sender->slot);
}

/* ---- Start game ---- */

/* Generate N pieces into player's server-side queue using server RNG */
void sv_gen_pieces(Player *p, Lobby *lb, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int tail = p->sv_piece_tail % 64;
        /* Weighted random using lobby piece frequencies */
        int total = 0, j, roll, acc = 0;
        for (j = 0; j < NUM_PIECES; j++) total += lb->piece_frequencies[j];
        if (total <= 0) total = NUM_PIECES;
        roll = rand() % total;
        int type = 0;
        for (j = 0; j < NUM_PIECES; j++) {
            acc += lb->piece_frequencies[j] > 0 ? lb->piece_frequencies[j] : 1;
            if (roll < acc) { type = j; break; }
        }
        p->sv_piece_queue[tail] = type;
        p->sv_piece_tail++;
        if (p->sv_piece_tail - p->sv_piece_head > 62)
            break; /* queue full */
    }
}

/* Send next piece(s) to client — keep client supplied with at least 2 ahead */
static void sv_send_pieces(Player *p) {
    while (p->sv_piece_sent < p->sv_piece_tail) {
        int idx = p->sv_piece_sent % 64;
        char msg[32];
        snprintf(msg, sizeof(msg), "nextpiece %c", PIECE_CHARS[p->sv_piece_queue[idx]]);
        net_send(p->sock, msg);
        p->sv_piece_sent++;
    }
}

void lobby_start_game(Lobby *lb) {
    char msg[MAX_MSG_LEN];
    int i;

    /* Need at least 1 active (non-spectator) player */
    int active = 0;
    for (i = 0; i < MAX_PLAYERS; i++)
        if (lb->players[i].connected && !lb->players[i].spectator) active++;
    if (active < 1) return;

    lb->game_state = STATE_PLAYING;
    lb->session_round++;
    char round_msg[48]; snprintf(round_msg,sizeof(round_msg),"roundid %d",lb->session_round);
    lobby_broadcast(lb,round_msg,-1);
    lb->game_start_time  = time(NULL);
    lb->speedup_last_add = time(NULL);
    /* Record game start in DB — only when 2+ human players */
    {int pc=0,humans=0,i2;
     for(i2=0;i2<MAX_PLAYERS;i2++)
         if(lb->players[i2].connected&&!lb->players[i2].spectator&&!lb->players[i2].is_bot)
             humans++;
     for(i2=0;i2<MAX_PLAYERS;i2++) if(lb->players[i2].connected&&!lb->players[i2].spectator) pc++;
     int capable=1;
     for(i2=0;i2<MAX_PLAYERS;i2++)if(lb->players[i2].connected&&!lb->players[i2].spectator&&!lb->players[i2].is_bot&&!lb->players[i2].input_capable)capable=0;
     lb->track_stats = (humans >= 2 && humans == pc && capable); /* store flag for end-of-game */
     auth_game_start(lb->id, pc);
     auth_game_ranked(lb->id,lb->track_stats);
     {
         for(i2=0;i2<MAX_PLAYERS;i2++){
             if(lb->players[i2].connected&&!lb->players[i2].spectator&&!lb->players[i2].is_bot){
                 auth_set_status(lb->players[i2].nick,"playing");
                 auth_game_stat_start(lb->players[i2].nick,lb->id);
             }
         }

     }}
    if (g_cfg.debug_ingame) {
        /* List all active players at game start */
        char plist[256]=""; int pi;
        for(pi=0;pi<MAX_PLAYERS;pi++){
            if(lb->players[pi].connected&&lb->players[pi].nick[0]&&!lb->players[pi].spectator){
                if(plist[0]) strncat(plist,", ",sizeof(plist)-strlen(plist)-1);
                strncat(plist,lb->players[pi].nick,sizeof(plist)-strlen(plist)-1);
            }
        }
        log_info("[INGAME R%d] game start: players=[%s] no_attacks=%d last_wins=%d",
                 lb->id, plist, lb->no_attacks, lb->last_player_wins);
    }

    /* Clear ready flags — they must re-ready for the next round */
    for (i = 0; i < MAX_PLAYERS; i++)
        lb->players[i].ready = 0;

    /* When specials are disabled, send zero frequency/capacity so clients
     * never receive special blocks in their piece queue */
    int lps  = lb->no_attacks ? 0 : lb->lines_per_special;
    int sadd = lb->no_attacks ? 0 : lb->specials_added;
    int scap = lb->no_attacks ? 0 : lb->specials_capacity;
    int sf0  = lb->no_attacks ? 0 : lb->special_frequencies[0];
    int sf1  = lb->no_attacks ? 0 : lb->special_frequencies[1];
    int sf2  = lb->no_attacks ? 0 : lb->special_frequencies[2];
    int sf3  = lb->no_attacks ? 0 : lb->special_frequencies[3];
    int sf4  = lb->no_attacks ? 0 : lb->special_frequencies[4];
    int sf5  = lb->no_attacks ? 0 : lb->special_frequencies[5];
    int sf6  = lb->no_attacks ? 0 : lb->special_frequencies[6];
    int sf7  = lb->no_attacks ? 0 : lb->special_frequencies[7];
    int sf8  = lb->no_attacks ? 0 : lb->special_frequencies[8];

    snprintf(msg, sizeof(msg),
        "newgame 0 %d %d %d %d %d %d "
        "%d %d %d %d %d %d %d "
        "%d %d %d %d %d %d %d %d %d "
        "0 0",
        lb->starting_level, lb->lines_per_level, lb->level_increment,
        lps, sadd, scap,
        lb->piece_frequencies[0], lb->piece_frequencies[1],
        lb->piece_frequencies[2], lb->piece_frequencies[3],
        lb->piece_frequencies[4], lb->piece_frequencies[5],
        lb->piece_frequencies[6],
        sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7, sf8);

    for (i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &lb->players[i];
        if (p->connected) {
            if (!p->spectator) {
                p->alive = 1;
                field_init(&p->field);
                p->hold_type = -1;   /* clear hold slot */
                p->hold_used = 0;
                /* Reset anti-cheat counters */
                p->sv_lines = 0;
                p->sv_real_level = lb->starting_level;
                p->sv_specials_head = p->sv_specials_tail = 0;
                p->sv_last_field = time(NULL);
                p->sv_cheat_strikes = 0;
                p->sv_combo = 0;
                p->sv_b2b   = 0;
                if (p->is_bot) {
                    bot_start(p, lb);
                } else {
                /* Initialise server piece queue */
                p->sv_piece_head = p->sv_piece_tail = p->sv_piece_sent = 0;
                field_init(&p->sv_field);
                sv_gen_pieces(p, lb, 32); /* pre-generate 32 pieces */
                sv_send_pieces(p);        /* send them all at game start */
                /* Init server's current piece to first in queue */
                if (p->sv_piece_head < p->sv_piece_tail) {
                    piece_init(&p->sv_cur, p->sv_piece_queue[p->sv_piece_head % 64]);
                    piece_spawn(&p->sv_field, &p->sv_cur);
                }
                } /* end else (!is_bot) */
        g_stat_field_updates++;
            } else {
                p->alive = 0;
            }
        }
    }

    lobby_broadcast(lb, msg, -1);
    for(i=0;i<MAX_PLAYERS;i++)if(lb->players[i].connected&&!lb->players[i].is_bot&&!lb->players[i].spectator)authority_start(lb,&lb->players[i]);
    /* Tell clients the lock delay so they can implement it locally */
    { char ldmsg[32]; snprintf(ldmsg,sizeof(ldmsg),"lockdelay %d",lb->lock_delay_ms);
      lobby_broadcast(lb, ldmsg, -1); }
    broadcast_lobby_list_update();
    {
        g_stat_games_started++;
    log_info("Lobby %d: Game started (no_attacks=%d last_player_wins=%d)",
                 lb->id, lb->no_attacks, lb->last_player_wins);
    }
}

/* ---- End game ---- */
void lobby_end_game(Lobby *lb, const char *winner) {
    char msg[MAX_MSG_LEN];
    int i;
    lb->game_state = STATE_MENU;
    /* Stop bots immediately and broadcast their final field state */
    for (i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected && lb->players[i].is_bot) {
            lb->players[i].alive = 0;
            /* Send bot final field so clients see stopped state */
            char _bfe[FIELD_WIDTH * FIELD_HEIGHT + 8];
            field_encode(&lb->players[i].field, _bfe, sizeof(_bfe));
            snprintf(msg, sizeof(msg), "f %d %s", lb->players[i].slot, _bfe);
            lobby_broadcast(lb, msg, -1);
        }
    }

    if (winner && winner[0]) {
        if (g_cfg.debug_ingame)
            log_info("[INGAME R%d] game over: winner=%s", lb->id, winner);
        snprintf(msg, sizeof(msg), "playerwon %s", winner);
        lobby_broadcast(lb, msg, -1);
        snprintf(msg, sizeof(msg), "gmsg *** %s wins! ***", winner);
        lobby_broadcast(lb, msg, -1);
    } else {
        if (g_cfg.debug_ingame)
            log_info("[INGAME R%d] game over: no winner (draw or all topped out)", lb->id);
    }
    /* Tell all clients the game has ended so they return to lobby state.
     * This MUST happen before any goto skip_stats so clients always get it. */
    lobby_broadcast(lb, "endgame", -1);

    /* Reset per-player state and re-announce — also before skip_stats */
    for (i = 0; i < MAX_PLAYERS; i++) {
        Player *pp = &lb->players[i];
        if (!pp->connected) continue;
        pp->alive             = 0;
        pp->ready             = pp->is_bot ? 1 : 0;
        pp->sv_lines          = 0;
        pp->sv_specials_head  = pp->sv_specials_tail = 0;
        pp->sv_last_field     = time(NULL);
        pp->sv_cheat_strikes  = 0;
        char pmsg[MAX_MSG_LEN];
        snprintf(pmsg, sizeof(pmsg), "playerjoin %d %s %s",
                 pp->slot, pp->nick, pp->team[0] ? pp->team : "-");
        lobby_broadcast(lb, pmsg, -1);
        if (pp->is_bot) {
            snprintf(pmsg, sizeof(pmsg), "botinfo %d %s", pp->slot,
                     pp->bot_difficulty==AI_EASY?"easy":
                     pp->bot_difficulty==AI_MEDIUM?"medium":"hard");
            lobby_broadcast(lb, pmsg, -1);
        }
        snprintf(pmsg, sizeof(pmsg), "readystatus %d %d", pp->slot, pp->ready);
        lobby_broadcast(lb, pmsg, -1);
        log_info("Lobby %d: %s ready=%d after game end", lb->id, pp->nick, pp->ready);
    }

    /* Every round gets a completed match record, including draws and unranked games. */
    {
        char winner_team[MAX_NICK_LEN]="";
        for (i=0;i<MAX_PLAYERS;i++) {
            Player *pp=&lb->players[i];
            if (!pp->connected) continue;
            if (winner && !strcasecmp(pp->nick,winner))
                strncpy(winner_team,pp->team,MAX_NICK_LEN-1);
            if (pp->is_bot || !pp->nick[0]) continue;
            if (lb->track_stats && !pp->spectator)
                auth_game_stat_end(pp->nick,lb->id,winner && !strcmp(pp->nick,winner));
            auth_set_status(pp->nick,"idle");
        }
        auth_game_end(lb->id,winner?winner:"",winner_team);
    }

    /* Skip DB stats if bot-only or solo vs bots game */
    if (!lb->track_stats) {
        goto skip_stats;
    }

    /* Promote spectators who joined mid-game to full players and tell them */
    for (i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected && lb->players[i].spectator) {
            lb->players[i].spectator = 0;
            /* Tell the client it is no longer a spectator */
            net_send(lb->players[i].sock, "spectator 0");
            log_info("Lobby %d: Spectator %s promoted to full player",
                     lb->id, lb->players[i].nick);
        }
    }

    /* Update leaderboard — in-memory and persistent DB */
    lb->round_num++;
    if (winner && winner[0]) {
        int found = 0, i;
        /* Persist to DB first */
        if(lb->track_stats) auth_add_win(winner);
        /* Update in-memory lobby leaderboard */
        for (i = 0; i < lb->lb_count; i++) {
            if (strcmp(lb->lb_nicks[i], winner) == 0) {
                lb->lb_wins[i]++;
                found = 1;
                break;
            }
        }
        if (!found && lb->lb_count < MAX_PLAYERS) {
            strncpy(lb->lb_nicks[lb->lb_count], winner, MAX_NICK_LEN-1);
            lb->lb_wins[lb->lb_count] = 1;
            lb->lb_count++;
        }
        /* Update streaks: winner gets +1, everyone else resets */
        { int si;
          for (si = 0; si < lb->lb_count; si++) {
              if (strcmp(lb->lb_nicks[si], winner) == 0)
                  lb->lb_streaks[si]++;
              else
                  lb->lb_streaks[si] = 0;
          }
        }
    }
    /* Update per-room team leaderboard */
    if (winner && winner[0]) {
        /* Find winner's team */
        char wteam[MAX_NICK_LEN]=""; int i;
        for (i=0;i<MAX_PLAYERS;i++) {
            if (lb->players[i].connected &&
                !strcasecmp(lb->players[i].nick,winner) &&
                lb->players[i].team[0]) {
                strncpy(wteam,lb->players[i].team,MAX_NICK_LEN-1);
                break;
            }
        }
        if (wteam[0]) {
            int found=0;
            for (i=0;i<lb->lb_team_count;i++) {
                if (!strcasecmp(lb->lb_team_nicks[i],wteam)) {
                    lb->lb_team_wins[i]++; found=1; break;
                }
            }
            if (!found && lb->lb_team_count<MAX_PLAYERS) {
                strncpy(lb->lb_team_nicks[lb->lb_team_count],wteam,MAX_NICK_LEN-1);
                lb->lb_team_wins[lb->lb_team_count]=1;
                lb->lb_team_count++;
            }
            /* Sort team LB by wins descending */
            for (i=0;i<lb->lb_team_count-1;i++) {
                int j;
                for (j=i+1;j<lb->lb_team_count;j++) {
                    if (lb->lb_team_wins[j]>lb->lb_team_wins[i]) {
                        int tmp=lb->lb_team_wins[i]; char tn[MAX_NICK_LEN];
                        lb->lb_team_wins[i]=lb->lb_team_wins[j]; lb->lb_team_wins[j]=tmp;
                        memcpy(tn,lb->lb_team_nicks[i],MAX_NICK_LEN);
                        memcpy(lb->lb_team_nicks[i],lb->lb_team_nicks[j],MAX_NICK_LEN);
                        memcpy(lb->lb_team_nicks[j],tn,MAX_NICK_LEN);
                    }
                }
            }
        }
    }

    /* Sort leaderboard by wins descending (simple bubble) */
    {
        int i, j;
        for (i = 0; i < lb->lb_count-1; i++) {
            for (j = i+1; j < lb->lb_count; j++) {
                if (lb->lb_wins[j] > lb->lb_wins[i]) {
                    int tmp = lb->lb_wins[i];
                    char tn[MAX_NICK_LEN];
                    lb->lb_wins[i] = lb->lb_wins[j]; lb->lb_wins[j] = tmp;
                    memcpy(tn, lb->lb_nicks[i], MAX_NICK_LEN);
                    memcpy(lb->lb_nicks[i], lb->lb_nicks[j], MAX_NICK_LEN);
                    memcpy(lb->lb_nicks[j], tn, MAX_NICK_LEN);
                }
            }
        }
    }
    /* Broadcast leaderboard to lobby:
       "leaderboard <round> <count> <nick1> <wins1> <nick2> <wins2> ..." */
    {
        char lbmsg[MAX_MSG_LEN];
        int i, pos;
        pos = snprintf(lbmsg, sizeof(lbmsg), "leaderboard %d %d",
                       lb->round_num, lb->lb_count);
        for (i = 0; i < lb->lb_count && pos < (int)sizeof(lbmsg)-64; i++)
            pos += snprintf(lbmsg+pos, sizeof(lbmsg)-pos, " %s %d %d",
                            lb->lb_nicks[i], lb->lb_wins[i], lb->lb_streaks[i]);
        lobby_broadcast(lb, lbmsg, -1);
    }
    /* Broadcast team leaderboard */
    {
        char tlbmsg[MAX_MSG_LEN]; int i, pos;
        pos = snprintf(tlbmsg, sizeof(tlbmsg), "teamleaderboard %d",
                       lb->lb_team_count);
        for (i=0;i<lb->lb_team_count&&pos<(int)sizeof(tlbmsg)-64;i++)
            pos += snprintf(tlbmsg+pos, sizeof(tlbmsg)-pos, " %s %d",
                            lb->lb_team_nicks[i], lb->lb_team_wins[i]);
        lobby_broadcast(lb, tlbmsg, -1);
    }

skip_stats:;
    /* Final explicit bot ready broadcast — ensures clients never miss it */
    {
        char brmsg[MAX_MSG_LEN];
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (lb->players[i].connected && lb->players[i].is_bot) {
                lb->players[i].ready = 1;
                snprintf(brmsg, sizeof(brmsg), "readystatus %d 1", lb->players[i].slot);
                lobby_broadcast(lb, brmsg, -1);
                log_info("Lobby %d: bot %s ready for next round (slot %d)",
                         lb->id, lb->players[i].nick, lb->players[i].slot);
            }
        }
    }
        lobby_announce_host(lb);
    broadcast_lobby_list_update();
    g_stat_games_ended++;
    log_info("Lobby %d: Game ended. Winner: %s (round %d)",
             lb->id, winner ? winner : "none", lb->round_num);
}

/* ---- Message handler ---- */

void schat_broadcast(const char *msg) {
    char gbuf[MAX_MSG_LEN];
    int li2, pi2, pk2;
    snprintf(gbuf, sizeof(gbuf), "%s", msg);
    /* Log */
    if (strncmp(gbuf,"schat ",6)==0){
        const char *rest=gbuf+6;
        const char *sp=strchr(rest,' ');
        if(sp) log_info("[MSG] [GLOBAL] <%.*s> %s",(int)(sp-rest),rest,sp+1);
        else   log_info("[MSG] [GLOBAL] %s",rest);
    } else {
        log_info("[MSG] [GLOBAL] %s", gbuf);
    }
    /* Send to all rooms AND lobby — used by say global / server announcements */
    for(li2=0;li2<g_server.lobby_count;li2++)
        for(pi2=0;pi2<MAX_PLAYERS;pi2++){
            Player *tp=&g_server.lobbies[li2].players[pi2];
            if(tp->connected) net_send(tp->sock,gbuf);}
    for(pk2=0;pk2<MAX_PENDING;pk2++)
        if(g_pending[pk2].active&&g_pending[pk2].auth_ok&&!g_pending[pk2].is_tool)
            net_send(g_pending[pk2].sock,gbuf);
}

/* schat_lobby — sends server chat only to lobby-select players, NOT in-room players */
void schat_lobby(const char *msg) {
    char gbuf[MAX_MSG_LEN];
    int pk2;
    snprintf(gbuf, sizeof(gbuf), "%s", msg);
    /* Log */
    if (strncmp(gbuf,"schat ",6)==0){
        const char *rest=gbuf+6;
        const char *sp=strchr(rest,' ');
        if(sp) log_info("[MSG] [LOBBY] <%.*s> %s",(int)(sp-rest),rest,sp+1);
        else   log_info("[MSG] [LOBBY] %s",rest);
    } else {
        log_info("[MSG] [LOBBY] %s", gbuf);
    }
    /* Pending (lobby-select) players only — skip tool sessions */
    for(pk2=0;pk2<MAX_PENDING;pk2++)
        if(g_pending[pk2].active&&g_pending[pk2].auth_ok&&!g_pending[pk2].is_tool)
            net_send(g_pending[pk2].sock,gbuf);
}

/* ---- Admin /command handler for schat (mirrors server_console_command) ---- */
/* ---- Output callback for admin schat: send reply back to player ---- */
static void schat_cmd_out(const char *ln, void *ctx) {
    Player *p = (Player *)ctx;
    char rb[MAX_MSG_LEN];
    snprintf(rb, sizeof(rb), "gmsg *** [CMD] %s", ln);
    net_send(p->sock, rb);
}

/* ---- Output callback variant for pending (lobby-select) players ---- */
static void schat_cmd_out_pending(const char *ln, void *ctx) {
    Pending *pend = (Pending *)ctx;
    char rb[MAX_MSG_LEN];
    snprintf(rb, sizeof(rb), "gmsg *** [CMD] %s", ln);
    net_send(pend->sock, rb);
}

/* ---- Admin /command from schat — routed to unified command system ---- */
static void server_admin_gchat_command(Player *p, const char *line_in) {
    if (server_exec_command(line_in, p->nick, schat_cmd_out, p) != 0)
        net_send(p->sock, "gmsg *** Unknown command. Type /help");
}

/* ---- Admin /command from schat for pending (lobby-select) players ---- */
static void server_admin_gchat_command_pending(Pending *pend, const char *line_in) {
    if (server_exec_command(line_in, pend->pending_nick, schat_cmd_out_pending, pend) != 0)
        net_send(pend->sock, "gmsg *** Unknown command. Type /help");
}


void lobby_handle_message(Lobby *lb, Player *p, const char *msg) {
    char cmd[MAX_MSG_LEN];
    char args[16][MAX_MSG_LEN];
    int n;
    char buf[MAX_MSG_LEN];

    g_stat_msgs_recv++;
    g_stat_bytes_recv += strlen(msg);
    n = proto_parse(msg, cmd, args, 16);
    if (n == 0) return;
    if(strcmp(cmd,"__hb__") && !p->authority_internal)p->sv_last_field=time(NULL);

    if(!strcmp(cmd,"inputv1")) {
        if(n==3)authority_input(lb,p,args[0],args[1]);
        return;
    }
    if(p->authority && !p->authority_internal &&
       (!strcmp(cmd,"f")||!strcmp(cmd,"lineclear")||!strcmp(cmd,"lvl")||
        !strcmp(cmd,"holdpiece")||!strcmp(cmd,"holdclear")||!strcmp(cmd,"playerlost")))return;
    /* Never dispatch gameplay before enforcing round and pause state. */
    {
        int gameplay = !strcmp(cmd,"f") || !strcmp(cmd,"lineclear") ||
            !strcmp(cmd,"sb") || !strcmp(cmd,"lvl") ||
            !strcmp(cmd,"holdpiece") || !strcmp(cmd,"holdclear");
        if (gameplay && (lb->game_state != STATE_PLAYING || !p->alive ||
                        p->spectator || lb->paused)) return;
    }
    /* Spectators may only chat or leave */
    if (p->spectator) {
        if (strcmp(cmd, "pline") == 0 && n >= 2) {
            char full_msg[MAX_MSG_LEN]=""; int ai;
            for(ai=1;ai<n-1&&ai<15;ai++){
                if(ai>1) strncat(full_msg," ",sizeof(full_msg)-strlen(full_msg)-1);
                strncat(full_msg,args[ai],sizeof(full_msg)-strlen(full_msg)-1);
            }
            if(full_msg[0]){
                snprintf(buf,sizeof(buf),"pline %s [spec] %s",p->nick,full_msg);
                lobby_broadcast(lb,buf,-1);
                log_info("[MSG] [Room %d] <%s> [spec] %s", lb->id, p->nick, full_msg);
            }
            return;
        }
        /* Allow spectators to leave cleanly */
        /* ---- PING ---- */
    if (strcmp(cmd, "ping") == 0 && n >= 2) {
        char pong[64]; snprintf(pong,sizeof(pong),"pong %s",args[0]);
        net_send(p->sock, pong); return;
    }
    if (strcmp(cmd, "leaveLobby") == 0 || strcmp(cmd, "listlobbies") == 0) {
            /* fall through to the handlers below */
        } else {
            return;
        }
    }

    /* ---- PING/PONG — latency measurement ---- */
    if (strcmp(cmd, "ping") == 0 && n >= 2) {
        char pong[64];
        snprintf(pong, sizeof(pong), "pong %s", args[0]);
        net_send(p->sock, pong);
        return;
    }

    /* ---- CHAT ---- */
    if (strcmp(cmd, "pline") == 0 && n >= 2) {
        /* Reconstruct full message (args[0]=slot, args[1..]=text words) */
        char full_msg[MAX_MSG_LEN]=""; int ai;
        for(ai=1;ai<n-1&&ai<15;ai++){
            if(ai>1) strncat(full_msg," ",sizeof(full_msg)-strlen(full_msg)-1);
            strncat(full_msg,args[ai],sizeof(full_msg)-strlen(full_msg)-1);
        }
        if(full_msg[0]){
            snprintf(buf, sizeof(buf), "pline %s %s", p->nick, full_msg);
            lobby_broadcast(lb, buf, -1);
            if(!p->is_bot) auth_add_messages(p->nick, 1);
            log_info("[MSG] [Room %d] <%s> %s", lb->id, p->nick, full_msg);
        }
        return;
    }

    /* ---- TEAM (legacy broadcast, untrusted) ---- */
    if (strcmp(cmd, "team") == 0 && n >= 3) {
        strncpy(p->team, args[1], MAX_NICK_LEN - 1);
        snprintf(buf, sizeof(buf), "team %d %s", p->slot, p->team);
        lobby_broadcast(lb, buf, p->slot);
        return;
    }

    /* ---- SETTEAM — player changes their own team, saved to DB ---- */
    if (strcmp(cmd, "setteam") == 0 && n >= 2) {
        const char *new_team = args[0];
        /* Empty string or "-" clears the team */
        if (strcmp(new_team, "-") == 0) new_team = "";
        strncpy(p->team, new_team, MAX_NICK_LEN - 1);
        p->team[MAX_NICK_LEN - 1] = '\0';
        auth_set_team(p->nick, p->team);
        /* Broadcast updated team to everyone including sender */
        snprintf(buf, sizeof(buf), "team %d %s", p->slot, p->team);
        lobby_broadcast(lb, buf, -1);
        /* Confirm back to sender */
        snprintf(buf, sizeof(buf), "yourteam %s", p->team[0] ? p->team : "-");
        net_send(p->sock, buf);
        return;
    }

    /* ---- FIELD UPDATE ---- */
    if (strcmp(cmd, "f") == 0 && n >= 3) {
        if (atoi(args[0]) != p->slot || strlen(args[1]) != FIELD_WIDTH * FIELD_HEIGHT) return;
        for (int ci=0; args[1][ci]; ci++)
            if (!((args[1][ci]>='0' && args[1][ci]<='9') ||
                  (args[1][ci]>='a' && args[1][ci]<='m'))) return;
        p->sv_last_field = time(NULL);
        int old_lines = p->field.lines;
        int old_score = p->field.score;
        field_decode(&p->field, args[1]);
        /* Lines and score can only increase */
        if (p->field.lines < old_lines) p->field.lines = old_lines;
        if (p->field.score < old_score) p->field.score = old_score;
        /* Re-encode from server copy to prevent injecting bad values */
        {
            char clean[FIELD_WIDTH * FIELD_HEIGHT + 8];
            field_encode(&p->field, clean, sizeof(clean));
            snprintf(buf, sizeof(buf), "f %d %s", p->slot, clean);
        }
        lobby_broadcast(lb, buf, p->slot);
        return;
    }

    /* ---- LINE CLEAR — client reports lines cleared; server awards specials & drives level ---- */
    if (strcmp(cmd, "lineclear") == 0 && n >= 2) {
        int gained = atoi(args[0]);
        if (gained < 1 || gained > 4) return;
        p->sv_lines += gained;

        /* ---- Award specials (server-authoritative, config-driven) ---- */
        if (!lb->no_attacks && lb->lines_per_special > 0 && lb->specials_added > 0) {
            int prev_trg = (p->sv_lines - gained) / lb->lines_per_special;
            int curr_trg = p->sv_lines / lb->lines_per_special;
            int to_award = (curr_trg - prev_trg) * lb->specials_added;
            /* Clamp to capacity */
            int held = (p->sv_specials_tail - p->sv_specials_head + 64) % 64;
            if (held + to_award > lb->specials_capacity)
                to_award = lb->specials_capacity - held;
            for (int gi = 0; gi < to_award; gi++) {
                if ((p->sv_specials_tail - p->sv_specials_head + 64) % 64 >= 62) break;
                uint8 sp = (uint8)((rand() % NUM_SPECIALS) + 1);
                p->sv_specials[p->sv_specials_tail % 64] = sp;
                p->sv_specials_tail = (p->sv_specials_tail + 1) % 64;
                char spbuf[32];
                snprintf(spbuf, sizeof(spbuf), "addspecial %c", SPECIAL_CHARS[sp-1]);
                net_send(p->sock, spbuf);
                if (g_cfg.debug_ingame)
                    log_info("Room %d: awarded special '%c' to %s (lines=%d)",
                             lb->id, SPECIAL_CHARS[sp-1], p->nick, p->sv_lines);
            }
        }
        g_stat_lines_cleared += gained;
        if (lb->track_stats && !p->is_bot) auth_add_lines(p->nick, gained);

        /* ---- Level-up (server-authoritative, config-driven) ---- */
        if (lb->lines_per_level > 0) {
            int prev_trg = (p->sv_lines - gained) / lb->lines_per_level;
            int curr_trg = p->sv_lines / lb->lines_per_level;
            int lvl_ups  = curr_trg - prev_trg;
            if (lvl_ups > 0) {
                p->sv_real_level += lvl_ups * lb->level_increment;

                /* Compute the active level for broadcast */
                int active_level;
                if (lb->average_levels) {
                    /* Average all alive players' real levels (rounded up) */
                    int sum = 0, count = 0, ai;
                    for (ai = 0; ai < MAX_PLAYERS; ai++) {
                        Player *ap = &lb->players[ai];
                        if (ap->connected && ap->alive && !ap->spectator) {
                            sum += ap->sv_real_level;
                            count++;
                        }
                    }
                    active_level = count > 0 ? (sum + count - 1) / count : p->sv_real_level;
                } else {
                    active_level = p->sv_real_level;
                }

                if (g_cfg.debug_ingame)
                    log_info("[INGAME R%d] level: %s real=%d active=%d (lines=%d avg=%d)",
                             lb->id, p->nick, p->sv_real_level, active_level,
                             p->sv_lines, lb->average_levels);

                /* Push active level to all eligible players */
                {int li2;
                 for (li2 = 0; li2 < MAX_PLAYERS; li2++) {
                     Player *op = &lb->players[li2];
                     if (!op->connected || op->spectator || !op->alive) continue;
                     /* Non-averaging: only update this player */
                     if (!lb->average_levels && op->slot != p->slot) continue;
                     if (op->field.level >= active_level) continue;
                     op->field.level = active_level;
                     char lbuf[MAX_MSG_LEN];
                     snprintf(lbuf, sizeof(lbuf), "lvl %d %d", op->slot, active_level);
                     if (!op->is_bot) net_send(op->sock, lbuf); /* bots use field.level directly */
                     lobby_broadcast(lb, lbuf, op->slot); /* tell others for display */
                 }}
            }
        }

        /* ---- Garbage attacks from line clears (server-authoritative) ---- */
        if (!lb->no_attacks && gained >= 1) {
            int garbage = 0;
            int is_power_clear = (gained == 4); /* falling blocks — qualifies for B2B */
            /* Perfect clear: field empty after lines removed */
            int is_perfect = 1;
            for (int _pc=0; _pc<FIELD_WIDTH&&is_perfect; _pc++)
                for (int _pr=0; _pr<FIELD_HEIGHT&&is_perfect; _pr++)
                    if (p->field.cells[_pr][_pc]) is_perfect=0;

            if (lb->garbage_system == 1) {
                /* ── Guideline system: B2B + Combos ── */
                static const int base_garbage[5] = {0, 0, 1, 2, 4};
                garbage = base_garbage[gained];
                /* Perfect clear overrides — broadcast and skip combo/B2B */
                if (is_perfect && g_cfg.perfect_clear_bonus > 0) {
                    garbage = g_cfg.perfect_clear_bonus;
                    char pcmsg[MAX_MSG_LEN];
                    snprintf(pcmsg,sizeof(pcmsg),"gmsg *** %s: PERFECT CLEAR! (+%d lines!)",
                             p->nick, garbage);
                    lobby_broadcast(lb, pcmsg, -1);
                    p->sv_b2b = 0; p->sv_combo = 0;
                    goto send_garbage_human;
                }

                /* Back-to-Back bonus: configurable (default +1) */
                if (is_power_clear && p->sv_b2b && garbage > 0)
                    garbage += g_cfg.btb_bonus;

                /* Combo bonus: capped by config (default 4) */
                if (p->sv_combo > 0) {
                    int combo_bonus = (p->sv_combo - 1) / 2;
                    if (combo_bonus > g_cfg.combo_cap) combo_bonus = g_cfg.combo_cap;
                    if (garbage > 0 || p->sv_combo >= 2) garbage += combo_bonus;
                }

                /* Notify clients of combo/b2b for display */
                if (p->sv_combo >= 2 || (is_power_clear && p->sv_b2b)) {
                    char gmsg[MAX_MSG_LEN];
                    if (is_power_clear && p->sv_b2b && p->sv_combo >= 2)
                        snprintf(gmsg,sizeof(gmsg),"gmsg *** %s: B2B falling blocks + %d-combo! (+%d lines)",
                                 p->nick, p->sv_combo, garbage);
                    else if (is_power_clear && p->sv_b2b)
                        snprintf(gmsg,sizeof(gmsg),"gmsg *** %s: Back-to-Back falling blocks! (+%d lines)",
                                 p->nick, garbage);
                    else
                        snprintf(gmsg,sizeof(gmsg),"gmsg *** %s: %d-combo! (+%d lines)",
                                 p->nick, p->sv_combo, garbage);
                    lobby_broadcast(lb, gmsg, -1);
                }

                /* Update B2B state */
                p->sv_b2b = is_power_clear ? 1 : 0;
                /* Update combo counter */
                p->sv_combo++;

            } else {
                /* ── Classic LineClash: 2=1, 3=2, 4=4 ── */
                static const int classic_garbage[5] = {0, 0, 1, 2, 4};
                garbage = classic_garbage[gained];
                /* Classic has no combo/b2b — reset */
                p->sv_combo = 0;
                p->sv_b2b   = 0;
            }

            /* Pick targets and send garbage */
            send_garbage_human:;
            if (garbage > 0) {
                int targets[MAX_PLAYERS], nt = 0, ti;
                for (ti = 0; ti < MAX_PLAYERS; ti++) {
                    Player *tp = &lb->players[ti];
                    if (tp->connected && tp->alive && !tp->spectator && tp->slot != p->slot)
                        targets[nt++] = ti;
                }
                if (nt > 0) {
                    int send_to[MAX_PLAYERS], ns = 0;
                    if (lb->garbage_targeting == 1) {
                        send_to[0] = targets[rand() % nt]; ns = 1;
                    } else if (lb->garbage_targeting == 2) {
                        static int focus_idx[MAX_PLAYERS] = {0};
                        int fi = focus_idx[p->slot-1] % nt;
                        send_to[0] = targets[fi]; ns = 1;
                        focus_idx[p->slot-1] = (fi + 1) % nt;
                    } else {
                        for (int si=0;si<nt;si++) { send_to[si]=targets[si]; } ns=nt;
                    }
                    for (int si = 0; si < ns; si++) {
                        Player *tgt = &lb->players[send_to[si]];
                        if(tgt->authority && tgt->immunity_until>(long long)net_now_ms())continue;
                        /* Send garbage lines using configured hole style */
                        char sbuf[48];
                        for (int gl = 0; gl < garbage; gl++) {
                            snprintf(sbuf, sizeof(sbuf), "sb %d a %d", tgt->slot, p->slot);
                            lobby_broadcast(lb, sbuf, -1);
                        }
                        field_add_garbage_styled(&tgt->field, garbage, g_cfg.garbage_hole_style);
                        lobby_send_field(lb, tgt);
                        if (!tgt->is_bot) {
                            char enc[FIELD_WIDTH*FIELD_HEIGHT+8], fm[MAX_MSG_LEN];
                            field_encode(&tgt->field, enc, sizeof(enc));
                            snprintf(fm, sizeof(fm), "f %d %s", tgt->slot, enc);
                            net_send(tgt->sock, fm);
                        }
                    }
                }
            } else if (lb->garbage_system == 1) {
                if (gained < 2) { p->sv_combo = 0; p->sv_b2b = 0; }
            }
        } else {
            /* no_attacks off or 0 lines — reset combo in both modes */
            p->sv_combo = 0; p->sv_b2b = 0;
        }

        return;
    }

    /* ---- HOLD PIECE — client requests to swap current piece with hold slot ---- */
    if (strcmp(cmd, "holdpiece") == 0 && n >= 2) {
        int ptype = atoi(args[0]); /* client tells us the held piece type */
        if (ptype < 0 || ptype >= NUM_PIECES) return;
        if (!lb->hold_enabled) return; /* hold disabled for this room */
        if (p->hold_used) return; /* only one hold per piece */
        p->hold_used = 1;
        /* Acknowledge with current hold state so all clients can show it */
        char hbuf[32]; snprintf(hbuf, sizeof(hbuf), "holdstate %d %d", p->slot, ptype);
        lobby_broadcast(lb, hbuf, -1);
        return;
    }
    /* ---- HOLD CLEAR — piece locked, hold resets to usable ---- */
    if (strcmp(cmd, "holdclear") == 0) {
        p->hold_used = 0;
        return;
    }
    /* ---- LEVEL — client confirms its display level (anti-cheat check only) ---- */
    if (strcmp(cmd, "lvl") == 0 && n >= 3) {
        int new_level = atoi(args[1]);
        /* Anti-cheat: client must not claim a level higher than server has granted */
        int max_granted = lb->starting_level +
            (p->sv_lines / (lb->lines_per_level > 0 ? lb->lines_per_level : 1))
            * lb->level_increment;
        int is_cheat = (new_level < lb->starting_level) ||
                       (new_level > max_granted + lb->level_increment + 2);
        if (is_cheat) {
            log_info("[ANTICHEAT R%d] %s bad lvl %d (max_granted=%d lines=%d) — kick",
                     lb->id, p->nick, new_level, max_granted, p->sv_lines);
            g_stat_kicks++;
            net_send(p->sock, "kicked Cheating detected");
            net_send(p->sock, "noconnecting Kicked for cheating");
            { char kb[MAX_MSG_LEN];
              snprintf(kb, sizeof(kb), "gmsg *** %s was removed (cheat detected)", p->nick);
              lobby_broadcast(lb, kb, -1); }
            p->alive = 0;
            snprintf(buf, sizeof(buf), "playerlost %d", p->slot);
            lobby_broadcast(lb, buf, p->slot);
            sock_close(p->sock); p->sock = INVALID_SOCK;
            lobby_remove_player(lb, p);
            return;
        }
        /* Display levels are server-owned; client claims cannot raise them. */
        snprintf(buf, sizeof(buf), "lvl %d %d", p->slot, p->field.level);
        lobby_broadcast(lb, buf, p->slot);
        return;
    }

    /* ---- SPECIAL BLOCK ---- */
    if (strcmp(cmd, "sb") == 0 && n >= 4) {
        int to_slot   = atoi(args[0]);
        char sp_char  = args[1][0];
        int from_slot = p->slot; /* identity comes from the authenticated socket */
        Player *target = NULL;
        int i;

        /* In no_attacks mode silently drop specials */
        if (lb->no_attacks) return;

        /* Anti-cheat: verify sender has this special in server-side queue */
        {
            uint8 sp_req = 0; int si2;
            for (si2=0; SPECIAL_CHARS[si2]; si2++)
                if (SPECIAL_CHARS[si2]==sp_char){sp_req=(uint8)(si2+1);break;}
            if (sp_req == 0) return; /* unknown special char */
            /* Validate the target and exact inventory item before consuming. */
            if (strlen(args[1]) != 1 || to_slot < 1 || to_slot > MAX_PLAYERS ||
                !lb->players[to_slot-1].connected || !lb->players[to_slot-1].alive ||
                lb->players[to_slot-1].spectator) return;
            if (p->sv_specials_head == p->sv_specials_tail ||
                p->sv_specials[p->sv_specials_head % 64] != sp_req) return;
            /* Consume from server queue */
            p->sv_specials_head = (p->sv_specials_head + 1) % 64;
        }

        /* Record attack stat for sender */
        if(lb->track_stats && !p->is_bot) { auth_add_attacks(p->nick, 1); auth_add_specials(p->nick, 1); }

        for (i = 0; i < MAX_PLAYERS; i++) {
            if (lb->players[i].connected && lb->players[i].slot == to_slot) {
                target = &lb->players[i];
                break;
            }
        }
        if(target && target->authority && target->immunity_until>(long long)net_now_ms() && sp_char!='i'){
            authority_send(lb,p);authority_send(lb,target);return;
        }
        if (target && target->connected && !target->spectator) {
            uint8 sp = 0;
            int si;
            for (si = 0; SPECIAL_CHARS[si]; si++)
                if (SPECIAL_CHARS[si] == sp_char) { sp = (uint8)(si+1); break; }
            g_stat_specials_fired++;
            log_info("Room %d: %s fires special '%c' on %s",
                     lb->id, p->nick, sp_char, target->nick);
            switch (sp) {
                case SP_ADDLINE:     field_add_garbage(&target->field, 1);    break;
                case SP_CLEARLINE:   field_clear_line_bottom(&target->field);       break;
                case SP_NUKEFIELD:   field_nuke(&target->field);              break;
                case SP_RANDOMCLEAR: field_random_clear(&target->field);      break;
                case SP_CLEARSPECIAL:field_clear_specials(&target->field); target->sv_specials_head=target->sv_specials_tail; break;
                case SP_GRAVITY:     field_gravity(&target->field);           break;
                case SP_QUAKEFIELD:  field_quake(&target->field);             break;
                case SP_BLOCKBOMB:   field_block_bomb(&target->field, target->authority?&target->sv_cur:NULL);  break;
                case SP_CLEARCOLUMN: field_clear_column(&target->field);      break;
                /* Non-field specials — effects applied client-side from sb broadcast */
                case SP_IMMUNITY: if(target->authority)target->immunity_until=net_now_ms()+5000; break;
                case SP_MUTATE: if(target->authority){target->sv_cur.type=rand()%NUM_PIECES;target->mutate_until=net_now_ms()+5000;} break;
                case SP_SWITCHFIELD: {
                    Player *src = NULL;
                    for (i = 0; i < MAX_PLAYERS; i++) {
                        if (lb->players[i].slot == from_slot) { src = &lb->players[i]; break; }
                    }
                    if (src && !src->spectator) {
                        uint8 cells[FIELD_HEIGHT][FIELD_WIDTH];
                        memcpy(cells,src->field.cells,sizeof(cells));
                        memcpy(src->field.cells,target->field.cells,sizeof(cells));
                        memcpy(target->field.cells,cells,sizeof(cells));
                        /* Broadcast each player's new field display to all spectators/others */
                        lobby_send_field(lb, src);
                        lobby_send_field(lb, target);
                        /* Tell each human player their OWN new field via "myfield" command.
                         * This is accepted unconditionally (unlike "f <own_slot>" which
                         * is ignored by the client). The client-side sb handler does
                         * NOT swap locally — the server is authoritative. */
                        if (!src->is_bot) {
                            char enc[FIELD_WIDTH*FIELD_HEIGHT+8], fm[MAX_MSG_LEN];
                            field_encode(&src->field, enc, sizeof(enc));
                            snprintf(fm, sizeof(fm), "myfield %s", enc);
                            net_send(src->sock, fm); }
                        if (!target->is_bot) {
                            char enc[FIELD_WIDTH*FIELD_HEIGHT+8], fm[MAX_MSG_LEN];
                            field_encode(&target->field, enc, sizeof(enc));
                            snprintf(fm, sizeof(fm), "myfield %s", enc);
                            net_send(target->sock, fm); }
                    }
                    break;
                }
                case SP_DARKNESS:
                    if(target->authority)target->darkness_until=net_now_ms()+5000;
                    if (target->is_bot) target->bot_darkness_ticks = 8;
                    /* Human darkness applied client-side from sb broadcast */
                    break;
                case SP_CONFUSION:
                    if(target->authority)target->confusion_until=net_now_ms()+5000;
                    if (target->is_bot) target->bot_confusion_ticks = 80;
                    /* Human confusion applied client-side from sb broadcast */
                    break;
                default: break;
            }
            /* Send updated target field (except switchfield which already sent both) */
            if (sp != SP_SWITCHFIELD) {
                /* Broadcast updated field to all other players */
                lobby_send_field(lb, target);
                /* Note: target applies the effect locally from the sb broadcast;
                 * sending "f <own_slot>" to target is ignored by the client. */
            }
        }
        if (g_cfg.debug_ingame) {
            const char *tname = (target && target->nick[0]) ? target->nick : "?";
            log_info("[INGAME R%d] special: %s -> %s used %s ('%c')",
                     lb->id, p->nick, tname, sp_name(sp_char), sp_char);
        }
        snprintf(buf, sizeof(buf), "sb %d %c %d", to_slot, sp_char, from_slot);
        lobby_broadcast(lb, buf, -1);
        return;
    }

    /* ---- PLAYER LOST ---- */
    /* ---- ANTI-CHEAT: heartbeat check (called with cmd="__tick__") ---- */
    if (strcmp(cmd, "__hb__") == 0) {
        time_t now = time(NULL);
        if (lb->paused) { p->sv_last_field=now; return; }
        if (p->is_bot) return; /* human clients keep the connection alive with pings */
        if (p->alive && !p->spectator &&
            lb->game_state == STATE_PLAYING &&
            now - p->sv_last_field > 30) {
            log_info("[ANTICHEAT R%d] %s field timeout (%lds) — kicking",
                     lb->id, p->nick, (long)(now - p->sv_last_field));
            p->alive = 0;
            snprintf(buf, sizeof(buf), "playerlost %d", p->slot);
            lobby_broadcast(lb, buf, p->slot);
            net_send(p->sock, "kicked Connection timed out");
            net_send(p->sock, "noconnecting Timed out");
            { char kb[MAX_MSG_LEN];
              snprintf(kb,sizeof(kb),"gmsg *** %s timed out and was disconnected",p->nick);
              lobby_broadcast(lb,kb,-1); }
            sock_close(p->sock); p->sock=INVALID_SOCK;
            lobby_remove_player(lb,p);
        }
        return;
    }

    if (strcmp(cmd, "playerlost") == 0) {
        int alive_count = 0, i;
        Player *last_alive = NULL;

        /* Ignore if game already ended (e.g. bot triggered end first) */
        if (lb->game_state != STATE_PLAYING) return;
        if (!p->alive) return; /* already counted as lost */
        p->alive = 0;
        /* Broadcast final field state before playerlost so clients see topped-out field */
        {
            char fenc[FIELD_WIDTH * FIELD_HEIGHT + 8];
            field_encode(&p->field, fenc, sizeof(fenc));
            char fbuf[MAX_MSG_LEN];
            snprintf(fbuf, sizeof(fbuf), "f %d %s", p->slot, fenc);
            lobby_broadcast(lb, fbuf, p->slot);
        }
        snprintf(buf, sizeof(buf), "playerlost %d", p->slot);
        lobby_broadcast(lb, buf, p->slot);
        log_info("Lobby %d: %s lost", lb->id, p->nick);

        if (lb->last_player_wins) {
            for (i = 0; i < MAX_PLAYERS; i++) {
                if ((lb->players[i].connected || lb->players[i].reconnect_expires>time(NULL)) && lb->players[i].alive
                        && !lb->players[i].spectator) {
                    alive_count++;
                    last_alive = &lb->players[i];
                }
            }
            /* End if ≤1 alive, OR if all remaining alive are bots */
            int human_alive = 0;
            for (i = 0; i < MAX_PLAYERS; i++) {
                if ((lb->players[i].connected || lb->players[i].reconnect_expires>time(NULL)) && lb->players[i].alive
                        && !lb->players[i].spectator && !lb->players[i].is_bot)
                    human_alive++;
            }
            if ((alive_count <= 1 || human_alive == 0) && lb->game_state == STATE_PLAYING)
                lobby_end_game(lb, last_alive && !last_alive->is_bot ? last_alive->nick : NULL);
        }
        /* If last_player_wins==0: game ends when all humans gone or all dead */
        else {
            int human_alive2 = 0;
            for (i = 0; i < MAX_PLAYERS; i++) {
                if ((lb->players[i].connected || lb->players[i].reconnect_expires>time(NULL)) && lb->players[i].alive
                        && !lb->players[i].spectator) {
                    alive_count++;
                    last_alive = &lb->players[i];
                    if (!lb->players[i].is_bot) human_alive2++;
                }
            }
            if ((alive_count == 0 || human_alive2 == 0) && lb->game_state == STATE_PLAYING)
                lobby_end_game(lb, NULL);
        }
        return;
    }

    /* ---- PLAYER READY TOGGLE ---- */
    /* ---- IN-ROOM BOT MANAGEMENT (host or admin) ---- */
    if (strcmp(cmd, "addbot") == 0 && n >= 2) {
        if (!g_cfg.allow_bots) {
            net_send(p->sock, "gmsg *** Bots are disabled (see lineclashd.cfg).");
            return;
        }
        if (lb->game_state != STATE_MENU) {
            net_send(p->sock, "gmsg *** Cannot add bot during a game."); return;
        }
        if (p->slot != lb->host_slot && !p->is_admin && p->priv_level > 2) {
            net_send(p->sock, "gmsg *** Only the host or admin can add bots."); return;
        }
        int diff = AI_EASY;
        if (!strcasecmp(args[0],"medium")) diff = AI_MEDIUM;
        else if (!strcasecmp(args[0],"hard")) diff = AI_HARD;
        int count = 0;
        for (int ci=0; ci<MAX_PLAYERS; ci++) if (lb->players[ci].connected) count++;
        if (count >= lb->max_players) {
            net_send(p->sock, "gmsg *** Room is full."); return;
        }
        int slot = lobby_add_bot(lb, diff);
        if (slot < 0) { net_send(p->sock, "gmsg *** Could not add bot."); return; }
        Player *bp = NULL;
        for (int ci=0; ci<MAX_PLAYERS; ci++)
            if (lb->players[ci].connected && lb->players[ci].slot==slot)
                { bp=&lb->players[ci]; break; }
        if (bp) {
            char jmsg[MAX_MSG_LEN];
            /* Announce join to room */
            snprintf(jmsg,sizeof(jmsg),"playerjoin %d %s -",bp->slot,bp->nick);
            lobby_broadcast(lb,jmsg,-1);
            /* Tell all clients it is a bot and its difficulty */
            snprintf(jmsg,sizeof(jmsg),"botinfo %d %s",bp->slot,
                     diff==AI_EASY?"easy":diff==AI_MEDIUM?"medium":"hard");
            lobby_broadcast(lb,jmsg,-1);
            bp->ready = 1;
            snprintf(jmsg,sizeof(jmsg),"readystatus %d 1",bp->slot);
            lobby_broadcast(lb,jmsg,-1);
            snprintf(jmsg,sizeof(jmsg),"gmsg *** %s bot '%s' added (slot %d)",
                     diff==AI_EASY?"Easy":diff==AI_MEDIUM?"Medium":"Hard",
                     bp->nick, bp->slot);
            lobby_broadcast(lb,jmsg,-1);
            log_info("Room %d: %s added %s bot %s (slot %d)",
                     lb->id, p->nick,
                     diff==AI_EASY?"easy":diff==AI_MEDIUM?"medium":"hard",
                     bp->nick, bp->slot);
            /* Persist bot count for this room */
            { int bc=0;
              for(int pi=0;pi<MAX_PLAYERS;pi++)
                  if(lb->players[pi].connected&&lb->players[pi].is_bot) bc++;
              auth_set_room_bots(lb->id, bc, diff); }
        }
        return;
    }
    if (strcmp(cmd, "removebot") == 0 && n >= 2) {
        if (lb->game_state != STATE_MENU) {
            net_send(p->sock, "gmsg *** Cannot remove bot during a game."); return;
        }
        if (p->slot != lb->host_slot && !p->is_admin && p->priv_level > 2) {
            net_send(p->sock, "gmsg *** Only the host or admin can remove bots."); return;
        }
        int rslot = atoi(args[0]);
        int found = 0;
        for (int ci=0; ci<MAX_PLAYERS; ci++) {
            Player *bp = &lb->players[ci];
            if (!bp->connected || !bp->is_bot || bp->slot!=rslot) continue;
            char rmsg[MAX_MSG_LEN];
            snprintf(rmsg,sizeof(rmsg),"gmsg *** Bot '%s' removed from room",bp->nick);
            lobby_broadcast(lb,rmsg,-1);
            log_info("Room %d: %s removed bot %s (slot %d)",
                     lb->id, p->nick, bp->nick, rslot);
            lobby_remove_bot(lb, rslot);
            /* Update persistent count */
            { int bc=0, bd=1;
              for(int pi=0;pi<MAX_PLAYERS;pi++)
                  if(lb->players[pi].connected&&lb->players[pi].is_bot)
                  { bc++; bd=lb->players[pi].bot_difficulty; }
              auth_set_room_bots(lb->id, bc, bc>0?bd:0); }
            found=1; break;
        }
        if (!found) net_send(p->sock, "gmsg *** No bot in that slot.");
        return;
    }

    if (strcmp(cmd, "playerready") == 0) {
        if (lb->game_state != STATE_MENU) return;
        if (p->spectator) return;
        p->ready = !p->ready;
        log_info("Lobby %d: %s is %s", lb->id, p->nick, p->ready ? "READY" : "not ready");
        {
            char rmsg[MAX_MSG_LEN];
            snprintf(rmsg, sizeof(rmsg), "readystatus %d %d", p->slot, p->ready);
            lobby_broadcast(lb, rmsg, -1);
        }
        {
            char rmsg[MAX_MSG_LEN];
            snprintf(rmsg, sizeof(rmsg), "gmsg *** %s is %s",
                     p->nick, p->ready ? "READY" : "not ready");
            lobby_broadcast(lb, rmsg, -1);
        }
        return;
    }

    /* ---- START GAME (host only, all non-spectators must be ready) ---- */
    if (strcmp(cmd, "startgame") == 0) {
        if (lb->game_state != STATE_MENU) {
            net_send(p->sock, "gmsg *** Game already in progress");
            return;
        }
        if (p->slot != lb->host_slot && !p->is_admin && p->priv_level > 2) {
            net_send(p->sock, "gmsg *** Only the host or an admin can start the game");
            snprintf(buf, sizeof(buf), "host %d", lb->host_slot);
            net_send(p->sock, buf);
            return;
        }
        /* Check all non-spectator players are ready */
        {
            int i, all_ready = 1, count = 0;
            for (i = 0; i < MAX_PLAYERS; i++) {
                if (lb->players[i].connected && !lb->players[i].spectator) {
                    count++;
                    if (!lb->players[i].ready) { all_ready = 0; break; }
                }
            }
            if (count < 1) return;
            if (!all_ready) {
                net_send(p->sock, "gmsg *** Not all players are ready");
                return;
            }
        }
        lobby_start_game(lb);
        return;
    }

    /* ---- PAUSE ---- */
    if (strcmp(cmd, "pause") == 0 && n >= 2) {
        int new_state = atoi(args[0]);
        lb->paused = new_state;
        char pbuf[MAX_MSG_LEN];
        snprintf(pbuf, sizeof(pbuf), "pause %d %d", new_state, p->slot);
        lobby_broadcast(lb, pbuf, -1);
        broadcast_lobby_list_update();
        log_info("Lobby %d: %s by %s", lb->id,
                 new_state ? "paused" : "resumed", p->nick);
        /* Reset pause violation counter on state change */
        for (int pi=0; pi<MAX_PLAYERS; pi++)
            if (lb->players[pi].connected) lb->players[pi].sv_cheat_strikes = 0;
        return;
    }

    /* ---- END GAME — host, admin, or priv level ≤ 2 can force-end a round ---- */
    if (strcmp(cmd, "endgame") == 0) {
        int can=(p->slot==lb->host_slot||p->is_admin||p->priv_level<=2);
        if(!can){ net_send(p->sock,"gmsg *** Only the host or admin can end the game"); return; }
        if(lb->game_state!=STATE_PLAYING){ net_send(p->sock,"gmsg *** No game in progress"); return; }
        char ebuf[MAX_MSG_LEN];
        snprintf(ebuf,sizeof(ebuf),"gmsg *** Game ended by %s",p->nick);
        lobby_broadcast(lb,ebuf,-1);
        log_info("Lobby %d: game ended by %s",lb->id,p->nick);
        auth_game_stopped(lb->id);
        lobby_end_game(lb,NULL);
        return;
    }

    /* ---- GMSG (room chat) ---- */
    if (strcmp(cmd, "gmsg") == 0 && n >= 2) {
        /* Reconstruct full message — proto_parse splits on spaces */
        char gmsg_full[MAX_MSG_LEN]=""; int gmi;
        for(gmi=0;gmi<n-1&&gmi<15;gmi++){
            if(gmi) strncat(gmsg_full," ",sizeof(gmsg_full)-strlen(gmsg_full)-1);
            strncat(gmsg_full,args[gmi],sizeof(gmsg_full)-strlen(gmsg_full)-1);
        }
        const char *msg = gmsg_full;
        if (msg[0] == '/') {
            int can_cmd = (p->is_admin || p->priv_level <= 2);
            int is_host = (p->slot == lb->host_slot);
            if (can_cmd) {
                /* priv 1+2: /kick in room chat does a room-kick, not server-kick */
                char acmd[32]="", a1[MAX_NICK_LEN]="";
                sscanf(msg+1,"%31s %31s",acmd,a1);
                if (!strcasecmp(acmd,"kick") && a1[0]) {
                    int tpriv=0;
                    int r=kick_player_from_room(lb,a1,p->nick,&tpriv);
                    if (r==1)       { /* success */ }
                    else if (r==-1) net_send(p->sock,"gmsg *** Cannot kick an admin or manager");
                    else            net_send(p->sock,"gmsg *** Player not found in this room");
                } else if (server_exec_command(msg+1, p->nick, schat_cmd_out, p) != 0) {
                    net_send(p->sock,"gmsg *** Unknown command. Type help");
                }
            } else if (is_host) {
                /* Host: can only room-kick (not full admin commands) */
                char acmd[32]="", a1[MAX_NICK_LEN]="";
                sscanf(msg+1,"%31s %31s",acmd,a1);
                if (!strcasecmp(acmd,"kick") && a1[0]) {
                    int tpriv=0;
                    int r=kick_player_from_room(lb,a1,p->nick,&tpriv);
                    if (r==1)        { /* success — broadcast already sent in helper */ }
                    else if (r==-1)  net_send(p->sock,"gmsg *** Cannot kick an admin or manager");
                    else             net_send(p->sock,"gmsg *** Player not found in this room");
                } else {
                    net_send(p->sock,"gmsg *** As host you can use: /kick <nick>");
                }
            } else {
                net_send(p->sock,"gmsg *** Only admins/managers can use / commands");
            }
            return;
        }
        snprintf(buf, sizeof(buf), "gmsg %s", msg);
        lobby_broadcast(lb, buf, p->slot);
        return;
    }

    /* ---- GLOBAL CHAT ---- */
    if (strcmp(cmd, "schat") == 0 && n >= 2) {
        /* Reconstruct full message (proto_parse splits on spaces) */
        char full_msg[MAX_MSG_LEN]=""; int ai;
        for(ai=0;ai<n-1&&ai<15;ai++){
            if(ai) strncat(full_msg," ",sizeof(full_msg)-strlen(full_msg)-1);
            strncat(full_msg,args[ai],sizeof(full_msg)-strlen(full_msg)-1);
        }
        const char *msg = full_msg;
        if (msg[0]=='/') {
            /* Admin commands via schat — priv 1 (admin) and priv 2 (mgr) */
            if (p->is_admin || p->priv_level <= 2) {
                server_admin_gchat_command(p, msg+1);
            } else {
                net_send(p->sock,"gmsg *** Only admins can use / commands");
            }
            return;
        }
        /* All authenticated users can send to server chat — lobby-select only */
        {char gmsg2[MAX_MSG_LEN];
         snprintf(gmsg2, sizeof(gmsg2), "schat %s %s", p->nick, msg);
         schat_lobby(gmsg2);}
        return;
    }

    /* ---- REQUEST LOBBY LIST ---- */
    if (strcmp(cmd, "listlobbies") == 0) {
        send_lobby_list(p->sock);
        return;
    }

    /* ---- REQUEST SERVER LEADERBOARD ---- */
    if (strcmp(cmd, "reqleaderboard") == 0) {
        send_server_leaderboard(p->sock);
        send_server_team_leaderboard(p->sock);
        /* Send list of all connected players with playing/idle status */
        {char pmsg[MAX_MSG_LEN]; int pos,li2,pi2,pk2;
         pos=snprintf(pmsg,sizeof(pmsg),"playerlist");
         /* Players in lobbies — format: nick status room_id */
         for(li2=0;li2<g_server.lobby_count&&pos<(int)sizeof(pmsg)-64;li2++){
             Lobby *lb2=&g_server.lobbies[li2];
             for(pi2=0;pi2<MAX_PLAYERS&&pos<(int)sizeof(pmsg)-64;pi2++){
                 Player *pp=&lb2->players[pi2];
                 if(!pp->connected||pp->spectator||!pp->nick[0]||pp->is_bot) continue;
                 int playing=(lb2->game_state==STATE_PLAYING&&pp->alive);
                 pos+=snprintf(pmsg+pos,sizeof(pmsg)-pos," %s %s %d",
                               pp->nick, playing?"playing":"idle", lb2->id);
             }
         }
         /* Pending users in lobby-select: room_id=0 */
         for(pk2=0;pk2<MAX_PENDING&&pos<(int)sizeof(pmsg)-64;pk2++){
             if(g_pending[pk2].active&&g_pending[pk2].auth_ok&&g_pending[pk2].pending_nick[0]&&!g_pending[pk2].is_tool)
                 pos+=snprintf(pmsg+pos,sizeof(pmsg)-pos," %s idle 0",
                               g_pending[pk2].pending_nick);
         }
         net_send(p->sock,pmsg);}
        return;
    }

    /* ---- LEAVE LOBBY — client is going back to the lobby select screen.
       Remove the player from the lobby so they get a clean re-join later.
       We keep the TCP connection alive; the client will send lineclash_join
       when they pick a new (or the same) lobby. ---- */
    if (strcmp(cmd, "leaveLobby") == 0) {
        sock_t saved_sock = p->sock; /* save before any removal */
        Pending returning;memset(&returning,0,sizeof(returning));
        returning.sock=saved_sock;returning.active=returning.auth_ok=1;returning.input_capable=p->input_capable;
        snprintf(returning.pending_nick,sizeof(returning.pending_nick),"%s",p->nick);
        snprintf(returning.ip,sizeof(returning.ip),"%s",p->ip);
        snprintf(returning.client_type,sizeof(returning.client_type),"%s",p->client_type);
        snprintf(returning.client_platform,sizeof(returning.client_platform),"%s",p->client_platform);
        snprintf(returning.client_version,sizeof(returning.client_version),"%s",p->client_version);
        returning.recv_len=p->recv_len;memcpy(returning.recv_buf,p->recv_buf,(size_t)p->recv_len);

        /* If leaving mid-game, treat as a loss */
        if (lb->game_state == STATE_PLAYING && !p->spectator && p->alive) {
            char lbuf[MAX_MSG_LEN];
            p->alive = 0;
            snprintf(lbuf, sizeof(lbuf), "playerlost %d", p->slot);
            lobby_broadcast(lb, lbuf, -1);
            snprintf(lbuf, sizeof(lbuf), "gmsg *** %s left the game (OUT)", p->nick);
            lobby_broadcast(lb, lbuf, p->slot);
            /* Check if game should end — end whenever no humans remain alive */
            {
                int alive_human = 0, alive_cnt = 0; Player *last = NULL;
                for (int ci=0; ci<MAX_PLAYERS; ci++) {
                    Player *cp = &lb->players[ci];
                    if (!cp->connected || !cp->alive || cp->spectator) continue;
                    alive_cnt++; last = cp;
                    if (!cp->is_bot) alive_human++;
                }
                /* End if: last_player_wins with ≤1 alive, OR no humans left alive */
                if ((lb->last_player_wins && alive_cnt <= 1) || alive_human == 0) {
                    /* End game immediately — no point waiting for bots to finish */
                    p->sock = INVALID_SOCK; /* avoid double-close below */
                    lobby_remove_player(lb, p);
                    lobby_end_game(lb, last ? last->nick : NULL);
                    /* Player's socket goes to pending after end_game cleans up */
                    for (int k=0; k<MAX_PENDING; k++) {
                        if (!g_pending[k].active) {
                            g_pending[k]=returning;
                    send_lobby_list(saved_sock);
                            break;
                        }
                    }
                    return;
                }
            }
        }
        /* saved_sock declared at top of leaveLobby handler */
        p->sock = INVALID_SOCK;
        lobby_remove_player(lb, p);
        /* Move the connection to the pending list so it can re-handshake */
        {
            int k;
            for (k = 0; k < MAX_PENDING; k++) {
                if (!g_pending[k].active) {
                    g_pending[k]=returning;
                    send_lobby_list(saved_sock);
                    log_info("Player %s returned to lobby select", p->nick);
                    break;
                }
            }
            if (k == MAX_PENDING) sock_close(saved_sock);
        }
        return;
    }

    /* ---- HOST CHANGES SETTINGS (host or admin, lobby must be idle) ---- */
    if (strcmp(cmd, "setsettings") == 0 && n >= 3) {
        int can_settings = (p->slot == lb->host_slot || p->is_admin || p->priv_level <= 2);
        if (!can_settings) {
            net_send(p->sock, "gmsg *** Only the host or an admin can change settings");
            return;
        }
        if (lb->game_state == STATE_PLAYING) {
            net_send(p->sock, "gmsg *** Cannot change settings during a game");
            return;
        }
        lb->no_attacks       = atoi(args[0]) ? 1 : 0;
        lb->last_player_wins = atoi(args[1]) ? 1 : 0;
        lb->no_taunts        = (n >= 4) ? (atoi(args[2]) ? 1 : 0) : lb->no_taunts;
        lb->speedup_enabled  = (n >= 5) ? (atoi(args[3]) ? 1 : 0) : lb->speedup_enabled;
        if (n >= 6) { lb->garbage_system    = atoi(args[4]); if (lb->garbage_system < 0) lb->garbage_system = 0; if (lb->garbage_system > 1) lb->garbage_system = 1; }
        if (n >= 7) { lb->garbage_targeting = atoi(args[5]); if (lb->garbage_targeting < 0) lb->garbage_targeting = 0; if (lb->garbage_targeting > 2) lb->garbage_targeting = 2; }
        if (n >= 8) lb->hold_enabled = atoi(args[6]) ? 1 : 0;
        auth_set_lobby_settings_full(lb->id, lb->no_attacks, lb->last_player_wins,
            lb->no_taunts, lb->speedup_enabled, lb->garbage_system, lb->garbage_targeting);
        {
            int i;
            for (i = 0; i < MAX_PLAYERS; i++) {
                if (lb->players[i].connected)
                    lobby_send_settings(lb, lb->players[i].sock);
            }
        }
        broadcast_lobby_list_update();
        return;
    }

    /* ---- TAUNT — player sends a taunt, broadcast to lobby ---- */
    if (strcmp(cmd, "taunt") == 0 && n >= 2) {
        if (!g_cfg.global_taunts || lb->no_taunts) {
            net_send(p->sock, "gmsg *** Taunts are disabled");
            return;
        }
        /* args[0] = target nick, args[1..] = taunt text (already assembled by client) */
        if (g_cfg.debug_ingame)
            log_info("[INGAME R%d] taunt: %s -> %s", lb->id, p->nick, args[0]);
        snprintf(buf, sizeof(buf), "taunt %s %s", p->nick, args[0]);
        lobby_broadcast(lb, buf, -1);
        auth_add_taunts(p->nick, 1);
        return;
    }
    return; /* unrecognised in-game command — ignored */
}

/* ---- Add player to lobby ---- */
static Player *lobby_add_player(Lobby *lb, sock_t sock,
                                 const char *nick, const char *team,
                                 char *recv_buf, int recv_len) {
    int i, slot = -1;
    char buf[MAX_MSG_LEN];
    Player *p;
    int is_first;
    int joining_mid_game = (lb->game_state == STATE_PLAYING);

    {
        /* Restore a reservation regardless of whether the room is full. */
        int rslot = -1;
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (!lb->players[i].connected && lb->players[i].reconnect_nick[0] &&
                strcasecmp(lb->players[i].reconnect_nick, nick) == 0 &&
                lb->players[i].reconnect_expires > time(NULL)) {
                rslot = i; break;
            }
        }
        if (rslot >= 0) {
        /* Reconnect: restore this slot */
        Player *rp = &lb->players[rslot];
        rp->sock = sock;
        rp->connected = 1;
        rp->reconnect_nick[0] = '\0';
        rp->reconnect_expires = 0;
        rp->recv_len = 0; rp->recv_buf[0] = 0;
        rp->sv_last_field = time(NULL);
        /* Restore recv buffer */
        if (recv_buf && recv_len > 0) {
            memcpy(rp->recv_buf, recv_buf, recv_len);
            rp->recv_len = recv_len;
        }
        /* Reserved slots already count toward player_count. */
        char rbuf[MAX_MSG_LEN];
        snprintf(rbuf, sizeof(rbuf), "gmsg *** %s reconnected!", nick);
        lobby_broadcast(lb, rbuf, -1);
        /* Re-send slot and host info first so client knows its identity */
        { char slotbuf[32]; snprintf(slotbuf,sizeof(slotbuf),"playernum %d",rp->slot);
          net_send(sock, slotbuf); }
        { char hbuf[32]; snprintf(hbuf,sizeof(hbuf),"host %d",lb->host_slot);
          net_send(sock, hbuf); }
        lobby_send_settings(lb, sock);
        /* Re-send game state to reconnecting client */
        snprintf(rbuf,sizeof(rbuf),"resume_ok %d %d",lb->session_round,rp->alive);
        net_send(sock,rbuf);
        snprintf(rbuf,sizeof(rbuf),"spectator %d",rp->spectator); net_send(sock,rbuf);
        snprintf(rbuf,sizeof(rbuf),"pause %d",lb->paused); net_send(sock,rbuf);
        for (int oi=0; oi<MAX_PLAYERS; oi++) {
            Player *other=&lb->players[oi];
            if (!other->connected || other==rp) continue;
            snprintf(rbuf,sizeof(rbuf),"playerjoin %d %s",other->slot,other->nick); net_send(sock,rbuf);
            char enc[FIELD_WIDTH*FIELD_HEIGHT+8]; field_encode(&other->field,enc,sizeof(enc));
            snprintf(rbuf,sizeof(rbuf),"f %d %s",other->slot,enc); net_send(sock,rbuf);
            if(!other->alive) { snprintf(rbuf,sizeof(rbuf),"playerlost %d",other->slot); net_send(sock,rbuf); }
        }
        char inventory[65]; int ic=0;
        for(int ii=rp->sv_specials_head;ii!=rp->sv_specials_tail;ii=(ii+1)%64)
            inventory[ic++]=SPECIAL_CHARS[rp->sv_specials[ii%64]-1];
        inventory[ic]=0;
        snprintf(rbuf,sizeof(rbuf),"resume_inventory %s",ic?inventory:"-"); net_send(sock,rbuf);
        net_send(sock, "ingame");
        { char ldbuf[32]; snprintf(ldbuf,sizeof(ldbuf),"lockdelay %d",lb->lock_delay_ms);
          net_send(sock, ldbuf); }
        { char enc[FIELD_WIDTH*FIELD_HEIGHT+8]; char fm[MAX_MSG_LEN];
          field_encode(&rp->field, enc, sizeof(enc));
          snprintf(fm, sizeof(fm), "resume_field %s", enc);
          net_send(sock, fm); }
        if(rp->authority)authority_send(lb,rp);
        log_info("Lobby %d: %s reconnected to slot %d", lb->id, nick, rp->slot);
        return rp;
        }
    }
    if (lb->player_count >= lb->max_players) return NULL;

    for (i = 0; i < MAX_PLAYERS; i++) {
        if (!lb->players[i].connected && !lb->players[i].reconnect_nick[0]) { slot = i + 1; break; }
    }
    if (slot < 0) return NULL;

    /* Count non-spectator players to check if this is truly the first active one */
    {
        int active = 0;
        for (i = 0; i < MAX_PLAYERS; i++)
            if (lb->players[i].connected && !lb->players[i].spectator) active++;
        is_first = (active == 0);
    }

    p = &lb->players[slot - 1];
    memset(p, 0, sizeof(*p));
    p->slot      = slot;
    p->sock      = sock;
    p->connected = 1;
    p->alive     = 0;
    p->spectator = joining_mid_game ? 1 : 0;
    strncpy(p->nick, nick, MAX_NICK_LEN - 1);
    /* Load team from DB (overrides the team arg which may be stale) */
    auth_get_team(nick, p->team, MAX_NICK_LEN);
    /* If client explicitly sent a non-empty team and DB is empty, use it and save */
    if (p->team[0] == '\0' && team && team[0] && strcmp(team, "-") != 0) {
        strncpy(p->team, team, MAX_NICK_LEN - 1);
        auth_set_team(nick, p->team);
    }
    field_init(&p->field);

    if (recv_buf && recv_len > 0) {
        memcpy(p->recv_buf, recv_buf, recv_len);
        p->recv_len = recv_len;
    }

    lb->player_count++;


    /* Assign host if no active host exists (covers is_first AND orphaned lobbies) */
    if (!joining_mid_game && (is_first || lb->host_slot == 0)) {
        lb->host_slot = slot;
        log_info("Lobby %d: %s is the new host (slot %d)", lb->id, nick, slot);
    }

    /* Tell new player their slot */
    snprintf(buf, sizeof(buf), "playernum %d", slot);
    net_send(sock, buf);

    /* Tell new player their team (loaded from DB) */
    snprintf(buf, sizeof(buf), "yourteam %s", p->team[0] ? p->team : "-");
    net_send(sock, buf);

    /* Tell new player current host */
    snprintf(buf, sizeof(buf), "host %d", lb->host_slot);
    net_send(sock, buf);

    /* Tell new player current settings */
    lobby_send_settings(lb, sock);

    /* Tell new player current leaderboard (always send — zero clears stale client data) */
    {char lbmsg[MAX_MSG_LEN];
     int i, pos;
     pos = snprintf(lbmsg, sizeof(lbmsg), "leaderboard %d %d",
                    lb->round_num, lb->lb_count);
     for (i = 0; i < lb->lb_count && pos < (int)sizeof(lbmsg)-64; i++)
         pos += snprintf(lbmsg+pos, sizeof(lbmsg)-pos, " %s %d",
                         lb->lb_nicks[i], lb->lb_wins[i]);
     net_send(sock, lbmsg);}
    /* Tell new player current team leaderboard */
    {char tlbmsg[MAX_MSG_LEN]; int i, pos;
     pos = snprintf(tlbmsg, sizeof(tlbmsg), "teamleaderboard %d",
                    lb->lb_team_count);
     for (i=0;i<lb->lb_team_count&&pos<(int)sizeof(tlbmsg)-64;i++)
         pos += snprintf(tlbmsg+pos, sizeof(tlbmsg)-pos, " %s %d",
                         lb->lb_team_nicks[i], lb->lb_team_wins[i]);
     net_send(sock, tlbmsg);}

    /* Tell new player whether bots are allowed on this server */
    snprintf(buf, sizeof(buf), "botsallowed %d", g_cfg.allow_bots ? 1 : 0);
    net_send(sock, buf);

    /* Guarantee bots are ready when game is not running */
    if (lb->game_state == STATE_MENU) {
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (lb->players[i].connected && lb->players[i].is_bot &&
                !lb->players[i].ready) {
                lb->players[i].ready = 1;
                /* Broadcast updated ready status to everyone already in room */
                char rbuf[MAX_MSG_LEN];
                snprintf(rbuf,sizeof(rbuf),"readystatus %d 1",lb->players[i].slot);
                lobby_broadcast(lb, rbuf, -1);
            }
        }
    }

    /* Tell new player about existing players (including their ready status) */
    for (i = 0; i < MAX_PLAYERS; i++) {
        Player *other = &lb->players[i];
        if (other->connected && other->slot != slot) {
            snprintf(buf, sizeof(buf), "playerjoin %d %s", other->slot, other->nick);
            net_send(sock, buf);
            if (other->team[0]) {
                snprintf(buf, sizeof(buf), "team %d %s", other->slot, other->team);
                net_send(sock, buf);
            }
            /* Send ready state of existing player to newcomer */
            snprintf(buf, sizeof(buf), "readystatus %d %d", other->slot, other->ready);
            net_send(sock, buf);
            /* If existing player is a bot, tell newcomer */
            if (other->is_bot) {
                snprintf(buf, sizeof(buf), "botinfo %d %s", other->slot,
                         other->bot_difficulty==AI_EASY?"easy":
                         other->bot_difficulty==AI_MEDIUM?"medium":"hard");
                net_send(sock, buf);
            }
        }
    }

    /* Tell existing players about new one */
    snprintf(buf, sizeof(buf), "playerjoin %d %s", slot, nick);
    lobby_broadcast(lb, buf, slot);
    if (p->team[0]) {
        snprintf(buf, sizeof(buf), "team %d %s", slot, p->team);
        lobby_broadcast(lb, buf, slot);
    }

    if (joining_mid_game) {
        /* Tell new player they're a spectator */
        net_send(sock, "spectator 1");
        net_send(sock, "ingame");
        /* If lobby is currently paused, tell the new spectator */
        if(lb->paused){
            char pbuf[32]; snprintf(pbuf,sizeof(pbuf),"pause 1 0");
            net_send(sock, pbuf);
        }
        /* Send current field states of all active players */
        for (i = 0; i < MAX_PLAYERS; i++) {
            Player *other = &lb->players[i];
            if (other->connected && other->slot != slot && other->alive) {
                char encoded[FIELD_WIDTH * FIELD_HEIGHT + 8];
                field_encode(&other->field, encoded, sizeof(encoded));
                snprintf(buf, sizeof(buf), "f %d %s", other->slot, encoded);
                net_send(sock, buf);
            }
        }
        snprintf(buf, sizeof(buf), "gmsg *** %s joined as spectator (game in progress)", nick);
        lobby_broadcast(lb, buf, -1);
    } else {
        net_send(sock, "spectator 0");
        if (is_first) {
            snprintf(buf, sizeof(buf), "gmsg *** %s joined as host -- press Enter to start", nick);
        } else {
            snprintf(buf, sizeof(buf), "gmsg *** %s joined the lobby", nick);
        }
        lobby_broadcast(lb, buf, -1);
    }

    broadcast_lobby_list_update();
    g_stat_connects++;
    log_info("Lobby %d: %s joined (slot %d, spectator=%d, host=%d)",
             lb->id, nick, slot, p->spectator, lb->host_slot);
    return p;
}

/* ---- Remove player ---- */
void lobby_remove_player(Lobby *lb, Player *p) {
    if (!p->is_bot) g_stat_disconnects++;

    if (p->is_bot) { memset(p, 0, sizeof(*p)); return; }
    char buf[MAX_MSG_LEN];
    int i;
    int was_host     = (p->slot == lb->host_slot);
    int was_spectator= p->spectator;

    if (!p->connected) return;

    /* If game is in progress and reconnect window is enabled, reserve the slot */
    if (lb->game_state == STATE_PLAYING && !was_spectator
        && g_cfg.reconnect_window > 0 && p->nick[0]) {
        snprintf(buf, sizeof(buf), "gmsg *** %s disconnected (%.0ds reconnect window)",
                 p->nick, g_cfg.reconnect_window);
        lobby_broadcast(lb, buf, p->slot);
        log_info("Lobby %d: %s disconnected — slot %d reserved for %ds",
                 lb->id, p->nick, p->slot, g_cfg.reconnect_window);
        /* Reserve slot: keep connected=1, mark the expire time */
        strncpy(p->reconnect_nick, p->nick, MAX_NICK_LEN-1);
        p->reconnect_expires = time(NULL) + g_cfg.reconnect_window;
        if (p->sock != INVALID_SOCK) { sock_close(p->sock); p->sock = INVALID_SOCK; }
        /* Mark as disconnected but slot reserved */
        p->connected = 0;
        /* Don't decrement player_count — slot is held */
        return;
    }

    snprintf(buf, sizeof(buf), "playerleave %d", p->slot);
    lobby_broadcast(lb, buf, p->slot);
    log_info("Lobby %d: %s left (slot %d)", lb->id, p->nick, p->slot);
    auth_set_status(p->nick, "offline");

    if (p->sock != INVALID_SOCK) { sock_close(p->sock); p->sock = INVALID_SOCK; }
    p->connected = 0;
    p->alive     = 0;
    lb->player_count--;

    /* Check game-over condition if this was an active player */
    if (!was_spectator && lb->game_state == STATE_PLAYING && lb->last_player_wins) {
        int alive_count = 0;
        Player *last_alive = NULL;
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (lb->players[i].connected && lb->players[i].alive
                    && !lb->players[i].spectator) {
                alive_count++;
                last_alive = &lb->players[i];
            }
        }
        if (alive_count <= 1)
            lobby_end_game(lb, last_alive ? last_alive->nick : NULL);
    }

    if (was_host && lb->player_count > 0) {
        lobby_assign_host(lb);
        if (lb->host_slot > 0) lobby_announce_host(lb);
    } else if (lb->player_count == 0) {
        lb->host_slot  = 0;
        lb->game_state = STATE_MENU;
        /* NOTE: no_attacks and last_player_wins are NOT reset — they persist
           in the DB and survive both lobby-empty and server restart */
        lb->lb_count      = 0;
        lb->lb_team_count = 0;
        lb->round_num     = 0;
        memset(lb->lb_nicks,      0, sizeof(lb->lb_nicks));
        memset(lb->lb_wins,       0, sizeof(lb->lb_wins));
        memset(lb->lb_team_nicks, 0, sizeof(lb->lb_team_nicks));
        memset(lb->lb_team_wins,  0, sizeof(lb->lb_team_wins));
    }

    broadcast_lobby_list_update();
}

/* ---- Main server loop ---- */
/* ---- Server console command processor ---- */
/* ---- Server console command -- routed to unified command system ---- */
static void console_out(const char *ln, void *ctx) {
    (void)ctx; printf("%s\n", ln);
}
static void server_console_command(const char *line_in) {
    if (server_exec_command(line_in, "console", console_out, NULL) != 0)
        printf("Unknown command. Type 'help'.\n");
}

/* ---- Output callback for remote admin: sends each line as remoteadmin_resp ---- */
void remote_admin_out(const char *ln, void *ctx) {
    struct { sock_t sock; } *c = ctx;
    char rb[MAX_MSG_LEN];
    snprintf(rb, sizeof(rb), "remoteadmin_resp %s", ln);
    net_send(c->sock, rb);
}

void server_run(int port, const char *cfg_path) {
    fd_set read_fds;
    struct timeval tv;
    int i, j, n;
    char line[MAX_MSG_LEN];
    (void)cfg_path;  /* config already loaded by main() */

    /* Config is already loaded + log file opened by main().
       server_run just applies runtime overrides. */
    if (port != DEFAULT_PORT && port > 0) g_cfg.port = port;
    port = g_cfg.port;

    log_info("LineClash server v%s (%s) starting on port %d",
             TETRINETZ_VERSION, BUILD_DATE, port);
    printf("LineClash server v%s  --  port %d\n", TETRINETZ_VERSION, port);
    printf("Config  : %s\n", cfg_path ? cfg_path : CFG_FILE);
    printf("Database: %s\n", g_cfg.db_file);
    printf("Log     : %s\n", g_cfg.log_file[0] ? g_cfg.log_file : "(none)");

    g_stat_start_time = time(NULL);
    srand((unsigned)g_stat_start_time);
    net_init();
    if (!auth_open(g_cfg.db_file)) {
        log_error("Failed to open authentication database");
        return;
    }
    server_init(&g_server);
    /* Apply config room count and player limits */
    {
        int li;
        g_server.lobby_count = g_cfg.num_lobbies;
        for (li = 0; li < g_server.lobby_count; li++) {
            Lobby *lb = &g_server.lobbies[li];
            lb->id = li + 1;
            snprintf(lb->name, MAX_LOBBY_NAME, "Room %d", li + 1);
            lb->max_players = g_cfg.max_players_per_room;
            lb->player_count = 0;
            lobby_init_defaults(lb);
            auth_get_lobby_settings_full(lb->id,
                &lb->no_attacks, &lb->last_player_wins, &lb->no_taunts,
                &lb->speedup_enabled, &lb->garbage_system, &lb->garbage_targeting);
            auth_get_room_topic(lb->id, lb->topic, sizeof(lb->topic));
            /* Restore persistent bots for this room */
            if (g_cfg.allow_bots) {
                int saved_count=0, saved_diff=1;
                auth_get_room_bots(lb->id, &saved_count, &saved_diff);
                for (int bi=0; bi<saved_count; bi++) {
                    int slot = lobby_add_bot(lb, saved_diff);
                    if (slot > 0) {
                        /* Mark bot as ready */
                        for (int pi=0; pi<MAX_PLAYERS; pi++)
                            if (lb->players[pi].connected && lb->players[pi].slot==slot)
                                lb->players[pi].ready = 1;
                    }
                }
                if (saved_count > 0)
                    log_info("Room %d: restored %d persistent bot(s) (diff=%d)",
                             lb->id, saved_count, saved_diff);
            }
        }
    }

    g_server.listen_sock = net_listen(port);
    if (g_server.listen_sock == INVALID_SOCK) {
        log_error("Failed to start server");
        return;
    }

    log_info("LineClash server started on port %d", port);
    log_info("%d lobbies available", g_server.lobby_count);
    log_info("Press Ctrl+C to stop. Type 'help' for console commands.");
    server_api_init();
    g_cfg.port=port;server_directory_start();

    while (g_server.running) {
        server_directory_tick();
        for(int al=0;al<g_server.lobby_count;al++)for(int ap=0;ap<MAX_PLAYERS;ap++)authority_tick(&g_server.lobbies[al],&g_server.lobbies[al].players[ap]);
        net_flush_all();
        server_api_poll();
        {
            time_t now_expiry=time(NULL);
            for (int eli=0; eli<g_server.lobby_count; eli++) {
                Lobby *expiry_lb=&g_server.lobbies[eli];
                        /* Expire stale reconnect slots */
                        { int ri;
                          for (ri = 0; ri < MAX_PLAYERS; ri++) {
                              Player *rp = &expiry_lb->players[ri];
                              if (!rp->connected && rp->reconnect_nick[0] &&
                                  rp->reconnect_expires > 0 && now_expiry >= rp->reconnect_expires) {
                                  char rbuf[MAX_MSG_LEN];
                                  snprintf(rbuf, sizeof(rbuf), "gmsg *** %s reconnect window expired",
                                           rp->reconnect_nick);
                                  lobby_broadcast(expiry_lb, rbuf, -1);
                                  snprintf(rbuf, sizeof(rbuf), "playerleave %d", rp->slot);
                                  lobby_broadcast(expiry_lb, rbuf, rp->slot);
                                  auth_set_status(rp->reconnect_nick, "offline");
                                  int expired_host=(rp->slot==expiry_lb->host_slot);
                                  memset(rp, 0, sizeof(*rp));
                                  expiry_lb->player_count--;
                                  if(expired_host) {
                                      expiry_lb->host_slot=0;
                                      for(int hi=0;hi<MAX_PLAYERS;hi++)
                                          if(expiry_lb->players[hi].connected && !expiry_lb->players[hi].is_bot) {
                                              expiry_lb->host_slot=expiry_lb->players[hi].slot; break;
                                          }
                                      snprintf(rbuf,sizeof(rbuf),"host %d",expiry_lb->host_slot);
                                      lobby_broadcast(expiry_lb,rbuf,-1);
                                  }
                              }
                          }
                        }

                int active=0, reserved=0;
                for (int ri=0; ri<MAX_PLAYERS; ri++) {
                    Player *rp=&expiry_lb->players[ri];
                    if (rp->connected && rp->alive && !rp->is_bot && !rp->spectator) active++;
                    if (rp->reconnect_nick[0] && rp->reconnect_expires>now_expiry) reserved++;
                }
                if (!active && !reserved && expiry_lb->game_state==STATE_PLAYING)
                    lobby_end_game(expiry_lb,NULL);
            }
        }

        sock_t max_fd = g_server.listen_sock;

        FD_ZERO(&read_fds);
        FD_SET(g_server.listen_sock, &read_fds);

        /* API status port */
        {sock_t api = server_api_get_sock();
         if (api != INVALID_SOCK) {
             FD_SET(api, &read_fds);
             if (api > max_fd) max_fd = api;
         }}

        /* Watch stdin for console commands (POSIX only — Windows uses a thread) */
#if defined(PLATFORM_POSIX)
        FD_SET(0, &read_fds);  /* STDIN_FILENO = 0 */
        if ((sock_t)0 > max_fd) max_fd = (sock_t)0;
#endif

        for (i = 0; i < MAX_PENDING; i++) {
            if (g_pending[i].active) {
                FD_SET(g_pending[i].sock, &read_fds);
                if (g_pending[i].sock > max_fd) max_fd = g_pending[i].sock;
            }
        }
        for (i = 0; i < g_server.lobby_count; i++) {
            Lobby *lb = &g_server.lobbies[i];
            for (j = 0; j < MAX_PLAYERS; j++) {
                Player *p = &lb->players[j];
                if (p->connected && p->sock != INVALID_SOCK) {
                    FD_SET(p->sock, &read_fds);
                    if (p->sock > max_fd) max_fd = p->sock;
                }
            }
        }

        tv.tv_sec = 0; tv.tv_usec = 50000;
        n = select((int)(max_fd + 1), &read_fds, NULL, NULL, &tv);
        if (n < 0) {
#ifdef PLATFORM_WINDOWS
            if (WSAGetLastError() == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            log_error("select() failed"); break;
        }

        /* ---- Bot AI tick (every main loop iteration) ---- */
        for (int bli=0; bli<g_server.lobby_count; bli++)
            lobby_tick_bots(&g_server.lobbies[bli]);

        /* ---- Periodic heartbeat / anti-cheat check (1/sec) ---- */
        {
            static int hb_tick = 0;
            if (++hb_tick >= 20) { /* 20 × 50ms = 1 second */
                hb_tick = 0;
                int li2, pi2;
                for (li2=0; li2<g_server.lobby_count; li2++) {
                    Lobby *lb2 = &g_server.lobbies[li2];
                    if (lb2->game_state != STATE_PLAYING) continue;

                    /* ---- Anti-cheat heartbeat ---- */
                    for (pi2=0; pi2<MAX_PLAYERS; pi2++) {
                        Player *pp = &lb2->players[pi2];
                        if (!pp->connected || !pp->alive || pp->spectator) continue;
                        lobby_handle_message(lb2, pp, "__hb__");
                    }

                    /* ---- Speed-up: add lines after delay, every interval ---- */
                    if (lb2->speedup_enabled && lb2->game_start_time > 0) {
                        time_t now2 = time(NULL);
                        int elapsed = (int)(now2 - lb2->game_start_time);
                        int delay_s = lb2->speedup_delay * 60;
                        if (elapsed >= delay_s) {
                            int since_last = (int)(now2 - lb2->speedup_last_add);
                            if (since_last >= lb2->speedup_interval) {
                                lb2->speedup_last_add = now2;
                                /* Randomized speedup announcement */
                                static const char *speedup_msgs[] = {
                                    "FASTER! No coffee breaks! +%d line%s for everyone!",
                                    "Speed up! +%d line%s incoming — run!",
                                    "Getting bored? +%d line%s for everyone!",
                                    "Time's up! +%d extra line%s — panic!",
                                    "LUDICROUS SPEED! +%d line%s from above!",
                                    "The floor is rising! +%d line%s!",
                                    "TIDE COMING IN! +%d garbage line%s!",
                                    "House adds +%d line%s. The house always wins.",
                                };
                                static int speedup_msg_idx = 0;
                                int mi = speedup_msg_idx % (int)(sizeof(speedup_msgs)/sizeof(speedup_msgs[0]));
                                speedup_msg_idx++;
                                char su_plain[MAX_MSG_LEN];
                                snprintf(su_plain, sizeof(su_plain), speedup_msgs[mi],
                                    lb2->speedup_lines,
                                    lb2->speedup_lines == 1 ? "" : "s");
                                /* Use dedicated "speedup" cmd — clients show it in bright color */
                                char su_msg[MAX_MSG_LEN];
                                snprintf(su_msg, sizeof(su_msg), "speedup %s", su_plain);
                                lobby_broadcast(lb2, su_msg, -1);
                                /* Send addline to every alive human player;
                                 * apply garbage directly to bot fields */
                                for (pi2=0; pi2<MAX_PLAYERS; pi2++) {
                                    Player *pp = &lb2->players[pi2];
                                    if (!pp->connected || !pp->alive || pp->spectator)
                                        continue;
                                    if (pp->is_bot) {
                                        /* Apply garbage directly to bot's server field */
                                        field_add_garbage(&pp->field, lb2->speedup_lines);
                                    } else {
                                        char al_buf[32];
                                        snprintf(al_buf, sizeof(al_buf), "addline %d", lb2->speedup_lines);
                                        net_send(pp->sock, al_buf);
                                    }
                                }
                                if (g_cfg.debug_ingame)
                                    log_info("[SPEEDUP R%d] +%d line%s (elapsed=%ds)",
                                             lb2->id, lb2->speedup_lines,
                                             lb2->speedup_lines == 1 ? "" : "s", elapsed);
                            }
                        }
                    }
                }
            }
        }

        /* ---- Shutdown countdown tick (20 ticks/sec at 50ms) ---- */
        if (g_server.shutdown_ticks > 0) {
            g_server.shutdown_ticks--;
            /* Warn at 5 min, 2 min, 1 min, 30s remaining */
            int ticks_left = g_server.shutdown_ticks;
            struct { int ticks; int idx; const char *label; } warns[] = {
                {6000, 0, "5 minutes"},
                {2400, 1, "2 minutes"},
                {1200, 2, "1 minute"},
                {600,  3, "30 seconds"},
            };
            int wi;
            for (wi = 0; wi < 4; wi++) {
                if (ticks_left <= warns[wi].ticks && !g_server.shutdown_warned[wi]) {
                    g_server.shutdown_warned[wi] = 1;
                    char wbuf[MAX_MSG_LEN];
                    if (g_server.shutdown_reason[0])
                        snprintf(wbuf, sizeof(wbuf),
                            "gmsg *** Server shutting down in %s — %s",
                            warns[wi].label, g_server.shutdown_reason);
                    else
                        snprintf(wbuf, sizeof(wbuf),
                            "gmsg *** Server shutting down in %s",
                            warns[wi].label);
                    schat_broadcast(wbuf);
                    log_info("Shutdown warning: %s remaining", warns[wi].label);
                }
            }
            if (g_server.shutdown_ticks == 0) {
                schat_broadcast("gmsg *** Server is shutting down now. Goodbye!");
                log_info("Shutdown countdown reached zero — stopping server.");
                g_server.running = 0;
            }
        }

        /* ---- Server console (stdin) ---- */
#if defined(PLATFORM_POSIX)
        if (FD_ISSET(0, &read_fds)) {
            /* Simple in-memory command history (non-persistent) */
            static char hist[64][512];
            static int  hist_count=0, hist_pos=-1;
            char console_line[512];
            if (fgets(console_line, sizeof(console_line), stdin)) {
                char *nl = strchr(console_line,'\n'); if(nl) *nl='\0';
                char *cr = strchr(console_line,'\r'); if(cr) *cr='\0';
                if (console_line[0]) {
                    /* Save to history */
                    if(hist_count<64){
                        strncpy(hist[hist_count],console_line,511);
                        hist_count++;
                    } else {
                        memmove(hist[0],hist[1],63*512);
                        strncpy(hist[63],console_line,511);
                    }
                    hist_pos=-1;
                    server_console_command(console_line); /* slash stripped inside */
                }
                (void)hist_pos;
            }
        }
#endif
        /* API status port */
        {sock_t api=server_api_get_sock();
         if(api!=INVALID_SOCK&&FD_ISSET(api,&read_fds)) server_api_accept();}

        if (FD_ISSET(g_server.listen_sock, &read_fds)) {
            struct sockaddr_in ca;
#ifdef PLATFORM_WINDOWS
            int addrlen = sizeof(ca);
#else
            socklen_t addrlen = sizeof(ca);
#endif
            sock_t client = accept(g_server.listen_sock, (struct sockaddr*)&ca, &addrlen);
            if (client != INVALID_SOCK) {
                int found = 0;
                char client_ip[48];
                strncpy(client_ip, inet_ntoa(ca.sin_addr), sizeof(client_ip)-1);
                net_set_nonblocking(client);
                /* IP ban check */
                if (auth_is_banned_ip(client_ip)) {
                    log_info("Rejected banned IP: %s", client_ip);
                    net_send(client, "noconnecting You are banned from this server");
                    sock_close(client);
                } else {
                    for (i = 0; i < MAX_PENDING; i++) {
                        if (!g_pending[i].active) {
                            g_pending[i].sock    = client;
                            g_pending[i].recv_buf[0] = '\0';
                            g_pending[i].recv_len= 0;
                            g_pending[i].active  = 1;
                            g_pending[i].auth_ok = 0;
                            g_pending[i].is_tool = 0;
                            g_pending[i].nick_is_new = 0;
                            g_pending[i].nonce[0] = '\0';
                            g_pending[i].pending_nick[0] = '\0';
                            strncpy(g_pending[i].ip, client_ip, sizeof(g_pending[i].ip)-1);
                            found = 1;
                            log_info("New connection from %s", client_ip);
                            net_send(client, "noconnecting LineClash/1.0");
                            /* Send server name so client can display it */
                            {char sn[MAX_MSG_LEN];
                             snprintf(sn,sizeof(sn),"servername %s",g_cfg.server_name);
                             net_send(client,sn);}
                            /* Send MOTD if configured */
                            if (g_cfg.motd[0]) {
                                char motd_msg[MAX_MSG_LEN];
                                char motd_trim[MAX_MSG_LEN-8];
                                strncpy(motd_trim, g_cfg.motd, sizeof(motd_trim)-1);
                                motd_trim[sizeof(motd_trim)-1] = '\0';
                                snprintf(motd_msg, sizeof(motd_msg), "motd %s", motd_trim);
                                net_send(client, motd_msg);
                            }
                            send_lobby_list(client);
                            break;
                        }
                    }
                    if (!found) { net_send(client, "noconnecting Server is full"); sock_close(client); }
                }
            }
        }

        /* Pending — multi-step auth flow then lineclash_join:
         *   1. CLIENT → authnick <nick>
         *   2. SERVER → authchallenge <nonce>   (existing nick)
         *              OR authchallenge new       (unknown nick, needs pw hash)
         *   3. CLIENT → authresp <hmac_or_pwhash>
         *   4. SERVER → authok / autherr
         *   5. CLIENT → lineclash_join <nick> <team> <lobby>
         */
        for (i = 0; i < MAX_PENDING; i++) {
            Pending *pend = &g_pending[i];
            if (!pend->active) continue;
            /* Process slot if socket is readable OR if app-buffer already has data.
             * Without the recv_len check, lines buffered from a multi-line recv()
             * (e.g. toolmode + authnick arriving in the same TCP segment) are
             * never processed: select() reports not-readable once the kernel
             * buffer is drained, but pend->recv_buf still holds the remaining lines. */
            if (!FD_ISSET(pend->sock, &read_fds) && pend->recv_len == 0) continue;
            do {
            n = net_recv(pend->sock, pend->recv_buf, &pend->recv_len, line, sizeof(line));
            if (n < 0) {
                if (!pend->is_tool && pend->auth_ok && pend->pending_nick[0])
                    auth_set_status(pend->pending_nick, "offline");
                sock_close(pend->sock); pend->active = 0; break;
            }
            if (n == 1 && line[0]) {
                char cmd[MAX_MSG_LEN], args[16][MAX_MSG_LEN];
                int np = proto_parse(line, cmd, args, 16);

                if (!strcmp(cmd,"ping") && np>=2) {
                    char pong[64]; snprintf(pong,sizeof(pong),"pong %.58s",args[0]);
                    net_send(pend->sock,pong); continue;
                }
                /* ---- CLIENTINFO — client type/platform/version ---- */
                if (strcmp(cmd, "clientinfo") == 0 && np >= 4 && pend->auth_ok) {
                    /* Store temporarily in pending; copied to Player on lineclash_join */
                    strncpy(pend->client_type,     args[0], sizeof(pend->client_type)-1);
                    strncpy(pend->client_platform, args[1], sizeof(pend->client_platform)-1);
                    strncpy(pend->client_version,  args[2], sizeof(pend->client_version)-1);
                    pend->input_capable=(np>=5 && !strcmp(args[3],"input-v1"));
                    continue;
                }

                /* ---- LISTLOBBIES — refresh while in pending state ---- */
                if (strcmp(cmd, "listlobbies") == 0) {
                    send_lobby_list(pend->sock);
                    continue;
                }

                /* ---- REQLEADERBOARD — refresh from lobby select ---- */
                if (strcmp(cmd, "reqleaderboard") == 0 && pend->auth_ok) {
                    send_server_leaderboard(pend->sock);
                    send_server_team_leaderboard(pend->sock);
                    /* playerlist */
                    {char pmsg[MAX_MSG_LEN]; int pos,li2,pi2,pk2;
                     pos=snprintf(pmsg,sizeof(pmsg),"playerlist");
                     for(li2=0;li2<g_server.lobby_count&&pos<(int)sizeof(pmsg)-64;li2++){
                         Lobby *lb2=&g_server.lobbies[li2];
                         for(pi2=0;pi2<MAX_PLAYERS&&pos<(int)sizeof(pmsg)-64;pi2++){
                             Player *pp=&lb2->players[pi2];
                             if(!pp->connected||pp->spectator||!pp->nick[0]||pp->is_bot) continue;
                             int playing=(lb2->game_state==STATE_PLAYING&&pp->alive);
                             pos+=snprintf(pmsg+pos,sizeof(pmsg)-pos," %s %s %d",
                                           pp->nick,playing?"playing":"idle",lb2->id);
                         }
                     }
                     for(pk2=0;pk2<MAX_PENDING&&pos<(int)sizeof(pmsg)-64;pk2++){
                         if(g_pending[pk2].active&&g_pending[pk2].auth_ok&&
                            g_pending[pk2].pending_nick[0]&&(&g_pending[pk2]!=pend))
                             pos+=snprintf(pmsg+pos,sizeof(pmsg)-pos," %s idle 0",
                                           g_pending[pk2].pending_nick);
                     }
                     net_send(pend->sock,pmsg);}
                    continue;
                }

                /* ---- GCHAT — global chat from lobby select ---- */
                if (strcmp(cmd, "schat") == 0 && pend->auth_ok && np >= 2) {
                    int priv = auth_get_priv_level(pend->pending_nick);
                    char full_msg[MAX_MSG_LEN]=""; int ai2;
                    for(ai2=0;ai2<np-1&&ai2<15;ai2++){
                        if(ai2) strncat(full_msg," ",sizeof(full_msg)-strlen(full_msg)-1);
                        strncat(full_msg,args[ai2],sizeof(full_msg)-strlen(full_msg)-1);
                    }
                    const char *msg2 = full_msg;
                    if (msg2[0] == '/') {
                        if (priv <= 2) {
                            server_admin_gchat_command_pending(pend, msg2+1);
                        } else {
                            net_send(pend->sock,"gmsg *** Only admins can use / commands");
                        }
                    } else {
                        /* All users can send server chat — lobby-select only */
                        char gmsg2[MAX_MSG_LEN];
                        snprintf(gmsg2,sizeof(gmsg2),"schat %s %s",pend->pending_nick,msg2);
                        schat_lobby(gmsg2);
                    }
                    continue;
                }

                /* ---- REMOTE ADMIN — authenticated admin connected remotely ---- */
                if (strcmp(cmd, "remoteadmin") == 0 && pend->auth_ok && np >= 2) {
                    int priv = auth_get_priv_level(pend->pending_nick);
                    if (priv > 2) {
                        net_send(pend->sock, "remoteadmin_err Access denied: admin or manager account required");
                        continue;
                    }
                    /* Reconstruct full command line from args (args[0..np-2]) */
                    char radm_line[MAX_MSG_LEN] = "";
                    int ai;
                    for (ai = 0; ai < np-1 && ai < 15; ai++) {
                        if (ai) strncat(radm_line," ",sizeof(radm_line)-strlen(radm_line)-1);
                        strncat(radm_line,args[ai],sizeof(radm_line)-strlen(radm_line)-1);
                    }
                    if (!strcmp(args[0],"quit")) {
                        net_send(pend->sock,"remoteadmin_resp Goodbye");
                        sock_close(pend->sock); pend->active=0;
                        log_info("Remote admin %s disconnected", pend->pending_nick);
                    } else {
                        /* Output callback: send each reply line back as remoteadmin_resp */
                        struct { sock_t sock; } radm_ctx; radm_ctx.sock=pend->sock;
                        log_info("Remote admin %s: %s", pend->pending_nick, radm_line);
                        if (server_exec_command(radm_line, pend->pending_nick,
                                                remote_admin_out, &radm_ctx) != 0)
                            net_send(pend->sock,"remoteadmin_resp Unknown command. Type /help");
                    }
                    continue;
                }

                /* ---- Tool-mode flag — sent by lineclashd-tool before authnick ---- */
                if (strcmp(cmd, "toolmode") == 0) {
                    pend->is_tool = 1;
                    continue;
                }

                /* ---- Step 1: client sends nick, server issues challenge ---- */
                if (strcmp(cmd, "authnick") == 0 && np >= 2) {
                    const char *nick = args[0];
                    char msg[MAX_MSG_LEN];
                    int i2, nlen = (int)strlen(nick);
                    int valid = (nlen >= 1 && nlen <= 15);
                    if (valid) {
                        for (i2=0; i2<nlen; i2++) {
                            unsigned char ch=(unsigned char)nick[i2];
                            if (!isprint(ch)||ch==':'||ch==' ') { valid=0; break; }
                        }
                    }
                    if (!valid) {
                        net_send(pend->sock, "autherr Invalid nickname (1-15 chars, no spaces)");
                        continue;
                    }
                    /* Nick ban check */
                    if (auth_nick_exists(nick) && auth_is_banned_nick(nick)) {
                        net_send(pend->sock, "autherr Your account has been banned");
                        log_info("Auth: banned nick rejected from %s", pend->ip);
                        continue;
                    }
                    /* Activation check — deactivated accounts cannot login */
                    if (auth_nick_exists(nick) && !auth_is_activated(nick)) {
                        net_send(pend->sock, "autherr Your account has been deactivated");
                        log_info("Auth: deactivated nick rejected: %s from %s", nick, pend->ip);
                        continue;
                    }
                    /* Duplicate login check — skip for admin tool sessions */
                    if (!pend->is_tool) {
                        int li2,pi2,dup=0;
                        /* Check rooms */
                        for(li2=0;li2<g_server.lobby_count&&!dup;li2++)
                            for(pi2=0;pi2<MAX_PLAYERS&&!dup;pi2++)
                                if(g_server.lobbies[li2].players[pi2].connected&&
                                   !strcasecmp(g_server.lobbies[li2].players[pi2].nick,nick))
                                    dup=1;
                        /* Check pending — auth_ok OR mid-auth (nick claimed, not yet verified)
                         * but exclude tool sessions and the current connection itself */
                        for(pi2=0;pi2<MAX_PENDING&&!dup;pi2++)
                            if(g_pending[pi2].active&&
                               g_pending[pi2].pending_nick[0]&&
                               !g_pending[pi2].is_tool&&
                               !g_pending[pi2].is_room_kicked&&
                               &g_pending[pi2]!=pend&&
                               !strcasecmp(g_pending[pi2].pending_nick,nick))
                                dup=1;
                        if(dup){
                            net_send(pend->sock,"autherr That account is already logged in");
                            log_info("Auth: duplicate login rejected for '%s' from %s",nick,pend->ip);
                            continue;
                        }
                    }
                    /* require_registered: block new accounts */
                    if (g_cfg.require_registered && !auth_nick_exists(nick)) {
                        net_send(pend->sock, "autherr This server only allows registered players");
                        continue;
                    }
                    strncpy(pend->pending_nick, nick, sizeof(pend->pending_nick)-1);
                    pend->nick_is_new = !auth_nick_exists(nick);
                    if (pend->nick_is_new) {
                        snprintf(msg, sizeof(msg), "authchallenge new");
                        pend->nonce[0] = '\0';
                    } else {
                        auth_make_nonce(pend->nonce);
                        snprintf(msg, sizeof(msg), "authchallenge %s", pend->nonce);
                    }
                    net_send(pend->sock, msg);
                    continue;
                }

                /* ---- Step 3: client sends response ---- */
                if (strcmp(cmd, "authresp") == 0 && np >= 2) {
                    const char *resp = args[0];
                    AuthResult res;
                    if (!pend->pending_nick[0]) {
                        net_send(pend->sock, "autherr Send authnick first");
                        continue;
                    }
                    if (pend->nick_is_new) {
                        res = auth_register(pend->pending_nick, resp);
                    } else {
                        res = auth_verify_login(pend->pending_nick, pend->nonce, resp);
                    }
                    if (res == AUTH_OK || res == AUTH_NEW_USER) {
                        int is_adm = auth_is_admin(pend->pending_nick);
                        net_send(pend->sock, res==AUTH_NEW_USER
                            ? "authok new Account created -- welcome!"
                            : "authok existing Welcome back!");
                        if (is_adm) net_send(pend->sock, "adminok");
                        pend->auth_ok = 1;
                        if (!pend->is_tool)
                            auth_set_status(pend->pending_nick, "idle");
                    } else if (res == AUTH_WRONG_PASSWORD) {
                        net_send(pend->sock, "autherr Wrong password");
                        /* Keep connection open — client can retry authnick */
                        pend->pending_nick[0] = '\0';
                        pend->nonce[0] = '\0';
                    } else {
                        net_send(pend->sock, "autherr Authentication error");
                        pend->pending_nick[0] = '\0';
                    }
                    continue;
                }

                /* ---- Step 5: join a lobby (only after auth) ---- */
                if (strcmp(cmd, "lineclash_join") == 0 && np >= 3) {
                    if (!pend->auth_ok) {
                        net_send(pend->sock, "noconnecting Authentication required");
                        sock_close(pend->sock);
                        pend->active = 0;
                        continue;
                    }
                    {
                        char nick[MAX_NICK_LEN], team_s[MAX_NICK_LEN];
                        int room_id = 1;
                        Lobby *lb;
                        Player *player;
                        if (strcasecmp(args[0], pend->pending_nick)) {
                            net_send(pend->sock, "joinerr Nickname must match authenticated account");
                            continue;
                        }
                        snprintf(nick, MAX_NICK_LEN, "%s", pend->pending_nick);
                        snprintf(team_s, MAX_NICK_LEN, "%s", args[1]); team_s[MAX_NICK_LEN-1]='\0';
                        if (np >= 4) room_id = atoi(args[2]);
                        if (room_id < 1 || room_id > g_server.lobby_count) room_id = 1;
                        lb = server_find_lobby(&g_server, room_id);
                        if (!lb) lb = &g_server.lobbies[0];
                        if (np >= 6 && !strcmp(args[3], "resume")) {
                            int reserved = 0;
                            for (int ri=0; ri<MAX_PLAYERS; ri++) {
                                Player *rp=&lb->players[ri];
                                if (!rp->connected && !strcasecmp(rp->reconnect_nick,nick) &&
                                    rp->reconnect_expires>time(NULL)) reserved=1;
                            }
                            if (!reserved || atoi(args[4]) != lb->session_round || lb->game_state != STATE_PLAYING) {
                                net_send(pend->sock, "reconnect_failed Round ended or reconnect window expired");
                                continue;
                            }
                        }
                        player = lobby_add_player(lb, pend->sock, nick, team_s,
                                                  pend->recv_buf, pend->recv_len);
                        if (!player) {
                            /* Room is full — tell the client and keep them in lobby-select */
                            net_send(pend->sock, "gmsg *** Room is full — cannot join.");
                            /* Re-send current lobby list so their display is up to date */
                            { int li3;
                              char lbuf[MAX_MSG_LEN];
                              for (li3=0; li3<g_server.lobby_count; li3++) {
                                  format_lobbyinfo(lbuf, sizeof(lbuf), &g_server.lobbies[li3], 0);
                                  net_send(pend->sock, lbuf);
                              }
                              net_send(pend->sock, "lobbylistend");
                            }
                            /* Stay pending — don't deactivate */
                            continue;
                        } else {
                            /* Copy IP, admin flag, privilege, and client info */
                            strncpy(player->ip, pend->ip, sizeof(player->ip)-1);
                            player->is_admin   = auth_is_admin(nick);
                            player->priv_level = auth_get_priv_level(nick);
                            player->input_capable=pend->input_capable;
                            strncpy(player->client_type,     pend->client_type,     sizeof(player->client_type)-1);
                            strncpy(player->client_platform, pend->client_platform, sizeof(player->client_platform)-1);
                            strncpy(player->client_version,  pend->client_version,  sizeof(player->client_version)-1);
                            if(player->priv_level>1){
                                /* Send privilege level to client (1=admin already gets adminok) */
                                char plbuf[32];
                                snprintf(plbuf,sizeof(plbuf),"privlevel %d",player->priv_level);
                                net_send(player->sock,plbuf);
                            }
                        }
                        pend->active = 0;
                    }
                    continue;
                }
            }
            } while (n == 1 && pend->active && pend->recv_len > 0);
        }

        /* Registered players */
        for (i = 0; i < g_server.lobby_count; i++) {
            Lobby *lb = &g_server.lobbies[i];
            for (j = 0; j < MAX_PLAYERS; j++) {
                Player *p = &lb->players[j];
                if (!p->connected || p->sock == INVALID_SOCK) continue;
                if (!FD_ISSET(p->sock, &read_fds)) continue;
                /* Drain ALL buffered messages per player — not just one per tick.
                 * Without this, rapid-send clients (hard drop) queue many messages
                 * and "playerlost" may be delayed by N*50ms (one msg per select). */
                do {
                n = net_recv(p->sock, p->recv_buf, &p->recv_len, line, sizeof(line));
                if (n < 0) { lobby_remove_player(lb, p); break; }
                if (n == 1 && line[0]) {
                    char cmd[MAX_MSG_LEN], args[8][MAX_MSG_LEN];
                    int np = proto_parse(line, cmd, args, 8);
                    if (strcmp(cmd, "join") == 0 && np >= 2) {
                        int new_id = atoi(args[0]);
                        Lobby *new_lb = server_find_lobby(&g_server, new_id);
                        if (new_lb && new_lb != lb) {
                            if (lb->game_state == STATE_PLAYING && !p->spectator) {
                                net_send(p->sock, "gmsg *** Cannot switch lobby during a game");
                            } else {
                                char sn[MAX_NICK_LEN], st[MAX_NICK_LEN], sr[RECV_BUF_SIZE];
                                int  sl = p->recv_len;
                                sock_t ss = p->sock;
                                strncpy(sn, p->nick, MAX_NICK_LEN-1);
                                strncpy(st, p->team, MAX_NICK_LEN-1);
                                memcpy(sr, p->recv_buf, p->recv_len);
                                p->sock = INVALID_SOCK;
                                lobby_remove_player(lb, p);
                                Player *np2 = lobby_add_player(new_lb, ss, sn, st, sr, sl);
                                if (!np2) { net_send(ss, "noconnecting Lobby is full"); sock_close(ss); }
                            }
                        } else if (new_lb == lb) {
                            /* Already in this lobby — nothing to do.
                               The client should have sent leaveLobby before
                               navigating away; if it didn't, just ignore. */
                            net_send(p->sock, "gmsg *** Already in this lobby");
                        } else {
                            net_send(p->sock, "gmsg *** No such lobby");
                        }
                    } else {
                        lobby_handle_message(lb, p, line);
                    }
                }
                /* Keep draining while there are complete lines in the buffer */
                } while (n == 1 && p->recv_len > 0 && p->sock != INVALID_SOCK);
            }
        }
    }

    /* Cleanup */
    for (i = 0; i < MAX_PENDING; i++)
        if (g_pending[i].active) sock_close(g_pending[i].sock);
    for (i = 0; i < g_server.lobby_count; i++) {
        Lobby *lb = &g_server.lobbies[i];
        for (j = 0; j < MAX_PLAYERS; j++)
            if (lb->players[j].connected && lb->players[j].sock != INVALID_SOCK)
                sock_close(lb->players[j].sock);
    }
    sock_close(g_server.listen_sock);
    server_directory_stop();
    server_api_shutdown();
    auth_close();
    net_cleanup();
    log_info("Server stopped.");
}
