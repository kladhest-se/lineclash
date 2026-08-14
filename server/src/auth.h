#ifndef AUTH_H
#define AUTH_H
#include <sqlite3.h>
/* auth.h — Server-side auth with HMAC-SHA256 challenge-response + SQLite stats. */

#define AUTH_HASH_LEN  65
#define AUTH_NONCE_LEN 65

void       sha256_hex(const char *input, char out[AUTH_HASH_LEN]);
void       hmac_sha256_hex(const unsigned char key[32], const char *msg,
                           char out[AUTH_HASH_LEN]);
void       auth_make_nonce(char out[AUTH_NONCE_LEN]);

typedef enum { AUTH_OK=0, AUTH_NEW_USER, AUTH_WRONG_PASSWORD, AUTH_ERROR } AuthResult;

int        auth_open(const char *path);
const char *auth_get_db_path(void);
sqlite3    *auth_get_db(void);
void       auth_close(void);
int        auth_count_users(void);
int        auth_nick_exists(const char *nick);
AuthResult auth_verify_login(const char *nick, const char *nonce_hex, const char *hmac_hex);
AuthResult auth_register(const char *nick, const char *pw_hash_hex);

void       auth_get_team(const char *nick, char *team_out, int maxlen);
void       auth_set_team(const char *nick, const char *team);

int        auth_get_wins(const char *nick);
void       auth_add_win(const char *nick);
void       auth_add_attacks(const char *nick, int count);
void       auth_add_taunts(const char *nick, int n);
void       auth_add_specials(const char *nick, int n);
void       auth_add_blocks(const char *nick, int n);
void       auth_add_lines(const char *nick, int n);
void       auth_add_messages(const char *nick, int n);
void       auth_add_kick_server(const char *nick);
void       auth_add_kick_room(const char *nick);
void       auth_add_ban(const char *nick);
/* Status tracking */
void       auth_set_status(const char *nick, const char *status); /* "offline"/"idle"/"playing" */
/* Game statistics */
void       auth_game_stat_start(const char *nick, int room_id);
void       auth_game_stat_end(const char *nick, int room_id, int won);
int        auth_game_start(int room_id, int player_count);
void       auth_game_end(int room_id, const char *winner_nick, const char *winner_team);

void       auth_get_lobby_settings(int room_id, int *no_attacks, int *last_player_wins, int *speedup_enabled);
void       auth_set_lobby_settings(int room_id, int no_attacks, int last_player_wins, int speedup_enabled);
void       auth_get_lobby_settings_full(int room_id, int *no_attacks, int *last_player_wins, int *no_taunts, int *speedup_enabled, int *garbage_system, int *garbage_targeting);
void       auth_set_lobby_settings_full(int room_id, int no_attacks, int last_player_wins, int no_taunts, int speedup_enabled, int garbage_system, int garbage_targeting);

/* Individual leaderboard — sorted by wins desc, includes attack count */
#define LEADERBOARD_MAX 20
typedef struct {
    char nick[32];
    char team[32];
    int  wins;
    int  attacks;
} LbRow;
int auth_get_leaderboard(LbRow *rows, int max_rows);

/* Team leaderboard — sum wins per team name */
typedef struct {
    char team[32];
    int  wins;
    int  members;
} TeamLbRow;

#define RECENT_GAMES_MAX 50
typedef struct {
    int  game_id, room_id, player_count;
    char started_at[32], ended_at[32];
    char winner_nick[32], winner_team[32];
} RecentGame;

int  auth_get_room_player_wins(int room_id, LbRow *rows, int max_rows);
int  auth_get_room_team_wins(int room_id, TeamLbRow *rows, int max_rows);
int  auth_get_recent_games(int limit, RecentGame *out, int max_out);
int auth_get_team_leaderboard(TeamLbRow *rows, int max_rows);

#endif /* AUTH_H */

/* ---- Admin & ban management ---- */
int  auth_is_admin(const char *nick);
int  auth_get_priv_level(const char *nick); /* 1=admin, 2=mgr, 3=normal */
void auth_set_priv_level(const char *nick, int priv); /* store 1/2/3 in admin column */
void auth_set_admin(const char *nick, int is_admin);
int  auth_is_activated(const char *nick);
void auth_set_activated(const char *nick, int on);
int  auth_is_banned_nick(const char *nick);
void auth_ban_nick(const char *nick);
void auth_unban_nick(const char *nick);
void auth_delete_user(const char *nick);

/* IP-based bans (no account required) */
int  auth_is_banned_ip(const char *ip);
void auth_ban_ip(const char *ip, const char *reason, const char *banned_by);
void auth_unban_ip(const char *ip);
void auth_set_room_topic(int room_id, const char *topic);
void auth_get_room_bots(int room_id, int *bot_count, int *bot_difficulty);
void auth_set_room_bots(int room_id, int bot_count, int bot_difficulty);
void auth_get_room_topic(int room_id, char *out, int outsz);

/* List users — for admin tool. buf receives newline-separated lines.
   Returns number of users written. */
int  auth_list_users(char *buf, int bufsz);

/* ---- Additional admin helpers ---- */
int  auth_reset_password(const char *nick, const char *pw_hash_hex);
int  auth_get_raw_hash(const char *nick, char *out, int outsz, int *outlen);
int  auth_list_all_nicks(char *out, int outsz, const char *filter);
int  auth_delete_corrupt_dupes(const char *nick);
int  auth_fix_hash(const char *nick, const char *pw_hash_hex);
int  auth_list_banned_ips(char *buf, int bufsz);
