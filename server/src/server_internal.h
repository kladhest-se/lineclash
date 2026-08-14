#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H
/* server_internal.h — private types shared between server.c and server_cmd.c */
#include "../../shared/include/lineclash.h"

#define MAX_PENDING 32

typedef struct {
    sock_t sock;
    char   recv_buf[RECV_BUF_SIZE];
    int    recv_len;
    int    active;
    int    auth_ok;
    char   nonce[65];
    char   pending_nick[MAX_NICK_LEN];
    char   ip[48];
    int    nick_is_new;
    int    is_tool;        /* 1 = admin tool session, not a game client */
    int    is_room_kicked; /* 1 = returned from room-kick, allow reconnect */
    char   client_type[32];
    char   client_platform[32];
    char   client_version[16];
} Pending;

/* Globals exposed to server_cmd.c */
extern Server  g_server;
extern unsigned long g_stat_bytes_sent, g_stat_msgs_sent;
extern unsigned long g_stat_bytes_recv, g_stat_msgs_recv;
extern unsigned long g_stat_field_updates;
extern unsigned long g_stat_games_started, g_stat_games_ended;
extern unsigned long g_stat_specials_fired, g_stat_lines_cleared;
extern unsigned long g_stat_kicks, g_stat_connects, g_stat_disconnects;
extern unsigned long g_stat_log_lines;
extern time_t        g_stat_start_time;
extern Pending g_pending[MAX_PENDING];

/* Functions in server.c used by server_cmd.c */
void   lobby_remove_player(Lobby *lb, Player *p);
void   lobby_broadcast(Lobby *lb, const char *msg, int exclude_slot);
Lobby *server_find_lobby(Server *srv, int id);
void   schat_broadcast(const char *msg);  /* to all rooms + lobby */
int    kick_player_from_room(Lobby *lb, const char *nick, const char *by_nick, int *target_priv);
void   send_lobby_list(sock_t sock);
void   kick_player_ip(const char *nick, char *ip_out, int ip_sz);
void   schat_lobby(const char *msg);      /* to lobby-select only */

#endif

/* server_api.c — HTTP JSON status API */
void   server_api_init(void);
void   server_api_shutdown(void);
sock_t server_api_get_sock(void);
void   server_api_accept(void);
